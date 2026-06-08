#include "domain/ScenarioSimulationInternal.h"

#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"
#include "domain/ScenarioSimulationSystems.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace safecrowd::domain::simulation_internal {

Bounds boundsOf(const Polygon2D& polygon) {
    if (polygon.outline.empty()) {
        return {};
    }

    Bounds bounds{
        .minX = polygon.outline.front().x,
        .minY = polygon.outline.front().y,
        .maxX = polygon.outline.front().x,
        .maxY = polygon.outline.front().y,
    };
    for (const auto& point : polygon.outline) {
        bounds.minX = std::min(bounds.minX, point.x);
        bounds.minY = std::min(bounds.minY, point.y);
        bounds.maxX = std::max(bounds.maxX, point.x);
        bounds.maxY = std::max(bounds.maxY, point.y);
    }
    return bounds;
}

double distanceBetween(const Point2D& lhs, const Point2D& rhs) {
    return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y);
}

double occupantInteractionPressureScore(double distance, double lhsRadius, double rhsRadius) noexcept {
    const auto pressureDistance = std::max(0.0, lhsRadius) + std::max(0.0, rhsRadius) + kPersonalSpaceBuffer;
    if (distance >= pressureDistance) {
        return 0.0;
    }

    return std::clamp((pressureDistance - std::max(0.0, distance)) / kPersonalSpaceBuffer, 0.0, 1.0);
}

Point2D operator+(const Point2D& lhs, const Point2D& rhs) {
    return {.x = lhs.x + rhs.x, .y = lhs.y + rhs.y};
}

Point2D operator-(const Point2D& lhs, const Point2D& rhs) {
    return {.x = lhs.x - rhs.x, .y = lhs.y - rhs.y};
}

Point2D operator*(const Point2D& point, double scalar) {
    return {.x = point.x * scalar, .y = point.y * scalar};
}

double lengthOf(const Point2D& point) {
    return std::hypot(point.x, point.y);
}

double dot(const Point2D& lhs, const Point2D& rhs) {
    return (lhs.x * rhs.x) + (lhs.y * rhs.y);
}

double crossMagnitude(const Point2D& lhs, const Point2D& rhs) {
    return std::fabs((lhs.x * rhs.y) - (lhs.y * rhs.x));
}

Point2D perpendicularLeft(const Point2D& point) {
    return {.x = -point.y, .y = point.x};
}

Point2D normalizedOr(const Point2D& point, Point2D fallback) {
    const auto length = lengthOf(point);
    if (length <= 1e-9) {
        return fallback;
    }
    return {.x = point.x / length, .y = point.y / length};
}

Point2D clampedToLength(const Point2D& point, double maxLength) {
    const auto length = lengthOf(point);
    if (length <= maxLength || length <= 1e-9) {
        return point;
    }
    return point * (maxLength / length);
}

Point2D midpoint(const LineSegment2D& line) {
    return {
        .x = (line.start.x + line.end.x) * 0.5,
        .y = (line.start.y + line.end.y) * 0.5,
    };
}

double lengthSquaredOf(const LineSegment2D& line) {
    const auto delta = line.end - line.start;
    return dot(delta, delta);
}

LineSegment2D pointPassage(const Point2D& point) {
    return {.start = point, .end = point};
}

LineSegment2D passageWithClearance(const Connection2D& connection, double clearance) {
    const auto span = connection.centerSpan;
    const auto delta = span.end - span.start;
    const auto length = lengthOf(delta);
    if (length <= 1e-9) {
        return span;
    }

    const auto inset = std::min(clearance, length * 0.45);
    const auto direction = delta * (1.0 / length);
    return {
        .start = span.start + (direction * inset),
        .end = span.end - (direction * inset),
    };
}

Point2D routeWaypointTarget(const EvacuationRoute& route, const Point2D& position) {
    if (route.nextWaypointIndex < route.waypointPassages.size()
        && lengthSquaredOf(route.waypointPassages[route.nextWaypointIndex]) > 1e-9) {
        const auto& passage = route.waypointPassages[route.nextWaypointIndex];
        return closestPointOnSegment(position, passage.start, passage.end);
    }

    if (route.nextWaypointIndex < route.waypoints.size()) {
        return route.waypoints[route.nextWaypointIndex];
    }
    return position;
}

double distanceToRouteWaypoint(const EvacuationRoute& route, const Point2D& position) {
    if (route.nextWaypointIndex < route.waypointPassages.size()
        && lengthSquaredOf(route.waypointPassages[route.nextWaypointIndex]) > 1e-9) {
        const auto& passage = route.waypointPassages[route.nextWaypointIndex];
        return distanceBetween(position, closestPointOnSegment(position, passage.start, passage.end));
    }
    return distanceBetween(position, routeWaypointTarget(route, position));
}

LayoutBounds boundsOf(const FacilityLayout2D& layout) {
    LayoutBounds bounds;
    for (const auto& zone : layout.zones) {
        for (const auto& point : zone.area.outline) {
            bounds.minX = std::min(bounds.minX, point.x);
            bounds.minY = std::min(bounds.minY, point.y);
            bounds.maxX = std::max(bounds.maxX, point.x);
            bounds.maxY = std::max(bounds.maxY, point.y);
        }
    }
    return bounds;
}

double cross(const Point2D& a, const Point2D& b, const Point2D& c) {
    return ((b.x - a.x) * (c.y - a.y)) - ((b.y - a.y) * (c.x - a.x));
}

bool pointOnSegment(const Point2D& point, const Point2D& start, const Point2D& end) {
    return std::fabs(cross(start, end, point)) <= kGeometryEpsilon
        && point.x >= std::min(start.x, end.x) - kGeometryEpsilon
        && point.x <= std::max(start.x, end.x) + kGeometryEpsilon
        && point.y >= std::min(start.y, end.y) - kGeometryEpsilon
        && point.y <= std::max(start.y, end.y) + kGeometryEpsilon;
}

bool segmentsIntersect(const Point2D& firstStart, const Point2D& firstEnd, const Point2D& secondStart, const Point2D& secondEnd) {
    const auto d1 = cross(firstStart, firstEnd, secondStart);
    const auto d2 = cross(firstStart, firstEnd, secondEnd);
    const auto d3 = cross(secondStart, secondEnd, firstStart);
    const auto d4 = cross(secondStart, secondEnd, firstEnd);

    if (((d1 > kGeometryEpsilon && d2 < -kGeometryEpsilon) || (d1 < -kGeometryEpsilon && d2 > kGeometryEpsilon))
        && ((d3 > kGeometryEpsilon && d4 < -kGeometryEpsilon) || (d3 < -kGeometryEpsilon && d4 > kGeometryEpsilon))) {
        return true;
    }

    return pointOnSegment(secondStart, firstStart, firstEnd)
        || pointOnSegment(secondEnd, firstStart, firstEnd)
        || pointOnSegment(firstStart, secondStart, secondEnd)
        || pointOnSegment(firstEnd, secondStart, secondEnd);
}

bool segmentBoundsOverlap(
    const Point2D& firstStart,
    const Point2D& firstEnd,
    const Point2D& secondStart,
    const Point2D& secondEnd,
    double padding = 0.0) {
    const auto firstMinX = std::min(firstStart.x, firstEnd.x) - padding;
    const auto firstMaxX = std::max(firstStart.x, firstEnd.x) + padding;
    const auto firstMinY = std::min(firstStart.y, firstEnd.y) - padding;
    const auto firstMaxY = std::max(firstStart.y, firstEnd.y) + padding;
    const auto secondMinX = std::min(secondStart.x, secondEnd.x);
    const auto secondMaxX = std::max(secondStart.x, secondEnd.x);
    const auto secondMinY = std::min(secondStart.y, secondEnd.y);
    const auto secondMaxY = std::max(secondStart.y, secondEnd.y);
    return firstMinX <= secondMaxX
        && firstMaxX >= secondMinX
        && firstMinY <= secondMaxY
        && firstMaxY >= secondMinY;
}

bool pointWithinSegmentBounds(const Point2D& point, const Point2D& start, const Point2D& end, double padding) {
    return point.x >= std::min(start.x, end.x) - padding
        && point.x <= std::max(start.x, end.x) + padding
        && point.y >= std::min(start.y, end.y) - padding
        && point.y <= std::max(start.y, end.y) + padding;
}

double segmentDistance(const Point2D& firstStart, const Point2D& firstEnd, const Point2D& secondStart, const Point2D& secondEnd) {
    if (segmentsIntersect(firstStart, firstEnd, secondStart, secondEnd)) {
        return 0.0;
    }
    return std::min({
        distancePointToSegment(firstStart, secondStart, secondEnd),
        distancePointToSegment(firstEnd, secondStart, secondEnd),
        distancePointToSegment(secondStart, firstStart, firstEnd),
        distancePointToSegment(secondEnd, firstStart, firstEnd),
    });
}

bool pointInAnyZone(const FacilityLayout2D& layout, const Point2D& point) {
    return std::any_of(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return pointInRing(zone.area.outline, point);
    });
}

const Zone2D* findZone(const FacilityLayout2D& layout, const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

std::string floorIdForZone(const FacilityLayout2D& layout, const std::string& zoneId) {
    const auto* zone = findZone(layout, zoneId);
    return zone == nullptr ? std::string{} : zone->floorId;
}

bool isVerticalConnection(const Connection2D& connection) {
    return connection.kind == ConnectionKind::Stair || connection.kind == ConnectionKind::Ramp
        || connection.isStair || connection.isRamp;
}

bool canTraverseConnection(const FacilityLayout2D& layout, const Connection2D& connection) {
    const auto fromFloorId = floorIdForZone(layout, connection.fromZoneId);
    const auto toFloorId = floorIdForZone(layout, connection.toZoneId);
    if (fromFloorId.empty() || toFloorId.empty() || fromFloorId == toFloorId) {
        return true;
    }
    return isVerticalConnection(connection);
}

double floorElevation(const FacilityLayout2D& layout, const std::string& floorId) {
    const auto it = std::find_if(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
        return floor.id == floorId;
    });
    return it == layout.floors.end() ? 0.0 : it->elevationMeters;
}

double floorElevation(const ScenarioLayoutCacheResource& cache, const std::string& floorId) {
    const auto it = cache.floorElevations.find(floorId);
    return it == cache.floorElevations.end() ? 0.0 : it->second;
}

StairEntryDirection stairEntryDirectionForFloor(
    const FacilityLayout2D& layout,
    const Connection2D& connection,
    const std::string& floorId) {
    if (!isVerticalConnection(connection)) {
        return StairEntryDirection::Unspecified;
    }

    const auto fromFloorId = floorIdForZone(layout, connection.fromZoneId);
    const auto toFloorId = floorIdForZone(layout, connection.toZoneId);
    if (fromFloorId.empty() || toFloorId.empty() || fromFloorId == toFloorId) {
        return StairEntryDirection::Unspecified;
    }

    const auto fromElevation = floorElevation(layout, fromFloorId);
    const auto toElevation = floorElevation(layout, toFloorId);
    const bool fromIsLower = fromElevation <= toElevation;
    if (floorId == fromFloorId) {
        return fromIsLower ? connection.lowerEntryDirection : connection.upperEntryDirection;
    }
    if (floorId == toFloorId) {
        return fromIsLower ? connection.upperEntryDirection : connection.lowerEntryDirection;
    }
    return StairEntryDirection::Unspecified;
}

std::optional<ZoneRouteResult> searchZoneRouteToExit(
    const ScenarioLayoutCacheResource& cache,
    const Point2D& startPosition,
    const std::string& startZoneId,
    const std::string& exitZoneId,
    bool allowVerticalConnections) {
    if (startZoneId.empty()) {
        return std::nullopt;
    }

    std::unordered_set<std::string> exitZoneIds;
    if (exitZoneId.empty()) {
        exitZoneIds.reserve(cache.layout.zones.size());
        for (const auto& zone : cache.layout.zones) {
            if (zone.kind == ZoneKind::Exit) {
                exitZoneIds.insert(zone.id);
            }
        }
        if (exitZoneIds.empty()) {
            return std::nullopt;
        }
    } else {
        const auto* exitZone = findCachedZone(cache, exitZoneId);
        if (exitZone == nullptr || exitZone->kind != ZoneKind::Exit) {
            return std::nullopt;
        }
    }

    auto reachedExit = [&](const std::string& zoneId) {
        return exitZoneId.empty() ? exitZoneIds.contains(zoneId) : zoneId == exitZoneId;
    };
    if (reachedExit(startZoneId)) {
        return ZoneRouteResult{.route = ZoneRouteToExit{.zoneIds = {startZoneId}}, .distance = 0.0};
    }

    constexpr double kDefaultVerticalRouteCost = 3.0;
    constexpr std::size_t kStartConnectionIndex = static_cast<std::size_t>(-1);

    auto stateKey = [](const std::string& zoneId, std::size_t entryConnectionIndex) {
        return zoneId + '\x1f' + std::to_string(entryConnectionIndex);
    };
    auto verticalRouteCost = [&](const Connection2D& connection) {
        if (!isVerticalConnection(connection)) {
            return 0.0;
        }

        const auto fromFloorId = cachedFloorIdForZone(cache, connection.fromZoneId);
        const auto toFloorId = cachedFloorIdForZone(cache, connection.toZoneId);
        if (fromFloorId.empty() || toFloorId.empty() || fromFloorId == toFloorId) {
            return kDefaultVerticalRouteCost;
        }

        const auto elevationDelta = std::fabs(floorElevation(cache, fromFloorId) - floorElevation(cache, toFloorId));
        return elevationDelta > kGeometryEpsilon ? elevationDelta : kDefaultVerticalRouteCost;
    };

    struct QueueItem {
        double distance{0.0};
        std::string key{};
        std::string zoneId{};
        Point2D point{};
        std::size_t entryConnectionIndex{static_cast<std::size_t>(-1)};

        bool operator>(const QueueItem& other) const noexcept {
            return distance > other.distance;
        }
    };

    std::unordered_map<std::string, double> distances;
    distances.reserve(cache.layout.connections.size() + 1);
    std::unordered_map<std::string, std::string> previous;
    previous.reserve(cache.layout.connections.size() + 1);
    std::unordered_map<std::string, std::string> stateZones;
    stateZones.reserve(cache.layout.connections.size() + 1);
    std::unordered_map<std::string, std::size_t> stateConnectionIndices;
    stateConnectionIndices.reserve(cache.layout.connections.size() + 1);
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;

    const auto startKey = stateKey(startZoneId, kStartConnectionIndex);
    distances[startKey] = 0.0;
    stateZones[startKey] = startZoneId;
    stateConnectionIndices[startKey] = kStartConnectionIndex;
    queue.push({
        .distance = 0.0,
        .key = startKey,
        .zoneId = startZoneId,
        .point = startPosition,
        .entryConnectionIndex = kStartConnectionIndex,
    });

    double bestExitDistance = std::numeric_limits<double>::max();
    std::string bestExitKey;

    while (!queue.empty()) {
        const auto current = queue.top();
        queue.pop();

        if (current.distance > bestExitDistance + 1e-12) {
            continue;
        }

        const auto bestIt = distances.find(current.key);
        if (bestIt == distances.end() || current.distance > bestIt->second + 1e-12) {
            continue;
        }

        if (reachedExit(current.zoneId)) {
            if (current.distance + 1e-12 < bestExitDistance) {
                bestExitDistance = current.distance;
                bestExitKey = current.key;
            }
            continue;
        }

        for (const auto& traversal : cachedTraversalsForZone(cache, current.zoneId)) {
            if (traversal.nextZoneId.empty() || traversal.connectionIndex >= cache.layout.connections.size()) {
                continue;
            }

            const auto& connection = cache.layout.connections[traversal.connectionIndex];
            if (!allowVerticalConnections && isVerticalConnection(connection)) {
                continue;
            }
            const auto portal = midpoint(connection.centerSpan);
            const auto nextDistance = current.distance + distanceBetween(current.point, portal) + verticalRouteCost(connection);
            const auto nextKey = stateKey(traversal.nextZoneId, traversal.connectionIndex);
            const auto distanceIt = distances.find(nextKey);
            if (distanceIt == distances.end() || nextDistance + 1e-12 < distanceIt->second) {
                distances[nextKey] = nextDistance;
                previous[nextKey] = current.key;
                stateZones[nextKey] = traversal.nextZoneId;
                stateConnectionIndices[nextKey] = traversal.connectionIndex;
                queue.push({
                    .distance = nextDistance,
                    .key = nextKey,
                    .zoneId = traversal.nextZoneId,
                    .point = portal,
                    .entryConnectionIndex = traversal.connectionIndex,
                });
            }
        }
    }

    if (bestExitKey.empty()) {
        return std::nullopt;
    }

    ZoneRouteToExit route;
    for (auto key = bestExitKey; !key.empty();) {
        const auto zoneIt = stateZones.find(key);
        if (zoneIt != stateZones.end()) {
            route.zoneIds.push_back(zoneIt->second);
        }
        const auto connectionIt = stateConnectionIndices.find(key);
        if (connectionIt != stateConnectionIndices.end() && connectionIt->second != kStartConnectionIndex) {
            route.connectionIndices.push_back(connectionIt->second);
        }
        const auto previousIt = previous.find(key);
        key = previousIt == previous.end() ? std::string{} : previousIt->second;
    }
    std::reverse(route.zoneIds.begin(), route.zoneIds.end());
    std::reverse(route.connectionIndices.begin(), route.connectionIndices.end());
    if (route.zoneIds.empty() || route.connectionIndices.size() + 1 != route.zoneIds.size()) {
        return std::nullopt;
    }
    return ZoneRouteResult{.route = std::move(route), .distance = bestExitDistance};
}

bool zoneMatchesFloor(const Zone2D& zone, const std::string& floorId) {
    return safecrowd::domain::matchesFloor(zone.floorId, floorId);
}

std::vector<LineSegment2D> stairEntryBarrierSegments(const Zone2D& zone, StairEntryDirection entryDirection) {
    std::vector<LineSegment2D> segments;
    if (entryDirection == StairEntryDirection::Unspecified || zone.area.outline.empty()) {
        return segments;
    }

    const auto bounds = boundsOf(zone.area);
    const LineSegment2D north{{bounds.minX, bounds.maxY}, {bounds.maxX, bounds.maxY}};
    const LineSegment2D east{{bounds.maxX, bounds.maxY}, {bounds.maxX, bounds.minY}};
    const LineSegment2D south{{bounds.maxX, bounds.minY}, {bounds.minX, bounds.minY}};
    const LineSegment2D west{{bounds.minX, bounds.minY}, {bounds.minX, bounds.maxY}};

    if (entryDirection != StairEntryDirection::North) {
        segments.push_back(north);
    }
    if (entryDirection != StairEntryDirection::East) {
        segments.push_back(east);
    }
    if (entryDirection != StairEntryDirection::South) {
        segments.push_back(south);
    }
    if (entryDirection != StairEntryDirection::West) {
        segments.push_back(west);
    }
    return segments;
}

std::vector<LineSegment2D> barrierSegmentAfterConnectionGap(
    const LineSegment2D& segment,
    const LineSegment2D& gap) {
    const auto segmentDelta = segment.end - segment.start;
    const auto segmentLength = lengthOf(segmentDelta);
    if (segmentLength <= kGeometryEpsilon) {
        return {};
    }

    if (std::fabs(cross(segment.start, segment.end, gap.start)) > kGeometryEpsilon
        || std::fabs(cross(segment.start, segment.end, gap.end)) > kGeometryEpsilon) {
        return {segment};
    }

    const auto direction = segmentDelta * (1.0 / segmentLength);
    auto gapStart = dot(gap.start - segment.start, direction);
    auto gapEnd = dot(gap.end - segment.start, direction);
    if (gapStart > gapEnd) {
        std::swap(gapStart, gapEnd);
    }

    const auto overlapStart = std::max(0.0, gapStart);
    const auto overlapEnd = std::min(segmentLength, gapEnd);
    if (overlapEnd <= overlapStart + kGeometryEpsilon) {
        return {segment};
    }

    std::vector<LineSegment2D> remaining;
    if (overlapStart > kGeometryEpsilon) {
        remaining.push_back({
            .start = segment.start,
            .end = segment.start + (direction * overlapStart),
        });
    }
    if (overlapEnd < segmentLength - kGeometryEpsilon) {
        remaining.push_back({
            .start = segment.start + (direction * overlapEnd),
            .end = segment.end,
        });
    }
    return remaining;
}

std::vector<LineSegment2D> clipStairEntryBarrierSegment(
    const LineSegment2D& segment,
    const FacilityLayout2D& source,
    const Zone2D& stairZone) {
    std::vector<LineSegment2D> remaining{segment};
    for (const auto& connection : source.connections) {
        if (!isVerticalConnection(connection)
            || (connection.fromZoneId != stairZone.id && connection.toZoneId != stairZone.id)) {
            continue;
        }

        std::vector<LineSegment2D> next;
        for (const auto& candidate : remaining) {
            auto clipped = barrierSegmentAfterConnectionGap(candidate, connection.centerSpan);
            next.insert(next.end(), clipped.begin(), clipped.end());
        }
        remaining = std::move(next);
        if (remaining.empty()) {
            break;
        }
    }
    return remaining;
}

void appendStairEntryBarriers(FacilityLayout2D& filtered, const FacilityLayout2D& source, const std::string& floorId) {
    int suffix = 1;
    for (const auto& connection : source.connections) {
        const auto direction = stairEntryDirectionForFloor(source, connection, floorId);
        if (direction == StairEntryDirection::Unspecified) {
            continue;
        }

        const auto* fromZone = findZone(source, connection.fromZoneId);
        const auto* toZone = findZone(source, connection.toZoneId);
        const auto* stairZone = fromZone != nullptr && zoneMatchesFloor(*fromZone, floorId)
            ? fromZone
            : (toZone != nullptr && zoneMatchesFloor(*toZone, floorId) ? toZone : nullptr);
        if (stairZone == nullptr || (!stairZone->isStair && !stairZone->isRamp && stairZone->kind != ZoneKind::Stair)) {
            continue;
        }

        for (const auto& segment : stairEntryBarrierSegments(*stairZone, direction)) {
            for (const auto& clippedSegment : clipStairEntryBarrierSegment(segment, source, *stairZone)) {
                filtered.barriers.push_back({
                    .id = connection.id + "-entry-wall-" + std::to_string(suffix++),
                    .floorId = stairZone->floorId,
                    .geometry = {.vertices = {clippedSegment.start, clippedSegment.end}},
                    .blocksMovement = true,
                });
            }
        }
    }
}

std::string verticalPhysicsFloorIdForConnectionId(const std::string& connectionId) {
    return connectionId.empty() ? std::string{} : "vertical:" + connectionId;
}

FacilityLayout2D layoutForFloor(const FacilityLayout2D& layout, const std::string& floorId) {
    if (floorId.empty()) {
        return layout;
    }

    FacilityLayout2D filtered;
    filtered.id = layout.id;
    filtered.name = layout.name;
    filtered.levelId = layout.levelId;
    filtered.floors = layout.floors;
    for (const auto& zone : layout.zones) {
        if (matchesFloor(zone.floorId, floorId)) {
            filtered.zones.push_back(zone);
        }
    }
    for (const auto& connection : layout.connections) {
        if (matchesFloor(connection.floorId, floorId)) {
            filtered.connections.push_back(connection);
        }
    }
    for (const auto& barrier : layout.barriers) {
        if (matchesFloor(barrier.floorId, floorId)) {
            filtered.barriers.push_back(barrier);
        }
    }
    for (const auto& control : layout.controls) {
        if (matchesFloor(control.floorId, floorId)) {
            filtered.controls.push_back(control);
        }
    }
    appendStairEntryBarriers(filtered, layout, floorId);
    return filtered;
}

bool sameBarrierPoint(const Point2D& lhs, const Point2D& rhs) {
    return std::fabs(lhs.x - rhs.x) <= kGeometryEpsilon
        && std::fabs(lhs.y - rhs.y) <= kGeometryEpsilon;
}

bool sameBarrierGeometry(const Barrier2D& lhs, const Barrier2D& rhs) {
    if (lhs.blocksMovement != rhs.blocksMovement
        || lhs.geometry.closed != rhs.geometry.closed
        || lhs.geometry.vertices.size() != rhs.geometry.vertices.size()) {
        return false;
    }

    const auto& lhsVertices = lhs.geometry.vertices;
    const auto& rhsVertices = rhs.geometry.vertices;
    const bool sameOrder = [&]() {
        for (std::size_t index = 0; index < lhsVertices.size(); ++index) {
            if (!sameBarrierPoint(lhsVertices[index], rhsVertices[index])) {
                return false;
            }
        }
        return true;
    }();
    if (sameOrder) {
        return true;
    }

    if (lhsVertices.size() != 2) {
        return false;
    }
    return sameBarrierPoint(lhsVertices[0], rhsVertices[1])
        && sameBarrierPoint(lhsVertices[1], rhsVertices[0]);
}

void appendUniqueBarrier(std::vector<Barrier2D>& barriers, Barrier2D barrier) {
    if (std::any_of(barriers.begin(), barriers.end(), [&](const auto& existing) {
            return sameBarrierGeometry(existing, barrier);
        })) {
        return;
    }
    barriers.push_back(std::move(barrier));
}

FacilityLayout2D layoutForVerticalConnection(const FacilityLayout2D& layout, const Connection2D& connection) {
    const auto virtualFloorId = verticalPhysicsFloorIdForConnectionId(connection.id);
    FacilityLayout2D filtered;
    filtered.id = layout.id;
    filtered.name = layout.name;
    filtered.levelId = layout.levelId;
    filtered.floors.push_back({
        .id = virtualFloorId,
        .label = connection.id.empty() ? std::string{"Vertical connection"} : connection.id,
    });

    auto appendEndpointZone = [&](const std::string& zoneId) {
        const auto* zone = findZone(layout, zoneId);
        if (zone == nullptr) {
            return;
        }
        auto copy = *zone;
        copy.floorId = virtualFloorId;
        filtered.zones.push_back(std::move(copy));
    };
    appendEndpointZone(connection.fromZoneId);
    appendEndpointZone(connection.toZoneId);

    auto verticalConnection = connection;
    verticalConnection.floorId = virtualFloorId;
    filtered.connections.push_back(std::move(verticalConnection));

    auto appendFloorBarriers = [&](const std::string& floorId) {
        if (floorId.empty()) {
            return;
        }
        auto floorLayout = layoutForFloor(layout, floorId);
        for (auto barrier : floorLayout.barriers) {
            barrier.floorId = virtualFloorId;
            appendUniqueBarrier(filtered.barriers, std::move(barrier));
        }
    };
    appendFloorBarriers(floorIdForZone(layout, connection.fromZoneId));
    appendFloorBarriers(floorIdForZone(layout, connection.toZoneId));

    return filtered;
}

ScenarioLayoutCacheResource buildScenarioLayoutCache(FacilityLayout2D layout) {
    ScenarioLayoutCacheResource cache;
    cache.layout = std::move(layout);

    std::vector<std::string> floorIds;
    auto addFloorId = [&](const std::string& floorId) {
        if (floorId.empty()) {
            return;
        }
        if (std::find(floorIds.begin(), floorIds.end(), floorId) == floorIds.end()) {
            floorIds.push_back(floorId);
        }
    };

    for (const auto& floor : cache.layout.floors) {
        cache.floorElevations[floor.id] = floor.elevationMeters;
        addFloorId(floor.id);
    }
    for (std::size_t index = 0; index < cache.layout.zones.size(); ++index) {
        const auto& zone = cache.layout.zones[index];
        cache.zoneIndices[zone.id] = index;
        cache.zoneFloorIds[zone.id] = zone.floorId;
        addFloorId(zone.floorId);
    }
    for (const auto& connection : cache.layout.connections) {
        addFloorId(connection.floorId);
    }
    for (const auto& barrier : cache.layout.barriers) {
        addFloorId(barrier.floorId);
    }
    for (const auto& control : cache.layout.controls) {
        addFloorId(control.floorId);
    }

    for (const auto& floorId : floorIds) {
        cache.floorLayouts.emplace(floorId, layoutForFloor(cache.layout, floorId));
    }
    for (const auto& connection : cache.layout.connections) {
        if (!isVerticalConnection(connection) || connection.id.empty()) {
            continue;
        }
        cache.floorLayouts.emplace(
            verticalPhysicsFloorIdForConnectionId(connection.id),
            layoutForVerticalConnection(cache.layout, connection));
    }

    for (std::size_t index = 0; index < cache.layout.connections.size(); ++index) {
        const auto& connection = cache.layout.connections[index];
        if (!connection.id.empty()) {
            cache.connectionIndices[connection.id] = index;
        }
        if (connection.directionality == TravelDirection::Closed || !canTraverseConnection(cache.layout, connection)) {
            continue;
        }
        if (connection.directionality != TravelDirection::ReverseOnly) {
            cache.traversableConnectionsByZone[connection.fromZoneId].push_back({
                .nextZoneId = connection.toZoneId,
                .connectionIndex = index,
            });
        }
        if (connection.directionality != TravelDirection::ForwardOnly) {
            cache.traversableConnectionsByZone[connection.toZoneId].push_back({
                .nextZoneId = connection.fromZoneId,
                .connectionIndex = index,
            });
        }
    }

    return cache;
}

const FacilityLayout2D& cachedLayoutForFloor(const ScenarioLayoutCacheResource& cache, const std::string& floorId) {
    if (floorId.empty()) {
        return cache.layout;
    }
    const auto it = cache.floorLayouts.find(floorId);
    return it == cache.floorLayouts.end() ? cache.layout : it->second;
}

const Zone2D* findCachedZone(const ScenarioLayoutCacheResource& cache, const std::string& zoneId) {
    const auto it = cache.zoneIndices.find(zoneId);
    if (it == cache.zoneIndices.end() || it->second >= cache.layout.zones.size()) {
        return nullptr;
    }
    return &cache.layout.zones[it->second];
}

const Connection2D* findCachedConnectionBetween(
    const ScenarioLayoutCacheResource& cache,
    const std::string& from,
    const std::string& to) {
    const auto it = cache.traversableConnectionsByZone.find(from);
    if (it == cache.traversableConnectionsByZone.end()) {
        return nullptr;
    }
    for (const auto& traversal : it->second) {
        if (traversal.nextZoneId == to && traversal.connectionIndex < cache.layout.connections.size()) {
            return &cache.layout.connections[traversal.connectionIndex];
        }
    }
    return nullptr;
}

std::string cachedFloorIdForZone(const ScenarioLayoutCacheResource& cache, const std::string& zoneId) {
    const auto it = cache.zoneFloorIds.find(zoneId);
    return it == cache.zoneFloorIds.end() ? std::string{} : it->second;
}

const std::vector<ScenarioConnectionTraversal>& cachedTraversalsForZone(
    const ScenarioLayoutCacheResource& cache,
    const std::string& zoneId) {
    static const std::vector<ScenarioConnectionTraversal> empty;
    const auto it = cache.traversableConnectionsByZone.find(zoneId);
    return it == cache.traversableConnectionsByZone.end() ? empty : it->second;
}

std::string agentDisplayFloorId(const EvacuationRoute& route) {
    return route.displayFloorId.empty() ? route.currentFloorId : route.displayFloorId;
}

std::string agentCollisionFloorId(const EvacuationRoute& route) {
    return route.physicsFloorId.empty() ? agentDisplayFloorId(route) : route.physicsFloorId;
}

bool agentCollisionScopesOverlap(const EvacuationRoute& lhs, const EvacuationRoute& rhs) {
    if (agentCollisionFloorId(lhs) == agentCollisionFloorId(rhs)) {
        return true;
    }
    const auto lhsDisplayFloorId = agentDisplayFloorId(lhs);
    return !lhsDisplayFloorId.empty() && lhsDisplayFloorId == agentDisplayFloorId(rhs);
}

std::string verticalPhysicsFloorId(const Connection2D& connection) {
    return verticalPhysicsFloorIdForConnectionId(connection.id);
}

std::string physicsFloorIdForVerticalConnection(const Connection2D* connection) {
    if (connection == nullptr || !isVerticalConnection(*connection)) {
        return {};
    }
    return verticalPhysicsFloorId(*connection);
}

std::string physicsFloorIdForPositionInEndpointZone(
    const ScenarioLayoutCacheResource& cache,
    const Point2D& position,
    const Agent& agent,
    const EvacuationRoute& route) {
    const auto currentFloorId = route.currentFloorId.empty() ? agentDisplayFloorId(route) : route.currentFloorId;
    const auto currentZoneId = zoneAt(cache, position, currentFloorId);
    if (currentZoneId.empty()) {
        return {};
    }

    const auto* currentZone = findCachedZone(cache, currentZoneId);
    if (currentZone == nullptr) {
        return {};
    }

    const auto influenceRadius =
        static_cast<double>(agent.radius) + kDefaultAgentRadius + kPersonalSpaceBuffer;

    const Connection2D* bestConnection = nullptr;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (const auto& traversal : cachedTraversalsForZone(cache, currentZoneId)) {
        if (traversal.connectionIndex >= cache.layout.connections.size()) {
            continue;
        }

        const auto& connection = cache.layout.connections[traversal.connectionIndex];
        if (!isVerticalConnection(connection)) {
            continue;
        }

        const auto distanceToPassage = distancePointToSegment(position, connection.centerSpan.start, connection.centerSpan.end);
        if (distanceToPassage > influenceRadius) {
            continue;
        }
        if (distanceToPassage < bestDistance) {
            bestConnection = &connection;
            bestDistance = distanceToPassage;
        }
    }

    return physicsFloorIdForVerticalConnection(bestConnection);
}

void updateAgentPhysicsFloorIds(
    engine::WorldQuery& query,
    const ScenarioLayoutCacheResource& cache,
    const std::vector<engine::Entity>& entities) {
    for (const auto entity : entities) {
        if (!query.contains<Position>(entity)
            || !query.contains<Agent>(entity)
            || !query.contains<EvacuationRoute>(entity)
            || !query.contains<EvacuationStatus>(entity)) {
            continue;
        }
        if (query.get<EvacuationStatus>(entity).evacuated) {
            query.get<EvacuationRoute>(entity).physicsFloorId.clear();
            continue;
        }

        const auto& position = query.get<Position>(entity);
        const auto& agent = query.get<Agent>(entity);
        auto& route = query.get<EvacuationRoute>(entity);
        route.physicsFloorId = physicsFloorIdForPositionInEndpointZone(cache, position.value, agent, route);
    }
}

std::string zoneAt(const ScenarioLayoutCacheResource& cache, const Point2D& point, const std::string& floorId) {
    const auto& floorLayout = cachedLayoutForFloor(cache, floorId);
    const Zone2D* boundaryZone = nullptr;
    double boundaryDistance = std::numeric_limits<double>::infinity();
    constexpr double kZoneBoundaryTolerance = 0.04;
    for (const auto& zone : floorLayout.zones) {
        if (pointInRing(zone.area.outline, point)) {
            return zone.id;
        }
        const auto distance = distanceToPolygonBoundary(zone.area, point);
        if (distance <= kZoneBoundaryTolerance && distance < boundaryDistance) {
            boundaryZone = &zone;
            boundaryDistance = distance;
        }
    }
    return boundaryZone == nullptr ? std::string{} : boundaryZone->id;
}

Point2D passageNormalToward(const LineSegment2D& passage, const Zone2D& fromZone, const Zone2D& toZone) {
    const auto passageDirection = passage.end - passage.start;
    const auto firstNormal = normalizedOr(perpendicularLeft(passageDirection), {});
    if (lengthOf(firstNormal) <= 1e-9) {
        return normalizedOr(polygonCenter(toZone.area) - polygonCenter(fromZone.area), {});
    }

    const auto towardToZone = polygonCenter(toZone.area) - midpoint(passage);
    if (dot(firstNormal, towardToZone) >= 0.0) {
        return firstNormal;
    }
    return firstNormal * -1.0;
}

bool routePassageCrossed(
    const FacilityLayout2D& layout,
    const EvacuationRoute& route,
    const Point2D& position,
    double agentRadius) {
    (void)agentRadius;
    if (route.nextWaypointIndex >= route.waypointPassages.size()
        || route.nextWaypointIndex >= route.waypointFromZoneIds.size()
        || route.nextWaypointIndex >= route.waypointZoneIds.size()) {
        return false;
    }

    const auto& passage = route.waypointPassages[route.nextWaypointIndex];
    if (lengthSquaredOf(passage) <= 1e-9) {
        return false;
    }

    const auto& fromZoneId = route.waypointFromZoneIds[route.nextWaypointIndex];
    const auto& toZoneId = route.waypointZoneIds[route.nextWaypointIndex];
    if (fromZoneId.empty() || toZoneId.empty()) {
        return false;
    }

    const auto* fromZone = findZone(layout, fromZoneId);
    const auto* toZone = findZone(layout, toZoneId);
    if (fromZone == nullptr || toZone == nullptr) {
        return false;
    }

    const auto normal = passageNormalToward(passage, *fromZone, *toZone);
    if (lengthOf(normal) <= 1e-9) {
        return false;
    }

    const auto signedDistance = dot(position - midpoint(passage), normal);
    return signedDistance > kPortalCrossingEpsilon;
}

const Connection2D* findConnectionBetween(const FacilityLayout2D& layout, const std::string& from, const std::string& to) {
    const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        if (connection.directionality == TravelDirection::Closed) {
            return false;
        }
        if (!canTraverseConnection(layout, connection)) {
            return false;
        }
        const bool forward = connection.fromZoneId == from && connection.toZoneId == to;
        const bool reverse = connection.fromZoneId == to && connection.toZoneId == from;
        if (forward) {
            return connection.directionality != TravelDirection::ReverseOnly;
        }
        if (reverse) {
            return connection.directionality != TravelDirection::ForwardOnly;
        }
        return false;
    });
    return it == layout.connections.end() ? nullptr : &(*it);
}

std::optional<ZoneRouteToExit> zoneRouteToNearestExit(
    const ScenarioLayoutCacheResource& cache,
    const Point2D& startPosition,
    const std::string& startZoneId) {
    if (const auto sameFloorRoute = searchZoneRouteToExit(cache, startPosition, startZoneId, std::string{}, false);
        sameFloorRoute.has_value()) {
        return sameFloorRoute->route;
    }
    const auto route = searchZoneRouteToExit(cache, startPosition, startZoneId, std::string{}, true);
    return route.has_value() ? std::optional<ZoneRouteToExit>{route->route} : std::nullopt;
}

std::optional<ZoneRouteResult> zoneRouteToExit(
    const ScenarioLayoutCacheResource& cache,
    const Point2D& startPosition,
    const std::string& startZoneId,
    const std::string& exitZoneId) {
    const auto startFloorId = cachedFloorIdForZone(cache, startZoneId);
    const auto exitFloorId = cachedFloorIdForZone(cache, exitZoneId);
    if (!startFloorId.empty() && startFloorId == exitFloorId) {
        if (const auto sameFloorRoute = searchZoneRouteToExit(cache, startPosition, startZoneId, exitZoneId, false);
            sameFloorRoute.has_value()) {
            return sameFloorRoute;
        }
    }
    return searchZoneRouteToExit(cache, startPosition, startZoneId, exitZoneId, true);
}

double speedOf(const Point2D& velocity) {
    const auto speed = std::hypot(velocity.x, velocity.y);
    return speed > 0.0 ? speed : kDefaultAgentSpeed;
}

std::vector<engine::Entity> simulationEntities(engine::WorldQuery& query) {
    return query.view<Position, Agent, Velocity, AvoidanceState, EvacuationRoute, EvacuationStatus>();
}

void propagateStalledStateThroughQueues(SimulationFrame& frame) {
    std::vector<std::size_t> stalledIndexes;
    stalledIndexes.reserve(frame.agents.size());
    for (std::size_t index = 0; index < frame.agents.size(); ++index) {
        if (frame.agents[index].stalled) {
            stalledIndexes.push_back(index);
        }
    }
    if (stalledIndexes.size() < 2) {
        return;
    }

    std::vector<std::size_t> propagatedIndexes;
    for (std::size_t index = 0; index < frame.agents.size(); ++index) {
        auto& candidate = frame.agents[index];
        if (candidate.stalled) {
            continue;
        }

        const auto speed = lengthOf(candidate.velocity);
        if (speed <= kScenarioStalledSpeedThreshold) {
            continue;
        }

        const auto forward = candidate.velocity * (1.0 / speed);
        bool hasStalledAhead = false;
        bool hasStalledBehind = false;
        for (const auto stalledIndex : stalledIndexes) {
            const auto& stalled = frame.agents[stalledIndex];
            if (stalled.floorId != candidate.floorId) {
                continue;
            }

            const auto offset = stalled.position - candidate.position;
            const auto distance = lengthOf(offset);
            const auto reach = std::max(0.0, candidate.radius)
                + std::max(0.0, stalled.radius)
                + kStalledQueuePropagationExtraReach;
            if (distance > reach) {
                continue;
            }

            const auto lateralDistance = crossMagnitude(forward, offset);
            const auto lateralTolerance = std::max(0.0, candidate.radius)
                + std::max(0.0, stalled.radius)
                + kStalledQueuePropagationLateralBuffer;
            if (lateralDistance > lateralTolerance) {
                continue;
            }

            const auto longitudinalDistance = dot(offset, forward);
            if (longitudinalDistance >= kStalledQueuePropagationMinimumLongitudinal) {
                hasStalledAhead = true;
            } else if (longitudinalDistance <= -kStalledQueuePropagationMinimumLongitudinal) {
                hasStalledBehind = true;
            }

            if (hasStalledAhead && hasStalledBehind) {
                propagatedIndexes.push_back(index);
                break;
            }
        }
    }

    for (const auto index : propagatedIndexes) {
        frame.agents[index].stalled = true;
    }
}

AgentSpatialIndex buildAgentSpatialIndex(
    engine::WorldQuery& query,
    const std::vector<engine::Entity>& entities,
    double cellSize) {
    AgentSpatialIndex index;
    index.cellSize = cellSize;
    index.cellsByFloor.reserve(4);
    index.displayCellsByFloor.reserve(4);

    for (const auto entity : entities) {
        const auto& status = query.get<EvacuationStatus>(entity);
        if (status.evacuated) {
            continue;
        }
        const auto& position = query.get<Position>(entity);
        const auto* route = query.contains<EvacuationRoute>(entity) ? &query.get<EvacuationRoute>(entity) : nullptr;
        const auto floorId = route != nullptr ? agentCollisionFloorId(*route) : std::string{};
        const auto displayFloorId = route != nullptr ? agentDisplayFloorId(*route) : floorId;
        auto& floorCells = index.cellsByFloor[floorId];
        const auto cellKey = spatialKey(spatialCellFor(position.value, cellSize));
        floorCells[cellKey].push_back(entity);
        index.displayCellsByFloor[displayFloorId][cellKey].push_back(entity);
    }
    return index;
}

std::vector<engine::Entity> nearbyAgentsFromCells(
    engine::WorldQuery& query,
    const std::unordered_map<std::string, std::unordered_map<long long, std::vector<engine::Entity>>>& cellsByFloor,
    double cellSize,
    const Point2D& point,
    const std::string& floorId,
    double radius) {
    std::vector<engine::Entity> candidates;
    const auto floorIt = cellsByFloor.find(floorId);
    if (floorIt == cellsByFloor.end()) {
        return candidates;
    }

    const auto center = spatialCellFor(point, cellSize);
    const auto range = std::max(1, static_cast<int>(std::ceil(radius / cellSize)));
    for (int dy = -range; dy <= range; ++dy) {
        for (int dx = -range; dx <= range; ++dx) {
            const auto it = floorIt->second.find(spatialKey({.x = center.x + dx, .y = center.y + dy}));
            if (it == floorIt->second.end()) {
                continue;
            }
            for (const auto entity : it->second) {
                const auto& otherPosition = query.get<Position>(entity);
                if (distanceBetween(point, otherPosition.value) <= radius) {
                    candidates.push_back(entity);
                }
            }
        }
    }
    return candidates;
}

std::vector<engine::Entity> nearbyAgents(
    engine::WorldQuery& query,
    const AgentSpatialIndex& index,
    const Point2D& point,
    const std::string& floorId,
    double radius) {
    return nearbyAgentsFromCells(query, index.cellsByFloor, index.cellSize, point, floorId, radius);
}

std::vector<engine::Entity> nearbyAgents(
    engine::WorldQuery& query,
    const AgentSpatialIndex& index,
    const Point2D& point,
    double radius) {
    return nearbyAgents(query, index, point, std::string{}, radius);
}

std::vector<engine::Entity> nearbyDisplayAgents(
    engine::WorldQuery& query,
    const AgentSpatialIndex& index,
    const Point2D& point,
    const std::string& floorId,
    double radius) {
    return nearbyAgentsFromCells(query, index.displayCellsByFloor, index.cellSize, point, floorId, radius);
}

Point2D deterministicFallbackDirection(engine::Entity entity) {
    const auto seed = static_cast<double>((entity.index % 17U) + 1U);
    return normalizedOr({.x = std::cos(seed * 1.37), .y = std::sin(seed * 1.37)}, {.x = 1.0, .y = 0.0});
}

int deterministicPairSide(engine::Entity first, engine::Entity second) {
    const auto minIndex = std::min(first.index, second.index);
    const auto maxIndex = std::max(first.index, second.index);
    return ((minIndex + maxIndex) % 2U) == 0U ? -1 : 1;
}

Point2D forwardPreservingAgentAvoidanceVelocity(
    engine::WorldQuery& query,
    engine::Entity entity,
    const std::vector<engine::Entity>& candidates,
    const Point2D& desiredVelocity,
    double deltaSeconds,
    double& speedScale) {
    const auto& position = query.get<Position>(entity);
    const auto& agent = query.get<Agent>(entity);
    const auto& route = query.get<EvacuationRoute>(entity);
    auto& avoidance = query.get<AvoidanceState>(entity);
    const auto desiredSpeed = lengthOf(desiredVelocity);
    if (desiredSpeed <= 1e-9) {
        speedScale = 1.0;
        return {};
    }

    if (avoidance.sideLockSeconds > 0.0) {
        avoidance.sideLockSeconds = std::max(0.0, avoidance.sideLockSeconds - std::max(0.0, deltaSeconds));
        if (avoidance.sideLockSeconds <= 0.0) {
            avoidance.preferredSide = 0;
        }
    }

    const auto forward = desiredVelocity * (1.0 / desiredSpeed);
    const auto lateral = perpendicularLeft(forward);
    Point2D lateralCorrection{};
    speedScale = 1.0;
    bool lockedSideThisFrame = false;

    for (const auto other : candidates) {
        if (other == entity) {
            continue;
        }
        const auto& otherStatus = query.get<EvacuationStatus>(other);
        if (otherStatus.evacuated) {
            continue;
        }
        const auto& otherPosition = query.get<Position>(other);
        const auto& otherAgent = query.get<Agent>(other);
        const auto& otherRoute = query.get<EvacuationRoute>(other);
        if (!agentCollisionScopesOverlap(otherRoute, route)) {
            continue;
        }
        const auto offsetToOther = otherPosition.value - position.value;
        const auto distance = lengthOf(offsetToOther);
        const auto desiredDistance = static_cast<double>(agent.radius + otherAgent.radius) + kPersonalSpaceBuffer;
        const auto forwardDistance = dot(offsetToOther, forward);
        const auto lateralDistance = dot(offsetToOther, lateral);

        bool headOn = false;
        if (route.nextWaypointIndex < route.waypoints.size() && distance <= kHeadOnLookAheadDistance) {
            if (otherRoute.nextWaypointIndex < otherRoute.waypoints.size()) {
                const auto otherTarget = routeWaypointTarget(otherRoute, otherPosition.value);
                const auto otherTargetDistance = distanceBetween(otherPosition.value, otherTarget);
                if (otherTargetDistance > kArrivalEpsilon) {
                    const auto otherForward = (otherTarget - otherPosition.value) * (1.0 / otherTargetDistance);
                    const auto otherForwardDistance = dot(position.value - otherPosition.value, otherForward);
                    headOn = dot(forward, otherForward) <= kHeadOnDirectionDotThreshold
                        && forwardDistance > 0.0
                        && otherForwardDistance > 0.0;
                }
            }
        }

        if (distance >= desiredDistance && !headOn) {
            continue;
        }

        const auto pressure = headOn
            ? std::clamp((kHeadOnLookAheadDistance - distance) / kHeadOnLookAheadDistance, 0.15, 1.0)
            : (desiredDistance - distance) / desiredDistance;

        if (forwardDistance > -desiredDistance && forwardDistance < kHeadOnLookAheadDistance) {
            speedScale = std::min(speedScale, std::max(0.2, 1.0 - (pressure * kAvoidanceSlowdownStrength)));
        }

        double side = 0.0;
        if (avoidance.preferredSide != 0 && avoidance.sideLockSeconds > 0.0) {
            side = static_cast<double>(avoidance.preferredSide);
        } else if (headOn) {
            side = static_cast<double>(deterministicPairSide(entity, other));
        } else if (std::fabs(lateralDistance) > 1e-6) {
            side = lateralDistance < 0.0 ? 1.0 : -1.0;
        } else {
            side = entity.index < other.index ? -1.0 : 1.0;
        }

        if (headOn) {
            avoidance.preferredSide = side < 0.0 ? -1 : 1;
            avoidance.sideLockSeconds = kAvoidanceSideLockSeconds;
            lockedSideThisFrame = true;
            const bool shouldYield = entity.index > other.index;
            speedScale = std::min(
                speedScale,
                shouldYield
                    ? std::max(0.18, 1.0 - (pressure * 1.05))
                    : std::max(0.55, 1.0 - (pressure * 0.35)));
            const auto sideStepMultiplier = shouldYield ? 1.35 : 0.85;
            lateralCorrection = lateralCorrection + (lateral * (
                side * pressure * sideStepMultiplier * kAvoidanceLateralStrength * desiredSpeed));
        } else {
            lateralCorrection = lateralCorrection + (lateral * (
                side * pressure * kAvoidanceLateralStrength * desiredSpeed));
        }
    }

    if (!lockedSideThisFrame && avoidance.sideLockSeconds <= 0.0) {
        avoidance.preferredSide = 0;
    }

    return clampedToLength(lateralCorrection, desiredSpeed * 0.45);
}

Point2D barrierSeparationVelocity(
    const std::vector<const Barrier2D*>& barriers,
    const Position& position,
    const Agent& agent,
    double referenceSpeed) {
    Point2D correction{};
    const auto keepoutDistance = static_cast<double>(agent.radius) + kBarrierAvoidanceBuffer;
    const auto separationSpeed = std::max(0.0, referenceSpeed);

    for (const auto* barrier : barriers) {
        if (barrier == nullptr || !barrier->blocksMovement || barrier->geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier->geometry.vertices;
        for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
            const auto closest = closestPointOnSegment(position.value, vertices[index], vertices[index + 1]);
            const auto delta = position.value - closest;
            const auto distance = lengthOf(delta);
            if (distance < keepoutDistance) {
                const auto direction = normalizedOr(delta, deterministicFallbackDirection({static_cast<engine::EntityIndex>(index + 1), 0}));
                const auto pressure = (keepoutDistance - distance) / keepoutDistance;
                correction = correction + (direction * (pressure * kBarrierAvoidanceStrength * separationSpeed));
            }
        }

        if (barrier->geometry.closed) {
            const auto closest = closestPointOnSegment(position.value, vertices.back(), vertices.front());
            const auto delta = position.value - closest;
            const auto distance = lengthOf(delta);
            if (distance < keepoutDistance || pointInRing(vertices, position.value)) {
                const auto direction = normalizedOr(delta, {.x = 0.0, .y = 1.0});
                const auto pressure = distance < keepoutDistance ? (keepoutDistance - distance) / keepoutDistance : 1.0;
                correction = correction + (direction * (pressure * kBarrierAvoidanceStrength * separationSpeed));
            }
        }
    }

    return correction;
}

Point2D barrierSeparationVelocity(
    const FacilityLayout2D& layout,
    const Position& position,
    const Agent& agent,
    double referenceSpeed) {
    std::vector<const Barrier2D*> barriers;
    barriers.reserve(layout.barriers.size());
    for (const auto& barrier : layout.barriers) {
        barriers.push_back(&barrier);
    }
    return barrierSeparationVelocity(barriers, position, agent, referenceSpeed);
}

bool movementCrossesBarrier(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to) {
    if (distanceBetween(from, to) <= kGeometryEpsilon) {
        return false;
    }

    for (const auto& barrier : layout.barriers) {
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
            if (!segmentBoundsOverlap(from, to, vertices[index], vertices[index + 1])) {
                continue;
            }
            if (segmentsIntersect(from, to, vertices[index], vertices[index + 1])) {
                return true;
            }
        }
        if (barrier.geometry.closed
            && segmentBoundsOverlap(from, to, vertices.back(), vertices.front())
            && segmentsIntersect(from, to, vertices.back(), vertices.front())) {
            return true;
        }
        if (barrier.geometry.closed && pointInRing(vertices, to)) {
            return true;
        }
    }

    return false;
}

bool pointInsideClosedBarrier(const FacilityLayout2D& layout, const Point2D& point) {
    for (const auto& barrier : layout.barriers) {
        if (!barrier.blocksMovement || !barrier.geometry.closed || barrier.geometry.vertices.size() < 3) {
            continue;
        }
        if (pointInRing(barrier.geometry.vertices, point)) {
            return true;
        }
    }
    return false;
}

bool pointHasBarrierClearance(const FacilityLayout2D& layout, const Point2D& point, double clearance) {
    if (pointInsideClosedBarrier(layout, point)) {
        return false;
    }

    for (const auto& barrier : layout.barriers) {
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
            if (!pointWithinSegmentBounds(point, vertices[index], vertices[index + 1], clearance)) {
                continue;
            }
            if (distancePointToSegment(point, vertices[index], vertices[index + 1]) < clearance) {
                return false;
            }
        }
        if (barrier.geometry.closed
            && pointWithinSegmentBounds(point, vertices.back(), vertices.front(), clearance)
            && distancePointToSegment(point, vertices.back(), vertices.front()) < clearance) {
            return false;
        }
    }
    return true;
}

bool pointHasClearance(const FacilityLayout2D& layout, const Point2D& point, double clearance) {
    return (layout.zones.empty() || pointInAnyZone(layout, point))
        && pointHasBarrierClearance(layout, point, clearance);
}

bool lineOfSightClear(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to, double clearance) {
    if (movementCrossesBarrier(layout, from, to)) {
        return false;
    }

    for (const auto& barrier : layout.barriers) {
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
            if (!segmentBoundsOverlap(from, to, vertices[index], vertices[index + 1], clearance)) {
                continue;
            }
            if (segmentDistance(from, to, vertices[index], vertices[index + 1]) < clearance) {
                return false;
            }
        }
        if (barrier.geometry.closed
            && segmentBoundsOverlap(from, to, vertices.back(), vertices.front(), clearance)
            && segmentDistance(from, to, vertices.back(), vertices.front()) < clearance) {
            return false;
        }
    }
    return true;
}

std::vector<Point2D> clearanceCandidatesForEndpoint(
    const FacilityLayout2D& layout,
    const Point2D& endpoint,
    double clearance) {
    static constexpr Point2D directions[] = {
        {.x = 1.0, .y = 0.0},
        {.x = -1.0, .y = 0.0},
        {.x = 0.0, .y = 1.0},
        {.x = 0.0, .y = -1.0},
        {.x = 0.70710678118, .y = 0.70710678118},
        {.x = 0.70710678118, .y = -0.70710678118},
        {.x = -0.70710678118, .y = 0.70710678118},
        {.x = -0.70710678118, .y = -0.70710678118},
    };

    std::vector<Point2D> candidates;
    for (const auto& direction : directions) {
        const auto candidate = endpoint + (direction * (clearance + kPathClearance));
        if (pointHasClearance(layout, candidate, clearance)
            && !movementCrossesBarrier(layout, endpoint, candidate)) {
            candidates.push_back(candidate);
        }
    }
    return candidates;
}

bool nearlySamePoint(const Point2D& lhs, const Point2D& rhs) {
    return distanceBetween(lhs, rhs) <= 1e-6;
}

std::vector<Point2D> buildVisibilityPath(
    const FacilityLayout2D& layout,
    const Point2D& start,
    const Point2D& goal,
    double clearance) {
    std::vector<VisibilityNode> nodes;
    nodes.push_back({.point = start});
    nodes.push_back({.point = goal});

    std::size_t candidateCapacity = nodes.size();
    for (const auto& barrier : layout.barriers) {
        candidateCapacity += barrier.blocksMovement ? barrier.geometry.vertices.size() * 8 : 0;
    }
    nodes.reserve(candidateCapacity);

    auto addCandidate = [&](const Point2D& candidate) {
        if (std::any_of(nodes.begin(), nodes.end(), [&](const auto& node) {
                return nearlySamePoint(node.point, candidate);
            })) {
            return;
        }
        nodes.push_back({.point = candidate});
    };

    for (const auto& barrier : layout.barriers) {
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }
        for (const auto& vertex : barrier.geometry.vertices) {
            for (const auto& candidate : clearanceCandidatesForEndpoint(layout, vertex, clearance)) {
                addCandidate(candidate);
            }
        }
    }

    const auto nodeCount = nodes.size();
    std::vector<std::vector<std::pair<std::size_t, double>>> graph(nodeCount);
    for (std::size_t i = 0; i < nodeCount; ++i) {
        for (std::size_t j = i + 1; j < nodeCount; ++j) {
            if (!lineOfSightClear(layout, nodes[i].point, nodes[j].point, clearance)) {
                continue;
            }
            const auto cost = distanceBetween(nodes[i].point, nodes[j].point);
            graph[i].push_back({j, cost});
            graph[j].push_back({i, cost});
        }
    }

    std::vector<double> cost(nodeCount, std::numeric_limits<double>::infinity());
    std::vector<std::optional<std::size_t>> previous(nodeCount);
    std::priority_queue<PathQueueNode, std::vector<PathQueueNode>, std::greater<>> queue;
    cost[0] = 0.0;
    queue.push({.index = 0, .priority = distanceBetween(start, goal)});

    while (!queue.empty()) {
        const auto current = queue.top();
        queue.pop();
        if (current.index == 1) {
            break;
        }
        if (current.priority > cost[current.index] + distanceBetween(nodes[current.index].point, goal) + 1e-9) {
            continue;
        }
        for (const auto& [next, edgeCost] : graph[current.index]) {
            const auto nextCost = cost[current.index] + edgeCost;
            if (nextCost >= cost[next]) {
                continue;
            }
            cost[next] = nextCost;
            previous[next] = current.index;
            queue.push({.index = next, .priority = nextCost + distanceBetween(nodes[next].point, goal)});
        }
    }

    if (!previous[1].has_value()) {
        return {goal};
    }

    std::vector<Point2D> reversed;
    for (std::optional<std::size_t> at = 1; at.has_value(); at = previous[*at]) {
        reversed.push_back(nodes[*at].point);
        if (*at == 0) {
            break;
        }
    }
    std::reverse(reversed.begin(), reversed.end());
    if (!reversed.empty() && nearlySamePoint(reversed.front(), start)) {
        reversed.erase(reversed.begin());
    }

    std::vector<Point2D> smoothed;
    Point2D anchor = start;
    for (std::size_t index = 0; index < reversed.size();) {
        std::size_t farthest = index;
        for (std::size_t candidate = reversed.size(); candidate > index; --candidate) {
            if (lineOfSightClear(layout, anchor, reversed[candidate - 1], clearance)) {
                farthest = candidate - 1;
                break;
            }
        }
        smoothed.push_back(reversed[farthest]);
        anchor = reversed[farthest];
        index = farthest + 1;
    }
    return smoothed.empty() ? std::vector<Point2D>{goal} : smoothed;
}

std::optional<std::vector<Point2D>> buildGridPath(
    const FacilityLayout2D& layout,
    const Point2D& start,
    const Point2D& goal,
    double clearance) {
    constexpr double kGridResolution = 0.5;
    const auto bounds = boundsOf(layout);
    if (!bounds.valid()) {
        return std::nullopt;
    }

    const auto minX = bounds.minX - kGridResolution;
    const auto minY = bounds.minY - kGridResolution;
    const auto width = static_cast<int>(std::ceil((bounds.maxX - bounds.minX + (2.0 * kGridResolution)) / kGridResolution)) + 1;
    const auto height = static_cast<int>(std::ceil((bounds.maxY - bounds.minY + (2.0 * kGridResolution)) / kGridResolution)) + 1;
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }

    auto toIndex = [width](int x, int y) {
        return static_cast<std::size_t>(y * width + x);
    };
    auto toPoint = [&](int x, int y) {
        return Point2D{
            .x = minX + (static_cast<double>(x) * kGridResolution),
            .y = minY + (static_cast<double>(y) * kGridResolution),
        };
    };

    const auto cellCount = static_cast<std::size_t>(width * height);
    std::vector<bool> walkable(cellCount, false);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            walkable[toIndex(x, y)] = pointHasClearance(layout, toPoint(x, y), clearance);
        }
    }

    auto nearestVisibleCell = [&](const Point2D& point) -> std::optional<std::size_t> {
        std::optional<std::size_t> best;
        double bestDistance = std::numeric_limits<double>::infinity();
        const auto centerX = std::clamp(static_cast<int>(std::llround((point.x - minX) / kGridResolution)), 0, width - 1);
        const auto centerY = std::clamp(static_cast<int>(std::llround((point.y - minY) / kGridResolution)), 0, height - 1);
        const auto maxRing = std::max({centerX, width - 1 - centerX, centerY, height - 1 - centerY});

        auto considerCell = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= width || y >= height) {
                return;
            }
            const auto index = toIndex(x, y);
            if (!walkable[index]) {
                return;
            }
            const auto candidate = toPoint(x, y);
            const auto distance = distanceBetween(point, candidate);
            if (distance >= bestDistance || !lineOfSightClear(layout, point, candidate, clearance)) {
                return;
            }
            best = index;
            bestDistance = distance;
        };

        for (int ring = 0; ring <= maxRing; ++ring) {
            if (ring == 0) {
                considerCell(centerX, centerY);
            } else {
                for (int x = centerX - ring; x <= centerX + ring; ++x) {
                    considerCell(x, centerY - ring);
                    considerCell(x, centerY + ring);
                }
                for (int y = centerY - ring + 1; y <= centerY + ring - 1; ++y) {
                    considerCell(centerX - ring, y);
                    considerCell(centerX + ring, y);
                }
            }

            if (best.has_value() && static_cast<double>(std::max(0, ring - 1)) * kGridResolution > bestDistance) {
                break;
            }
        }
        return best;
    };

    const auto startIndex = nearestVisibleCell(start);
    const auto goalIndex = nearestVisibleCell(goal);
    if (!startIndex.has_value() || !goalIndex.has_value()) {
        return std::nullopt;
    }

    std::vector<double> cost(cellCount, std::numeric_limits<double>::infinity());
    std::vector<std::optional<std::size_t>> previous(cellCount);
    std::priority_queue<PathQueueNode, std::vector<PathQueueNode>, std::greater<>> queue;
    cost[*startIndex] = 0.0;

    auto pointForIndex = [&](std::size_t index) {
        const auto x = static_cast<int>(index % static_cast<std::size_t>(width));
        const auto y = static_cast<int>(index / static_cast<std::size_t>(width));
        return toPoint(x, y);
    };

    queue.push({.index = *startIndex, .priority = distanceBetween(pointForIndex(*startIndex), goal)});
    static constexpr int kNeighborOffsets[8][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
    };

    while (!queue.empty()) {
        const auto current = queue.top();
        queue.pop();
        if (current.index == *goalIndex) {
            break;
        }
        const auto currentPoint = pointForIndex(current.index);
        const auto cx = static_cast<int>(current.index % static_cast<std::size_t>(width));
        const auto cy = static_cast<int>(current.index / static_cast<std::size_t>(width));
        for (const auto& offset : kNeighborOffsets) {
            const auto nx = cx + offset[0];
            const auto ny = cy + offset[1];
            if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                continue;
            }
            const auto next = toIndex(nx, ny);
            if (!walkable[next]) {
                continue;
            }
            const auto nextPoint = toPoint(nx, ny);
            if (!lineOfSightClear(layout, currentPoint, nextPoint, clearance)) {
                continue;
            }
            const auto nextCost = cost[current.index] + distanceBetween(currentPoint, nextPoint);
            if (nextCost >= cost[next]) {
                continue;
            }
            cost[next] = nextCost;
            previous[next] = current.index;
            queue.push({.index = next, .priority = nextCost + distanceBetween(nextPoint, goal)});
        }
    }

    if (!previous[*goalIndex].has_value() && *startIndex != *goalIndex) {
        return std::nullopt;
    }

    std::vector<Point2D> reversed;
    for (std::optional<std::size_t> at = *goalIndex; at.has_value(); at = previous[*at]) {
        reversed.push_back(pointForIndex(*at));
        if (*at == *startIndex) {
            break;
        }
    }
    std::reverse(reversed.begin(), reversed.end());

    std::vector<Point2D> raw;
    for (std::size_t index = 1; index < reversed.size(); ++index) {
        raw.push_back(reversed[index]);
    }
    raw.push_back(goal);

    std::vector<Point2D> smoothed;
    Point2D anchor = start;
    for (std::size_t index = 0; index < raw.size();) {
        std::size_t farthest = index;
        for (std::size_t candidate = raw.size(); candidate > index; --candidate) {
            if (lineOfSightClear(layout, anchor, raw[candidate - 1], clearance)) {
                farthest = candidate - 1;
                break;
            }
        }
        smoothed.push_back(raw[farthest]);
        anchor = raw[farthest];
        index = farthest + 1;
    }
    return smoothed;
}

std::vector<Point2D> buildPath(const FacilityLayout2D& layout, const Point2D& start, const Point2D& goal, double clearance) {
    if (lineOfSightClear(layout, start, goal, clearance)) {
        return {goal};
    }

    auto path = buildVisibilityPath(layout, start, goal, clearance);
    if (path.size() == 1) {
        if (auto gridPath = buildGridPath(layout, start, goal, clearance); gridPath.has_value() && !gridPath->empty()) {
            return *gridPath;
        }
    }
    return path;
}

Point2D constrainedMove(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to, double clearance) {
    const auto effectiveClearance = std::max(0.0, clearance);
    auto validDestination = [&](const Point2D& point) {
        if (effectiveClearance > 0.0) {
            return pointHasClearance(layout, point, effectiveClearance);
        }
        return (layout.zones.empty() || pointInAnyZone(layout, point)) && !pointInsideClosedBarrier(layout, point);
    };
    auto validMove = [&](const Point2D& candidate) {
        return validDestination(candidate) && !movementCrossesBarrier(layout, from, candidate);
    };

    if (validMove(to)) {
        return to;
    }

    const Point2D xOnly{.x = to.x, .y = from.y};
    if (validMove(xOnly)) {
        return xOnly;
    }

    const Point2D yOnly{.x = from.x, .y = to.y};
    if (validMove(yOnly)) {
        return yOnly;
    }

    Point2D best = from;
    double low = 0.0;
    double high = 1.0;
    for (int i = 0; i < 8; ++i) {
        const auto t = (low + high) * 0.5;
        const auto candidate = from + ((to - from) * t);
        if (validMove(candidate)) {
            low = t;
            best = candidate;
        } else {
            high = t;
        }
    }
    return best;
}

Point2D constrainedMoveWithBarrierClearance(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to, double clearance) {
    const auto effectiveClearance = std::max(0.0, clearance);
    auto validMove = [&](const Point2D& candidate) {
        return pointHasBarrierClearance(layout, candidate, effectiveClearance)
            && !movementCrossesBarrier(layout, from, candidate);
    };

    if (validMove(to)) {
        return to;
    }

    const Point2D xOnly{.x = to.x, .y = from.y};
    if (validMove(xOnly)) {
        return xOnly;
    }

    const Point2D yOnly{.x = from.x, .y = to.y};
    if (validMove(yOnly)) {
        return yOnly;
    }

    Point2D best = from;
    double low = 0.0;
    double high = 1.0;
    for (int i = 0; i < 8; ++i) {
        const auto t = (low + high) * 0.5;
        const auto candidate = from + ((to - from) * t);
        if (validMove(candidate)) {
            low = t;
            best = candidate;
        } else {
            high = t;
        }
    }
    return best;
}

}  // namespace safecrowd::domain::simulation_internal
