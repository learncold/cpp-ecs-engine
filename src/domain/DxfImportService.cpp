#include "domain/DxfImportService.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "domain/FacilityLayoutBuilder.h"
#include "domain/ImportValidationService.h"

namespace safecrowd::domain {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct DxfGroup {
    int code{0};
    std::string value{};
};

struct BlockDefinition {
    std::string name{};
    std::vector<RawTracedPolyline2D> polylines{};
    std::vector<RawTracedPolygon2D> polygons{};
};

enum class GeometrySemantic {
    Unknown,
    Wall,
    Opening,
    Exit,
    Obstacle,
    Walkable,
};

std::string trim(std::string value) {
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };

    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) { return !isSpace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) { return !isSpace(ch); }).base(), value.end());
    return value;
}

std::string toUpper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::optional<double> parseDouble(const std::string& text) {
    try {
        std::size_t index = 0;
        const double value = std::stod(text, &index);
        if (index == text.size()) {
            return value;
        }
    } catch (...) {
    }

    return std::nullopt;
}

std::optional<int> parseInt(const std::string& text) {
    try {
        std::size_t index = 0;
        const int value = std::stoi(text, &index);
        if (index == text.size()) {
            return value;
        }
    } catch (...) {
    }

    return std::nullopt;
}

std::vector<DxfGroup> loadGroups(const std::filesystem::path& sourcePath) {
    std::ifstream input(sourcePath);
    if (!input) {
        return {};
    }

    std::vector<DxfGroup> groups;
    std::string codeLine;
    std::string valueLine;

    while (std::getline(input, codeLine)) {
        if (!std::getline(input, valueLine)) {
            break;
        }

        const auto code = parseInt(trim(codeLine));
        if (!code.has_value()) {
            continue;
        }

        groups.push_back({
            .code = *code,
            .value = trim(valueLine),
        });
    }

    return groups;
}

ImportUnit parseUnits(const std::vector<DxfGroup>& groups) {
    for (std::size_t i = 0; i + 1 < groups.size(); ++i) {
        if (groups[i].code == 9 && toUpper(groups[i].value) == "$INSUNITS" && groups[i + 1].code == 70) {
            const auto unitCode = parseInt(groups[i + 1].value);
            if (!unitCode.has_value()) {
                return ImportUnit::Unknown;
            }

            switch (*unitCode) {
            case 4:
                return ImportUnit::Millimeter;
            case 5:
                return ImportUnit::Centimeter;
            case 6:
                return ImportUnit::Meter;
            default:
                return ImportUnit::Unknown;
            }
        }
    }

    return ImportUnit::Unknown;
}

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

GeometrySemantic classifySemantic(const SourceTrace& trace) {
    const auto signature = toUpper(trace.layerName + " " + trace.objectName);

    if (signature.find("EXIT") != std::string::npos) {
        return GeometrySemantic::Exit;
    }
    if (signature.find("OPEN") != std::string::npos || signature.find("DOOR") != std::string::npos || signature.find("PORTAL") != std::string::npos) {
        return GeometrySemantic::Opening;
    }
    if (signature.find("WALL") != std::string::npos || signature.find("BARRIER") != std::string::npos) {
        return GeometrySemantic::Wall;
    }
    if (signature.find("OBST") != std::string::npos || signature.find("COLUMN") != std::string::npos || signature.find("FURN") != std::string::npos) {
        return GeometrySemantic::Obstacle;
    }
    if (signature.find("WALK") != std::string::npos || signature.find("FLOOR") != std::string::npos || signature.find("SPACE") != std::string::npos) {
        return GeometrySemantic::Walkable;
    }

    return GeometrySemantic::Unknown;
}

OpeningKind toOpeningKind(GeometrySemantic semantic) {
    switch (semantic) {
    case GeometrySemantic::Exit:
        return OpeningKind::Exit;
    case GeometrySemantic::Opening:
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

void appendLayoutTraceRef(std::vector<ImportTraceRef>& traceRefs, const std::string& targetId, const ElementProvenance& provenance) {
    traceRefs.push_back({
        .targetId = targetId,
        .sourceIds = provenance.sourceIds,
        .canonicalIds = provenance.canonicalIds,
    });
}

void appendLayoutTraceRefs(const FacilityLayout2D& layout, std::vector<ImportTraceRef>& traceRefs) {
    for (const auto& zone : layout.zones) {
        appendLayoutTraceRef(traceRefs, zone.id, zone.provenance);
    }

    for (const auto& connection : layout.connections) {
        appendLayoutTraceRef(traceRefs, connection.id, connection.provenance);
    }

    for (const auto& barrier : layout.barriers) {
        appendLayoutTraceRef(traceRefs, barrier.id, barrier.provenance);
    }

    for (const auto& control : layout.controls) {
        appendLayoutTraceRef(traceRefs, control.id, control.provenance);
    }
}

class DxfAsciiParser {
public:
    DxfAsciiParser(std::filesystem::path sourcePath, std::vector<DxfGroup> groups)
        : sourcePath_(std::move(sourcePath)),
          groups_(std::move(groups)) {
    }

    ImportResult parse() {
        ImportResult result;

        if (groups_.empty()) {
            result.issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::FileReadFailed,
                .message = "Failed to read DXF file.",
                .sourceId = sourcePath_.generic_string(),
            });
            result.reviewStatus = ImportReviewStatus::Rejected;
            return result;
        }

        rawModel_.format = ImportedFileFormat::Dxf;
        rawModel_.unit = parseUnits(groups_);
        rawModel_.sourceDocumentId = sourcePath_.filename().generic_string();
        rawModel_.levelId = sourcePath_.stem().generic_string();
        canonicalGeometry_.levelId = rawModel_.levelId;

        parseSections();
        buildCanonicalGeometry();

        if (canonicalGeometry_.walkableAreas.empty()
            && canonicalGeometry_.walls.empty()
            && canonicalGeometry_.openings.empty()
            && canonicalGeometry_.obstacles.empty()) {
            issues_.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::MissingSourceGeometry,
                .message = "No importable geometry was extracted from the DXF file.",
                .sourceId = rawModel_.sourceDocumentId,
            });
        }

        result.rawModel = rawModel_;
        result.canonicalGeometry = canonicalGeometry_;
        result.issues = issues_;
        result.traceRefs = traceRefs_;
        result.reviewStatus = hasBlockingImportIssue(result.issues) ? ImportReviewStatus::Rejected : ImportReviewStatus::Pending;
        return result;
    }

private:
    std::filesystem::path sourcePath_{};
    std::vector<DxfGroup> groups_{};
    std::size_t index_{0};
    RawImportModel rawModel_{};
    CanonicalGeometry canonicalGeometry_{};
    std::vector<ImportIssue> issues_{};
    std::vector<ImportTraceRef> traceRefs_{};
    std::map<std::string, BlockDefinition> blockDefinitions_{};
    std::size_t sourceCounter_{0};
    std::size_t walkableCounter_{0};
    std::size_t wallCounter_{0};
    std::size_t openingCounter_{0};
    std::size_t obstacleCounter_{0};

    bool hasMore() const noexcept {
        return index_ < groups_.size();
    }

    const DxfGroup& current() const {
        return groups_[index_];
    }

    bool at(int code, std::string_view value) const {
        return hasMore() && current().code == code && current().value == value;
    }

    const DxfGroup& advance() {
        return groups_[index_++];
    }

    void parseSections() {
        while (hasMore()) {
            if (!at(0, "SECTION")) {
                advance();
                continue;
            }

            advance();
            if (!hasMore() || current().code != 2) {
                continue;
            }

            const auto sectionName = current().value;
            advance();

            if (sectionName == "BLOCKS") {
                parseBlocksSection();
            } else if (sectionName == "ENTITIES") {
                parseEntitiesSection();
            } else {
                skipToSectionEnd();
            }
        }
    }

    void skipToSectionEnd() {
        while (hasMore()) {
            if (at(0, "ENDSEC")) {
                advance();
                return;
            }
            advance();
        }
    }

    void parseBlocksSection() {
        while (hasMore() && !at(0, "ENDSEC")) {
            if (at(0, "BLOCK")) {
                parseBlockDefinition();
            } else {
                advance();
            }
        }

        if (at(0, "ENDSEC")) {
            advance();
        }
    }

    void parseBlockDefinition() {
        advance();

        BlockDefinition definition;

        while (hasMore()) {
            if (at(0, "ENDBLK")) {
                advance();
                break;
            }

            if (at(0, "LWPOLYLINE")) {
                auto entity = parseLwPolylineEntity();
                if (std::holds_alternative<Polyline2D>(entity.payload)) {
                    definition.polylines.push_back({
                        .trace = entity.trace,
                        .geometry = std::get<Polyline2D>(entity.payload),
                        .metadata = entity.metadata,
                    });
                } else if (std::holds_alternative<Polygon2D>(entity.payload)) {
                    definition.polygons.push_back({
                        .trace = entity.trace,
                        .geometry = std::get<Polygon2D>(entity.payload),
                        .metadata = entity.metadata,
                    });
                }
                continue;
            }

            if (at(0, "POLYLINE")) {
                auto entity = parseClassicPolylineEntity();
                if (std::holds_alternative<Polyline2D>(entity.payload)) {
                    definition.polylines.push_back({
                        .trace = entity.trace,
                        .geometry = std::get<Polyline2D>(entity.payload),
                        .metadata = entity.metadata,
                    });
                } else if (std::holds_alternative<Polygon2D>(entity.payload)) {
                    definition.polygons.push_back({
                        .trace = entity.trace,
                        .geometry = std::get<Polygon2D>(entity.payload),
                        .metadata = entity.metadata,
                    });
                }
                continue;
            }

            if (at(0, "LINE")) {
                auto entity = parseLineEntity();
                if (std::holds_alternative<LineSegment2D>(entity.payload)) {
                    const auto& segment = std::get<LineSegment2D>(entity.payload);
                    definition.polylines.push_back({
                        .trace = entity.trace,
                        .geometry = {
                            .vertices = {segment.start, segment.end},
                            .closed = false,
                        },
                        .metadata = entity.metadata,
                    });
                }
                continue;
            }

            if (current().code == 2 && definition.name.empty()) {
                definition.name = current().value;
            }

            advance();
        }

        if (!definition.name.empty()) {
            blockDefinitions_[definition.name] = std::move(definition);
        }
    }

    void parseEntitiesSection() {
        while (hasMore() && !at(0, "ENDSEC")) {
            if (at(0, "LWPOLYLINE")) {
                rawModel_.entities.push_back(parseLwPolylineEntity());
                continue;
            }

            if (at(0, "POLYLINE")) {
                rawModel_.entities.push_back(parseClassicPolylineEntity());
                continue;
            }

            if (at(0, "LINE")) {
                rawModel_.entities.push_back(parseLineEntity());
                continue;
            }

            if (at(0, "INSERT")) {
                rawModel_.entities.push_back(parseInsertEntity());
                continue;
            }

            if (current().code == 0) {
                issues_.push_back({
                    .severity = ImportIssueSeverity::Warning,
                    .code = ImportIssueCode::UnsupportedEntity,
                    .message = "Unsupported DXF entity type: " + current().value,
                    .sourceId = rawModel_.sourceDocumentId,
                });
            }

            advance();
        }

        if (at(0, "ENDSEC")) {
            advance();
        }
    }

    RawEntity2D parseLineEntity() {
        RawEntity2D entity;
        entity.kind = RawEntityKind::Line;

        advance();

        Point2D start{};
        Point2D end{};

        while (hasMore() && current().code != 0) {
            const auto group = advance();

            switch (group.code) {
            case 8:
                entity.trace.layerName = group.value;
                break;
            case 10:
                start.x = parseDouble(group.value).value_or(start.x);
                break;
            case 20:
                start.y = parseDouble(group.value).value_or(start.y);
                break;
            case 11:
                end.x = parseDouble(group.value).value_or(end.x);
                break;
            case 21:
                end.y = parseDouble(group.value).value_or(end.y);
                break;
            default:
                break;
            }
        }

        entity.trace.sourceId = nextSourceId("line");
        entity.payload = LineSegment2D{.start = start, .end = end};
        return entity;
    }

    RawEntity2D parseLwPolylineEntity() {
        RawEntity2D entity;
        entity.kind = RawEntityKind::Polyline;

        advance();

        Polyline2D polyline;
        std::optional<double> pendingX;

        while (hasMore() && current().code != 0) {
            const auto group = advance();

            switch (group.code) {
            case 8:
                entity.trace.layerName = group.value;
                break;
            case 70: {
                const auto flags = parseInt(group.value).value_or(0);
                polyline.closed = (flags & 1) != 0;
                break;
            }
            case 10:
                pendingX = parseDouble(group.value);
                break;
            case 20:
                if (pendingX.has_value()) {
                    polyline.vertices.push_back({
                        .x = *pendingX,
                        .y = parseDouble(group.value).value_or(0.0),
                    });
                    pendingX.reset();
                }
                break;
            default:
                break;
            }
        }

        entity.trace.sourceId = nextSourceId("polyline");

        if (polyline.closed && hasMinimumVertices(polyline.vertices, 3)) {
            entity.kind = RawEntityKind::Polygon;
            entity.payload = toPolygon(polyline);
        } else {
            entity.payload = polyline;
        }

        return entity;
    }

    RawEntity2D parseClassicPolylineEntity() {
        RawEntity2D entity;
        entity.kind = RawEntityKind::Polyline;

        advance();

        Polyline2D polyline;

        while (hasMore() && current().code != 0) {
            const auto group = advance();

            switch (group.code) {
            case 8:
                entity.trace.layerName = group.value;
                break;
            case 70: {
                const auto flags = parseInt(group.value).value_or(0);
                polyline.closed = (flags & 1) != 0;
                break;
            }
            default:
                break;
            }
        }

        while (hasMore()) {
            if (at(0, "SEQEND")) {
                advance();
                break;
            }

            if (!at(0, "VERTEX")) {
                if (current().code == 0) {
                    break;
                }
                advance();
                continue;
            }

            advance();

            Point2D vertex{};
            std::optional<double> vertexX;

            while (hasMore() && current().code != 0) {
                const auto group = advance();

                switch (group.code) {
                case 8:
                    if (entity.trace.layerName.empty() || entity.trace.layerName == "0") {
                        entity.trace.layerName = group.value;
                    }
                    break;
                case 10:
                    vertexX = parseDouble(group.value);
                    break;
                case 20:
                    if (vertexX.has_value()) {
                        vertex = {
                            .x = *vertexX,
                            .y = parseDouble(group.value).value_or(0.0),
                        };
                    }
                    break;
                default:
                    break;
                }
            }

            polyline.vertices.push_back(vertex);
        }

        entity.trace.sourceId = nextSourceId("polyline");

        if (polyline.closed && hasMinimumVertices(polyline.vertices, 3)) {
            entity.kind = RawEntityKind::Polygon;
            entity.payload = toPolygon(polyline);
        } else {
            entity.payload = polyline;
        }

        return entity;
    }

    RawEntity2D parseInsertEntity() {
        RawEntity2D entity;
        entity.kind = RawEntityKind::BlockReference;

        RawBlockReference2D blockReference;
        std::string blockName;

        advance();

        while (hasMore() && current().code != 0) {
            const auto group = advance();

            switch (group.code) {
            case 2:
                blockName = group.value;
                blockReference.blockName = group.value;
                entity.trace.objectName = group.value;
                break;
            case 8:
                entity.trace.layerName = group.value;
                break;
            case 10:
                blockReference.insertionPoint.x = parseDouble(group.value).value_or(blockReference.insertionPoint.x);
                break;
            case 20:
                blockReference.insertionPoint.y = parseDouble(group.value).value_or(blockReference.insertionPoint.y);
                break;
            case 41:
                blockReference.scaleX = parseDouble(group.value).value_or(blockReference.scaleX);
                break;
            case 42:
                blockReference.scaleY = parseDouble(group.value).value_or(blockReference.scaleY);
                break;
            case 50:
                blockReference.rotationRadians = parseDouble(group.value).value_or(0.0) * (kPi / 180.0);
                break;
            default:
                break;
            }
        }

        entity.trace.sourceId = nextSourceId("insert");

        const auto blockIt = blockDefinitions_.find(blockName);
        if (blockIt != blockDefinitions_.end()) {
            blockReference.polylines = blockIt->second.polylines;
            blockReference.polygons = blockIt->second.polygons;
        } else {
            issues_.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::MissingBlockDefinition,
                .message = "DXF insert references a missing block definition: " + blockName,
                .sourceId = entity.trace.sourceId,
                .targetId = blockName,
            });
        }

        entity.payload = std::move(blockReference);
        return entity;
    }

    std::string nextSourceId(std::string_view prefix) {
        ++sourceCounter_;
        std::ostringstream stream;
        stream << prefix << '-' << sourceCounter_;
        return stream.str();
    }

    void appendTraceRef(const std::string& targetId, const std::vector<std::string>& sourceIds) {
        traceRefs_.push_back({
            .targetId = targetId,
            .sourceIds = sourceIds,
            .canonicalIds = {targetId},
        });
    }

    void addWallFromPolyline(const Polyline2D& polyline, const SourceTrace& trace) {
        if (!hasMinimumVertices(polyline.vertices, 2)) {
            issues_.push_back({
                .severity = ImportIssueSeverity::Warning,
                .code = ImportIssueCode::InvalidGeometry,
                .message = "Wall polyline does not have enough vertices.",
                .sourceId = trace.sourceId,
            });
            return;
        }

        const auto sourceIds = collectSourceIds(trace);
        const auto segmentCount = polyline.closed ? polyline.vertices.size() : polyline.vertices.size() - 1;
        for (std::size_t i = 0; i < segmentCount; ++i) {
            const auto& start = polyline.vertices[i];
            const auto& end = polyline.vertices[(i + 1) % polyline.vertices.size()];
            const auto canonicalId = nextCanonicalId("wall", wallCounter_);

            canonicalGeometry_.walls.push_back({
                .id = canonicalId,
                .segment = {.start = start, .end = end},
                .sourceIds = sourceIds,
            });
            appendTraceRef(canonicalId, sourceIds);
        }
    }

    void addOpeningFromPolyline(const Polyline2D& polyline, GeometrySemantic semantic, const SourceTrace& trace) {
        if (!hasMinimumVertices(polyline.vertices, 2)) {
            issues_.push_back({
                .severity = ImportIssueSeverity::Warning,
                .code = ImportIssueCode::InvalidGeometry,
                .message = "Opening geometry does not have enough vertices.",
                .sourceId = trace.sourceId,
            });
            return;
        }

        const LineSegment2D span{
            .start = polyline.vertices.front(),
            .end = polyline.vertices.back(),
        };
        const auto sourceIds = collectSourceIds(trace);
        const auto canonicalId = nextCanonicalId("opening", openingCounter_);

        canonicalGeometry_.openings.push_back({
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
            issues_.push_back({
                .severity = ImportIssueSeverity::Warning,
                .code = ImportIssueCode::InvalidGeometry,
                .message = "Walkable polygon does not have enough vertices.",
                .sourceId = trace.sourceId,
            });
            return;
        }

        const auto sourceIds = collectSourceIds(trace);
        const auto canonicalId = nextCanonicalId("walkable", walkableCounter_);
        canonicalGeometry_.walkableAreas.push_back({
            .id = canonicalId,
            .polygon = polygon,
            .sourceIds = sourceIds,
        });
        appendTraceRef(canonicalId, sourceIds);
    }

    void addObstaclePolygon(const Polygon2D& polygon, const SourceTrace& trace) {
        if (!hasMinimumVertices(polygon.outline, 3)) {
            issues_.push_back({
                .severity = ImportIssueSeverity::Warning,
                .code = ImportIssueCode::InvalidGeometry,
                .message = "Obstacle polygon does not have enough vertices.",
                .sourceId = trace.sourceId,
            });
            return;
        }

        const auto sourceIds = collectSourceIds(trace);
        const auto canonicalId = nextCanonicalId("obstacle", obstacleCounter_);
        canonicalGeometry_.obstacles.push_back({
            .id = canonicalId,
            .footprint = polygon,
            .sourceIds = sourceIds,
        });
        appendTraceRef(canonicalId, sourceIds);
    }

    void buildCanonicalGeometry() {
        for (const auto& entity : rawModel_.entities) {
            const auto semantic = classifySemantic(entity.trace);
            const auto sourceIds = collectSourceIds(entity.trace);

            if (std::holds_alternative<LineSegment2D>(entity.payload)) {
                const auto& line = std::get<LineSegment2D>(entity.payload);
                if (semantic == GeometrySemantic::Wall) {
                    const auto canonicalId = nextCanonicalId("wall", wallCounter_);
                    canonicalGeometry_.walls.push_back({
                        .id = canonicalId,
                        .segment = line,
                        .sourceIds = sourceIds,
                    });
                    appendTraceRef(canonicalId, sourceIds);
                } else if (semantic == GeometrySemantic::Opening || semantic == GeometrySemantic::Exit) {
                    const auto canonicalId = nextCanonicalId("opening", openingCounter_);
                    canonicalGeometry_.openings.push_back({
                        .id = canonicalId,
                        .kind = toOpeningKind(semantic),
                        .span = line,
                        .width = segmentLength(line),
                        .sourceIds = sourceIds,
                    });
                    appendTraceRef(canonicalId, sourceIds);
                }
                continue;
            }

            if (std::holds_alternative<Polyline2D>(entity.payload)) {
                const auto& polyline = std::get<Polyline2D>(entity.payload);
                if (semantic == GeometrySemantic::Wall) {
                    addWallFromPolyline(polyline, entity.trace);
                } else if (semantic == GeometrySemantic::Opening || semantic == GeometrySemantic::Exit) {
                    addOpeningFromPolyline(polyline, semantic, entity.trace);
                }
                continue;
            }

            if (std::holds_alternative<Polygon2D>(entity.payload)) {
                const auto& polygon = std::get<Polygon2D>(entity.payload);
                if (semantic == GeometrySemantic::Walkable) {
                    addWalkablePolygon(polygon, entity.trace);
                } else if (semantic == GeometrySemantic::Obstacle) {
                    addObstaclePolygon(polygon, entity.trace);
                } else if (semantic == GeometrySemantic::Wall) {
                    addWallFromPolyline({
                        .vertices = polygon.outline,
                        .closed = true,
                    }, entity.trace);
                }
                continue;
            }

            if (!std::holds_alternative<RawBlockReference2D>(entity.payload)) {
                continue;
            }

            const auto& block = std::get<RawBlockReference2D>(entity.payload);
            for (const auto& polylineChild : block.polylines) {
                const auto childTrace = inheritBlockChildTrace(entity.trace, polylineChild.trace);
                const auto childSemantic = classifySemantic(childTrace);
                const auto transformed = transformPolyline(polylineChild.geometry, block);
                if (childSemantic == GeometrySemantic::Wall) {
                    addWallFromPolyline(transformed, childTrace);
                } else if (childSemantic == GeometrySemantic::Opening || childSemantic == GeometrySemantic::Exit) {
                    addOpeningFromPolyline(transformed, childSemantic, childTrace);
                } else if (childSemantic == GeometrySemantic::Obstacle && transformed.closed) {
                    addObstaclePolygon(toPolygon(transformed), childTrace);
                } else if (childSemantic == GeometrySemantic::Walkable && transformed.closed) {
                    addWalkablePolygon(toPolygon(transformed), childTrace);
                }
            }

            for (const auto& polygonChild : block.polygons) {
                const auto childTrace = inheritBlockChildTrace(entity.trace, polygonChild.trace);
                const auto childSemantic = classifySemantic(childTrace);
                const auto transformed = transformPolygon(polygonChild.geometry, block);
                if (childSemantic == GeometrySemantic::Obstacle) {
                    addObstaclePolygon(transformed, childTrace);
                } else if (childSemantic == GeometrySemantic::Walkable) {
                    addWalkablePolygon(transformed, childTrace);
                } else if (childSemantic == GeometrySemantic::Wall) {
                    addWallFromPolyline({
                        .vertices = transformed.outline,
                        .closed = true,
                    }, childTrace);
                }
            }
        }
    }

    std::string nextCanonicalId(std::string_view prefix, std::size_t& counter) {
        ++counter;
        std::ostringstream stream;
        stream << prefix << '-' << counter;
        return stream.str();
    }
};

}  // namespace

ImportResult DxfImportService::importFile(const ImportRequest& request) const {
    ImportResult result;

    const auto extension = toUpper(request.sourcePath.extension().generic_string());
    if (request.requestedFormat == ImportedFileFormat::Ifc || (!extension.empty() && extension != ".DXF")) {
        result.issues.push_back({
            .severity = ImportIssueSeverity::Error,
            .code = ImportIssueCode::UnsupportedFormat,
            .message = "DxfImportService only supports DXF files.",
            .sourceId = request.sourcePath.generic_string(),
            .isBlocking = true,
        });
        result.reviewStatus = ImportReviewStatus::Rejected;
        return result;
    }

    auto groups = loadGroups(request.sourcePath);
    DxfAsciiParser parser(request.sourcePath, std::move(groups));
    result = parser.parse();

    if (result.canonicalGeometry.has_value()) {
        FacilityLayoutBuilder builder;
        auto buildResult = builder.build(*result.canonicalGeometry);
        result.layout = std::move(buildResult.layout);
        result.issues.insert(result.issues.end(), buildResult.issues.begin(), buildResult.issues.end());
        appendLayoutTraceRefs(*result.layout, result.traceRefs);

        if (request.runValidation) {
            ImportValidationService validator;
            auto validationIssues = validator.validate(*result.layout);
            result.issues.insert(result.issues.end(), validationIssues.begin(), validationIssues.end());
        }
    }

    if (!request.preserveRawModel) {
        result.rawModel.reset();
    }

    result.reviewStatus = hasBlockingImportIssue(result.issues) ? ImportReviewStatus::Rejected : ImportReviewStatus::Pending;
    return result;
}

}  // namespace safecrowd::domain
