#include "domain/ScenarioSimulationRunner.h"

#include <algorithm>
#include <deque>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace safecrowd::domain {
namespace {

constexpr double kDefaultTimeLimitSeconds = 60.0;
constexpr double kDefaultAgentRadius = 0.25;
constexpr double kDefaultAgentSpeed = 1.5;
constexpr double kArrivalEpsilon = 0.05;
constexpr double kPersonalSpaceBuffer = 0.08;
constexpr double kAvoidanceLateralStrength = 0.65;
constexpr double kAvoidanceSlowdownStrength = 0.7;
constexpr double kBarrierAvoidanceBuffer = 0.18;
constexpr double kBarrierAvoidanceStrength = 1.1;
constexpr int kOverlapRelaxationIterations = 4;
constexpr double kGeometryEpsilon = 1e-9;
constexpr double kPathClearance = 0.08;
constexpr double kCandidateClearance = kDefaultAgentRadius + kBarrierAvoidanceBuffer;
constexpr double kWaypointCrossingEpsilon = 0.08;
constexpr double kWaypointProgressEpsilon = 0.02;
constexpr double kWaypointStallSeconds = 0.75;

struct Bounds {
    double minX{0.0};
    double minY{0.0};
    double maxX{0.0};
    double maxY{0.0};
};

struct MovementPlan {
    engine::Entity entity{};
    Point2D velocity{};
};

struct LayoutBounds {
    double minX{std::numeric_limits<double>::max()};
    double minY{std::numeric_limits<double>::max()};
    double maxX{std::numeric_limits<double>::lowest()};
    double maxY{std::numeric_limits<double>::lowest()};

    bool valid() const noexcept {
        return minX <= maxX && minY <= maxY;
    }
};

struct VisibilityNode {
    Point2D point{};
};

struct PathQueueNode {
    std::size_t index{0};
    double priority{0.0};

    bool operator>(const PathQueueNode& other) const noexcept {
        return priority > other.priority;
    }
};

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

std::vector<engine::Entity> simulationEntities(engine::EcsCore& core) {
    std::vector<engine::Entity> entities;
    core.entityRegistry().eachAlive([&](engine::Entity entity, const engine::Signature&) {
        if (core.hasComponent<Position>(entity)
            && core.hasComponent<Agent>(entity)
            && core.hasComponent<Velocity>(entity)
            && core.hasComponent<EvacuationRoute>(entity)
            && core.hasComponent<EvacuationStatus>(entity)) {
            entities.push_back(entity);
        }
    });
    return entities;
}

Point2D deterministicFallbackDirection(engine::Entity entity) {
    const auto seed = static_cast<double>((entity.index % 17U) + 1U);
    return normalizedOr({.x = std::cos(seed * 1.37), .y = std::sin(seed * 1.37)}, {.x = 1.0, .y = 0.0});
}

Point2D forwardPreservingAgentAvoidanceVelocity(
    engine::EcsCore& core,
    engine::Entity entity,
    const std::vector<engine::Entity>& entities,
    const Point2D& desiredVelocity,
    double& speedScale) {
    const auto& position = core.getComponent<Position>(entity);
    const auto& agent = core.getComponent<Agent>(entity);
    const auto desiredSpeed = lengthOf(desiredVelocity);
    if (desiredSpeed <= 1e-9) {
        speedScale = 1.0;
        return {};
    }

    const auto forward = desiredVelocity * (1.0 / desiredSpeed);
    const auto lateral = perpendicularLeft(forward);
    Point2D lateralCorrection{};
    speedScale = 1.0;

    for (const auto other : entities) {
        if (other == entity) {
            continue;
        }
        const auto& otherStatus = core.getComponent<EvacuationStatus>(other);
        if (otherStatus.evacuated) {
            continue;
        }
        const auto& otherPosition = core.getComponent<Position>(other);
        const auto& otherAgent = core.getComponent<Agent>(other);
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

}  // namespace

ScenarioSimulationRunner::ScenarioSimulationRunner(FacilityLayout2D layout, ScenarioDraft scenario) {
    reset(std::move(layout), std::move(scenario));
}

void ScenarioSimulationRunner::reset(FacilityLayout2D layout, ScenarioDraft scenario) {
    layout_ = std::move(layout);
    scenario_ = std::move(scenario);
    core_ = engine::EcsCore{4096};
    frame_ = {};
    timeLimitSeconds_ = scenario_.execution.timeLimitSeconds > 0.0
        ? scenario_.execution.timeLimitSeconds
        : kDefaultTimeLimitSeconds;
    initializeAgents();
}

void ScenarioSimulationRunner::step(double deltaSeconds) {
    if (frame_.complete || deltaSeconds <= 0.0) {
        return;
    }

    const auto remaining = std::max(0.0, timeLimitSeconds_ - frame_.elapsedSeconds);
    const auto clampedDelta = std::min(deltaSeconds, remaining);

    const auto entities = simulationEntities(core_);
    std::vector<MovementPlan> plans;
    plans.reserve(entities.size());
    advanceRoutesForCurrentZones();
    advanceRoutesForWaypointProgress(0.0);
    replanBlockedRouteSegments();

    for (const auto entity : entities) {
        auto& position = core_.getComponent<Position>(entity);
        const auto& agent = core_.getComponent<Agent>(entity);
        auto& velocity = core_.getComponent<Velocity>(entity);
        auto& route = core_.getComponent<EvacuationRoute>(entity);
        auto& status = core_.getComponent<EvacuationStatus>(entity);
        if (status.evacuated) {
            continue;
        }

        const auto* destinationZone = findZone(layout_, route.destinationZoneId);
        if (destinationZone != nullptr && pointInRing(destinationZone->area.outline, position.value)) {
            status.evacuated = true;
            status.completionTimeSeconds = frame_.elapsedSeconds;
            velocity.value = {};
            continue;
        }

        if (route.nextWaypointIndex >= route.waypoints.size()) {
            velocity.value = {};
            continue;
        }

        const auto target = routeWaypointTarget(route, position.value);
        const auto distance = distanceBetween(position.value, target);
        if (distance <= kArrivalEpsilon) {
            position.value = target;
            advanceRouteWaypoint(route, target);
            velocity.value = {};
            continue;
        }

        const auto routeDirection = (target - position.value) * (1.0 / distance);
        const auto desiredVelocity = routeDirection * static_cast<double>(agent.maxSpeed);
        double speedScale = 1.0;
        const auto avoidanceVelocity = forwardPreservingAgentAvoidanceVelocity(core_, entity, entities, desiredVelocity, speedScale);
        const auto barrierVelocity = barrierSeparationVelocity(layout_, position, agent);
        auto finalVelocity = (desiredVelocity * speedScale) + avoidanceVelocity + barrierVelocity;
        if (dot(finalVelocity, routeDirection) < 0.0) {
            const auto lateral = perpendicularLeft(routeDirection);
            finalVelocity = (routeDirection * (static_cast<double>(agent.maxSpeed) * 0.15))
                + (lateral * dot(finalVelocity, lateral));
        }
        plans.push_back({
            .entity = entity,
            .velocity = clampedToLength(finalVelocity, static_cast<double>(agent.maxSpeed)),
        });
    }

    for (const auto& plan : plans) {
        auto& position = core_.getComponent<Position>(plan.entity);
        auto& velocity = core_.getComponent<Velocity>(plan.entity);
        auto& route = core_.getComponent<EvacuationRoute>(plan.entity);
        const auto& agent = core_.getComponent<Agent>(plan.entity);
        if (route.nextWaypointIndex >= route.waypoints.size()) {
            continue;
        }

        const auto target = routeWaypointTarget(route, position.value);
        const auto remainingDistance = distanceBetween(position.value, target);
        const auto stepVelocity = clampedToLength(plan.velocity, std::min(static_cast<double>(agent.maxSpeed), remainingDistance / clampedDelta));
        const auto previousPosition = position.value;
        const auto nextPosition = constrainedMove(layout_, previousPosition, previousPosition + (stepVelocity * clampedDelta));
        position.value = nextPosition;
        velocity.value = (nextPosition - previousPosition) * (1.0 / clampedDelta);
    }

    resolveAgentOverlaps();
    advanceRoutesForCurrentZones();
    advanceRoutesForWaypointProgress(clampedDelta);
    frame_.elapsedSeconds += clampedDelta;
    rebuildFrame();
    frame_.complete = frame_.elapsedSeconds >= timeLimitSeconds_
        || (frame_.totalAgentCount > 0 && frame_.evacuatedAgentCount >= frame_.totalAgentCount);
}

const SimulationFrame& ScenarioSimulationRunner::frame() const noexcept {
    return frame_;
}

double ScenarioSimulationRunner::timeLimitSeconds() const noexcept {
    return timeLimitSeconds_;
}

bool ScenarioSimulationRunner::complete() const noexcept {
    return frame_.complete;
}

void ScenarioSimulationRunner::initializeAgents() {
    for (const auto& placement : scenario_.population.initialPlacements) {
        const auto count = placement.targetAgentCount;
        for (std::size_t index = 0; index < count; ++index) {
            const auto position = placementPoint(placement, index);
            const auto startZoneId = !placement.zoneId.empty() ? placement.zoneId : zoneAt(position);
            const auto route = routePlan(position, startZoneId);
            const auto speed = speedOf(placement.initialVelocity);
            const auto entity = core_.createEntity();
            core_.addComponent(entity, Position{.value = position});
            core_.addComponent(entity, Agent{.radius = static_cast<float>(kDefaultAgentRadius), .maxSpeed = static_cast<float>(speed)});
            core_.addComponent(entity, Velocity{.value = {}});
            auto evacuationRoute = EvacuationRoute{
                .waypoints = route.waypoints,
                .waypointPassages = route.waypointPassages,
                .waypointZoneIds = route.waypointZoneIds,
                .nextWaypointIndex = 0,
                .currentSegmentStart = position,
                .previousDistanceToWaypoint = 0.0,
                .stalledSeconds = 0.0,
                .destinationZoneId = route.destinationZoneId,
            };
            evacuationRoute.previousDistanceToWaypoint = route.waypoints.empty()
                ? 0.0
                : distanceToRouteWaypoint(evacuationRoute, position);
            core_.addComponent(entity, std::move(evacuationRoute));
            core_.addComponent(entity, EvacuationStatus{});
        }
    }
    rebuildFrame();
}

void ScenarioSimulationRunner::advanceRouteWaypoint(EvacuationRoute& route, const Point2D& reachedPoint) const {
    if (route.nextWaypointIndex >= route.waypoints.size()) {
        return;
    }

    route.currentSegmentStart = reachedPoint;
    ++route.nextWaypointIndex;
    if (route.nextWaypointIndex < route.waypoints.size()) {
        route.previousDistanceToWaypoint =
            distanceToRouteWaypoint(route, route.currentSegmentStart);
    } else {
        route.previousDistanceToWaypoint = 0.0;
    }
    route.stalledSeconds = 0.0;
}

void ScenarioSimulationRunner::advanceRoutesForWaypointProgress(double deltaSeconds) {
    const auto entities = simulationEntities(core_);
    for (const auto entity : entities) {
        const auto& status = core_.getComponent<EvacuationStatus>(entity);
        if (status.evacuated) {
            continue;
        }

        const auto& position = core_.getComponent<Position>(entity);
        auto& route = core_.getComponent<EvacuationRoute>(entity);
        while (route.nextWaypointIndex < route.waypoints.size()) {
            const auto target = routeWaypointTarget(route, position.value);
            const auto segment = target - route.currentSegmentStart;
            const auto segmentLengthSquared = dot(segment, segment);
            const auto distance = distanceToRouteWaypoint(route, position.value);

            if (distance <= kArrivalEpsilon) {
                advanceRouteWaypoint(route, target);
                continue;
            }

            if (segmentLengthSquared > 1e-9) {
                const auto projection = dot(position.value - route.currentSegmentStart, segment);
                if (projection >= segmentLengthSquared - kWaypointCrossingEpsilon) {
                    advanceRouteWaypoint(route, target);
                    continue;
                }
            }

            if (route.previousDistanceToWaypoint <= 0.0 || distance < route.previousDistanceToWaypoint - kWaypointProgressEpsilon) {
                route.previousDistanceToWaypoint = distance;
                route.stalledSeconds = 0.0;
                break;
            }

            if (deltaSeconds > 0.0) {
                route.stalledSeconds += deltaSeconds;
            }

            if (route.stalledSeconds >= kWaypointStallSeconds
                && route.nextWaypointIndex + 1 < route.waypoints.size()
                && segmentLengthSquared > 1e-9) {
                const auto projection = dot(position.value - route.currentSegmentStart, segment);
                if (projection > segmentLengthSquared * 0.45) {
                    advanceRouteWaypoint(route, target);
                    continue;
                }
            }

            route.previousDistanceToWaypoint = std::min(route.previousDistanceToWaypoint, distance);
            break;
        }
    }
}

void ScenarioSimulationRunner::advanceRoutesForCurrentZones() {
    const auto entities = simulationEntities(core_);
    for (const auto entity : entities) {
        const auto& status = core_.getComponent<EvacuationStatus>(entity);
        if (status.evacuated) {
            continue;
        }

        const auto& position = core_.getComponent<Position>(entity);
        auto& route = core_.getComponent<EvacuationRoute>(entity);
        const auto currentZoneId = zoneAt(position.value);
        while (!currentZoneId.empty()
               && route.nextWaypointIndex < route.waypointZoneIds.size()
               && route.waypointZoneIds[route.nextWaypointIndex] == currentZoneId) {
            advanceRouteWaypoint(route, position.value);
        }
    }
}

void ScenarioSimulationRunner::rebuildFrame() {
    frame_.agents.clear();
    frame_.totalAgentCount = 0;
    frame_.evacuatedAgentCount = 0;

    const auto entities = simulationEntities(core_);
    for (const auto entity : entities) {
        ++frame_.totalAgentCount;
        const auto& status = core_.getComponent<EvacuationStatus>(entity);
        if (status.evacuated) {
            ++frame_.evacuatedAgentCount;
            continue;
        }
        const auto& position = core_.getComponent<Position>(entity);
        const auto& velocity = core_.getComponent<Velocity>(entity);
        const auto& agent = core_.getComponent<Agent>(entity);
        frame_.agents.push_back({
            .id = entity.index,
            .position = position.value,
            .velocity = velocity.value,
            .radius = agent.radius,
        });
    }
}

void ScenarioSimulationRunner::replanBlockedRouteSegments() {
    const auto entities = simulationEntities(core_);
    for (const auto entity : entities) {
        const auto& status = core_.getComponent<EvacuationStatus>(entity);
        if (status.evacuated) {
            continue;
        }

        const auto& position = core_.getComponent<Position>(entity);
        const auto& agent = core_.getComponent<Agent>(entity);
        auto& route = core_.getComponent<EvacuationRoute>(entity);
        if (route.nextWaypointIndex >= route.waypoints.size()) {
            continue;
        }

        const auto target = routeWaypointTarget(route, position.value);
        const auto clearance = static_cast<double>(agent.radius) + kPathClearance;
        if (lineOfSightClear(layout_, position.value, target, clearance)) {
            continue;
        }

        const auto replacement = buildPath(layout_, position.value, target, clearance);
        if (replacement.size() <= 1) {
            continue;
        }

        const auto originalTargetZoneId = route.nextWaypointIndex < route.waypointZoneIds.size()
            ? route.waypointZoneIds[route.nextWaypointIndex]
            : std::string{};
        const auto originalTargetPassage = route.nextWaypointIndex < route.waypointPassages.size()
            ? route.waypointPassages[route.nextWaypointIndex]
            : pointPassage(target);
        route.waypoints.erase(route.waypoints.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));
        route.waypointPassages.erase(route.waypointPassages.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));
        route.waypointZoneIds.erase(route.waypointZoneIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));

        std::vector<LineSegment2D> replacementPassages;
        replacementPassages.reserve(replacement.size());
        for (const auto& waypoint : replacement) {
            replacementPassages.push_back(pointPassage(waypoint));
        }
        replacementPassages.back() = originalTargetPassage;

        std::vector<std::string> replacementZoneIds(replacement.size(), std::string{});
        replacementZoneIds.back() = originalTargetZoneId;
        route.waypoints.insert(
            route.waypoints.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
            replacement.begin(),
            replacement.end());
        route.waypointPassages.insert(
            route.waypointPassages.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
            replacementPassages.begin(),
            replacementPassages.end());
        route.waypointZoneIds.insert(
            route.waypointZoneIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
            replacementZoneIds.begin(),
            replacementZoneIds.end());
        route.currentSegmentStart = position.value;
        route.previousDistanceToWaypoint = distanceToRouteWaypoint(route, position.value);
        route.stalledSeconds = 0.0;
    }
}

void ScenarioSimulationRunner::resolveAgentOverlaps() {
    const auto entities = simulationEntities(core_);
    for (int iteration = 0; iteration < kOverlapRelaxationIterations; ++iteration) {
        for (std::size_t firstIndex = 0; firstIndex < entities.size(); ++firstIndex) {
            for (std::size_t secondIndex = firstIndex + 1; secondIndex < entities.size(); ++secondIndex) {
                const auto first = entities[firstIndex];
                const auto second = entities[secondIndex];
                auto& firstStatus = core_.getComponent<EvacuationStatus>(first);
                auto& secondStatus = core_.getComponent<EvacuationStatus>(second);
                if (firstStatus.evacuated || secondStatus.evacuated) {
                    continue;
                }

                auto& firstPosition = core_.getComponent<Position>(first);
                auto& secondPosition = core_.getComponent<Position>(second);
                const auto& firstAgent = core_.getComponent<Agent>(first);
                const auto& secondAgent = core_.getComponent<Agent>(second);
                const auto delta = firstPosition.value - secondPosition.value;
                const auto distance = lengthOf(delta);
                const auto minimumDistance = static_cast<double>(firstAgent.radius + secondAgent.radius);
                if (distance >= minimumDistance) {
                    continue;
                }

                const auto direction = normalizedOr(delta, deterministicFallbackDirection(first));
                const auto push = std::min(0.08, (minimumDistance - distance) * 0.35);
                firstPosition.value = constrainedMove(layout_, firstPosition.value, firstPosition.value + (direction * push));
                secondPosition.value = constrainedMove(layout_, secondPosition.value, secondPosition.value - (direction * push));
            }
        }
    }
}

ScenarioSimulationRunner::RoutePlan ScenarioSimulationRunner::routePlan(const Point2D& start, const std::string& startZoneId) const {
    RoutePlan plan;
    auto zoneRoute = zoneRouteToExit(startZoneId);
    if (!zoneRoute.has_value() || zoneRoute->empty()) {
        return plan;
    }

    plan.destinationZoneId = zoneRoute->back();

    Point2D segmentStart = start;
    auto appendSegment = [&](const std::vector<Point2D>& segment,
                             const LineSegment2D& finalPassage,
                             const std::string& finalZoneId) {
        for (std::size_t waypointIndex = 0; waypointIndex < segment.size(); ++waypointIndex) {
            plan.waypoints.push_back(segment[waypointIndex]);
            plan.waypointPassages.push_back(
                waypointIndex + 1 == segment.size() ? finalPassage : pointPassage(segment[waypointIndex]));
            plan.waypointZoneIds.push_back(waypointIndex + 1 == segment.size() ? finalZoneId : std::string{});
        }
    };

    for (std::size_t index = 1; index < zoneRoute->size(); ++index) {
        if (const auto* connection = findConnectionBetween(layout_, (*zoneRoute)[index - 1], (*zoneRoute)[index])) {
            const auto passage = passageWithClearance(*connection, kCandidateClearance);
            const auto target = closestPointOnSegment(segmentStart, passage.start, passage.end);
            const auto segment = buildPath(layout_, segmentStart, target, kCandidateClearance);
            appendSegment(segment, passage, (*zoneRoute)[index]);
            segmentStart = target;
        }
    }
    if (const auto* exitZone = findZone(layout_, zoneRoute->back())) {
        const auto exitCenter = polygonCenter(exitZone->area);
        if (distanceBetween(segmentStart, exitCenter) > kArrivalEpsilon) {
            const auto segment = buildPath(layout_, segmentStart, exitCenter, kCandidateClearance);
            appendSegment(segment, pointPassage(exitCenter), exitZone->id);
        }
    }

    if (!plan.waypoints.empty() && distanceBetween(start, plan.waypoints.front()) <= kArrivalEpsilon) {
        plan.waypoints.erase(plan.waypoints.begin());
        plan.waypointPassages.erase(plan.waypointPassages.begin());
        plan.waypointZoneIds.erase(plan.waypointZoneIds.begin());
    }
    return plan;
}

std::optional<std::vector<std::string>> ScenarioSimulationRunner::zoneRouteToExit(const std::string& startZoneId) const {
    if (startZoneId.empty()) {
        return std::nullopt;
    }
    if (const auto* startZone = findZone(layout_, startZoneId); startZone != nullptr && startZone->kind == ZoneKind::Exit) {
        return std::vector<std::string>{startZoneId};
    }

    std::unordered_map<std::string, std::string> previous;
    std::unordered_set<std::string> visited;
    std::deque<std::string> queue;
    visited.insert(startZoneId);
    queue.push_back(startZoneId);

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop_front();
        if (const auto* zone = findZone(layout_, current); zone != nullptr && zone->kind == ZoneKind::Exit) {
            std::vector<std::string> route;
            for (auto zoneId = current; !zoneId.empty();) {
                route.push_back(zoneId);
                const auto prev = previous.find(zoneId);
                zoneId = prev == previous.end() ? std::string{} : prev->second;
            }
            std::reverse(route.begin(), route.end());
            return route;
        }

        for (const auto& connection : layout_.connections) {
            if (connection.directionality == TravelDirection::Closed) {
                continue;
            }
            std::string next;
            if (connection.fromZoneId == current && connection.directionality != TravelDirection::ReverseOnly) {
                next = connection.toZoneId;
            } else if (connection.toZoneId == current && connection.directionality != TravelDirection::ForwardOnly) {
                next = connection.fromZoneId;
            }
            if (!next.empty() && !visited.contains(next)) {
                visited.insert(next);
                previous[next] = current;
                queue.push_back(next);
            }
        }
    }

    return std::nullopt;
}

std::string ScenarioSimulationRunner::zoneAt(const Point2D& point) const {
    for (const auto& zone : layout_.zones) {
        if (pointInRing(zone.area.outline, point)) {
            return zone.id;
        }
    }
    return {};
}

Point2D ScenarioSimulationRunner::placementPoint(const InitialPlacement2D& placement, std::size_t index) const {
    if (placement.area.outline.empty()) {
        return {};
    }
    if (placement.targetAgentCount <= 1 || placement.area.outline.size() == 1) {
        return placement.area.outline.front();
    }

    const auto bounds = boundsOf(placement.area);
    const auto columns = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(std::sqrt(placement.targetAgentCount))));
    const auto rows = std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(static_cast<double>(placement.targetAgentCount) / columns)));
    const auto row = index / columns;
    const auto column = index % columns;
    const auto xStep = (bounds.maxX - bounds.minX) / static_cast<double>(columns);
    const auto yStep = (bounds.maxY - bounds.minY) / static_cast<double>(rows);
    return {
        .x = bounds.minX + ((static_cast<double>(column) + 0.5) * xStep),
        .y = bounds.minY + ((static_cast<double>(row) + 0.5) * yStep),
    };
}

}  // namespace safecrowd::domain
