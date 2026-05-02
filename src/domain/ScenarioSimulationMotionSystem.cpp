#include "domain/ScenarioSimulationSystems.h"

#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

        const std::uint64_t layoutRevision = resources.contains<ScenarioLayoutRevisionResource>()
            ? resources.get<ScenarioLayoutRevisionResource>().revision
            : 0U;
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
        replanBlockedExitRoutes(query, entities, layoutCache, clock.elapsedSeconds, layoutRevision);
        replanBlockedRouteSegments(query, entities, layoutCache, clock.elapsedSeconds, layoutRevision);

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
            const auto neighborRadius = std::max(
                static_cast<double>(agent.radius) + kDefaultAgentRadius + kPersonalSpaceBuffer,
                kHeadOnLookAheadDistance);
            const auto collisionFloorId = agentCollisionFloorId(route);
            const auto neighborCandidates = resources.contains<ScenarioAgentSpatialIndexResource>()
                ? scenarioNearbyAgents(
                    query,
                    resources.get<ScenarioAgentSpatialIndexResource>(),
                    position.value,
                    collisionFloorId,
                    neighborRadius)
                : nearbyAgents(query, localNeighborIndex, position.value, collisionFloorId, neighborRadius);
            const auto avoidanceVelocity =
                forwardPreservingAgentAvoidanceVelocity(
                    query,
                    entity,
                    neighborCandidates,
                    desiredVelocity,
                    clampedDelta,
                    speedScale);
            const auto barrierVelocity = barrierSeparationVelocity(floorLayout, position, agent);
            auto finalVelocity = (desiredVelocity * speedScale) + avoidanceVelocity + barrierVelocity;
            const auto lateral = perpendicularLeft(routeDirection);
            if (dot(finalVelocity, routeDirection) < 0.0) {
                finalVelocity = (routeDirection * (static_cast<double>(agent.maxSpeed) * 0.15))
                    + (lateral * dot(finalVelocity, lateral));
            }
            const auto forwardComponent = dot(finalVelocity, routeDirection);
            const auto lateralComponent = dot(finalVelocity, lateral);
            const auto maxLateralComponent = maxSpeed * 0.55;
            if (std::fabs(lateralComponent) > maxLateralComponent) {
                finalVelocity = (routeDirection * std::max(0.0, forwardComponent))
                    + (lateral * std::clamp(lateralComponent, -maxLateralComponent, maxLateralComponent));
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
    static constexpr double kExitReplanCooldownSeconds = 0.75;
    static constexpr double kNoExitReplanCooldownSeconds = 7.0;
    static constexpr double kSegmentReplanCooldownSeconds = 0.25;
    static constexpr double kFailedSegmentReplanCooldownSeconds = 1.25;

    struct RoutePlan {
        std::vector<Point2D> waypoints{};
        std::vector<LineSegment2D> waypointPassages{};
        std::vector<std::string> waypointFromZoneIds{};
        std::vector<std::string> waypointZoneIds{};
        std::vector<std::string> waypointFloorIds{};
        std::vector<std::string> waypointConnectionIds{};
        std::vector<bool> waypointVerticalTransitions{};
        std::string destinationZoneId{};
    };

    const Connection2D* findConnectionById(
        const ScenarioLayoutCacheResource& layoutCache,
        const std::string& connectionId) const {
        if (connectionId.empty()) {
            return nullptr;
        }
        const auto it = std::find_if(
            layoutCache.layout.connections.begin(),
            layoutCache.layout.connections.end(),
            [&](const auto& connection) {
                return connection.id == connectionId;
            });
        return it == layoutCache.layout.connections.end() ? nullptr : &(*it);
    }

    bool nextConnectionBlocked(const ScenarioLayoutCacheResource& layoutCache, const EvacuationRoute& route) const {
        if (route.nextWaypointIndex >= route.waypoints.size()
            || route.nextWaypointIndex >= route.waypointConnectionIds.size()) {
            return false;
        }
        for (std::size_t index = route.nextWaypointIndex; index < route.waypointConnectionIds.size(); ++index) {
            const auto& connectionId = route.waypointConnectionIds[index];
            if (connectionId.empty()) {
                continue;
            }
            const auto* connection = findConnectionById(layoutCache, connectionId);
            return connection != nullptr && connection->directionality == TravelDirection::Closed;
        }
        return false;
    }

    RoutePlan routePlanToNearestExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId) const {
        RoutePlan plan;
        auto zoneRoute = zoneRouteToNearestExit(layoutCache, startZoneId);
        if (!zoneRoute.has_value() || zoneRoute->empty()) {
            return plan;
        }

        plan.destinationZoneId = zoneRoute->back();

        Point2D segmentStart = start;
        auto appendSegment = [&](const std::vector<Point2D>& segment,
                                 const LineSegment2D& finalPassage,
                                 const std::string& finalFromZoneId,
                                 const std::string& finalZoneId,
                                 const std::string& finalFloorId,
                                 const std::string& finalConnectionId,
                                 bool finalVerticalTransition) {
            for (std::size_t waypointIndex = 0; waypointIndex < segment.size(); ++waypointIndex) {
                const bool isFinalWaypoint = waypointIndex + 1 == segment.size();
                plan.waypoints.push_back(segment[waypointIndex]);
                plan.waypointPassages.push_back(isFinalWaypoint ? finalPassage : pointPassage(segment[waypointIndex]));
                plan.waypointFromZoneIds.push_back(isFinalWaypoint ? finalFromZoneId : std::string{});
                plan.waypointZoneIds.push_back(isFinalWaypoint ? finalZoneId : std::string{});
                plan.waypointFloorIds.push_back(isFinalWaypoint ? finalFloorId : std::string{});
                plan.waypointConnectionIds.push_back(isFinalWaypoint ? finalConnectionId : std::string{});
                plan.waypointVerticalTransitions.push_back(isFinalWaypoint && finalVerticalTransition);
            }
        };

        for (std::size_t index = 1; index < zoneRoute->size(); ++index) {
            const auto& fromZoneId = (*zoneRoute)[index - 1];
            const auto& toZoneId = (*zoneRoute)[index];
            if (const auto* connection = findCachedConnectionBetween(layoutCache, fromZoneId, toZoneId)) {
                const auto passage = passageWithClearance(*connection, kCandidateClearance);
                const auto fromFloorId = cachedFloorIdForZone(layoutCache, fromZoneId);
                const auto toFloorId = cachedFloorIdForZone(layoutCache, toZoneId);
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, fromFloorId);
                const auto target = closestPointOnSegment(segmentStart, passage.start, passage.end);
                const auto segment = buildPath(segmentLayout, segmentStart, target, kCandidateClearance);
                appendSegment(
                    segment,
                    passage,
                    fromZoneId,
                    toZoneId,
                    toFloorId.empty() ? fromFloorId : toFloorId,
                    connection->id,
                    isVerticalConnection(*connection));
                segmentStart = target;
            }
        }

        if (const auto* exitZone = findCachedZone(layoutCache, zoneRoute->back())) {
            const auto exitCenter = polygonCenter(exitZone->area);
            if (distanceBetween(segmentStart, exitCenter) > kArrivalEpsilon) {
                const auto exitFloorId = exitZone->floorId;
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, exitFloorId);
                const auto segment = buildPath(segmentLayout, segmentStart, exitCenter, kCandidateClearance);
                appendSegment(segment, pointPassage(exitCenter), std::string{}, exitZone->id, exitFloorId, std::string{}, false);
            }
        }

        if (!plan.waypoints.empty() && distanceBetween(start, plan.waypoints.front()) <= kArrivalEpsilon) {
            plan.waypoints.erase(plan.waypoints.begin());
            plan.waypointPassages.erase(plan.waypointPassages.begin());
            plan.waypointFromZoneIds.erase(plan.waypointFromZoneIds.begin());
            plan.waypointZoneIds.erase(plan.waypointZoneIds.begin());
            plan.waypointFloorIds.erase(plan.waypointFloorIds.begin());
            plan.waypointConnectionIds.erase(plan.waypointConnectionIds.begin());
            plan.waypointVerticalTransitions.erase(plan.waypointVerticalTransitions.begin());
        }
        return plan;
    }

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
        route.nextSegmentReplanSeconds = 0.0;
    }

    bool waypointHasTransition(const EvacuationRoute& route) const {
        const auto index = route.nextWaypointIndex;
        return (index < route.waypointFromZoneIds.size() && !route.waypointFromZoneIds[index].empty())
            || (index < route.waypointZoneIds.size() && !route.waypointZoneIds[index].empty())
            || (index < route.waypointFloorIds.size() && !route.waypointFloorIds[index].empty())
            || (index < route.waypointConnectionIds.size() && !route.waypointConnectionIds[index].empty())
            || (index < route.waypointVerticalTransitions.size() && route.waypointVerticalTransitions[index]);
    }

    bool shouldAdvanceByBypassingWaypoint(
        const EvacuationRoute& route,
        const Position& position,
        const Agent& agent,
        const Point2D& segment,
        double segmentLengthSquared,
        double projection) const {
        if (waypointHasTransition(route) || route.nextWaypointIndex + 1 >= route.waypoints.size()) {
            return false;
        }
        if (segmentLengthSquared <= 1e-9) {
            return false;
        }

        const auto segmentLength = std::sqrt(segmentLengthSquared);
        const auto remainingAlongSegment = (segmentLengthSquared - projection) / segmentLength;
        if (remainingAlongSegment < -kWaypointCrossingEpsilon) {
            return true;
        }

        const auto longitudinalTolerance = std::max(
            kWaypointBypassLongitudinalTolerance,
            static_cast<double>(agent.radius) * 2.0);
        if (remainingAlongSegment > longitudinalTolerance) {
            return false;
        }

        const auto progress = std::clamp(projection / segmentLengthSquared, 0.0, 1.0);
        const auto closestOnSegment = route.currentSegmentStart + (segment * progress);
        const auto lateralDistance = distanceBetween(position.value, closestOnSegment);
        const auto lateralTolerance = std::max(
            kWaypointBypassLateralTolerance,
            static_cast<double>(agent.radius) * 2.5);
        return lateralDistance <= lateralTolerance;
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
                        advanceRouteWaypoint(route, position.value);
                        continue;
                    }
                    if (shouldAdvanceByBypassingWaypoint(
                            route,
                            position,
                            agent,
                            segment,
                            segmentLengthSquared,
                            projection)) {
                        advanceRouteWaypoint(route, position.value);
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
                        advanceRouteWaypoint(route, position.value);
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

    void replanBlockedExitRoutes(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache,
        double elapsedSeconds,
        std::uint64_t layoutRevision) const {
        for (const auto entity : entities) {
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            auto& route = query.get<EvacuationRoute>(entity);
            if (layoutRevision != route.observedLayoutRevision) {
                route.observedLayoutRevision = layoutRevision;
                route.nextExitReplanSeconds = 0.0;
                route.nextSegmentReplanSeconds = 0.0;
            }

            const bool blockedAhead = nextConnectionBlocked(layoutCache, route);
            if (!blockedAhead && !route.noExitAvailable) {
                continue;
            }
            if (elapsedSeconds + 1e-9 < route.nextExitReplanSeconds) {
                continue;
            }

            const auto& position = query.get<Position>(entity);
            const auto startZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
            if (startZoneId.empty()) {
                route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
                continue;
            }

            const auto plan = routePlanToNearestExit(layoutCache, position.value, startZoneId);
            if (plan.destinationZoneId.empty()) {
                route.noExitAvailable = true;
                route.destinationZoneId.clear();
                route.waypoints.clear();
                route.waypointPassages.clear();
                route.waypointFromZoneIds.clear();
                route.waypointZoneIds.clear();
                route.waypointFloorIds.clear();
                route.waypointConnectionIds.clear();
                route.waypointVerticalTransitions.clear();
                route.nextWaypointIndex = 0;
                route.currentSegmentStart = position.value;
                route.displayFloorId = route.currentFloorId;
                route.previousDistanceToWaypoint = 0.0;
                route.stalledSeconds = 0.0;
                route.nextExitReplanSeconds = elapsedSeconds + kNoExitReplanCooldownSeconds;
                continue;
            }

            route.destinationZoneId = plan.destinationZoneId;
            route.waypoints = plan.waypoints;
            route.waypointPassages = plan.waypointPassages;
            route.waypointFromZoneIds = plan.waypointFromZoneIds;
            route.waypointZoneIds = plan.waypointZoneIds;
            route.waypointFloorIds = plan.waypointFloorIds;
            route.waypointConnectionIds = plan.waypointConnectionIds;
            route.waypointVerticalTransitions = plan.waypointVerticalTransitions;
            route.nextWaypointIndex = 0;
            route.currentSegmentStart = position.value;
            route.displayFloorId = route.currentFloorId;
            route.previousDistanceToWaypoint = route.waypoints.empty()
                ? 0.0
                : distanceToRouteWaypoint(route, position.value);
            route.stalledSeconds = 0.0;
            route.noExitAvailable = false;
            route.nextSegmentReplanSeconds = 0.0;
            route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
        }
    }

    void replanBlockedRouteSegments(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache,
        double elapsedSeconds,
        std::uint64_t layoutRevision) const {
        for (const auto entity : entities) {
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            const auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            if (route.noExitAvailable) {
                continue;
            }
            if (layoutRevision != route.observedLayoutRevision) {
                route.observedLayoutRevision = layoutRevision;
                route.nextExitReplanSeconds = 0.0;
                route.nextSegmentReplanSeconds = 0.0;
            }
            if (route.nextWaypointIndex >= route.waypoints.size()) {
                continue;
            }
            if (nextConnectionBlocked(layoutCache, route)) {
                continue;
            }
            if (elapsedSeconds + 1e-9 < route.nextSegmentReplanSeconds) {
                continue;
            }

            const auto target = routeWaypointTarget(route, position.value);
            const auto clearance = static_cast<double>(agent.radius) + kPathClearance;
            const auto& floorLayout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
            if (lineOfSightClear(floorLayout, position.value, target, clearance)) {
                continue;
            }

            const auto replacement = buildPath(floorLayout, position.value, target, clearance);
            if (replacement.size() <= 1) {
                route.nextSegmentReplanSeconds = elapsedSeconds + kFailedSegmentReplanCooldownSeconds;
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
            route.nextSegmentReplanSeconds = elapsedSeconds + kSegmentReplanCooldownSeconds;
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
                const auto& firstRoute = query.get<EvacuationRoute>(first);
                const auto firstCollisionFloorId = agentCollisionFloorId(firstRoute);
                const auto candidates = nearbyAgents(
                    query,
                    spatialIndex,
                    firstPosition.value,
                    firstCollisionFloorId,
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
                    const auto& secondRoute = query.get<EvacuationRoute>(second);
                    if (agentCollisionFloorId(secondRoute) != firstCollisionFloorId) {
                        continue;
                    }
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
