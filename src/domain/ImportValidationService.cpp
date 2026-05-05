#include "domain/ImportValidationService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace safecrowd::domain {
namespace {

constexpr double kMinimumConnectionWidth = 0.9;
constexpr double kConnectionBoundaryTolerance = 0.25;

struct Vector2D {
    double x{0.0};
    double y{0.0};
};

bool hasValidFloorReference(const std::unordered_set<std::string>& floorIds, const std::string& floorId) {
    return floorIds.empty() || (!floorId.empty() && floorIds.contains(floorId));
}

bool isVerticalConnection(const Connection2D& connection) {
    return connection.kind == ConnectionKind::Stair || connection.kind == ConnectionKind::Ramp
        || connection.isStair || connection.isRamp;
}

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

double length(const Vector2D& value) {
    return std::sqrt(dot(value, value));
}

Vector2D normalize(const Vector2D& value) {
    const double magnitude = length(value);
    if (magnitude <= 1e-12) {
        return {};
    }

    return scale(value, 1.0 / magnitude);
}

double distanceBetween(const Point2D& lhs, const Point2D& rhs) {
    return length(subtract(lhs, rhs));
}

Point2D segmentMidpoint(const LineSegment2D& segment) {
    return {
        .x = (segment.start.x + segment.end.x) * 0.5,
        .y = (segment.start.y + segment.end.y) * 0.5,
    };
}

Vector2D segmentNormal(const LineSegment2D& segment) {
    const auto direction = normalize(subtract(segment.end, segment.start));
    return {
        .x = -direction.y,
        .y = direction.x,
    };
}

double distancePointToSegment(const Point2D& point, const LineSegment2D& segment) {
    const auto dx = segment.end.x - segment.start.x;
    const auto dy = segment.end.y - segment.start.y;
    const auto lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1e-12) {
        return distanceBetween(point, segment.start);
    }

    const auto t = std::clamp(
        ((point.x - segment.start.x) * dx + (point.y - segment.start.y) * dy) / lengthSquared,
        0.0,
        1.0);
    const Point2D projected{
        .x = segment.start.x + t * dx,
        .y = segment.start.y + t * dy,
    };
    return distanceBetween(point, projected);
}

double distancePointToRingBoundary(const Point2D& point, const std::vector<Point2D>& ring) {
    if (ring.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    auto bestDistance = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < ring.size(); ++index) {
        const auto& start = ring[index];
        const auto& end = ring[(index + 1) % ring.size()];
        bestDistance = std::min(bestDistance, distancePointToSegment(point, {.start = start, .end = end}));
    }
    return bestDistance;
}

double distancePointToPolygonBoundary(const Point2D& point, const Polygon2D& polygon) {
    auto bestDistance = distancePointToRingBoundary(point, polygon.outline);
    for (const auto& hole : polygon.holes) {
        bestDistance = std::min(bestDistance, distancePointToRingBoundary(point, hole));
    }
    return bestDistance;
}

bool pointInRingInclusive(const Point2D& point, const std::vector<Point2D>& ring) {
    if (ring.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t index = 0, previous = ring.size() - 1; index < ring.size(); previous = index++) {
        const auto& start = ring[previous];
        const auto& end = ring[index];
        if (distancePointToSegment(point, {.start = start, .end = end}) <= kConnectionBoundaryTolerance) {
            return true;
        }

        const bool crossesY = (start.y > point.y) != (end.y > point.y);
        if (!crossesY) {
            continue;
        }

        const auto xAtPointY = start.x + ((point.y - start.y) * (end.x - start.x) / (end.y - start.y));
        if (point.x <= xAtPointY + 1e-12) {
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

bool spanTouchesPolygon(const LineSegment2D& span, const Polygon2D& polygon) {
    const auto midpoint = segmentMidpoint(span);
    const auto normal = segmentNormal(span);
    const double probeDistance = std::max(0.35, distanceBetween(span.start, span.end) * 0.35);

    const std::vector<Point2D> probes = {
        span.start,
        span.end,
        midpoint,
        add(midpoint, scale(normal, probeDistance)),
        add(midpoint, scale(normal, -probeDistance)),
    };

    for (const auto& probe : probes) {
        if (pointInPolygonInclusive(probe, polygon)
            || distancePointToPolygonBoundary(probe, polygon) <= kConnectionBoundaryTolerance) {
            return true;
        }
    }

    return false;
}

const Zone2D* findZoneById(const FacilityLayout2D& layout, const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

bool connectionSpanMatchesReferencedZones(const FacilityLayout2D& layout, const Connection2D& connection) {
    if (isVerticalConnection(connection)) {
        return true;
    }

    const auto* fromZone = findZoneById(layout, connection.fromZoneId);
    const auto* toZone = findZoneById(layout, connection.toZoneId);
    if (fromZone == nullptr || toZone == nullptr) {
        return true;
    }

    if (connection.kind == ConnectionKind::Exit) {
        const auto* walkableZone = fromZone->kind == ZoneKind::Exit ? toZone : fromZone;
        return spanTouchesPolygon(connection.centerSpan, walkableZone->area);
    }

    return spanTouchesPolygon(connection.centerSpan, fromZone->area)
        && spanTouchesPolygon(connection.centerSpan, toZone->area);
}

bool canTravel(const Connection2D& connection, const std::string& fromZoneId, const std::string& toZoneId) {
    switch (connection.directionality) {
    case TravelDirection::Bidirectional:
        return (connection.fromZoneId == fromZoneId && connection.toZoneId == toZoneId)
            || (connection.fromZoneId == toZoneId && connection.toZoneId == fromZoneId);
    case TravelDirection::ForwardOnly:
        return connection.fromZoneId == fromZoneId && connection.toZoneId == toZoneId;
    case TravelDirection::ReverseOnly:
        return connection.fromZoneId == toZoneId && connection.toZoneId == fromZoneId;
    case TravelDirection::Closed:
        return false;
    }

    return false;
}

bool hasRouteToExit(
    const FacilityLayout2D& layout,
    const std::unordered_set<std::string>& exitZoneIds,
    const std::string& startZoneId) {
    if (exitZoneIds.contains(startZoneId)) {
        return true;
    }

    std::vector<std::string> frontier = {startZoneId};
    std::unordered_set<std::string> visited = {startZoneId};

    while (!frontier.empty()) {
        const auto currentZoneId = frontier.back();
        frontier.pop_back();

        if (exitZoneIds.contains(currentZoneId)) {
            return true;
        }

        for (const auto& connection : layout.connections) {
            if (connection.directionality == TravelDirection::Closed) {
                continue;
            }

            if (canTravel(connection, currentZoneId, connection.toZoneId) && !visited.contains(connection.toZoneId)) {
                frontier.push_back(connection.toZoneId);
                visited.insert(connection.toZoneId);
            }

            if (canTravel(connection, currentZoneId, connection.fromZoneId) && !visited.contains(connection.fromZoneId)) {
                frontier.push_back(connection.fromZoneId);
                visited.insert(connection.fromZoneId);
            }
        }
    }

    return false;
}

}  // namespace

std::vector<ImportIssue> ImportValidationService::validate(const FacilityLayout2D& layout) const {
    std::vector<ImportIssue> issues;

    std::unordered_set<std::string> floorIds;
    for (const auto& floor : layout.floors) {
        if (!floor.id.empty()) {
            floorIds.insert(floor.id);
        }
    }

    std::unordered_map<std::string, std::string> zoneFloorIds;
    for (const auto& zone : layout.zones) {
        zoneFloorIds[zone.id] = zone.floorId;
        if (!hasValidFloorReference(floorIds, zone.floorId)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Zone references a missing floor.",
                .targetId = zone.id,
                .isBlocking = true,
            });
        }
    }
    for (const auto& connection : layout.connections) {
        if (!hasValidFloorReference(floorIds, connection.floorId)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Connection references a missing floor.",
                .targetId = connection.id,
                .isBlocking = true,
            });
        }

        const auto fromFloor = zoneFloorIds.find(connection.fromZoneId);
        const auto toFloor = zoneFloorIds.find(connection.toZoneId);
        if (fromFloor != zoneFloorIds.end()
            && toFloor != zoneFloorIds.end()
            && !fromFloor->second.empty()
            && !toFloor->second.empty()
            && fromFloor->second != toFloor->second
            && !isVerticalConnection(connection)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Inter-floor connections must be marked as a stair or ramp.",
                .targetId = connection.id,
                .isBlocking = true,
            });
        }
    }
    for (const auto& barrier : layout.barriers) {
        if (!hasValidFloorReference(floorIds, barrier.floorId)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Wall references a missing floor.",
                .targetId = barrier.id,
                .isBlocking = true,
            });
        }
    }
    for (const auto& control : layout.controls) {
        if (!hasValidFloorReference(floorIds, control.floorId)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Control point references a missing floor.",
                .targetId = control.id,
                .isBlocking = true,
            });
        }
    }

    std::unordered_set<std::string> exitZoneIds;
    std::size_t roomZoneCount = 0;
    for (const auto& zone : layout.zones) {
        if (zone.kind == ZoneKind::Exit) {
            exitZoneIds.insert(zone.id);
        }
        if (zone.kind == ZoneKind::Room) {
            ++roomZoneCount;
        }
    }

    if (exitZoneIds.empty()) {
        issues.push_back({
            .severity = ImportIssueSeverity::Error,
            .code = ImportIssueCode::MissingExit,
            .message = "Imported layout does not contain an inferred exit zone.",
            .targetId = layout.id,
            .isBlocking = true,
        });
    }

    if (roomZoneCount == 0) {
        issues.push_back({
            .severity = ImportIssueSeverity::Warning,
            .code = ImportIssueCode::MissingRoom,
            .message = "Agents can only be placed inside Room or Exit zones.",
            .targetId = layout.id,
        });
    }

    for (const auto& connection : layout.connections) {
        if (connection.effectiveWidth > 0.0 && connection.effectiveWidth < kMinimumConnectionWidth) {
            issues.push_back({
                .severity = ImportIssueSeverity::Warning,
                .code = ImportIssueCode::WidthBelowMinimum,
                .message = "Connection width is below the demo minimum threshold.",
                .sourceId = connection.id,
                .targetId = connection.toZoneId,
            });
        }
        if (!connectionSpanMatchesReferencedZones(layout, connection)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidGeometry,
                .message = "Connection span is not aligned with the referenced zone boundary.",
                .sourceId = connection.id,
                .targetId = connection.toZoneId,
                .isBlocking = true,
            });
        }
    }

    for (const auto& zone : layout.zones) {
        if (zone.kind == ZoneKind::Exit) {
            continue;
        }

        if (!hasRouteToExit(layout, exitZoneIds, zone.id)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::DisconnectedWalkableArea,
                .message = "Walkable zone is not connected to any inferred exit.",
                .targetId = zone.id,
                .isBlocking = true,
            });
        }
    }

    return issues;
}

}  // namespace safecrowd::domain
