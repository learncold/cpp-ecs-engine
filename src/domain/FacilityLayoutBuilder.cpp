#include "domain/FacilityLayoutBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace safecrowd::domain {
namespace {

constexpr double kEpsilon = 1e-6;
constexpr double kBoundaryTolerance = 0.25;

struct Vector2D {
    double x{0.0};
    double y{0.0};
};

struct Bounds2D {
    double minX{0.0};
    double minY{0.0};
    double maxX{0.0};
    double maxY{0.0};
};

Vector2D subtract(const Point2D& lhs, const Point2D& rhs) {
    return {
        .x = lhs.x - rhs.x,
        .y = lhs.y - rhs.y,
    };
}

Point2D add(const Point2D& point, const Vector2D& delta) {
    return {
        .x = point.x + delta.x,
        .y = point.y + delta.y,
    };
}

Vector2D scale(const Vector2D& value, double factor) {
    return {
        .x = value.x * factor,
        .y = value.y * factor,
    };
}

double dot(const Vector2D& lhs, const Vector2D& rhs) {
    return (lhs.x * rhs.x) + (lhs.y * rhs.y);
}

double cross(const Vector2D& lhs, const Vector2D& rhs) {
    return (lhs.x * rhs.y) - (lhs.y * rhs.x);
}

double length(const Vector2D& value) {
    return std::sqrt(dot(value, value));
}

Vector2D normalize(const Vector2D& value) {
    const double magnitude = length(value);
    if (magnitude <= kEpsilon) {
        return {};
    }

    return scale(value, 1.0 / magnitude);
}

Vector2D segmentDirection(const LineSegment2D& segment) {
    return subtract(segment.end, segment.start);
}

Point2D segmentMidpoint(const LineSegment2D& segment) {
    return {
        .x = (segment.start.x + segment.end.x) * 0.5,
        .y = (segment.start.y + segment.end.y) * 0.5,
    };
}

Vector2D segmentNormal(const LineSegment2D& segment) {
    const auto direction = normalize(segmentDirection(segment));
    return {
        .x = -direction.y,
        .y = direction.x,
    };
}

double distanceSquared(const Point2D& lhs, const Point2D& rhs) {
    const auto delta = subtract(lhs, rhs);
    return dot(delta, delta);
}

double distanceBetween(const Point2D& lhs, const Point2D& rhs) {
    return std::sqrt(distanceSquared(lhs, rhs));
}

double distancePointToSegment(const Point2D& point, const LineSegment2D& segment) {
    const auto direction = subtract(segment.end, segment.start);
    const double magnitudeSquared = dot(direction, direction);

    if (magnitudeSquared <= kEpsilon) {
        return distanceBetween(point, segment.start);
    }

    const auto startToPoint = subtract(point, segment.start);
    const double t = std::clamp(dot(startToPoint, direction) / magnitudeSquared, 0.0, 1.0);
    const Point2D projection{
        .x = segment.start.x + (direction.x * t),
        .y = segment.start.y + (direction.y * t),
    };
    return distanceBetween(point, projection);
}

bool pointOnSegment(const Point2D& point, const LineSegment2D& segment, double tolerance = kBoundaryTolerance) {
    return distancePointToSegment(point, segment) <= tolerance;
}

std::vector<LineSegment2D> polygonEdges(const Polygon2D& polygon) {
    std::vector<LineSegment2D> edges;
    if (polygon.outline.size() < 2) {
        return edges;
    }

    edges.reserve(polygon.outline.size());
    for (std::size_t index = 0; index < polygon.outline.size(); ++index) {
        edges.push_back({
            .start = polygon.outline[index],
            .end = polygon.outline[(index + 1) % polygon.outline.size()],
        });
    }

    return edges;
}

Bounds2D computeBounds(const Polygon2D& polygon) {
    Bounds2D bounds;
    if (polygon.outline.empty()) {
        return bounds;
    }

    bounds.minX = bounds.maxX = polygon.outline.front().x;
    bounds.minY = bounds.maxY = polygon.outline.front().y;

    for (const auto& vertex : polygon.outline) {
        bounds.minX = std::min(bounds.minX, vertex.x);
        bounds.minY = std::min(bounds.minY, vertex.y);
        bounds.maxX = std::max(bounds.maxX, vertex.x);
        bounds.maxY = std::max(bounds.maxY, vertex.y);
    }

    return bounds;
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

Point2D polygonCentroid(const Polygon2D& polygon) {
    const auto area = signedRingArea(polygon.outline);
    if (std::fabs(area) <= kEpsilon) {
        Point2D centroid{};
        if (polygon.outline.empty()) {
            return centroid;
        }

        for (const auto& vertex : polygon.outline) {
            centroid.x += vertex.x;
            centroid.y += vertex.y;
        }

        const double count = static_cast<double>(polygon.outline.size());
        centroid.x /= count;
        centroid.y /= count;
        return centroid;
    }

    double factor = 0.0;
    Point2D centroid{};
    for (std::size_t index = 0; index < polygon.outline.size(); ++index) {
        const auto& current = polygon.outline[index];
        const auto& next = polygon.outline[(index + 1) % polygon.outline.size()];
        const double cross = (current.x * next.y) - (next.x * current.y);
        factor += cross;
        centroid.x += (current.x + next.x) * cross;
        centroid.y += (current.y + next.y) * cross;
    }

    const double denominator = 3.0 * factor;
    if (std::fabs(denominator) <= kEpsilon) {
        return centroid;
    }

    centroid.x /= denominator;
    centroid.y /= denominator;
    return centroid;
}

bool pointInRingInclusive(const Point2D& point, const std::vector<Point2D>& ring) {
    if (ring.size() < 3) {
        return false;
    }

    for (std::size_t index = 0; index < ring.size(); ++index) {
        const auto& start = ring[index];
        const auto& end = ring[(index + 1) % ring.size()];
        if (pointOnSegment(point, {.start = start, .end = end})) {
            return true;
        }
    }

    bool inside = false;
    for (std::size_t index = 0, previous = ring.size() - 1; index < ring.size(); previous = index++) {
        const auto& vertex = ring[index];
        const auto& prior = ring[previous];
        const bool crossesY = ((vertex.y > point.y) != (prior.y > point.y));
        if (!crossesY) {
            continue;
        }

        const double denominator = prior.y - vertex.y;
        if (std::fabs(denominator) <= kEpsilon) {
            continue;
        }

        const double xAtPointY = ((prior.x - vertex.x) * (point.y - vertex.y) / denominator) + vertex.x;
        if (point.x <= xAtPointY + kEpsilon) {
            inside = !inside;
        }
    }

    return inside;
}

bool pointInPolygonInclusive(const Point2D& point, const Polygon2D& polygon) {
    if (!pointInRingInclusive(point, polygon.outline)) {
        return false;
    }

    for (const auto& hole : polygon.holes) {
        if (pointInRingInclusive(point, hole)) {
            return false;
        }
    }

    return true;
}

double distancePointToPolygonBoundary(const Point2D& point, const Polygon2D& polygon) {
    if (polygon.outline.size() < 2) {
        return std::numeric_limits<double>::infinity();
    }

    double distance = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < polygon.outline.size(); ++index) {
        const auto& start = polygon.outline[index];
        const auto& end = polygon.outline[(index + 1) % polygon.outline.size()];
        distance = std::min(distance, distancePointToSegment(point, {.start = start, .end = end}));
    }
    return distance;
}

double distancePointToInfiniteLine(const Point2D& point, const LineSegment2D& line) {
    const auto direction = subtract(line.end, line.start);
    const double magnitude = length(direction);
    if (magnitude <= kEpsilon) {
        return distanceBetween(point, line.start);
    }

    const auto fromStart = subtract(point, line.start);
    return std::fabs(cross(fromStart, direction)) / magnitude;
}

bool pointInPolygonStrict(const Point2D& point, const Polygon2D& polygon) {
    return pointInPolygonInclusive(point, polygon)
        && distancePointToPolygonBoundary(point, polygon) > kBoundaryTolerance;
}

bool touchesPolygon(const Polygon2D& polygon, const LineSegment2D& segment) {
    const auto midpoint = segmentMidpoint(segment);
    const auto normal = segmentNormal(segment);
    const double probeDistance = std::max(0.35, distanceBetween(segment.start, segment.end) * 0.35);

    const std::vector<Point2D> probes = {
        segment.start,
        segment.end,
        midpoint,
        add(midpoint, scale(normal, probeDistance)),
        add(midpoint, scale(normal, -probeDistance)),
    };

    for (const auto& probe : probes) {
        if (pointInPolygonInclusive(probe, polygon)) {
            return true;
        }

        if (distancePointToPolygonBoundary(probe, polygon) <= kBoundaryTolerance) {
            return true;
        }
    }

    return false;
}

bool boundsOverlap(const Bounds2D& lhs, const Bounds2D& rhs, double tolerance = kBoundaryTolerance) {
    return lhs.minX <= rhs.maxX + tolerance
        && lhs.maxX >= rhs.minX - tolerance
        && lhs.minY <= rhs.maxY + tolerance
        && lhs.maxY >= rhs.minY - tolerance;
}

Point2D pointAlongEdgeForAxis(const LineSegment2D& edge, double axisValue, bool useX) {
    const double startAxis = useX ? edge.start.x : edge.start.y;
    const double endAxis = useX ? edge.end.x : edge.end.y;
    const double deltaAxis = endAxis - startAxis;

    if (std::fabs(deltaAxis) <= kEpsilon) {
        return edge.start;
    }

    const double t = (axisValue - startAxis) / deltaAxis;
    return {
        .x = edge.start.x + ((edge.end.x - edge.start.x) * t),
        .y = edge.start.y + ((edge.end.y - edge.start.y) * t),
    };
}

struct InferredConnectionGeometry {
    LineSegment2D span{};
    double width{0.0};
    bool derivedFromSharedBoundary{false};
};

std::optional<InferredConnectionGeometry> inferSharedBoundaryPortal(const Polygon2D& lhs, const Polygon2D& rhs) {
    std::optional<InferredConnectionGeometry> best;

    for (const auto& lhsEdge : polygonEdges(lhs)) {
        for (const auto& rhsEdge : polygonEdges(rhs)) {
            const auto lhsDirection = subtract(lhsEdge.end, lhsEdge.start);
            const auto rhsDirection = subtract(rhsEdge.end, rhsEdge.start);

            if (length(lhsDirection) <= kEpsilon || length(rhsDirection) <= kEpsilon) {
                continue;
            }

            if (std::fabs(cross(normalize(lhsDirection), normalize(rhsDirection))) > 0.05) {
                continue;
            }

            if (distancePointToInfiniteLine(lhsEdge.start, rhsEdge) > kBoundaryTolerance
                || distancePointToInfiniteLine(rhsEdge.start, lhsEdge) > kBoundaryTolerance) {
                continue;
            }

            const bool useX = std::fabs(lhsDirection.x) >= std::fabs(lhsDirection.y);
            const double lhsMin = std::min(useX ? lhsEdge.start.x : lhsEdge.start.y, useX ? lhsEdge.end.x : lhsEdge.end.y);
            const double lhsMax = std::max(useX ? lhsEdge.start.x : lhsEdge.start.y, useX ? lhsEdge.end.x : lhsEdge.end.y);
            const double rhsMin = std::min(useX ? rhsEdge.start.x : rhsEdge.start.y, useX ? rhsEdge.end.x : rhsEdge.end.y);
            const double rhsMax = std::max(useX ? rhsEdge.start.x : rhsEdge.start.y, useX ? rhsEdge.end.x : rhsEdge.end.y);
            const double overlapMin = std::max(lhsMin, rhsMin);
            const double overlapMax = std::min(lhsMax, rhsMax);
            const double overlapLength = overlapMax - overlapMin;

            if (overlapLength <= kBoundaryTolerance) {
                continue;
            }

            const auto start = pointAlongEdgeForAxis(lhsEdge, overlapMin, useX);
            const auto end = pointAlongEdgeForAxis(lhsEdge, overlapMax, useX);
            if (!best.has_value() || overlapLength > best->width) {
                best = InferredConnectionGeometry{
                    .span = {.start = start, .end = end},
                    .width = overlapLength,
                    .derivedFromSharedBoundary = true,
                };
            }
        }
    }

    return best;
}

bool polygonsOverlapByArea(const Polygon2D& lhs, const Polygon2D& rhs) {
    for (const auto& vertex : lhs.outline) {
        if (pointInPolygonStrict(vertex, rhs)) {
            return true;
        }
    }

    for (const auto& vertex : rhs.outline) {
        if (pointInPolygonStrict(vertex, lhs)) {
            return true;
        }
    }

    const auto lhsCentroid = polygonCentroid(lhs);
    const auto rhsCentroid = polygonCentroid(rhs);
    return pointInPolygonStrict(lhsCentroid, rhs) || pointInPolygonStrict(rhsCentroid, lhs);
}

std::optional<InferredConnectionGeometry> inferOverlapPortal(const Polygon2D& lhs, const Polygon2D& rhs) {
    const auto lhsBounds = computeBounds(lhs);
    const auto rhsBounds = computeBounds(rhs);
    if (!boundsOverlap(lhsBounds, rhsBounds, 0.0)) {
        return std::nullopt;
    }

    if (!polygonsOverlapByArea(lhs, rhs)) {
        return std::nullopt;
    }

    const double overlapMinX = std::max(lhsBounds.minX, rhsBounds.minX);
    const double overlapMaxX = std::min(lhsBounds.maxX, rhsBounds.maxX);
    const double overlapMinY = std::max(lhsBounds.minY, rhsBounds.minY);
    const double overlapMaxY = std::min(lhsBounds.maxY, rhsBounds.maxY);
    const double overlapWidth = overlapMaxX - overlapMinX;
    const double overlapHeight = overlapMaxY - overlapMinY;

    if (overlapWidth <= kBoundaryTolerance && overlapHeight <= kBoundaryTolerance) {
        return std::nullopt;
    }

    const Point2D center{
        .x = (overlapMinX + overlapMaxX) * 0.5,
        .y = (overlapMinY + overlapMaxY) * 0.5,
    };

    if (overlapWidth >= overlapHeight) {
        return InferredConnectionGeometry{
            .span = {
                .start = {overlapMinX, center.y},
                .end = {overlapMaxX, center.y},
            },
            .width = overlapWidth,
            .derivedFromSharedBoundary = false,
        };
    }

    return InferredConnectionGeometry{
        .span = {
            .start = {center.x, overlapMinY},
            .end = {center.x, overlapMaxY},
        },
        .width = overlapHeight,
        .derivedFromSharedBoundary = false,
    };
}

std::optional<InferredConnectionGeometry> carvePortalAgainstWalls(
    const InferredConnectionGeometry& candidate,
    const std::vector<WallSegment2D>& walls) {
    if (!candidate.derivedFromSharedBoundary) {
        return candidate;
    }

    const auto portalDirection = subtract(candidate.span.end, candidate.span.start);
    if (length(portalDirection) <= kEpsilon) {
        return std::nullopt;
    }

    const bool useX = std::fabs(portalDirection.x) >= std::fabs(portalDirection.y);
    const double portalMin = std::min(useX ? candidate.span.start.x : candidate.span.start.y, useX ? candidate.span.end.x : candidate.span.end.y);
    const double portalMax = std::max(useX ? candidate.span.start.x : candidate.span.start.y, useX ? candidate.span.end.x : candidate.span.end.y);

    std::vector<std::pair<double, double>> blockedIntervals;
    for (const auto& wall : walls) {
        const auto wallDirection = subtract(wall.segment.end, wall.segment.start);
        if (length(wallDirection) <= kEpsilon) {
            continue;
        }

        if (std::fabs(cross(normalize(portalDirection), normalize(wallDirection))) > 0.05) {
            continue;
        }

        if (distancePointToInfiniteLine(wall.segment.start, candidate.span) > kBoundaryTolerance
            || distancePointToInfiniteLine(wall.segment.end, candidate.span) > kBoundaryTolerance) {
            continue;
        }

        const double wallMin = std::min(useX ? wall.segment.start.x : wall.segment.start.y, useX ? wall.segment.end.x : wall.segment.end.y);
        const double wallMax = std::max(useX ? wall.segment.start.x : wall.segment.start.y, useX ? wall.segment.end.x : wall.segment.end.y);
        const double blockedMin = std::max(portalMin, wallMin);
        const double blockedMax = std::min(portalMax, wallMax);
        if (blockedMax - blockedMin > kBoundaryTolerance) {
            blockedIntervals.push_back({blockedMin, blockedMax});
        }
    }

    if (blockedIntervals.empty()) {
        return candidate;
    }

    std::sort(blockedIntervals.begin(), blockedIntervals.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });

    std::vector<std::pair<double, double>> mergedIntervals;
    for (const auto& interval : blockedIntervals) {
        if (mergedIntervals.empty() || interval.first > mergedIntervals.back().second + kBoundaryTolerance) {
            mergedIntervals.push_back(interval);
            continue;
        }

        mergedIntervals.back().second = std::max(mergedIntervals.back().second, interval.second);
    }

    double bestGapMin = 0.0;
    double bestGapMax = 0.0;
    double cursor = portalMin;

    for (const auto& interval : mergedIntervals) {
        const double gapMin = cursor;
        const double gapMax = std::min(interval.first, portalMax);
        if (gapMax - gapMin > bestGapMax - bestGapMin) {
            bestGapMin = gapMin;
            bestGapMax = gapMax;
        }
        cursor = std::max(cursor, interval.second);
        if (cursor >= portalMax) {
            break;
        }
    }

    if (portalMax - cursor > bestGapMax - bestGapMin) {
        bestGapMin = cursor;
        bestGapMax = portalMax;
    }

    if (bestGapMax - bestGapMin <= kBoundaryTolerance) {
        return std::nullopt;
    }

    return InferredConnectionGeometry{
        .span = {
            .start = pointAlongEdgeForAxis(candidate.span, bestGapMin, useX),
            .end = pointAlongEdgeForAxis(candidate.span, bestGapMax, useX),
        },
        .width = bestGapMax - bestGapMin,
        .derivedFromSharedBoundary = true,
    };
}

std::optional<InferredConnectionGeometry> inferZoneAdjacencyPortal(
    const Polygon2D& lhs,
    const Polygon2D& rhs,
    const std::vector<WallSegment2D>& walls) {
    const auto lhsBounds = computeBounds(lhs);
    const auto rhsBounds = computeBounds(rhs);
    if (!boundsOverlap(lhsBounds, rhsBounds)) {
        return std::nullopt;
    }

    if (auto sharedBoundary = inferSharedBoundaryPortal(lhs, rhs); sharedBoundary.has_value()) {
        return carvePortalAgainstWalls(*sharedBoundary, walls);
    }

    return inferOverlapPortal(lhs, rhs);
}

ZoneKind classifyWalkableZoneKind(const Polygon2D& polygon) {
    const auto bounds = computeBounds(polygon);
    const double width = std::max(0.0, bounds.maxX - bounds.minX);
    const double height = std::max(0.0, bounds.maxY - bounds.minY);
    const double shorterSide = std::min(width, height);
    const double longerSide = std::max(width, height);

    if (shorterSide <= kEpsilon) {
        return ZoneKind::Unknown;
    }

    const double aspectRatio = longerSide / shorterSide;
    const double boxArea = width * height;
    const double occupancy = boxArea <= kEpsilon ? 0.0 : polygonArea(polygon) / boxArea;

    if (aspectRatio >= 2.5 && occupancy >= 0.55) {
        return ZoneKind::Corridor;
    }

    return ZoneKind::Room;
}

std::string makeLayoutId(const CanonicalGeometry& geometry) {
    if (geometry.levelId.empty()) {
        return "layout-import";
    }
    return "layout-" + geometry.levelId;
}

std::string makeLayoutName(const CanonicalGeometry& geometry) {
    if (geometry.levelId.empty()) {
        return "Imported Layout";
    }
    return "Imported Layout " + geometry.levelId;
}

std::size_t estimateCapacity(const Polygon2D& polygon) {
    const double area = polygonArea(polygon);
    if (area <= kEpsilon) {
        return 1;
    }

    return std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(area / 1.8)));
}

Polygon2D makeExitZoneArea(const Opening2D& opening) {
    const auto direction = normalize(segmentDirection(opening.span));
    if (length(direction) <= kEpsilon) {
        const Point2D anchor = opening.span.start;
        return {
            .outline = {
                {anchor.x - 0.5, anchor.y - 0.5},
                {anchor.x + 0.5, anchor.y - 0.5},
                {anchor.x + 0.5, anchor.y + 0.5},
                {anchor.x - 0.5, anchor.y + 0.5},
            },
        };
    }

    const auto normal = segmentNormal(opening.span);
    const double halfDepth = std::max(0.5, opening.width * 0.5);

    return {
        .outline = {
            add(opening.span.start, scale(normal, -halfDepth)),
            add(opening.span.end, scale(normal, -halfDepth)),
            add(opening.span.end, scale(normal, halfDepth)),
            add(opening.span.start, scale(normal, halfDepth)),
        },
    };
}

void appendIssue(
    std::vector<ImportIssue>& issues,
    ImportIssueSeverity severity,
    ImportIssueCode code,
    std::string message,
    std::string sourceId = {},
    std::string targetId = {},
    bool isBlocking = false) {
    issues.push_back({
        .severity = severity,
        .code = code,
        .message = std::move(message),
        .sourceId = std::move(sourceId),
        .targetId = std::move(targetId),
        .isBlocking = isBlocking,
    });
}

Point2D zoneAnchor(const Zone2D& zone) {
    return polygonCentroid(zone.area);
}

ElementProvenance mergeProvenance(const ElementProvenance& lhs, const ElementProvenance& rhs) {
    ElementProvenance merged = lhs;
    merged.sourceIds.insert(merged.sourceIds.end(), rhs.sourceIds.begin(), rhs.sourceIds.end());
    merged.canonicalIds.insert(merged.canonicalIds.end(), rhs.canonicalIds.begin(), rhs.canonicalIds.end());

    std::sort(merged.sourceIds.begin(), merged.sourceIds.end());
    merged.sourceIds.erase(std::unique(merged.sourceIds.begin(), merged.sourceIds.end()), merged.sourceIds.end());
    std::sort(merged.canonicalIds.begin(), merged.canonicalIds.end());
    merged.canonicalIds.erase(std::unique(merged.canonicalIds.begin(), merged.canonicalIds.end()), merged.canonicalIds.end());
    return merged;
}

std::vector<std::size_t> sortZoneCandidatesByDistance(const std::vector<Zone2D>& zones, const std::vector<std::size_t>& candidates, const Point2D& anchor) {
    auto ordered = candidates;
    std::sort(ordered.begin(), ordered.end(), [&](std::size_t lhs, std::size_t rhs) {
        return distanceSquared(zoneAnchor(zones[lhs]), anchor) < distanceSquared(zoneAnchor(zones[rhs]), anchor);
    });
    ordered.erase(std::unique(ordered.begin(), ordered.end()), ordered.end());
    return ordered;
}

}  // namespace

FacilityLayoutBuildResult FacilityLayoutBuilder::build(const CanonicalGeometry& geometry) const {
    FacilityLayoutBuildResult result;
    result.layout.id = makeLayoutId(geometry);
    result.layout.name = makeLayoutName(geometry);
    result.layout.levelId = geometry.levelId;

    std::size_t roomCounter = 0;
    std::size_t corridorCounter = 0;
    std::size_t exitCounter = 0;
    std::size_t stairCounter = 0;
    std::size_t connectionCounter = 0;
    std::size_t barrierCounter = 0;
    std::size_t controlCounter = 0;

    for (const auto& walkable : geometry.walkableAreas) {
        const auto kind = classifyWalkableZoneKind(walkable.polygon);
        std::string label;
        switch (kind) {
        case ZoneKind::Corridor:
            ++corridorCounter;
            label = "Corridor " + std::to_string(corridorCounter);
            break;
        case ZoneKind::Room:
        case ZoneKind::Unknown:
        default:
            ++roomCounter;
            label = "Room " + std::to_string(roomCounter);
            break;
        }

        result.layout.zones.push_back({
            .id = "zone-" + std::to_string(result.layout.zones.size() + 1),
            .kind = kind == ZoneKind::Unknown ? ZoneKind::Room : kind,
            .label = std::move(label),
            .area = walkable.polygon,
            .defaultCapacity = estimateCapacity(walkable.polygon),
            .provenance = {
                .sourceIds = walkable.sourceIds,
                .canonicalIds = {walkable.id},
            },
        });
    }

    const auto walkableZoneCount = result.layout.zones.size();

    for (const auto& verticalLink : geometry.verticalLinks) {
        const bool isRamp = verticalLink.kind == VerticalLinkKind::Ramp;
        Polygon2D linkArea{
            .outline = {
                {verticalLink.anchor.x - 0.75, verticalLink.anchor.y - 0.75},
                {verticalLink.anchor.x + 0.75, verticalLink.anchor.y - 0.75},
                {verticalLink.anchor.x + 0.75, verticalLink.anchor.y + 0.75},
                {verticalLink.anchor.x - 0.75, verticalLink.anchor.y + 0.75},
            },
        };

        ++stairCounter;
        result.layout.zones.push_back({
            .id = "zone-" + std::to_string(result.layout.zones.size() + 1),
            .kind = ZoneKind::Stair,
            .label = isRamp ? "Ramp " + std::to_string(stairCounter) : "Stair " + std::to_string(stairCounter),
            .area = std::move(linkArea),
            .defaultCapacity = 8,
            .isStair = !isRamp,
            .isRamp = isRamp,
            .provenance = {
                .sourceIds = verticalLink.sourceIds,
                .canonicalIds = {verticalLink.id},
            },
        });
    }

    const auto nonExitZoneCount = result.layout.zones.size();
    std::vector<std::size_t> exitZoneIndices;
    exitZoneIndices.reserve(geometry.openings.size());

    for (const auto& opening : geometry.openings) {
        if (opening.kind != OpeningKind::Exit) {
            continue;
        }

        ++exitCounter;
        result.layout.zones.push_back({
            .id = "zone-" + std::to_string(result.layout.zones.size() + 1),
            .kind = ZoneKind::Exit,
            .label = "Exit " + std::to_string(exitCounter),
            .area = makeExitZoneArea(opening),
            .defaultCapacity = std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(std::max(1.0, opening.width) * 4.0))),
            .provenance = {
                .sourceIds = opening.sourceIds,
                .canonicalIds = {opening.id},
            },
        });
        exitZoneIndices.push_back(result.layout.zones.size() - 1);
    }

    auto findTouchingNonExitZones = [&](const Opening2D& opening) {
        std::vector<std::size_t> candidates;
        for (std::size_t index = 0; index < nonExitZoneCount; ++index) {
            if (touchesPolygon(result.layout.zones[index].area, opening.span)) {
                candidates.push_back(index);
            }
        }
        return sortZoneCandidatesByDistance(result.layout.zones, candidates, segmentMidpoint(opening.span));
    };

    std::size_t exitOpeningIndex = 0;
    for (const auto& opening : geometry.openings) {
        const auto candidates = findTouchingNonExitZones(opening);

        if (opening.kind == OpeningKind::Exit) {
            if (exitOpeningIndex >= exitZoneIndices.size()) {
                continue;
            }
            const auto exitZoneIndex = exitZoneIndices[exitOpeningIndex++];

            if (candidates.empty()) {
                appendIssue(
                    result.issues,
                    ImportIssueSeverity::Warning,
                    ImportIssueCode::UnmappedElement,
                    "Exit opening could not be matched to a walkable zone.",
                    opening.id,
                    result.layout.zones[exitZoneIndex].id);
                continue;
            }

            ++connectionCounter;
            result.layout.connections.push_back({
                .id = "connection-" + std::to_string(connectionCounter),
                .kind = ConnectionKind::Exit,
                .fromZoneId = result.layout.zones[candidates.front()].id,
                .toZoneId = result.layout.zones[exitZoneIndex].id,
                .effectiveWidth = opening.width,
                .directionality = TravelDirection::Bidirectional,
                .centerSpan = opening.span,
                .provenance = {
                    .sourceIds = opening.sourceIds,
                    .canonicalIds = {opening.id},
                },
            });
            continue;
        }

        if (candidates.size() < 2) {
            appendIssue(
                result.issues,
                ImportIssueSeverity::Warning,
                ImportIssueCode::UnmappedElement,
                "Opening could not be matched to two walkable zones.",
                opening.id);
            continue;
        }

        ++connectionCounter;
        result.layout.connections.push_back({
            .id = "connection-" + std::to_string(connectionCounter),
            .kind = ConnectionKind::Doorway,
            .fromZoneId = result.layout.zones[candidates[0]].id,
            .toZoneId = result.layout.zones[candidates[1]].id,
            .effectiveWidth = opening.width,
            .directionality = TravelDirection::Bidirectional,
            .centerSpan = opening.span,
            .provenance = {
                .sourceIds = opening.sourceIds,
                .canonicalIds = {opening.id},
            },
        });
    }

    const auto makeZonePairKey = [](const std::string& lhs, const std::string& rhs) {
        return lhs < rhs ? lhs + "|" + rhs : rhs + "|" + lhs;
    };

    std::unordered_set<std::string> connectedZonePairs;
    for (const auto& connection : result.layout.connections) {
        connectedZonePairs.insert(makeZonePairKey(connection.fromZoneId, connection.toZoneId));
    }

    for (std::size_t lhsIndex = 0; lhsIndex < walkableZoneCount; ++lhsIndex) {
        for (std::size_t rhsIndex = lhsIndex + 1; rhsIndex < walkableZoneCount; ++rhsIndex) {
            const auto zonePairKey = makeZonePairKey(result.layout.zones[lhsIndex].id, result.layout.zones[rhsIndex].id);
            if (connectedZonePairs.contains(zonePairKey)) {
                continue;
            }

            const auto inferredPortal = inferZoneAdjacencyPortal(
                result.layout.zones[lhsIndex].area,
                result.layout.zones[rhsIndex].area,
                geometry.walls);
            if (!inferredPortal.has_value()) {
                continue;
            }

            ++connectionCounter;
            result.layout.connections.push_back({
                .id = "connection-" + std::to_string(connectionCounter),
                .kind = ConnectionKind::Opening,
                .fromZoneId = result.layout.zones[lhsIndex].id,
                .toZoneId = result.layout.zones[rhsIndex].id,
                .effectiveWidth = inferredPortal->width,
                .directionality = TravelDirection::Bidirectional,
                .centerSpan = inferredPortal->span,
                .provenance = mergeProvenance(
                    result.layout.zones[lhsIndex].provenance,
                    result.layout.zones[rhsIndex].provenance),
            });
            connectedZonePairs.insert(zonePairKey);
        }
    }

    for (const auto& wall : geometry.walls) {
        ++barrierCounter;
        result.layout.barriers.push_back({
            .id = "barrier-" + std::to_string(barrierCounter),
            .geometry = {
                .vertices = {wall.segment.start, wall.segment.end},
                .closed = false,
            },
            .blocksMovement = true,
            .provenance = {
                .sourceIds = wall.sourceIds,
                .canonicalIds = {wall.id},
            },
        });
    }

    for (const auto& obstacle : geometry.obstacles) {
        ++barrierCounter;
        result.layout.barriers.push_back({
            .id = "barrier-" + std::to_string(barrierCounter),
            .geometry = {
                .vertices = obstacle.footprint.outline,
                .closed = true,
            },
            .blocksMovement = true,
            .provenance = {
                .sourceIds = obstacle.sourceIds,
                .canonicalIds = {obstacle.id},
            },
        });
    }

    for (const auto& verticalLink : geometry.verticalLinks) {
        std::vector<std::size_t> candidates;
        for (std::size_t index = 0; index < nonExitZoneCount; ++index) {
            if (pointInPolygonInclusive(verticalLink.anchor, result.layout.zones[index].area)
                || distancePointToPolygonBoundary(verticalLink.anchor, result.layout.zones[index].area) <= kBoundaryTolerance) {
                candidates.push_back(index);
            }
        }

        const auto ordered = sortZoneCandidatesByDistance(result.layout.zones, candidates, verticalLink.anchor);
        if (ordered.empty()) {
            continue;
        }

        ++controlCounter;
        result.layout.controls.push_back({
            .id = "control-" + std::to_string(controlCounter),
            .kind = verticalLink.kind == VerticalLinkKind::Elevator ? ControlKind::Gate : ControlKind::Unknown,
            .targetId = result.layout.zones[ordered.front()].id,
            .provenance = {
                .sourceIds = verticalLink.sourceIds,
                .canonicalIds = {verticalLink.id},
            },
        });
    }

    return result;
}

}  // namespace safecrowd::domain
