#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <utility>

namespace safecrowd::domain::simulation_internal {


long long spatialKey(const SpatialCell& cell) {
    return (static_cast<long long>(cell.x) << 32)
        ^ static_cast<unsigned int>(cell.y);
}

SpatialCell spatialCellFor(const Point2D& point, double cellSize) {
    return {
        .x = static_cast<int>(std::floor(point.x / cellSize)),
        .y = static_cast<int>(std::floor(point.y / cellSize)),
    };
}

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

Point2D closestPointOnSegment(const Point2D& point, const Point2D& start, const Point2D& end) {
    const auto segment = end - start;
    const auto lengthSquared = (segment.x * segment.x) + (segment.y * segment.y);
    if (lengthSquared <= 1e-9) {
        return start;
    }
    const auto t = std::clamp(
        (((point.x - start.x) * segment.x) + ((point.y - start.y) * segment.y)) / lengthSquared,
        0.0,
        1.0);
    return start + (segment * t);
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

double distancePointToSegment(const Point2D& point, const Point2D& start, const Point2D& end) {
    return distanceBetween(point, closestPointOnSegment(point, start, end));
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

Point2D polygonCenter(const Polygon2D& polygon) {
    if (polygon.outline.empty()) {
        return {};
    }

    Point2D center{};
    for (const auto& point : polygon.outline) {
        center.x += point.x;
        center.y += point.y;
    }
    const auto count = static_cast<double>(polygon.outline.size());
    center.x /= count;
    center.y /= count;
    return center;
}

bool pointInRing(const std::vector<Point2D>& ring, const Point2D& point) {
    if (ring.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const auto& a = ring[i];
        const auto& b = ring[j];
        const auto intersects = ((a.y > point.y) != (b.y > point.y))
            && (point.x < ((b.x - a.x) * (point.y - a.y) / ((b.y - a.y) == 0.0 ? 1e-9 : (b.y - a.y)) + a.x));
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
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

bool routePassageCrossed(
    const FacilityLayout2D& layout,
    const EvacuationRoute& route,
    const Point2D& position,
    double agentRadius) {
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

    const auto normal = normalizedOr(polygonCenter(toZone->area) - polygonCenter(fromZone->area), {});
    if (lengthOf(normal) <= 1e-9) {
        return false;
    }

    return dot(position - midpoint(passage), normal) > -std::max(kPortalCrossingEpsilon, agentRadius * 0.35);
}

const Connection2D* findConnectionBetween(const FacilityLayout2D& layout, const std::string& from, const std::string& to) {
    const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        if (connection.directionality == TravelDirection::Closed) {
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

double speedOf(const Point2D& velocity) {
    const auto speed = std::hypot(velocity.x, velocity.y);
    return speed > 0.0 ? speed : kDefaultAgentSpeed;
}

std::vector<engine::Entity> simulationEntities(engine::WorldQuery& query) {
    return query.view<Position, Agent, Velocity, EvacuationRoute, EvacuationStatus>();
}

AgentSpatialIndex buildAgentSpatialIndex(
    engine::WorldQuery& query,
    const std::vector<engine::Entity>& entities,
    double cellSize) {
    AgentSpatialIndex index;
    index.cellSize = cellSize;
    index.cells.reserve(entities.size() * 2);

    for (const auto entity : entities) {
        const auto& status = query.get<EvacuationStatus>(entity);
        if (status.evacuated) {
            continue;
        }
        const auto& position = query.get<Position>(entity);
        index.cells[spatialKey(spatialCellFor(position.value, cellSize))].push_back(entity);
    }
    return index;
}

std::vector<engine::Entity> nearbyAgents(
    engine::WorldQuery& query,
    const AgentSpatialIndex& index,
    const Point2D& point,
    double radius) {
    std::vector<engine::Entity> candidates;
    const auto center = spatialCellFor(point, index.cellSize);
    const auto range = std::max(1, static_cast<int>(std::ceil(radius / index.cellSize)));
    for (int dy = -range; dy <= range; ++dy) {
        for (int dx = -range; dx <= range; ++dx) {
            const auto it = index.cells.find(spatialKey({.x = center.x + dx, .y = center.y + dy}));
            if (it == index.cells.end()) {
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

Point2D deterministicFallbackDirection(engine::Entity entity) {
    const auto seed = static_cast<double>((entity.index % 17U) + 1U);
    return normalizedOr({.x = std::cos(seed * 1.37), .y = std::sin(seed * 1.37)}, {.x = 1.0, .y = 0.0});
}

Point2D forwardPreservingAgentAvoidanceVelocity(
    engine::WorldQuery& query,
    engine::Entity entity,
    const std::vector<engine::Entity>& candidates,
    const Point2D& desiredVelocity,
    double& speedScale) {
    const auto& position = query.get<Position>(entity);
    const auto& agent = query.get<Agent>(entity);
    const auto desiredSpeed = lengthOf(desiredVelocity);
    if (desiredSpeed <= 1e-9) {
        speedScale = 1.0;
        return {};
    }

    const auto forward = desiredVelocity * (1.0 / desiredSpeed);
    const auto lateral = perpendicularLeft(forward);
    Point2D lateralCorrection{};
    speedScale = 1.0;

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
        const auto offsetToOther = otherPosition.value - position.value;
        const auto distance = lengthOf(offsetToOther);
        const auto desiredDistance = static_cast<double>(agent.radius + otherAgent.radius) + kPersonalSpaceBuffer;
        if (distance >= desiredDistance) {
            continue;
        }

        const auto forwardDistance = dot(offsetToOther, forward);
        const auto lateralDistance = dot(offsetToOther, lateral);
        const auto pressure = (desiredDistance - distance) / desiredDistance;

        if (forwardDistance > -desiredDistance && forwardDistance < desiredDistance * 1.6) {
            speedScale = std::min(speedScale, std::max(0.2, 1.0 - (pressure * kAvoidanceSlowdownStrength)));
        }

        double side = 0.0;
        if (std::fabs(lateralDistance) > 1e-6) {
            side = lateralDistance < 0.0 ? 1.0 : -1.0;
        } else {
            side = entity.index < other.index ? -1.0 : 1.0;
        }
        lateralCorrection = lateralCorrection + (lateral * (side * pressure * kAvoidanceLateralStrength * static_cast<double>(agent.maxSpeed)));
    }

    return clampedToLength(lateralCorrection, static_cast<double>(agent.maxSpeed) * 0.45);
}

Point2D barrierSeparationVelocity(const FacilityLayout2D& layout, const Position& position, const Agent& agent) {
    Point2D correction{};
    const auto keepoutDistance = static_cast<double>(agent.radius) + kBarrierAvoidanceBuffer;

    for (const auto& barrier : layout.barriers) {
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
            const auto closest = closestPointOnSegment(position.value, vertices[index], vertices[index + 1]);
            const auto delta = position.value - closest;
            const auto distance = lengthOf(delta);
            if (distance < keepoutDistance) {
                const auto direction = normalizedOr(delta, deterministicFallbackDirection({static_cast<engine::EntityIndex>(index + 1), 0}));
                const auto pressure = (keepoutDistance - distance) / keepoutDistance;
                correction = correction + (direction * (pressure * kBarrierAvoidanceStrength * static_cast<double>(agent.maxSpeed)));
            }
        }

        if (barrier.geometry.closed) {
            const auto closest = closestPointOnSegment(position.value, vertices.back(), vertices.front());
            const auto delta = position.value - closest;
            const auto distance = lengthOf(delta);
            if (distance < keepoutDistance || pointInRing(vertices, position.value)) {
                const auto direction = normalizedOr(delta, {.x = 0.0, .y = 1.0});
                const auto pressure = distance < keepoutDistance ? (keepoutDistance - distance) / keepoutDistance : 1.0;
                correction = correction + (direction * (pressure * kBarrierAvoidanceStrength * static_cast<double>(agent.maxSpeed)));
            }
        }
    }

    return correction;
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
            if (segmentsIntersect(from, to, vertices[index], vertices[index + 1])) {
                return true;
            }
        }
        if (barrier.geometry.closed && segmentsIntersect(from, to, vertices.back(), vertices.front())) {
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

bool pointHasClearance(const FacilityLayout2D& layout, const Point2D& point, double clearance) {
    if (!pointInAnyZone(layout, point) || pointInsideClosedBarrier(layout, point)) {
        return false;
    }

    for (const auto& barrier : layout.barriers) {
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
            if (distancePointToSegment(point, vertices[index], vertices[index + 1]) < clearance) {
                return false;
            }
        }
        if (barrier.geometry.closed && distancePointToSegment(point, vertices.back(), vertices.front()) < clearance) {
            return false;
        }
    }
    return true;
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
            if (segmentDistance(from, to, vertices[index], vertices[index + 1]) < clearance) {
                return false;
            }
        }
        if (barrier.geometry.closed && segmentDistance(from, to, vertices.back(), vertices.front()) < clearance) {
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

std::vector<Point2D> buildVisibilityPath(const FacilityLayout2D& layout, const Point2D& start, const Point2D& goal, double clearance) {
    if (lineOfSightClear(layout, start, goal, clearance)) {
        return {goal};
    }

    std::vector<VisibilityNode> nodes;
    nodes.push_back({.point = start});
    nodes.push_back({.point = goal});

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
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto index = toIndex(x, y);
                if (!walkable[index]) {
                    continue;
                }
                const auto candidate = toPoint(x, y);
                const auto distance = distanceBetween(point, candidate);
                if (distance >= bestDistance || !lineOfSightClear(layout, point, candidate, clearance)) {
                    continue;
                }
                best = index;
                bestDistance = distance;
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
    auto path = buildVisibilityPath(layout, start, goal, clearance);
    if (path.size() == 1 && !lineOfSightClear(layout, start, goal, clearance)) {
        if (auto gridPath = buildGridPath(layout, start, goal, clearance); gridPath.has_value() && !gridPath->empty()) {
            path = *gridPath;
        }
    }
    return path;
}

Point2D constrainedMove(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to) {
    if (!movementCrossesBarrier(layout, from, to)) {
        return to;
    }

    const Point2D xOnly{.x = to.x, .y = from.y};
    if (!movementCrossesBarrier(layout, from, xOnly)) {
        return xOnly;
    }

    const Point2D yOnly{.x = from.x, .y = to.y};
    if (!movementCrossesBarrier(layout, from, yOnly)) {
        return yOnly;
    }

    Point2D best = from;
    double low = 0.0;
    double high = 1.0;
    for (int i = 0; i < 8; ++i) {
        const auto t = (low + high) * 0.5;
        const auto candidate = from + ((to - from) * t);
        if (movementCrossesBarrier(layout, from, candidate)) {
            high = t;
        } else {
            low = t;
            best = candidate;
        }
    }
    return best;
}

}  // namespace safecrowd::domain::simulation_internal
