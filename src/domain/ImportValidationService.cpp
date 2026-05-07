#include "domain/ImportValidationService.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace safecrowd::domain {
namespace {

constexpr double kMinimumConnectionWidth = 0.9;
constexpr double kConnectionBoundaryTolerance = 0.25;
constexpr double kGeometryEpsilon = 1e-9;

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
    if (magnitude <= 1e-12) {
        return {};
    }

    return scale(value, 1.0 / magnitude);
}

double distanceBetween(const Point2D& lhs, const Point2D& rhs) {
    return length(subtract(lhs, rhs));
}

double distancePointToLine(const Point2D& point, const LineSegment2D& line) {
    const auto direction = subtract(line.end, line.start);
    const auto lineLength = length(direction);
    if (lineLength <= kGeometryEpsilon) {
        return distanceBetween(point, line.start);
    }

    return std::abs(cross(subtract(point, line.start), direction)) / lineLength;
}

double projectPoint(const Point2D& point, const Vector2D& axis) {
    return point.x * axis.x + point.y * axis.y;
}

std::pair<double, double> projectedInterval(const LineSegment2D& segment, const Vector2D& axis) {
    const auto start = projectPoint(segment.start, axis);
    const auto end = projectPoint(segment.end, axis);
    return {std::min(start, end), std::max(start, end)};
}

double distancePointToSegment(const Point2D& point, const LineSegment2D& segment) {
    const auto direction = subtract(segment.end, segment.start);
    const auto lengthSquared = dot(direction, direction);
    if (lengthSquared <= kGeometryEpsilon) {
        return distanceBetween(point, segment.start);
    }

    const auto t = std::clamp(dot(subtract(point, segment.start), direction) / lengthSquared, 0.0, 1.0);
    const Point2D projected{
        .x = segment.start.x + (direction.x * t),
        .y = segment.start.y + (direction.y * t),
    };
    return distanceBetween(point, projected);
}

bool spansIntersect(const LineSegment2D& lhs, const LineSegment2D& rhs) {
    const auto lhsDirection = subtract(lhs.end, lhs.start);
    const auto rhsDirection = subtract(rhs.end, rhs.start);
    const auto denominator = cross(lhsDirection, rhsDirection);

    if (std::abs(denominator) <= kGeometryEpsilon) {
        if (distancePointToLine(lhs.start, rhs) > kConnectionBoundaryTolerance
            || distancePointToLine(lhs.end, rhs) > kConnectionBoundaryTolerance) {
            return false;
        }

        const auto axis = normalize(rhsDirection);
        const auto lhsInterval = projectedInterval(lhs, axis);
        const auto rhsInterval = projectedInterval(rhs, axis);
        const auto overlap =
            std::min(lhsInterval.second, rhsInterval.second) - std::max(lhsInterval.first, rhsInterval.first);
        return overlap > kGeometryEpsilon;
    }

    const auto delta = subtract(rhs.start, lhs.start);
    const auto lhsFraction = cross(delta, rhsDirection) / denominator;
    const auto rhsFraction = cross(delta, lhsDirection) / denominator;
    return lhsFraction >= -kGeometryEpsilon
        && lhsFraction <= 1.0 + kGeometryEpsilon
        && rhsFraction >= -kGeometryEpsilon
        && rhsFraction <= 1.0 + kGeometryEpsilon;
}

bool spanContactsBoundary(const LineSegment2D& span, const LineSegment2D& boundary) {
    const auto spanDirection = subtract(span.end, span.start);
    const auto boundaryDirection = subtract(boundary.end, boundary.start);
    if (length(spanDirection) <= kGeometryEpsilon || length(boundaryDirection) <= kGeometryEpsilon) {
        return false;
    }

    if (spansIntersect(span, boundary)) {
        return true;
    }

    const auto bestDistance = std::min({
        distancePointToSegment(span.start, boundary),
        distancePointToSegment(span.end, boundary),
        distancePointToSegment(boundary.start, span),
        distancePointToSegment(boundary.end, span),
    });
    return bestDistance <= kConnectionBoundaryTolerance;
}

bool spanContactsRingBoundary(const LineSegment2D& span, const std::vector<Point2D>& ring) {
    if (ring.size() < 2) {
        return false;
    }

    for (std::size_t index = 0; index < ring.size(); ++index) {
        const LineSegment2D boundary{
            .start = ring[index],
            .end = ring[(index + 1) % ring.size()],
        };
        if (spanContactsBoundary(span, boundary)) {
            return true;
        }
    }

    return false;
}

bool spanContactsPolygonBoundary(const LineSegment2D& span, const Polygon2D& polygon) {
    if (spanContactsRingBoundary(span, polygon.outline)) {
        return true;
    }

    return std::any_of(polygon.holes.begin(), polygon.holes.end(), [&](const auto& hole) {
        return spanContactsRingBoundary(span, hole);
    });
}

bool pointInRing(const Point2D& point, const std::vector<Point2D>& ring) {
    if (ring.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t index = 0, previous = ring.size() - 1; index < ring.size(); previous = index++) {
        const auto& start = ring[previous];
        const auto& end = ring[index];
        const bool crossesY = (start.y > point.y) != (end.y > point.y);
        if (!crossesY) {
            continue;
        }

        const auto xAtPointY = start.x + ((point.y - start.y) * (end.x - start.x) / (end.y - start.y));
        if (point.x <= xAtPointY + kGeometryEpsilon) {
            inside = !inside;
        }
    }

    return inside;
}

bool pointInPolygon(const Point2D& point, const Polygon2D& polygon) {
    if (!pointInRing(point, polygon.outline)) {
        return false;
    }

    for (const auto& hole : polygon.holes) {
        if (pointInRing(point, hole)) {
            return false;
        }
    }

    return true;
}

bool spanInteractsWithPolygon(const LineSegment2D& span, const Polygon2D& polygon) {
    return spanContactsPolygonBoundary(span, polygon)
        || pointInPolygon(span.start, polygon)
        || pointInPolygon(span.end, polygon);
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
        const auto* exitZone = fromZone->kind == ZoneKind::Exit ? fromZone : toZone;
        const auto* walkableZone = fromZone->kind == ZoneKind::Exit ? toZone : fromZone;
        return spanContactsPolygonBoundary(connection.centerSpan, walkableZone->area)
            && spanInteractsWithPolygon(connection.centerSpan, exitZone->area);
    }

    return spanContactsPolygonBoundary(connection.centerSpan, fromZone->area)
        && spanContactsPolygonBoundary(connection.centerSpan, toZone->area);
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
                .code = ImportIssueCode::ConnectionSpanMisaligned,
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
