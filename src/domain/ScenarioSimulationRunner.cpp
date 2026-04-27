#include "domain/ScenarioSimulationRunner.h"

#include <algorithm>
#include <deque>
#include <cmath>
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
constexpr double kSeparationStrength = 1.35;
constexpr double kBarrierAvoidanceBuffer = 0.18;
constexpr double kBarrierAvoidanceStrength = 1.1;
constexpr int kOverlapRelaxationIterations = 3;

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

Point2D agentSeparationVelocity(
    engine::EcsCore& core,
    engine::Entity entity,
    const std::vector<engine::Entity>& entities) {
    const auto& position = core.getComponent<Position>(entity);
    const auto& agent = core.getComponent<Agent>(entity);
    Point2D correction{};

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
        const auto delta = position.value - otherPosition.value;
        const auto distance = lengthOf(delta);
        const auto desiredDistance = static_cast<double>(agent.radius + otherAgent.radius) + kPersonalSpaceBuffer;
        if (distance >= desiredDistance) {
            continue;
        }

        const auto direction = normalizedOr(delta, deterministicFallbackDirection(entity));
        const auto pressure = (desiredDistance - distance) / desiredDistance;
        correction = correction + (direction * (pressure * kSeparationStrength * static_cast<double>(agent.maxSpeed)));
    }

    return correction;
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

        const auto target = route.waypoints[route.nextWaypointIndex];
        const auto distance = distanceBetween(position.value, target);
        if (distance <= kArrivalEpsilon) {
            position.value = target;
            ++route.nextWaypointIndex;
            velocity.value = {};
            continue;
        }

        const auto routeDirection = (target - position.value) * (1.0 / distance);
        const auto desiredVelocity = routeDirection * static_cast<double>(agent.maxSpeed);
        const auto separationVelocity = agentSeparationVelocity(core_, entity, entities);
        const auto barrierVelocity = barrierSeparationVelocity(layout_, position, agent);
        plans.push_back({
            .entity = entity,
            .velocity = clampedToLength(desiredVelocity + separationVelocity + barrierVelocity, static_cast<double>(agent.maxSpeed)),
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

        const auto target = route.waypoints[route.nextWaypointIndex];
        const auto remainingDistance = distanceBetween(position.value, target);
        const auto stepVelocity = clampedToLength(plan.velocity, std::min(static_cast<double>(agent.maxSpeed), remainingDistance / clampedDelta));
        position.value = position.value + (stepVelocity * clampedDelta);
        velocity.value = stepVelocity;
    }

    resolveAgentOverlaps();
    advanceRoutesForCurrentZones();
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
            core_.addComponent(entity, EvacuationRoute{
                .waypoints = route.waypoints,
                .waypointZoneIds = route.waypointZoneIds,
                .nextWaypointIndex = 0,
                .destinationZoneId = route.destinationZoneId,
            });
            core_.addComponent(entity, EvacuationStatus{});
        }
    }
    rebuildFrame();
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
            ++route.nextWaypointIndex;
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
                const auto push = (minimumDistance - distance) * 0.5;
                firstPosition.value = firstPosition.value + (direction * push);
                secondPosition.value = secondPosition.value - (direction * push);
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

    for (std::size_t index = 1; index < zoneRoute->size(); ++index) {
        if (const auto* connection = findConnectionBetween(layout_, (*zoneRoute)[index - 1], (*zoneRoute)[index])) {
            plan.waypoints.push_back(midpoint(connection->centerSpan));
            plan.waypointZoneIds.push_back((*zoneRoute)[index]);
        }
    }
    if (const auto* exitZone = findZone(layout_, zoneRoute->back())) {
        const auto exitCenter = polygonCenter(exitZone->area);
        if (plan.waypoints.empty() || distanceBetween(plan.waypoints.back(), exitCenter) > kArrivalEpsilon) {
            plan.waypoints.push_back(exitCenter);
            plan.waypointZoneIds.push_back(exitZone->id);
        }
    }

    if (!plan.waypoints.empty() && distanceBetween(start, plan.waypoints.front()) <= kArrivalEpsilon) {
        plan.waypoints.erase(plan.waypoints.begin());
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
