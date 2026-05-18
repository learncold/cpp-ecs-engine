#include "domain/GeometryNormalizer.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string_view>
#include <utility>

namespace safecrowd::domain {
namespace {

constexpr double kGeometryEpsilon = 1e-9;
constexpr double kFallbackObstacleAreaThreshold = 4.0;
constexpr double kFallbackConfidence = 0.55;

bool hasMinimumVertices(const std::vector<Point2D>& vertices, std::size_t minimum) {
    return vertices.size() >= minimum;
}

Polygon2D toPolygon(const Polyline2D& polyline) {
    return {.outline = polyline.vertices};
}

double segmentLength(const LineSegment2D& segment) {
    const auto dx = segment.end.x - segment.start.x;
    const auto dy = segment.end.y - segment.start.y;
    return std::sqrt((dx * dx) + (dy * dy));
}

double signedRingArea(const std::vector<Point2D>& ring) {
    if (ring.size() < 3) {
        return 0.0;
    }

    double area = 0.0;
    for (std::size_t index = 0; index < ring.size(); ++index) {
        const auto& current = ring[index];
        const auto& next = ring[(index + 1) % ring.size()];
        area += (current.x * next.y) - (next.x * current.y);
    }
    return area * 0.5;
}

double polygonArea(const Polygon2D& polygon) {
    double area = std::fabs(signedRingArea(polygon.outline));
    for (const auto& hole : polygon.holes) {
        area -= std::fabs(signedRingArea(hole));
    }
    return std::max(0.0, area);
}

Point2D transformPoint(const Point2D& point, const RawBlockReference2D& block) {
    const auto scaledX = point.x * block.scaleX;
    const auto scaledY = point.y * block.scaleY;
    const auto cosTheta = std::cos(block.rotationRadians);
    const auto sinTheta = std::sin(block.rotationRadians);

    return {
        .x = (scaledX * cosTheta) - (scaledY * sinTheta) + block.insertionPoint.x,
        .y = (scaledX * sinTheta) + (scaledY * cosTheta) + block.insertionPoint.y,
    };
}

Polyline2D transformPolyline(const Polyline2D& polyline, const RawBlockReference2D& block) {
    Polyline2D transformed{.closed = polyline.closed};
    transformed.vertices.reserve(polyline.vertices.size());

    for (const auto& vertex : polyline.vertices) {
        transformed.vertices.push_back(transformPoint(vertex, block));
    }

    return transformed;
}

Polygon2D transformPolygon(const Polygon2D& polygon, const RawBlockReference2D& block) {
    Polygon2D transformed;
    transformed.outline.reserve(polygon.outline.size());

    for (const auto& vertex : polygon.outline) {
        transformed.outline.push_back(transformPoint(vertex, block));
    }

    transformed.holes.reserve(polygon.holes.size());
    for (const auto& hole : polygon.holes) {
        std::vector<Point2D> transformedHole;
        transformedHole.reserve(hole.size());
        for (const auto& vertex : hole) {
            transformedHole.push_back(transformPoint(vertex, block));
        }
        transformed.holes.push_back(std::move(transformedHole));
    }

    return transformed;
}

OpeningKind toOpeningKind(ImportElementSemantic semantic) {
    switch (semantic) {
    case ImportElementSemantic::Exit:
        return OpeningKind::Exit;
    case ImportElementSemantic::Opening:
        return OpeningKind::Doorway;
    default:
        return OpeningKind::Unknown;
    }
}

SourceTrace inheritBlockChildTrace(const SourceTrace& insertTrace, SourceTrace childTrace) {
    childTrace.parentSourceId = insertTrace.sourceId;

    if (childTrace.layerName.empty() || childTrace.layerName == "0") {
        childTrace.layerName = insertTrace.layerName;
    }
    if (childTrace.objectName.empty()) {
        childTrace.objectName = insertTrace.objectName;
    }

    return childTrace;
}

std::vector<std::string> collectSourceIds(const SourceTrace& trace) {
    std::vector<std::string> sourceIds;

    if (!trace.sourceId.empty()) {
        sourceIds.push_back(trace.sourceId);
    }
    if (!trace.parentSourceId.empty() && trace.parentSourceId != trace.sourceId) {
        sourceIds.push_back(trace.parentSourceId);
    }

    return sourceIds;
}

void appendIssue(
    std::vector<ImportIssue>& issues,
    ImportIssueSeverity severity,
    ImportIssueCode code,
    std::string message,
    std::string sourceId,
    std::string suggestion = {},
    double confidence = 1.0) {
    issues.push_back({
        .severity = severity,
        .code = code,
        .message = std::move(message),
        .sourceId = std::move(sourceId),
        .suggestion = std::move(suggestion),
        .confidence = confidence,
    });
}

class Normalizer {
public:
    Normalizer(const RawImportModel& rawModel, GeometryNormalizerOptions options)
        : rawModel_(rawModel),
          options_(std::move(options)) {
        result_.geometry.levelId = rawModel_.levelId;
    }

    GeometryNormalizerResult run() {
        for (const auto& entity : rawModel_.entities) {
            normalizeEntity(entity);
        }
        return std::move(result_);
    }

private:
    const RawImportModel& rawModel_;
    GeometryNormalizerOptions options_;
    GeometryNormalizerResult result_;
    std::size_t walkableCounter_{0};
    std::size_t wallCounter_{0};
    std::size_t openingCounter_{0};
    std::size_t obstacleCounter_{0};

    bool fallbackEnabled() const noexcept {
        return options_.fallbackPolicy == ImportFallbackPolicy::ReviewableGeometry;
    }

    std::string nextCanonicalId(std::string_view prefix, std::size_t& counter) {
        ++counter;
        std::ostringstream stream;
        stream << prefix << '-' << counter;
        return stream.str();
    }

    void appendTraceRef(const std::string& targetId, const std::vector<std::string>& sourceIds) {
        result_.traceRefs.push_back({
            .targetId = targetId,
            .sourceIds = sourceIds,
            .canonicalIds = {targetId},
        });
    }

    ImportElementSemantic classify(const RawEntity2D& entity) const {
        return options_.semanticRules.classify(entity.trace, entity.metadata);
    }

    void addWallFromPolyline(const Polyline2D& polyline, const SourceTrace& trace) {
        if (!hasMinimumVertices(polyline.vertices, 2)) {
            appendIssue(
                result_.issues,
                ImportIssueSeverity::Warning,
                ImportIssueCode::InvalidGeometry,
                "Wall polyline does not have enough vertices.",
                trace.sourceId,
                "Review the source DXF wall entity or redraw the wall in Layout Review.");
            return;
        }

        const auto sourceIds = collectSourceIds(trace);
        const auto segmentCount = polyline.closed ? polyline.vertices.size() : polyline.vertices.size() - 1;
        for (std::size_t i = 0; i < segmentCount; ++i) {
            const auto& start = polyline.vertices[i];
            const auto& end = polyline.vertices[(i + 1) % polyline.vertices.size()];
            const auto canonicalId = nextCanonicalId("wall", wallCounter_);

            result_.geometry.walls.push_back({
                .id = canonicalId,
                .segment = {.start = start, .end = end},
                .sourceIds = sourceIds,
            });
            appendTraceRef(canonicalId, sourceIds);
        }
    }

    void addOpeningFromPolyline(const Polyline2D& polyline, ImportElementSemantic semantic, const SourceTrace& trace) {
        if (!hasMinimumVertices(polyline.vertices, 2)) {
            appendIssue(
                result_.issues,
                ImportIssueSeverity::Warning,
                ImportIssueCode::InvalidGeometry,
                "Opening geometry does not have enough vertices.",
                trace.sourceId,
                "Review the source DXF opening entity or redraw the doorway/exit in Layout Review.");
            return;
        }

        const LineSegment2D span{
            .start = polyline.vertices.front(),
            .end = polyline.vertices.back(),
        };
        const auto sourceIds = collectSourceIds(trace);
        const auto canonicalId = nextCanonicalId("opening", openingCounter_);

        result_.geometry.openings.push_back({
            .id = canonicalId,
            .kind = toOpeningKind(semantic),
            .span = span,
            .width = segmentLength(span),
            .sourceIds = sourceIds,
        });
        appendTraceRef(canonicalId, sourceIds);
    }

    void addWalkablePolygon(const Polygon2D& polygon, const SourceTrace& trace) {
        if (!hasMinimumVertices(polygon.outline, 3)) {
            appendIssue(
                result_.issues,
                ImportIssueSeverity::Warning,
                ImportIssueCode::InvalidGeometry,
                "Walkable polygon does not have enough vertices.",
                trace.sourceId,
                "Redraw or repair the room boundary in Layout Review.");
            return;
        }

        const auto sourceIds = collectSourceIds(trace);
        const auto canonicalId = nextCanonicalId("walkable", walkableCounter_);
        result_.geometry.walkableAreas.push_back({
            .id = canonicalId,
            .polygon = polygon,
            .sourceIds = sourceIds,
        });
        appendTraceRef(canonicalId, sourceIds);
    }

    void addObstaclePolygon(const Polygon2D& polygon, const SourceTrace& trace) {
        if (!hasMinimumVertices(polygon.outline, 3)) {
            appendIssue(
                result_.issues,
                ImportIssueSeverity::Warning,
                ImportIssueCode::InvalidGeometry,
                "Obstacle polygon does not have enough vertices.",
                trace.sourceId,
                "Review or redraw the obstruction in Layout Review.");
            return;
        }

        const auto sourceIds = collectSourceIds(trace);
        const auto canonicalId = nextCanonicalId("obstacle", obstacleCounter_);
        result_.geometry.obstacles.push_back({
            .id = canonicalId,
            .footprint = polygon,
            .sourceIds = sourceIds,
        });
        appendTraceRef(canonicalId, sourceIds);
    }

    void addFallbackPolygon(const Polygon2D& polygon, const SourceTrace& trace, std::string_view sourceDescription) {
        if (!fallbackEnabled() || !hasMinimumVertices(polygon.outline, 3)) {
            return;
        }

        const auto area = polygonArea(polygon);
        if (area <= kGeometryEpsilon) {
            return;
        }

        if (area <= kFallbackObstacleAreaThreshold) {
            addObstaclePolygon(polygon, trace);
            appendIssue(
                result_.issues,
                ImportIssueSeverity::Info,
                ImportIssueCode::UnmappedElement,
                "Inferred an obstruction candidate from unclassified " + std::string(sourceDescription) + ".",
                trace.sourceId,
                "Confirm this small region is really an obstruction.",
                kFallbackConfidence);
            return;
        }

        addWalkablePolygon(polygon, trace);
        appendIssue(
            result_.issues,
            ImportIssueSeverity::Info,
            ImportIssueCode::UnmappedElement,
            "Inferred a walkable area candidate from unclassified " + std::string(sourceDescription) + ".",
            trace.sourceId,
            "Confirm the inferred room area before approving the layout.",
            kFallbackConfidence);
    }

    void normalizeEntity(const RawEntity2D& entity) {
        const auto semantic = classify(entity);
        const auto sourceIds = collectSourceIds(entity.trace);

        if (std::holds_alternative<LineSegment2D>(entity.payload)) {
            const auto& line = std::get<LineSegment2D>(entity.payload);
            if (semantic == ImportElementSemantic::Wall) {
                const auto canonicalId = nextCanonicalId("wall", wallCounter_);
                result_.geometry.walls.push_back({
                    .id = canonicalId,
                    .segment = line,
                    .sourceIds = sourceIds,
                });
                appendTraceRef(canonicalId, sourceIds);
            } else if (semantic == ImportElementSemantic::Opening || semantic == ImportElementSemantic::Exit) {
                const auto canonicalId = nextCanonicalId("opening", openingCounter_);
                result_.geometry.openings.push_back({
                    .id = canonicalId,
                    .kind = toOpeningKind(semantic),
                    .span = line,
                    .width = segmentLength(line),
                    .sourceIds = sourceIds,
                });
                appendTraceRef(canonicalId, sourceIds);
            }
            return;
        }

        if (std::holds_alternative<Polyline2D>(entity.payload)) {
            const auto& polyline = std::get<Polyline2D>(entity.payload);
            if (semantic == ImportElementSemantic::Wall) {
                addWallFromPolyline(polyline, entity.trace);
            } else if (semantic == ImportElementSemantic::Opening || semantic == ImportElementSemantic::Exit) {
                addOpeningFromPolyline(polyline, semantic, entity.trace);
            } else if (polyline.closed) {
                addFallbackPolygon(toPolygon(polyline), entity.trace, "closed polyline");
            }
            return;
        }

        if (std::holds_alternative<Polygon2D>(entity.payload)) {
            normalizePolygon(std::get<Polygon2D>(entity.payload), semantic, entity.trace, "polygon");
            return;
        }

        if (std::holds_alternative<RawArc2D>(entity.payload)) {
            const auto& arc = std::get<RawArc2D>(entity.payload);
            if (semantic == ImportElementSemantic::Wall) {
                addWallFromPolyline(arc.approximation, entity.trace);
            } else if (semantic == ImportElementSemantic::Opening || semantic == ImportElementSemantic::Exit) {
                addOpeningFromPolyline(arc.approximation, semantic, entity.trace);
            }
            return;
        }

        if (std::holds_alternative<RawCircle2D>(entity.payload)) {
            const auto& circle = std::get<RawCircle2D>(entity.payload);
            const auto polygon = toPolygon(circle.approximation);
            normalizePolygon(polygon, semantic, entity.trace, "circle");
            return;
        }

        if (std::holds_alternative<RawHatchBoundary2D>(entity.payload)) {
            const auto& hatch = std::get<RawHatchBoundary2D>(entity.payload);
            normalizePolygon(hatch.boundary, semantic, entity.trace, "hatch region");
            return;
        }

        if (std::holds_alternative<RawBlockReference2D>(entity.payload)) {
            normalizeBlockReference(entity, std::get<RawBlockReference2D>(entity.payload));
            return;
        }
    }

    void normalizePolygon(
        const Polygon2D& polygon,
        ImportElementSemantic semantic,
        const SourceTrace& trace,
        std::string_view sourceDescription) {
        if (semantic == ImportElementSemantic::Walkable) {
            addWalkablePolygon(polygon, trace);
        } else if (semantic == ImportElementSemantic::Obstacle) {
            addObstaclePolygon(polygon, trace);
        } else if (semantic == ImportElementSemantic::Wall) {
            addWallFromPolyline({
                .vertices = polygon.outline,
                .closed = true,
            }, trace);
        } else {
            addFallbackPolygon(polygon, trace, sourceDescription);
        }
    }

    void normalizeBlockReference(const RawEntity2D& entity, const RawBlockReference2D& block) {
        for (const auto& polylineChild : block.polylines) {
            RawEntity2D child;
            child.kind = RawEntityKind::Polyline;
            child.trace = inheritBlockChildTrace(entity.trace, polylineChild.trace);
            child.payload = transformPolyline(polylineChild.geometry, block);
            child.metadata = polylineChild.metadata;
            normalizeEntity(child);
        }

        for (const auto& polygonChild : block.polygons) {
            RawEntity2D child;
            child.kind = RawEntityKind::Polygon;
            child.trace = inheritBlockChildTrace(entity.trace, polygonChild.trace);
            child.payload = transformPolygon(polygonChild.geometry, block);
            child.metadata = polygonChild.metadata;
            normalizeEntity(child);
        }
    }
};

}  // namespace

GeometryNormalizerResult GeometryNormalizer::normalize(
    const RawImportModel& rawModel,
    const GeometryNormalizerOptions& options) const {
    return Normalizer(rawModel, options).run();
}

}  // namespace safecrowd::domain
