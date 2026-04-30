#include "domain/ScenarioSimulationSystems.h"

#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>

namespace safecrowd::domain {
namespace {

using namespace simulation_internal;

class ScenarioSimulationMotionSystem final : public engine::EngineSystem {
public:
    ScenarioSimulationMotionSystem() = default;

    explicit ScenarioSimulationMotionSystem(FacilityLayout2D layout)
        : layoutCache_(buildScenarioLayoutCache(std::move(layout))) {
    }

    void configure(engine::EngineWorld& world) override {
        if (layoutCache_.has_value() && !world.resources().contains<ScenarioLayoutCacheResource>()) {
            world.resources().set(*layoutCache_);
        }
    }

    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override {
        (void)step;

        auto& resources = world.resources();
        if (!resources.contains<ScenarioSimulationStepResource>()) {
            return;
        }
        if (!resources.contains<ScenarioLayoutCacheResource>()) {
            return;
        }
        const auto& layoutCache = resources.get<ScenarioLayoutCacheResource>();

        auto& clock = resources.get<ScenarioSimulationClockResource>();
        if (clock.complete) {
            return;
        }

        const auto clampedDelta = std::max(0.0, resources.get<ScenarioSimulationStepResource>().deltaSeconds);
        if (clampedDelta <= 0.0) {
            return;
        }

        auto& query = world.query();
        const auto entities = simulationEntities(query);
        const auto localNeighborIndex = resources.contains<ScenarioAgentSpatialIndexResource>()
            ? AgentSpatialIndex{}
            : buildAgentSpatialIndex(query, entities, 1.0);
        std::vector<MovementPlan> plans;
        plans.reserve(entities.size());

        advanceRoutesForCurrentZones(query, entities, layoutCache);
        advanceRoutesForWaypointProgress(query, 0.0, entities, layoutCache);
        replanBlockedRouteSegments(query, entities, layoutCache, clampedDelta);

        for (const auto entity : entities) {
            auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
            auto& velocity = query.get<Velocity>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            const auto& floorLayout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
            const auto* destinationZone = findZone(floorLayout, route.destinationZoneId);
            if (destinationZone != nullptr && pointInRing(destinationZone->area.outline, position.value)) {
                status.evacuated = true;
                status.completionTimeSeconds = clock.elapsedSeconds;
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
            const auto maxSpeed = effectiveMaxSpeed(layoutCache, agent, route, position.value);
            const auto desiredVelocity = routeDirection * maxSpeed;
            double speedScale = 1.0;
            const auto neighborRadius = static_cast<double>(agent.radius) + kDefaultAgentRadius + kPersonalSpaceBuffer;
            const auto neighborCandidates = resources.contains<ScenarioAgentSpatialIndexResource>()
                ? scenarioNearbyAgents(query, resources.get<ScenarioAgentSpatialIndexResource>(), position.value, neighborRadius)
                : nearbyAgents(query, localNeighborIndex, position.value, neighborRadius);
            const auto avoidanceVelocity =
                forwardPreservingAgentAvoidanceVelocity(query, entity, neighborCandidates, desiredVelocity, speedScale);
            const auto barrierVelocity = barrierSeparationVelocity(floorLayout, position, agent);
            auto finalVelocity = (desiredVelocity * speedScale) + avoidanceVelocity + barrierVelocity;
            if (dot(finalVelocity, routeDirection) < 0.0) {
                const auto lateral = perpendicularLeft(routeDirection);
                finalVelocity = (routeDirection * (static_cast<double>(agent.maxSpeed) * 0.15))
                    + (lateral * dot(finalVelocity, lateral));
            }
            plans.push_back({
                .entity = entity,
                .velocity = clampedToLength(finalVelocity, maxSpeed),
            });
        }

        for (const auto& plan : plans) {
            auto& position = query.get<Position>(plan.entity);
            auto& velocity = query.get<Velocity>(plan.entity);
            auto& route = query.get<EvacuationRoute>(plan.entity);
            const auto& agent = query.get<Agent>(plan.entity);
            if (route.nextWaypointIndex >= route.waypoints.size()) {
                continue;
            }

            const auto target = routeWaypointTarget(route, position.value);
            const auto remainingDistance = distanceBetween(position.value, target);
            const auto maxSpeed = effectiveMaxSpeed(layoutCache, agent, route, position.value);
            const auto stepVelocity =
                clampedToLength(plan.velocity, std::min(maxSpeed, remainingDistance / clampedDelta));
            const auto previousPosition = position.value;
            const auto nextPosition = constrainedMove(
                cachedLayoutForFloor(layoutCache, route.currentFloorId),
                previousPosition,
                previousPosition + (stepVelocity * clampedDelta));
            position.value = nextPosition;
            velocity.value = (nextPosition - previousPosition) * (1.0 / clampedDelta);
            updateDisplayFloor(route, nextPosition);
        }

        resolveAgentOverlaps(query, entities, layoutCache);
        advanceRoutesForCurrentZones(query, entities, layoutCache);
        advanceRoutesForWaypointProgress(query, clampedDelta, entities, layoutCache);
        advanceClock(query, clock, entities, clampedDelta);
        resources.set(ScenarioSimulationStepResource{});
    }

private:
    void advanceRouteWaypoint(EvacuationRoute& route, const Point2D& reachedPoint) const {
        if (route.nextWaypointIndex >= route.waypoints.size()) {
            return;
        }

        const auto reachedIndex = route.nextWaypointIndex;
        if (reachedIndex < route.waypointFloorIds.size() && !route.waypointFloorIds[reachedIndex].empty()) {
            route.currentFloorId = route.waypointFloorIds[reachedIndex];
        }
        route.displayFloorId = route.currentFloorId;
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

    void advanceRoutesForWaypointProgress(
        engine::WorldQuery& query,
        double deltaSeconds,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache) const {
        for (const auto entity : entities) {
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            const auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            while (route.nextWaypointIndex < route.waypoints.size()) {
                const auto& floorLayout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
                if (routePassageCrossed(floorLayout, route, position.value, agent.radius)) {
                    advanceRouteWaypoint(route, position.value);
                    continue;
                }

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

                if (route.previousDistanceToWaypoint <= 0.0
                    || distance < route.previousDistanceToWaypoint - kWaypointProgressEpsilon) {
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

    void advanceRoutesForCurrentZones(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache) const {
        for (const auto entity : entities) {
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            const auto& position = query.get<Position>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            const auto currentZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
            while (!currentZoneId.empty() && route.nextWaypointIndex < route.waypointZoneIds.size()) {
                auto matchedIndex = route.waypointZoneIds.size();
                for (auto index = route.nextWaypointIndex; index < route.waypointZoneIds.size(); ++index) {
                    if (route.waypointZoneIds[index] == currentZoneId) {
                        matchedIndex = index;
                        break;
                    }
                }
                if (matchedIndex == route.waypointZoneIds.size()) {
                    break;
                }

                while (route.nextWaypointIndex <= matchedIndex && route.nextWaypointIndex < route.waypoints.size()) {
                    advanceRouteWaypoint(route, position.value);
                }
            }
        }
    }

    void replanBlockedRouteSegments(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache,
        double deltaSeconds) const {
        for (const auto entity : entities) {
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            const auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            if (route.nextWaypointIndex >= route.waypoints.size()) {
                continue;
            }
            if (route.replanCooldownSeconds > 0.0) {
                route.replanCooldownSeconds = std::max(0.0, route.replanCooldownSeconds - deltaSeconds);
                continue;
            }

            const auto target = routeWaypointTarget(route, position.value);
            const auto clearance = static_cast<double>(agent.radius) + kPathClearance;
            const auto& floorLayout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
            if (lineOfSightClear(floorLayout, position.value, target, clearance)) {
                continue;
            }

            const auto replacement = buildPath(floorLayout, position.value, target, clearance);
            route.replanCooldownSeconds = kRouteReplanCooldownSeconds;
            if (replacement.size() <= 1) {
                continue;
            }

            const auto originalTargetZoneId = route.nextWaypointIndex < route.waypointZoneIds.size()
                ? route.waypointZoneIds[route.nextWaypointIndex]
                : std::string{};
            const auto originalTargetPassage = route.nextWaypointIndex < route.waypointPassages.size()
                ? route.waypointPassages[route.nextWaypointIndex]
                : pointPassage(target);
            const auto originalFromZoneId = route.nextWaypointIndex < route.waypointFromZoneIds.size()
                ? route.waypointFromZoneIds[route.nextWaypointIndex]
                : std::string{};
            const auto originalFloorId = route.nextWaypointIndex < route.waypointFloorIds.size()
                ? route.waypointFloorIds[route.nextWaypointIndex]
                : std::string{};
            const auto originalConnectionId = route.nextWaypointIndex < route.waypointConnectionIds.size()
                ? route.waypointConnectionIds[route.nextWaypointIndex]
                : std::string{};
            const auto originalVerticalTransition = route.nextWaypointIndex < route.waypointVerticalTransitions.size()
                ? route.waypointVerticalTransitions[route.nextWaypointIndex]
                : false;
            route.waypoints.erase(route.waypoints.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));
            route.waypointPassages.erase(route.waypointPassages.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));
            route.waypointFromZoneIds.erase(route.waypointFromZoneIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));
            route.waypointZoneIds.erase(route.waypointZoneIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));
            if (route.nextWaypointIndex < route.waypointConnectionIds.size()) {
                route.waypointConnectionIds.erase(
                    route.waypointConnectionIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));
            }
            if (route.nextWaypointIndex < route.waypointVerticalTransitions.size()) {
                route.waypointVerticalTransitions.erase(
                    route.waypointVerticalTransitions.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));
            }
            if (route.nextWaypointIndex < route.waypointFloorIds.size()) {
                route.waypointFloorIds.erase(route.waypointFloorIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex));
            }

            std::vector<LineSegment2D> replacementPassages;
            replacementPassages.reserve(replacement.size());
            for (const auto& waypoint : replacement) {
                replacementPassages.push_back(pointPassage(waypoint));
            }
            replacementPassages.back() = originalTargetPassage;

            std::vector<std::string> replacementZoneIds(replacement.size(), std::string{});
            replacementZoneIds.back() = originalTargetZoneId;
            std::vector<std::string> replacementFromZoneIds(replacement.size(), std::string{});
            replacementFromZoneIds.back() = originalFromZoneId;
            std::vector<std::string> replacementFloorIds(replacement.size(), std::string{});
            replacementFloorIds.back() = originalFloorId;
            std::vector<std::string> replacementConnectionIds(replacement.size(), std::string{});
            replacementConnectionIds.back() = originalConnectionId;
            std::vector<bool> replacementVerticalTransitions(replacement.size(), false);
            replacementVerticalTransitions.back() = originalVerticalTransition;
            route.waypoints.insert(
                route.waypoints.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
                replacement.begin(),
                replacement.end());
            route.waypointPassages.insert(
                route.waypointPassages.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
                replacementPassages.begin(),
                replacementPassages.end());
            route.waypointFromZoneIds.insert(
                route.waypointFromZoneIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
                replacementFromZoneIds.begin(),
                replacementFromZoneIds.end());
            route.waypointZoneIds.insert(
                route.waypointZoneIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
                replacementZoneIds.begin(),
                replacementZoneIds.end());
            if (route.waypointConnectionIds.size() < route.nextWaypointIndex) {
                route.waypointConnectionIds.resize(route.nextWaypointIndex);
            }
            route.waypointConnectionIds.insert(
                route.waypointConnectionIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
                replacementConnectionIds.begin(),
                replacementConnectionIds.end());
            if (route.waypointVerticalTransitions.size() < route.nextWaypointIndex) {
                route.waypointVerticalTransitions.resize(route.nextWaypointIndex);
            }
            route.waypointVerticalTransitions.insert(
                route.waypointVerticalTransitions.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
                replacementVerticalTransitions.begin(),
                replacementVerticalTransitions.end());
            if (route.waypointFloorIds.size() < route.nextWaypointIndex) {
                route.waypointFloorIds.resize(route.nextWaypointIndex);
            }
            route.waypointFloorIds.insert(
                route.waypointFloorIds.begin() + static_cast<std::ptrdiff_t>(route.nextWaypointIndex),
                replacementFloorIds.begin(),
                replacementFloorIds.end());
            route.currentSegmentStart = position.value;
            route.previousDistanceToWaypoint = distanceToRouteWaypoint(route, position.value);
            route.stalledSeconds = 0.0;
        }
    }

    void resolveAgentOverlaps(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache) const {
        for (int iteration = 0; iteration < kOverlapRelaxationIterations; ++iteration) {
            const auto spatialIndex = buildAgentSpatialIndex(query, entities, 1.0);
            std::unordered_set<unsigned long long> checkedPairs;
            checkedPairs.reserve(entities.size() * 4);
            for (const auto first : entities) {
                auto& firstStatus = query.get<EvacuationStatus>(first);
                if (firstStatus.evacuated) {
                    continue;
                }

                auto& firstPosition = query.get<Position>(first);
                const auto& firstAgent = query.get<Agent>(first);
                const auto candidates = nearbyAgents(
                    query,
                    spatialIndex,
                    firstPosition.value,
                    static_cast<double>(firstAgent.radius) + kDefaultAgentRadius);
                for (const auto second : candidates) {
                    if (first == second) {
                        continue;
                    }
                    const auto minIndex = std::min(first.index, second.index);
                    const auto maxIndex = std::max(first.index, second.index);
                    const auto pairKey = (static_cast<unsigned long long>(minIndex) << 32)
                        ^ static_cast<unsigned int>(maxIndex);
                    if (!checkedPairs.insert(pairKey).second) {
                        continue;
                    }

                    auto& secondStatus = query.get<EvacuationStatus>(second);
                    if (firstStatus.evacuated || secondStatus.evacuated) {
                        continue;
                    }

                    auto& secondPosition = query.get<Position>(second);
                    const auto& secondAgent = query.get<Agent>(second);
                    const auto delta = firstPosition.value - secondPosition.value;
                    const auto distance = lengthOf(delta);
                    const auto minimumDistance = static_cast<double>(firstAgent.radius + secondAgent.radius);
                    if (distance >= minimumDistance) {
                        continue;
                    }

                    const auto direction = normalizedOr(delta, deterministicFallbackDirection(first));
                    const auto push = std::min(0.08, (minimumDistance - distance) * 0.35);
                    const auto& firstRoute = query.get<EvacuationRoute>(first);
                    const auto& secondRoute = query.get<EvacuationRoute>(second);
                    firstPosition.value = constrainedMove(
                        cachedLayoutForFloor(layoutCache, firstRoute.currentFloorId),
                        firstPosition.value,
                        firstPosition.value + (direction * push));
                    secondPosition.value = constrainedMove(
                        cachedLayoutForFloor(layoutCache, secondRoute.currentFloorId),
                        secondPosition.value,
                        secondPosition.value - (direction * push));
                }
            }
        }
    }

    void advanceClock(
        engine::WorldQuery& query,
        ScenarioSimulationClockResource& clock,
        const std::vector<engine::Entity>& entities,
        double deltaSeconds) const {
        clock.elapsedSeconds += deltaSeconds;
        clock.complete = clock.elapsedSeconds >= clock.timeLimitSeconds;
        if (clock.complete) {
            return;
        }

        std::size_t totalAgentCount = 0;
        std::size_t evacuatedAgentCount = 0;
        for (const auto entity : entities) {
            ++totalAgentCount;
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                ++evacuatedAgentCount;
            }
        }
        clock.complete = totalAgentCount > 0 && evacuatedAgentCount >= totalAgentCount;
    }

    bool currentWaypointIsVertical(const EvacuationRoute& route) const {
        return route.nextWaypointIndex < route.waypointVerticalTransitions.size()
            && route.waypointVerticalTransitions[route.nextWaypointIndex];
    }

    double effectiveMaxSpeed(
        const ScenarioLayoutCacheResource& layoutCache,
        const Agent& agent,
        const EvacuationRoute& route,
        const Point2D& position) const {
        const auto currentZoneId = zoneAt(layoutCache, position, route.currentFloorId);
        const auto* zone = findCachedZone(layoutCache, currentZoneId);
        const bool inStairZone = zone != nullptr
            && (zone->kind == ZoneKind::Stair || zone->isStair || zone->isRamp);
        const bool onVerticalTransition = currentWaypointIsVertical(route);
        return static_cast<double>(agent.maxSpeed) * (inStairZone || onVerticalTransition ? kStairSpeedMultiplier : 1.0);
    }

    void updateDisplayFloor(EvacuationRoute& route, const Point2D& position) const {
        route.displayFloorId = route.currentFloorId;
        if (!currentWaypointIsVertical(route)
            || route.nextWaypointIndex >= route.waypoints.size()
            || route.nextWaypointIndex >= route.waypointFloorIds.size()
            || route.waypointFloorIds[route.nextWaypointIndex].empty()) {
            return;
        }

        const auto segment = route.waypoints[route.nextWaypointIndex] - route.currentSegmentStart;
        const auto segmentLengthSquared = dot(segment, segment);
        if (segmentLengthSquared <= 1e-9) {
            return;
        }

        const auto progress = std::clamp(
            dot(position - route.currentSegmentStart, segment) / segmentLengthSquared,
            0.0,
            1.0);
        if (progress >= 0.5) {
            route.displayFloorId = route.waypointFloorIds[route.nextWaypointIndex];
        }
    }

    std::optional<ScenarioLayoutCacheResource> layoutCache_{};
};



}  // namespace

std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem() {
    return std::make_unique<ScenarioSimulationMotionSystem>();
}

std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem(FacilityLayout2D layout) {
    return std::make_unique<ScenarioSimulationMotionSystem>(std::move(layout));
}

}  // namespace safecrowd::domain
