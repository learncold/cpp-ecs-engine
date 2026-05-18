#include "domain/ScenarioSimulationSystems.h"

#include "domain/GeometryQueries.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
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

    ScenarioSimulationMotionSystem(FacilityLayout2D layout, std::vector<RouteGuidanceDraft> routeGuidances)
        : layoutCache_(buildScenarioLayoutCache(std::move(layout))),
          routeGuidances_(std::move(routeGuidances)) {
    }

    void configure(engine::EngineWorld& world) override {
        if (layoutCache_.has_value() && !world.resources().contains<ScenarioLayoutCacheResource>()) {
            world.resources().set(*layoutCache_);
        }
    }

    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override {
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
        refreshPlanningCache(layoutRevision);
        const auto clampedDelta = std::max(0.0, resources.get<ScenarioSimulationStepResource>().deltaSeconds);
        if (clampedDelta <= 0.0) {
            return;
        }

        auto& query = world.query();
        const auto entities = simulationEntities(query);
        refreshActiveEntities(query, entities);
        std::vector<MovementPlan> plans;
        plans.reserve(activeEntities_.size());
        if (!resources.contains<ScenarioEnvironmentReactionResource>()) {
            resources.set(ScenarioEnvironmentReactionResource{});
        }
        auto* reactions = &resources.get<ScenarioEnvironmentReactionResource>();
        const auto* activeHazards = resources.contains<ScenarioActiveEnvironmentHazardsResource>()
            ? &resources.get<ScenarioActiveEnvironmentHazardsResource>()
            : nullptr;
        const auto* sharedSpatialIndex = resources.contains<ScenarioAgentSpatialIndexResource>()
            ? &resources.get<ScenarioAgentSpatialIndexResource>()
            : nullptr;

        applyRouteGuidance(query, entities, layoutCache, clock.elapsedSeconds, step.derivedSeed, sharedSpatialIndex);
        advanceRoutesForCurrentZones(query, activeEntities_, layoutCache);
        replanBlockedExitRoutes(query, activeEntities_, layoutCache, clock.elapsedSeconds, layoutRevision, reactions);
        advanceRoutesForWaypointProgress(query, 0.0, activeEntities_, layoutCache);
        replanBlockedRouteSegments(query, activeEntities_, layoutCache, clock.elapsedSeconds, layoutRevision);
        replanHazardAwareExitRoutes(query, activeEntities_, layoutCache, clock.elapsedSeconds, reactions, activeHazards);
        updateAgentPhysicsFloorIds(query, layoutCache, activeEntities_);
        std::optional<AgentSpatialIndex> localNeighborIndex;
        if (sharedSpatialIndex == nullptr) {
            localNeighborIndex = buildAgentSpatialIndex(query, activeEntities_, 1.0);
        }
        const auto* pressureFeedback = resources.contains<ScenarioPressureFeedbackResource>()
            ? &resources.get<ScenarioPressureFeedbackResource>()
            : nullptr;

        if (!resources.contains<ScenarioTimingKeyframesResource>()) {
            resources.set(ScenarioTimingKeyframesResource{});
        }
        auto& timingKeyframes = resources.get<ScenarioTimingKeyframesResource>();
        const auto totalAgentCount = entities.size();
        const auto t90TargetCount = static_cast<std::size_t>(std::ceil(static_cast<double>(totalAgentCount) * 0.90));
        const auto t95TargetCount = static_cast<std::size_t>(std::ceil(static_cast<double>(totalAgentCount) * 0.95));

        std::size_t evacuatedAtStartCount = 0;
        std::size_t newlyEvacuatedCount = 0;
        for (const auto entity : activeEntities_) {
            auto& position = query.get<Position>(entity);
            auto& velocity = query.get<Velocity>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            auto& status = query.get<EvacuationStatus>(entity);
            if (route.destinationZoneId.empty()) {
                continue;
            }

            const auto& floorLayout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
            const auto* destinationZone = findZone(floorLayout, route.destinationZoneId);
            if (destinationZone != nullptr
                && simulation_internal::pointInRing(destinationZone->area.outline, position.value)) {
                status.evacuated = true;
                status.completionTimeSeconds = clock.elapsedSeconds;
                velocity.value = {};
                ++newlyEvacuatedCount;
            }
        }

        evacuatedAtStartCount = totalAgentCount - activeEntities_.size();
        const auto evacuatedAfterCount = evacuatedAtStartCount + newlyEvacuatedCount;
        if (newlyEvacuatedCount > 0) {
            refreshActiveEntities(query, entities);
        }
        const bool shouldCaptureT90 = t90TargetCount > 0
            && !timingKeyframes.t90Frame.has_value()
            && evacuatedAtStartCount < t90TargetCount
            && evacuatedAfterCount >= t90TargetCount;
        const bool shouldCaptureT95 = t95TargetCount > 0
            && !timingKeyframes.t95Frame.has_value()
            && evacuatedAtStartCount < t95TargetCount
            && evacuatedAfterCount >= t95TargetCount;
        if (shouldCaptureT90 || shouldCaptureT95) {
            SimulationFrame keyframe;
            keyframe.elapsedSeconds = clock.elapsedSeconds;
            keyframe.totalAgentCount = totalAgentCount;
            keyframe.evacuatedAgentCount = evacuatedAfterCount;
            keyframe.complete = totalAgentCount > 0 && evacuatedAfterCount >= totalAgentCount;

            keyframe.agents.reserve(activeEntities_.size());
            for (const auto entity : activeEntities_) {
                const auto& position = query.get<Position>(entity);
                const auto& velocity = query.get<Velocity>(entity);
                const auto& agent = query.get<Agent>(entity);
                const auto* route = query.contains<EvacuationRoute>(entity) ? &query.get<EvacuationRoute>(entity) : nullptr;
                keyframe.agents.push_back({
                    .id = entity.index,
                    .position = position.value,
                    .velocity = velocity.value,
                    .radius = agent.radius,
                    .floorId = route != nullptr
                        ? (!route->displayFloorId.empty()
                            ? route->displayFloorId
                            : route->currentFloorId)
                        : std::string{},
                    .stalled = route != nullptr
                        && scenarioAgentStalled(simulation_internal::lengthOf(velocity.value), route->stalledSeconds),
                });
            }

            if (shouldCaptureT90) {
                timingKeyframes.t90Frame = keyframe;
            }
            if (shouldCaptureT95) {
                timingKeyframes.t95Frame = keyframe;
            }
            if (resources.contains<ScenarioResultArtifactsResource>()) {
                auto& result = resources.get<ScenarioResultArtifactsResource>();
                if (shouldCaptureT90 && !result.artifacts.timingSummary.t90Frame.has_value()) {
                    result.artifacts.timingSummary.t90Frame = timingKeyframes.t90Frame;
                }
                if (shouldCaptureT95 && !result.artifacts.timingSummary.t95Frame.has_value()) {
                    result.artifacts.timingSummary.t95Frame = timingKeyframes.t95Frame;
                }
            }
        }

        for (const auto entity : activeEntities_) {
            auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
            auto& velocity = query.get<Velocity>(entity);
            auto& route = query.get<EvacuationRoute>(entity);

            if (route.nextWaypointIndex >= route.waypoints.size()) {
                velocity.value = {};
                continue;
            }

            const bool verticalTransition = currentWaypointIsVertical(route);
            const bool transitionWaypoint = waypointHasTransition(route);
            const auto target = movementTargetForCurrentWaypoint(
                layoutCache,
                route,
                position.value,
                static_cast<double>(agent.radius));
            const auto distance = distanceBetween(position.value, target);
            if (!verticalTransition && !transitionWaypoint && distance <= kArrivalEpsilon) {
                const auto advance = advanceRouteWaypoint(layoutCache, route, agent, target);
                position.value = advance.position;
                velocity.value = {};
                continue;
            }
            if (verticalTransition && distance <= kGeometryEpsilon) {
                velocity.value = {};
                continue;
            }

            const auto& floorLayout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
            const auto routeDirection = (target - position.value) * (1.0 / distance);
            const auto maxSpeed = effectiveMaxSpeed(layoutCache, agent, route, position.value);
            double pressureSpeedFactor = 1.0;
            double pressureAvoidanceScale = 1.0;
            double pressureBarrierScale = 1.0;
            if (!verticalTransition && pressureFeedback != nullptr) {
                const auto feedbackIt = pressureFeedback->agentsById.find(entity.index);
                if (feedbackIt != pressureFeedback->agentsById.end()) {
                    pressureSpeedFactor = feedbackIt->second.speedFactor;
                    pressureAvoidanceScale = feedbackIt->second.avoidanceScale;
                    pressureBarrierScale = feedbackIt->second.barrierScale;
                }
            }
            const auto adjustedMaxSpeed = maxSpeed * pressureSpeedFactor;
            const auto* hazardState = activeHazardState(reactions, entity.index);
            const auto hazardSpeedFactor = hazardState == nullptr
                ? 1.0
                : std::clamp(hazardState->hazardSpeedFactor, 0.0, 1.0);
            const auto* closureState = activeClosureSlowdownState(reactions, entity.index);
            const auto closureSpeedFactor = closureState == nullptr ? 1.0 : kClosureApproachSpeedFactor;
            const auto adjustedHazardMaxSpeed = adjustedMaxSpeed * hazardSpeedFactor * closureSpeedFactor;
            const auto desiredVelocity = routeDirection * adjustedHazardMaxSpeed;
            double speedScale = 1.0;
            const auto neighborRadius = std::max(
                static_cast<double>(agent.radius) + kDefaultAgentRadius + kPersonalSpaceBuffer,
                kHeadOnLookAheadDistance);
            const auto collisionFloorId = agentCollisionFloorId(route);
            const auto neighborCandidates = sharedSpatialIndex != nullptr
                ? scenarioNearbyAgents(
                    query,
                    *sharedSpatialIndex,
                    position.value,
                    collisionFloorId,
                    neighborRadius)
                : nearbyAgents(
                    query,
                    *localNeighborIndex,
                    position.value,
                    collisionFloorId,
                    neighborRadius);
            const auto avoidanceVelocity =
                forwardPreservingAgentAvoidanceVelocity(
                    query,
                    entity,
                    neighborCandidates,
                    desiredVelocity,
                    clampedDelta,
                    speedScale)
                * pressureAvoidanceScale;
            const auto barrierReferenceSpeed = std::max(
                adjustedHazardMaxSpeed,
                (static_cast<double>(agent.maxSpeed) * 0.75) * pressureSpeedFactor);
            const auto barrierVelocity = sharedSpatialIndex != nullptr
                ? barrierSeparationVelocity(
                    scenarioNearbyBarriers(
                        layoutCache.layout,
                        *sharedSpatialIndex,
                        position.value,
                        route.currentFloorId,
                        static_cast<double>(agent.radius) + kBarrierAvoidanceBuffer),
                    position,
                    agent,
                    barrierReferenceSpeed)
                : barrierSeparationVelocity(floorLayout, position, agent, barrierReferenceSpeed);
            const auto scaledBarrierVelocity = barrierVelocity * pressureBarrierScale;
            const auto hazardAvoidanceVelocity =
                hazardState == nullptr
                ? Point2D{}
                : hazardAvoidanceVelocityFor(*hazardState, position.value, routeDirection, adjustedHazardMaxSpeed);
            auto finalVelocity =
                (desiredVelocity * speedScale) + avoidanceVelocity + scaledBarrierVelocity + hazardAvoidanceVelocity;
            const auto lateral = perpendicularLeft(routeDirection);
            if (dot(finalVelocity, routeDirection) < 0.0) {
                finalVelocity = (routeDirection * (adjustedHazardMaxSpeed * 0.15))
                    + (lateral * dot(finalVelocity, lateral));
            }
            const auto forwardComponent = dot(finalVelocity, routeDirection);
            const auto lateralComponent = dot(finalVelocity, lateral);
            const auto maxLateralComponent = adjustedHazardMaxSpeed * 0.75;
            if (std::fabs(lateralComponent) > maxLateralComponent) {
                finalVelocity = (routeDirection * std::max(0.0, forwardComponent))
                    + (lateral * std::clamp(lateralComponent, -maxLateralComponent, maxLateralComponent));
            }
            finalVelocity = velocityWithBarrierEscape(finalVelocity, scaledBarrierVelocity, adjustedHazardMaxSpeed);
            plans.push_back({
                .entity = entity,
                .velocity = clampedToLength(finalVelocity, adjustedHazardMaxSpeed),
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

            const auto target = movementTargetForCurrentWaypoint(
                layoutCache,
                route,
                position.value,
                static_cast<double>(agent.radius));
            const auto remainingDistance = distanceBetween(position.value, target);
            const auto maxSpeed = effectiveMaxSpeed(layoutCache, agent, route, position.value);
            const auto stepVelocity =
                clampedToLength(plan.velocity, std::min(maxSpeed, remainingDistance / clampedDelta));
            const auto previousPosition = position.value;
            const auto proposedPosition = previousPosition + (stepVelocity * clampedDelta);
            const auto nextPosition = constrainedMoveForCurrentWaypoint(
                layoutCache,
                route,
                previousPosition,
                proposedPosition,
                static_cast<double>(agent.radius));
            position.value = nextPosition;
            velocity.value = (nextPosition - previousPosition) * (1.0 / clampedDelta);
            updateDisplayFloor(route, nextPosition);
        }

        advanceVerticalRoutesAtPortal(query, activeEntities_, layoutCache);
        updateAgentPhysicsFloorIds(query, layoutCache, activeEntities_);
        resolveAgentOverlaps(query, activeEntities_, layoutCache);
        advanceRoutesForCurrentZones(query, activeEntities_, layoutCache);
        advanceRoutesForWaypointProgress(query, clampedDelta, activeEntities_, layoutCache);
        updateAgentPhysicsFloorIds(query, layoutCache, activeEntities_);
        resolveAgentOverlaps(query, activeEntities_, layoutCache);
        const auto pendingScheduledSpawns = resources.contains<ScenarioScheduledSpawnResource>()
            ? resources.get<ScenarioScheduledSpawnResource>().pendingCount
            : std::size_t{0};
        advanceClock(query, clock, entities, clampedDelta, pendingScheduledSpawns);
        resources.set(ScenarioSimulationStepResource{});
    }

private:
    static constexpr double kExitReplanCooldownSeconds = 0.75;
    static constexpr double kNoExitReplanCooldownSeconds = 7.0;
    static constexpr double kSegmentReplanCooldownSeconds = 0.25;
    static constexpr double kFailedSegmentReplanCooldownSeconds = 1.25;
    static constexpr double kClosureApproachSpeedFactor = 0.35;

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

    struct RouteAdvanceResult {
        Point2D position{};
        bool advanced{false};
    };

    void refreshActiveEntities(engine::WorldQuery& query, const std::vector<engine::Entity>& entities) {
        activeEntities_.clear();
        activeEntities_.reserve(entities.size());
        for (const auto entity : entities) {
            if (!query.get<EvacuationStatus>(entity).evacuated) {
                activeEntities_.push_back(entity);
            }
        }
    }

    static const ScenarioEnvironmentReactionAgentState* activeHazardState(
        const ScenarioEnvironmentReactionResource* reactions,
        std::uint64_t agentId) {
        if (reactions == nullptr) {
            return nullptr;
        }
        const auto it = reactions->agentsById.find(agentId);
        if (it == reactions->agentsById.end()
            || !it->second.hazardAware
            || !it->second.hazardInRange) {
            return nullptr;
        }
        return &it->second;
    }

    static const ScenarioEnvironmentReactionAgentState* activeClosureSlowdownState(
        const ScenarioEnvironmentReactionResource* reactions,
        std::uint64_t agentId) {
        if (reactions == nullptr) {
            return nullptr;
        }
        const auto it = reactions->agentsById.find(agentId);
        if (it == reactions->agentsById.end()
            || !it->second.closureDetected
            || it->second.closureAware
            || it->second.blockedConnectionId.empty()) {
            return nullptr;
        }
        return &it->second;
    }

    static void clearClosureReaction(
        ScenarioEnvironmentReactionResource* reactions,
        std::uint64_t agentId) {
        if (reactions == nullptr) {
            return;
        }
        const auto it = reactions->agentsById.find(agentId);
        if (it == reactions->agentsById.end()) {
            return;
        }
        it->second.closureDetected = false;
        it->second.closureAware = false;
        it->second.blockedConnectionId.clear();
        it->second.closureDetectedAtSeconds = 0.0;
        it->second.closureReactionReadySeconds = 0.0;
    }

    static bool sameFloor(const std::string& lhs, const std::string& rhs) {
        return lhs == rhs || lhs.empty() || rhs.empty();
    }

    static long long quantizedPlanningCoordinate(double value) {
        return static_cast<long long>(std::llround(value * 1000.0));
    }

    static std::string pointCachePart(const Point2D& point) {
        return std::to_string(quantizedPlanningCoordinate(point.x))
            + ","
            + std::to_string(quantizedPlanningCoordinate(point.y));
    }

    static std::string zoneRouteCacheKey(
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& exitZoneId) {
        return startZoneId + '\x1f' + pointCachePart(start) + '\x1f' + exitZoneId;
    }

    static std::string pathCacheKey(
        const std::string& floorId,
        const Point2D& start,
        const Point2D& goal,
        double clearance) {
        return floorId
            + '\x1f'
            + pointCachePart(start)
            + '\x1f'
            + pointCachePart(goal)
            + '\x1f'
            + std::to_string(quantizedPlanningCoordinate(clearance));
    }

    void refreshPlanningCache(std::uint64_t layoutRevision) const {
        if (planningCacheRevision_ == layoutRevision) {
            return;
        }
        planningCacheRevision_ = layoutRevision;
        nearestExitRouteCache_.clear();
        exitRouteCache_.clear();
        pathCache_.clear();
    }

    std::optional<ZoneRouteToExit> cachedZoneRouteToNearestExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId) const {
        const auto key = zoneRouteCacheKey(start, startZoneId, std::string{"nearest"});
        const auto it = nearestExitRouteCache_.find(key);
        if (it != nearestExitRouteCache_.end()) {
            return it->second;
        }
        auto route = simulation_internal::zoneRouteToNearestExit(layoutCache, start, startZoneId);
        nearestExitRouteCache_.emplace(key, route);
        return route;
    }

    std::optional<ZoneRouteResult> cachedZoneRouteToExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& exitZoneId) const {
        const auto key = zoneRouteCacheKey(start, startZoneId, exitZoneId);
        const auto it = exitRouteCache_.find(key);
        if (it != exitRouteCache_.end()) {
            return it->second;
        }
        auto route = simulation_internal::zoneRouteToExit(layoutCache, start, startZoneId, exitZoneId);
        exitRouteCache_.emplace(key, route);
        return route;
    }

    const std::vector<Point2D>& cachedPath(
        const std::string& floorId,
        const FacilityLayout2D& layout,
        const Point2D& start,
        const Point2D& goal,
        double clearance) const {
        const auto key = pathCacheKey(floorId, start, goal, clearance);
        const auto it = pathCache_.find(key);
        if (it != pathCache_.end()) {
            return it->second;
        }
        auto [insertedIt, _] = pathCache_.emplace(key, buildPath(layout, start, goal, clearance));
        return insertedIt->second;
    }

    static Point2D hazardAvoidanceVelocityFor(
        const ScenarioEnvironmentReactionAgentState& state,
        const Point2D& position,
        const Point2D& routeDirection,
        double maxSpeed) {
        if (state.hazardRadiusMeters <= 1e-9 || maxSpeed <= 1e-9) {
            return {};
        }

        const auto away = normalizedOr(position - state.hazardPosition, perpendicularLeft(routeDirection));
        const auto proximity = std::clamp(
            1.0 - (std::max(0.0, state.hazardDistanceMeters) / state.hazardRadiusMeters),
            0.0,
            1.0);
        const auto kindScale = state.hazardKind == EnvironmentHazardKind::Fire ? 0.85 : 0.65;
        return away * (maxSpeed * kindScale * (0.35 + (0.65 * proximity)));
    }

    const Connection2D* findConnectionById(
        const ScenarioLayoutCacheResource& layoutCache,
        const std::string& connectionId) const {
        if (connectionId.empty()) {
            return nullptr;
        }
        const auto it = layoutCache.connectionIndices.find(connectionId);
        if (it == layoutCache.connectionIndices.end() || it->second >= layoutCache.layout.connections.size()) {
            return nullptr;
        }
        return &layoutCache.layout.connections[it->second];
    }

    bool connectionTouchesFloor(
        const ScenarioLayoutCacheResource& layoutCache,
        const Connection2D& connection,
        const std::string& floorId) const {
        if (sameFloor(connection.floorId, floorId)) {
            return true;
        }
        const auto fromFloorId = cachedFloorIdForZone(layoutCache, connection.fromZoneId);
        const auto toFloorId = cachedFloorIdForZone(layoutCache, connection.toZoneId);
        return sameFloor(fromFloorId, floorId) || sameFloor(toFloorId, floorId);
    }

    bool guidanceHasInstallPosition(const RouteGuidanceDraft& guidance) const {
        return !guidance.installFloorId.empty() || !guidance.installZoneId.empty();
    }

    bool agentCanSeeGuidanceAtInstallConnection(
        const ScenarioLayoutCacheResource& layoutCache,
        const RouteGuidanceDraft& guidance,
        const Position& position,
        const Agent& agent,
        const EvacuationRoute& route) const {
        if (guidance.installConnectionId.empty()) {
            return true;
        }

        const auto* connection = findConnectionById(layoutCache, guidance.installConnectionId);
        if (connection == nullptr || !connectionTouchesFloor(layoutCache, *connection, route.currentFloorId)) {
            return false;
        }

        const auto currentZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
        if (!currentZoneId.empty()
            && currentZoneId != connection->fromZoneId
            && currentZoneId != connection->toZoneId) {
            return false;
        }

        const auto closestOnInstall = closestPointOnSegment(
            position.value,
            connection->centerSpan.start,
            connection->centerSpan.end);
        const auto visibilityDistance = std::max(
            2.0,
            (connection->effectiveWidth * 0.5) + static_cast<double>(agent.radius) + 0.25);
        return distanceBetween(position.value, closestOnInstall) <= visibilityDistance;
    }

    bool agentCanSeeGuidanceAtInstallPosition(
        const ScenarioLayoutCacheResource& layoutCache,
        const RouteGuidanceDraft& guidance,
        const Position& position,
        const Agent& agent,
        const EvacuationRoute& route) const {
        if (!guidanceHasInstallPosition(guidance) || !sameFloor(guidance.installFloorId, route.currentFloorId)) {
            return false;
        }

        const auto currentZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
        if (!guidance.installZoneId.empty() && currentZoneId != guidance.installZoneId) {
            return false;
        }

        constexpr double kGuidanceVisibilityDistanceMeters = 2.5;
        if (distanceBetween(position.value, guidance.installPosition) > kGuidanceVisibilityDistanceMeters) {
            return false;
        }

        const auto& floorLayout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
        const auto clearance = std::max(0.08, static_cast<double>(agent.radius) + 0.05);
        return lineOfSightClear(floorLayout, position.value, guidance.installPosition, clearance);
    }

    const Connection2D* nextBlockedConnection(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route) const {
        if (route.nextWaypointIndex >= route.waypoints.size()
            || route.nextWaypointIndex >= route.waypointConnectionIds.size()) {
            return nullptr;
        }
        for (std::size_t index = route.nextWaypointIndex; index < route.waypointConnectionIds.size(); ++index) {
            const auto& connectionId = route.waypointConnectionIds[index];
            if (connectionId.empty()) {
                continue;
            }
            const auto* connection = findConnectionById(layoutCache, connectionId);
            if (connection != nullptr && connection->directionality == TravelDirection::Closed) {
                return connection;
            }
            return nullptr;
        }
        return nullptr;
    }

    bool nextConnectionBlocked(const ScenarioLayoutCacheResource& layoutCache, const EvacuationRoute& route) const {
        return nextBlockedConnection(layoutCache, route) != nullptr;
    }

    std::optional<Point2D> verticalTransitionNormal(
        const LineSegment2D& passage,
        const Zone2D& toZone,
        const Point2D& transitionStartPoint) const {
        auto normal = normalizedOr(perpendicularLeft(passage.end - passage.start), {});
        if (lengthOf(normal) <= 1e-9) {
            return std::nullopt;
        }

        const auto passageMidpoint = midpoint(passage);
        auto towardTarget = polygonCenter(toZone.area) - passageMidpoint;
        if (lengthOf(towardTarget) <= kArrivalEpsilon) {
            towardTarget = passageMidpoint - transitionStartPoint;
        }
        if (lengthOf(towardTarget) <= kArrivalEpsilon) {
            return std::nullopt;
        }
        if (dot(normal, towardTarget) < 0.0) {
            normal = normal * -1.0;
        }
        return normal;
    }

    bool verticalPassageCrossed(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route,
        const Point2D& position,
        double agentRadius) const {
        if (route.nextWaypointIndex >= route.waypointPassages.size()
            || route.nextWaypointIndex >= route.waypointFromZoneIds.size()
            || route.nextWaypointIndex >= route.waypointZoneIds.size()) {
            return false;
        }

        const auto& passage = route.waypointPassages[route.nextWaypointIndex];
        if (lengthSquaredOf(passage) <= 1e-9) {
            return false;
        }

        const auto* toZone = findCachedZone(layoutCache, route.waypointZoneIds[route.nextWaypointIndex]);
        if (toZone == nullptr) {
            return false;
        }

        const auto normal = verticalTransitionNormal(passage, *toZone, route.currentSegmentStart);
        const auto passageMidpoint = midpoint(passage);
        if (!normal.has_value()) {
            return false;
        }

        const auto passageVector = passage.end - passage.start;
        const auto passageLengthSquared = dot(passageVector, passageVector);
        const auto passageLength = std::sqrt(passageLengthSquared);
        const auto spanTolerance = static_cast<double>(agentRadius) * passageLength;
        const auto projection = dot(position - passage.start, passageVector);
        if (projection < -spanTolerance || projection > passageLengthSquared + spanTolerance) {
            return false;
        }

        return dot(position - passageMidpoint, *normal) >= -kGeometryEpsilon;
    }

    std::optional<Point2D> passageNormalTowardCurrentWaypoint(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route) const {
        if (route.nextWaypointIndex >= route.waypointPassages.size()
            || route.nextWaypointIndex >= route.waypointZoneIds.size()) {
            return std::nullopt;
        }

        const auto& passage = route.waypointPassages[route.nextWaypointIndex];
        if (lengthSquaredOf(passage) <= 1e-9) {
            return std::nullopt;
        }

        const auto* toZone = findCachedZone(layoutCache, route.waypointZoneIds[route.nextWaypointIndex]);
        if (toZone == nullptr) {
            return std::nullopt;
        }

        return verticalTransitionNormal(passage, *toZone, route.currentSegmentStart);
    }

    Point2D movementTargetForCurrentWaypoint(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route,
        const Point2D& position,
        double clearance) const {
        if (!currentWaypointIsVertical(route)
            && waypointHasTransition(route)
            && route.nextWaypointIndex < route.waypointPassages.size()) {
            const auto& passage = route.waypointPassages[route.nextWaypointIndex];
            if (lengthSquaredOf(passage) > 1e-9) {
                const auto normal = passageNormalTowardCurrentWaypoint(layoutCache, route);
                if (normal.has_value()) {
                    const auto doorwayTarget = closestPointOnSegment(position, passage.start, passage.end);
                    const auto offset = std::max(kPortalCrossingEpsilon * 2.0, clearance * 0.20);
                    return doorwayTarget + (*normal * offset);
                }
            }
        }

        return routeWaypointTarget(route, position);
    }

    Point2D velocityWithBarrierEscape(
        const Point2D& velocity,
        const Point2D& barrierVelocity,
        double maxSpeed) const {
        const auto barrierSpeed = lengthOf(barrierVelocity);
        if (barrierSpeed <= 1e-9 || maxSpeed <= 1e-9) {
            return velocity;
        }

        const auto escapeDirection = barrierVelocity * (1.0 / barrierSpeed);
        const auto minimumEscapeSpeed = std::min(maxSpeed * 0.5, barrierSpeed);
        const auto escapeComponent = dot(velocity, escapeDirection);
        if (escapeComponent >= minimumEscapeSpeed) {
            return velocity;
        }

        return velocity + (escapeDirection * (minimumEscapeSpeed - escapeComponent));
    }

    bool movingOutOfBarrierKeepout(
        const FacilityLayout2D& layout,
        const Position& position,
        const Agent& agent,
        const Velocity& velocity,
        double referenceSpeed) const {
        const auto barrierVelocity = barrierSeparationVelocity(layout, position, agent, referenceSpeed);
        const auto barrierSpeed = lengthOf(barrierVelocity);
        if (barrierSpeed <= 1e-9 || lengthOf(velocity.value) <= kScenarioStalledSpeedThreshold) {
            return false;
        }

        return dot(velocity.value, barrierVelocity) > 0.01;
    }

    const Zone2D* verticalEndpointZoneForCurrentFloor(
        const ScenarioLayoutCacheResource& layoutCache,
        const Connection2D& connection,
        const std::string& floorId) const {
        const auto fromFloorId = cachedFloorIdForZone(layoutCache, connection.fromZoneId);
        if (fromFloorId == floorId) {
            return findCachedZone(layoutCache, connection.fromZoneId);
        }

        const auto toFloorId = cachedFloorIdForZone(layoutCache, connection.toZoneId);
        if (toFloorId == floorId) {
            return findCachedZone(layoutCache, connection.toZoneId);
        }
        return nullptr;
    }

    Point2D constrainedMoveOutOfVerticalPortalContact(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route,
        const Point2D& from,
        const Point2D& to,
        double clearance) const {
        const auto& layout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
        const auto releaseDistance = std::max(clearance + kPathClearance, kPortalCrossingEpsilon * 2.0);
        const Connection2D* portalConnection = nullptr;
        const Zone2D* endpointZone = nullptr;
        for (const auto& connection : layoutCache.layout.connections) {
            if (!isVerticalConnection(connection)) {
                continue;
            }

            const auto* candidateZone = verticalEndpointZoneForCurrentFloor(layoutCache, connection, route.currentFloorId);
            if (candidateZone == nullptr) {
                continue;
            }
            const auto startDistance = distanceBetween(
                route.currentSegmentStart,
                closestPointOnSegment(route.currentSegmentStart, connection.centerSpan.start, connection.centerSpan.end));
            const auto fromDistance = distanceBetween(
                from,
                closestPointOnSegment(from, connection.centerSpan.start, connection.centerSpan.end));
            if (startDistance <= releaseDistance && fromDistance <= releaseDistance) {
                portalConnection = &connection;
                endpointZone = candidateZone;
                break;
            }
        }
        if (portalConnection == nullptr || endpointZone == nullptr) {
            return from;
        }

        const auto normal = verticalTransitionNormal(portalConnection->centerSpan, *endpointZone, route.currentSegmentStart);
        if (!normal.has_value()) {
            return from;
        }

        auto validPortalRelease = [&](const Point2D& candidate) {
            const auto movement = candidate - from;
            if (dot(movement, *normal) < -kGeometryEpsilon) {
                return false;
            }
            return simulation_internal::pointInRing(endpointZone->area.outline, candidate)
                && !movementCrossesBarrier(layout, from, candidate);
        };

        if (validPortalRelease(to)) {
            return to;
        }

        const Point2D xOnly{.x = to.x, .y = from.y};
        if (validPortalRelease(xOnly)) {
            return xOnly;
        }

        const Point2D yOnly{.x = from.x, .y = to.y};
        if (validPortalRelease(yOnly)) {
            return yOnly;
        }

        Point2D best = from;
        double low = 0.0;
        double high = 1.0;
        for (int i = 0; i < 8; ++i) {
            const auto t = (low + high) * 0.5;
            const auto candidate = from + ((to - from) * t);
            if (validPortalRelease(candidate)) {
                low = t;
                best = candidate;
            } else {
                high = t;
            }
        }
        return best;
    }

    Point2D constrainedMoveForCurrentWaypoint(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route,
        const Point2D& from,
        const Point2D& to,
        double clearance) const {
        const auto& layout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
        if (!currentWaypointIsVertical(route)) {
            const auto constrained = constrainedMove(layout, from, to, clearance);
            if (distanceBetween(constrained, from) > kGeometryEpsilon) {
                return constrained;
            }
            return constrainedMoveOutOfVerticalPortalContact(layoutCache, route, from, to, clearance);
        }

        return constrainedMoveWithBarrierClearance(layout, from, to, clearance);
    }

    std::optional<Point2D> verticalTransitionLandingPoint(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route,
        std::size_t reachedIndex,
        const Point2D& transitionStartPoint,
        const Point2D& reachedPoint) const {
        if (reachedIndex >= route.waypointVerticalTransitions.size()
            || !route.waypointVerticalTransitions[reachedIndex]
            || reachedIndex >= route.waypointPassages.size()
            || reachedIndex >= route.waypointFromZoneIds.size()
            || reachedIndex >= route.waypointZoneIds.size()) {
            return reachedPoint;
        }

        const auto* toZone = findCachedZone(layoutCache, route.waypointZoneIds[reachedIndex]);
        if (toZone == nullptr) {
            return reachedPoint;
        }

        const auto& passage = route.waypointPassages[reachedIndex];
        if (lengthSquaredOf(passage) <= 1e-9) {
            return reachedPoint;
        }

        const auto normal = verticalTransitionNormal(passage, *toZone, transitionStartPoint);
        if (!normal.has_value()) {
            return reachedPoint;
        }

        const auto portalDistance = dot(reachedPoint - midpoint(passage), *normal);
        if (portalDistance > kGeometryEpsilon) {
            return reachedPoint;
        }

        return reachedPoint + (*normal * (kGeometryEpsilon - portalDistance));
    }

    std::optional<Point2D> verticalTransitionPlanningPoint(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route,
        std::size_t reachedIndex,
        const Point2D& transitionStartPoint,
        const Point2D& reachedPoint,
        double clearance) const {
        if (reachedIndex >= route.waypointVerticalTransitions.size()
            || !route.waypointVerticalTransitions[reachedIndex]
            || reachedIndex >= route.waypointPassages.size()
            || reachedIndex >= route.waypointFromZoneIds.size()
            || reachedIndex >= route.waypointZoneIds.size()) {
            return reachedPoint;
        }

        const auto* toZone = findCachedZone(layoutCache, route.waypointZoneIds[reachedIndex]);
        if (toZone == nullptr) {
            return reachedPoint;
        }

        const auto& passage = route.waypointPassages[reachedIndex];
        if (lengthSquaredOf(passage) <= 1e-9) {
            return reachedPoint;
        }

        const auto landingOnPassage = closestPointOnSegment(reachedPoint, passage.start, passage.end);
        const auto targetFloorId = cachedFloorIdForZone(layoutCache, toZone->id);
        const auto& targetLayout = cachedLayoutForFloor(layoutCache, targetFloorId);
        const auto passageDirection = passage.end - passage.start;
        const auto tangent = normalizedOr(passageDirection, {});
        const auto normal = verticalTransitionNormal(passage, *toZone, transitionStartPoint);
        if (!normal.has_value()) {
            return reachedPoint;
        }

        auto validLanding = [&](const Point2D& candidate) {
            return simulation_internal::pointInRing(toZone->area.outline, candidate)
                && pointHasBarrierClearance(targetLayout, candidate, clearance);
        };

        const auto baseOffset = std::max(clearance + kPathClearance, 0.2);
        const double offsets[] = {baseOffset, 0.45, 0.75, 1.1, 1.6};
        const double slideUnit = std::max(clearance + kPathClearance, 0.25);
        const double slides[] = {0.0, slideUnit, -slideUnit, slideUnit * 2.0, slideUnit * -2.0};
        for (const auto offset : offsets) {
            for (const auto slide : slides) {
                const auto candidate = landingOnPassage + (*normal * offset) + (tangent * slide);
                if (validLanding(candidate)) {
                    return candidate;
                }
            }
        }

        return validLanding(reachedPoint) ? std::optional<Point2D>{reachedPoint} : std::nullopt;
    }

    std::string verticalTransitionTargetFloorId(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route,
        std::size_t reachedIndex) const {
        auto targetFloorId = reachedIndex < route.waypointFloorIds.size()
            ? route.waypointFloorIds[reachedIndex]
            : std::string{};
        if (reachedIndex < route.waypointZoneIds.size()) {
            const auto toZoneFloorId = cachedFloorIdForZone(layoutCache, route.waypointZoneIds[reachedIndex]);
            if (!toZoneFloorId.empty() && toZoneFloorId != route.currentFloorId) {
                targetFloorId = toZoneFloorId;
            } else if (targetFloorId.empty()) {
                targetFloorId = toZoneFloorId;
            }
        }
        return targetFloorId;
    }

    static std::uint64_t fnv1a64(const std::string& value) {
        std::uint64_t hash = 1469598103934665603ULL;
        for (const unsigned char ch : value) {
            hash ^= static_cast<std::uint64_t>(ch);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static std::uint64_t mix64(std::uint64_t value) {
        value += 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31U);
    }

    static double uniform01(std::uint64_t value) {
        const auto mixed = mix64(value);
        const auto mantissa = mixed >> 11U;
        return static_cast<double>(mantissa) * (1.0 / 9007199254740992.0);
    }

    static double clamp01(double value) {
        return std::clamp(value, 0.0, 1.0);
    }

    static double logit(double p) {
        const auto clamped = std::clamp(p, 1e-6, 1.0 - 1e-6);
        return std::log(clamped / (1.0 - clamped));
    }

    static double sigmoid(double x) {
        if (x >= 0.0) {
            const auto z = std::exp(-x);
            return 1.0 / (1.0 + z);
        }
        const auto z = std::exp(x);
        return z / (1.0 + z);
    }

    struct ActiveRouteGuidance {
        const RouteGuidanceDraft* guidance{nullptr};
        std::size_t guidanceIndex{0};
        std::size_t periodIndex{0};
        double startSeconds{0.0};
        double endSeconds{0.0};
    };

    std::vector<ActiveRouteGuidance> activeRouteGuidances(double elapsedSeconds) const {
        std::vector<ActiveRouteGuidance> active;
        for (std::size_t guidanceIndex = 0; guidanceIndex < routeGuidances_.size(); ++guidanceIndex) {
            const auto& guidance = routeGuidances_[guidanceIndex];
            if (guidance.periods.empty()) {
                const double start = 0.0;
                const double end = 1e18;
                if (elapsedSeconds + 1e-9 < start || elapsedSeconds > end + 1e-9) {
                    continue;
                }
                active.push_back(ActiveRouteGuidance{
                    .guidance = &guidance,
                    .guidanceIndex = guidanceIndex,
                    .periodIndex = 0,
                    .startSeconds = start,
                    .endSeconds = end,
                });
                continue;
            }

            for (std::size_t index = 0; index < guidance.periods.size(); ++index) {
                const auto& period = guidance.periods[index];
                const auto start = std::max(0.0, period.startSeconds);
                const auto end = std::max(start, std::max(0.0, period.endSeconds));
                if (elapsedSeconds + 1e-9 < start) {
                    continue;
                }
                if (elapsedSeconds > end + 1e-9) {
                    continue;
                }
                active.push_back(ActiveRouteGuidance{
                    .guidance = &guidance,
                    .guidanceIndex = guidanceIndex,
                    .periodIndex = index,
                    .startSeconds = start,
                    .endSeconds = end,
                });
            }
        }
        return active;
    }

    std::string activeRouteGuidanceKey(const ActiveRouteGuidance& active) const {
        if (active.guidance == nullptr) {
            return {};
        }
        auto id = active.guidance->id;
        if (id.empty()) {
            id = "route-guidance:";
            id.append(std::to_string(active.guidanceIndex));
        }
        if (!active.guidance->periods.empty()) {
            id.append(":p");
            id.append(std::to_string(active.periodIndex));
        }
        return id;
    }

    std::string activeRouteGuidanceSignature(const std::vector<ActiveRouteGuidance>& activeGuidances) const {
        std::string signature;
        for (const auto& active : activeGuidances) {
            if (!signature.empty()) {
                signature.push_back('\x1f');
            }
            signature.append(activeRouteGuidanceKey(active));
        }
        return signature;
    }

    const ActiveRouteGuidance* matchingActiveGuidance(
        const std::vector<ActiveRouteGuidance>& activeGuidances,
        const std::string& guidanceEventId) const {
        if (guidanceEventId.empty()) {
            return nullptr;
        }
        for (const auto& active : activeGuidances) {
            if (activeRouteGuidanceKey(active) == guidanceEventId) {
                return &active;
            }
        }
        return nullptr;
    }

    const ActiveRouteGuidance* applicableActiveGuidance(
        const std::vector<ActiveRouteGuidance>& activeGuidances,
        const ScenarioLayoutCacheResource& layoutCache,
        const Position& position,
        const Agent& agent,
        const EvacuationRoute& route) const {
        const ActiveRouteGuidance* best = nullptr;
        double bestStart = -1.0;
        for (const auto& active : activeGuidances) {
            if (active.guidance == nullptr) {
                continue;
            }
            const auto& guidance = *active.guidance;
            bool applicable = true;
            if (!guidance.installConnectionId.empty()) {
                applicable = agentCanSeeGuidanceAtInstallConnection(layoutCache, guidance, position, agent, route);
            } else if (guidanceHasInstallPosition(guidance)) {
                applicable = agentCanSeeGuidanceAtInstallPosition(layoutCache, guidance, position, agent, route);
            }
            if (!applicable) {
                continue;
            }
            if (best == nullptr || active.startSeconds >= bestStart) {
                bestStart = active.startSeconds;
                best = &active;
            }
        }
        return best;
    }

    std::optional<double> zoneDistanceToExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& exitZoneId) const {
        const auto result = zoneRouteToExit(layoutCache, start, startZoneId, exitZoneId);
        if (!result.has_value()) {
            return std::nullopt;
        }
        return result->distance;
    }

    double guidancePriorityRadiusMeters(
        const ScenarioLayoutCacheResource& layoutCache,
        const RouteGuidanceDraft& guidance) const {
        if (!guidance.installConnectionId.empty()) {
            if (const auto* connection = findConnectionById(layoutCache, guidance.installConnectionId);
                connection != nullptr) {
                return std::max(2.0, (connection->effectiveWidth * 0.5) + 0.5);
            }
            return 2.0;
        }
        if (guidanceHasInstallPosition(guidance)) {
            return 2.5;
        }
        return 0.0;
    }

    std::vector<engine::Entity> prioritizedGuidanceEntities(
        engine::WorldQuery& query,
        const std::vector<ActiveRouteGuidance>& activeGuidances,
        const ScenarioLayoutCacheResource& layoutCache,
        const ScenarioAgentSpatialIndexResource* sharedSpatialIndex,
        const AgentSpatialIndex* localSpatialIndex) const {
        std::vector<engine::Entity> prioritized;

        auto appendNearby = [&](const Point2D& anchor, const std::string& floorId, double radius) {
            if (radius <= 0.0) {
                return;
            }

            std::vector<engine::Entity> nearby;
            if (sharedSpatialIndex != nullptr) {
                if (floorId.empty()) {
                    for (const auto& [candidateFloorId, _] : sharedSpatialIndex->cellsByFloor) {
                        const auto floorNearby = scenarioNearbyAgents(query, *sharedSpatialIndex, anchor, candidateFloorId, radius);
                        nearby.insert(nearby.end(), floorNearby.begin(), floorNearby.end());
                    }
                } else {
                    nearby = scenarioNearbyAgents(query, *sharedSpatialIndex, anchor, floorId, radius);
                }
            } else if (localSpatialIndex != nullptr) {
                if (floorId.empty()) {
                    nearby = nearbyAgents(query, *localSpatialIndex, anchor, radius);
                } else {
                    nearby = nearbyAgents(query, *localSpatialIndex, anchor, floorId, radius);
                }
            }
            prioritized.insert(prioritized.end(), nearby.begin(), nearby.end());
        };

        for (const auto& active : activeGuidances) {
            if (active.guidance == nullptr) {
                continue;
            }

            const auto& guidance = *active.guidance;
            const auto radius = guidancePriorityRadiusMeters(layoutCache, guidance);
            if (!guidance.installConnectionId.empty()) {
                const auto* connection = findConnectionById(layoutCache, guidance.installConnectionId);
                if (connection == nullptr) {
                    continue;
                }
                const auto floorId = connection->floorId.empty() ? guidance.installFloorId : connection->floorId;
                appendNearby(midpoint(connection->centerSpan), floorId, radius);
                continue;
            }

            if (guidanceHasInstallPosition(guidance)) {
                appendNearby(guidance.installPosition, guidance.installFloorId, radius);
            }
        }

        std::sort(prioritized.begin(), prioritized.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.index != rhs.index) {
                return lhs.index < rhs.index;
            }
            return lhs.generation < rhs.generation;
        });
        prioritized.erase(std::unique(prioritized.begin(), prioritized.end()), prioritized.end());
        return prioritized;
    }

    double complianceProbability(
        const RouteGuidanceDraft& guidance,
        const Agent& agent,
        double detourMeters) const {
        constexpr double kStrengthBaseline = 0.55;
        constexpr double kStrengthWeight = 4.0;
        constexpr double kDetourWeight = 2.0;
        constexpr double kPropensityWeight = 1.0;

        const auto base = logit(clamp01(guidance.baseComplianceRate));
        const auto strength = clamp01(guidance.guidanceStrength);
        const auto detourRatio = std::max(0.0, detourMeters) / std::max(1e-6, guidance.maxDetourMeters);
        const auto propensity = clamp01(agent.guidancePropensity);
        const auto score =
            base
            + (kStrengthWeight * (strength - kStrengthBaseline))
            - (kDetourWeight * detourRatio)
            + (kPropensityWeight * logit(propensity));
        return clamp01(sigmoid(score));
    }

    void applyRouteGuidanceToEntity(
        engine::WorldQuery& query,
        engine::Entity entity,
        const ScenarioLayoutCacheResource& layoutCache,
        const std::vector<ActiveRouteGuidance>& activeGuidances,
        double elapsedSeconds,
        std::uint64_t stableSeed) {
        const auto& status = query.get<EvacuationStatus>(entity);
        if (status.evacuated) {
            return;
        }

        const auto& position = query.get<Position>(entity);
        const auto& agent = query.get<Agent>(entity);
        auto& route = query.get<EvacuationRoute>(entity);
        if (route.originalDestinationZoneId.empty()) {
            route.originalDestinationZoneId = route.destinationZoneId;
        }

        const auto startZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
        if (startZoneId.empty()) {
            return;
        }

        if (activeGuidances.empty()) {
            route.guidanceEventId.clear();
            route.followsGuidance = false;

            std::string desiredExit = route.originalDestinationZoneId;
            if (desiredExit.empty()) {
                return;
            }

            if (route.destinationZoneId == desiredExit && !route.waypoints.empty()) {
                return;
            }

            RoutePlan plan = routePlanToExit(layoutCache, position.value, startZoneId, desiredExit);
            if (plan.destinationZoneId.empty()) {
                plan = routePlanToNearestExit(layoutCache, position.value, startZoneId);
            }
            if (plan.destinationZoneId.empty()) {
                return;
            }
            replaceRouteWithPlan(route, plan, position.value);
            route.nextExitReplanSeconds = elapsedSeconds + 0.25;
            return;
        }

        const auto* visibleGuidance = applicableActiveGuidance(activeGuidances, layoutCache, position, agent, route);
        if (visibleGuidance != nullptr && visibleGuidance->guidance != nullptr
            && (!visibleGuidance->guidance->installConnectionId.empty()
                || guidanceHasInstallPosition(*visibleGuidance->guidance))) {
            if (route.guidanceEventId == activeRouteGuidanceKey(*visibleGuidance)) {
                return;
            }
        }

        const auto* activeGuidance = visibleGuidance;
        if (activeGuidance == nullptr) {
            if (const auto* retained = matchingActiveGuidance(activeGuidances, route.guidanceEventId);
                retained != nullptr
                && retained->guidance != nullptr
                && (!retained->guidance->installConnectionId.empty() || guidanceHasInstallPosition(*retained->guidance))) {
                return;
            }
        }

        if (activeGuidance == nullptr || activeGuidance->guidance == nullptr) {
            if (!route.guidanceEventId.empty() || route.followsGuidance) {
                route.guidanceEventId.clear();
                route.followsGuidance = false;
                const auto& desiredExit = route.originalDestinationZoneId;
                if (!desiredExit.empty() && (route.destinationZoneId != desiredExit || route.waypoints.empty())) {
                    auto plan = routePlanToExit(layoutCache, position.value, startZoneId, desiredExit);
                    if (plan.destinationZoneId.empty()) {
                        plan = routePlanToNearestExit(layoutCache, position.value, startZoneId);
                    }
                    if (!plan.destinationZoneId.empty()) {
                        replaceRouteWithPlan(route, plan, position.value);
                        route.nextExitReplanSeconds = elapsedSeconds + 0.25;
                    }
                }
            }
            return;
        }

        const auto& selectedGuidance = *activeGuidance->guidance;
        const auto activeId = activeRouteGuidanceKey(*activeGuidance);
        const auto activeIdHash = fnv1a64(activeId);

        bool guidedExitValid = false;
        if (!selectedGuidance.guidedExitZoneId.empty()) {
            if (const auto* exitZone = findCachedZone(layoutCache, selectedGuidance.guidedExitZoneId);
                exitZone != nullptr && exitZone->kind == ZoneKind::Exit) {
                guidedExitValid = true;
            }
        }

        double detourMeters = 0.0;
        if (guidedExitValid && !route.originalDestinationZoneId.empty()) {
            const auto* originalExit = findCachedZone(layoutCache, route.originalDestinationZoneId);
            const auto* guidedExit = findCachedZone(layoutCache, selectedGuidance.guidedExitZoneId);
            if (originalExit != nullptr && guidedExit != nullptr && originalExit->kind == ZoneKind::Exit
                && guidedExit->kind == ZoneKind::Exit) {
                const auto originalDistance = distanceBetween(position.value, polygonCenter(originalExit->area));
                const auto guidedDistance = distanceBetween(position.value, polygonCenter(guidedExit->area));
                detourMeters = std::max(0.0, guidedDistance - originalDistance);
            }
        }

        const auto pFollow = complianceProbability(selectedGuidance, agent, detourMeters);
        const auto u = uniform01(
            stableSeed
            ^ activeIdHash
            ^ (static_cast<std::uint64_t>(entity.index) << 1U)
            ^ static_cast<std::uint64_t>(entity.generation));
        const bool follows = u < pFollow;

        route.guidanceEventId = activeId;
        route.followsGuidance = follows;

        std::string desiredExit;
        if (follows && guidedExitValid) {
            desiredExit = selectedGuidance.guidedExitZoneId;
        } else if (!follows) {
            desiredExit = route.originalDestinationZoneId;
        }

        if (!desiredExit.empty() && route.destinationZoneId == desiredExit && !route.waypoints.empty()) {
            return;
        }

        RoutePlan plan;
        if (!desiredExit.empty()) {
            plan = routePlanToExit(layoutCache, position.value, startZoneId, desiredExit);
        }
        if (plan.destinationZoneId.empty()) {
            plan = routePlanToNearestExit(layoutCache, position.value, startZoneId);
        }
        if (plan.destinationZoneId.empty()) {
            return;
        }
        replaceRouteWithPlan(route, plan, position.value);
        route.nextExitReplanSeconds = elapsedSeconds + 0.25;
    }

    void applyRouteGuidance(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache,
        double elapsedSeconds,
        std::uint64_t derivedSeed,
        const ScenarioAgentSpatialIndexResource* sharedSpatialIndex) {
        // Keep this small to avoid frame spikes when guidance toggles.
        // Higher values converge faster but may cause noticeable hitching with many agents.
        constexpr std::size_t kGuidanceReplanBudgetPerFrame = 50;

        const auto activeGuidances = activeRouteGuidances(elapsedSeconds);
        const auto activeSignature = activeRouteGuidanceSignature(activeGuidances);
        const auto hasVisibilityAnchoredGuidance = std::any_of(activeGuidances.begin(), activeGuidances.end(), [&](const auto& active) {
            return active.guidance != nullptr
                && (!active.guidance->installConnectionId.empty() || guidanceHasInstallPosition(*active.guidance));
        });

        if (activeSignature != activeRouteGuidanceId_) {
            activeRouteGuidanceId_ = activeSignature;
            guidanceReplanCursor_ = 0;
            guidanceReplanSeed_ = derivedSeed;
            guidanceReplanPending_ = true;
        } else if (hasVisibilityAnchoredGuidance && !guidanceReplanPending_) {
            guidanceReplanCursor_ = 0;
            guidanceReplanSeed_ = derivedSeed;
            guidanceReplanPending_ = true;
        }

        std::optional<AgentSpatialIndex> localGuidanceIndex;
        if (hasVisibilityAnchoredGuidance && sharedSpatialIndex == nullptr) {
            localGuidanceIndex = buildAgentSpatialIndex(query, entities, 1.0);
        }

        if (hasVisibilityAnchoredGuidance) {
            const auto prioritizedEntities = prioritizedGuidanceEntities(
                query,
                activeGuidances,
                layoutCache,
                sharedSpatialIndex,
                localGuidanceIndex.has_value() ? &(*localGuidanceIndex) : nullptr);
            for (const auto entity : prioritizedEntities) {
                applyRouteGuidanceToEntity(
                    query,
                    entity,
                    layoutCache,
                    activeGuidances,
                    elapsedSeconds,
                    guidanceReplanSeed_);
            }
        }

        if (!guidanceReplanPending_ || guidanceReplanCursor_ >= entities.size()) {
            return;
        }

        const auto endIndex = std::min<std::size_t>(entities.size(), guidanceReplanCursor_ + kGuidanceReplanBudgetPerFrame);
        const auto stableSeed = guidanceReplanSeed_;

        for (std::size_t i = guidanceReplanCursor_; i < endIndex; ++i) {
            applyRouteGuidanceToEntity(query, entities[i], layoutCache, activeGuidances, elapsedSeconds, stableSeed);
        }

        guidanceReplanCursor_ = endIndex;
        if (guidanceReplanCursor_ >= entities.size()) {
            guidanceReplanPending_ = false;
        }
    }

    RoutePlan routePlanToNearestExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId) const {
        RoutePlan plan;
        auto zoneRoute = cachedZoneRouteToNearestExit(layoutCache, start, startZoneId);
        if (!zoneRoute.has_value() || zoneRoute->empty()) {
            return plan;
        }

        plan.destinationZoneId = zoneRoute->zoneIds.back();

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

        for (std::size_t index = 1; index < zoneRoute->zoneIds.size(); ++index) {
            const auto& fromZoneId = zoneRoute->zoneIds[index - 1];
            const auto& toZoneId = zoneRoute->zoneIds[index];
            const auto connectionIndex = zoneRoute->connectionIndices[index - 1];
            if (connectionIndex < layoutCache.layout.connections.size()) {
                const auto* connection = &layoutCache.layout.connections[connectionIndex];
                const auto passage = passageWithClearance(*connection, kCandidateClearance);
                const auto fromFloorId = cachedFloorIdForZone(layoutCache, fromZoneId);
                const auto toFloorId = cachedFloorIdForZone(layoutCache, toZoneId);
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, fromFloorId);
                const auto target = closestPointOnSegment(segmentStart, passage.start, passage.end);
                const auto& segment = cachedPath(fromFloorId, segmentLayout, segmentStart, target, kCandidateClearance);
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

        if (const auto* exitZone = findCachedZone(layoutCache, zoneRoute->zoneIds.back())) {
            const auto exitCenter = polygonCenter(exitZone->area);
            if (distanceBetween(segmentStart, exitCenter) > kArrivalEpsilon) {
                const auto exitFloorId = exitZone->floorId;
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, exitFloorId);
                const auto& segment = cachedPath(exitFloorId, segmentLayout, segmentStart, exitCenter, kCandidateClearance);
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

    RoutePlan routePlanToExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& exitZoneId) const {
        RoutePlan plan;
        const auto zoneRouteResult = cachedZoneRouteToExit(layoutCache, start, startZoneId, exitZoneId);
        if (!zoneRouteResult.has_value() || zoneRouteResult->route.empty()) {
            return plan;
        }

        const auto& zoneRoute = zoneRouteResult->route;
        plan.destinationZoneId = zoneRoute.zoneIds.back();

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

        for (std::size_t index = 1; index < zoneRoute.zoneIds.size(); ++index) {
            const auto& fromZoneId = zoneRoute.zoneIds[index - 1];
            const auto& toZoneId = zoneRoute.zoneIds[index];
            const auto connectionIndex = zoneRoute.connectionIndices[index - 1];
            if (connectionIndex < layoutCache.layout.connections.size()) {
                const auto* connection = &layoutCache.layout.connections[connectionIndex];
                const auto passage = passageWithClearance(*connection, kCandidateClearance);
                const auto fromFloorId = cachedFloorIdForZone(layoutCache, fromZoneId);
                const auto toFloorId = cachedFloorIdForZone(layoutCache, toZoneId);
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, fromFloorId);
                const auto target = closestPointOnSegment(segmentStart, passage.start, passage.end);
                const auto& segment = cachedPath(fromFloorId, segmentLayout, segmentStart, target, kCandidateClearance);
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

        if (const auto* exitZone = findCachedZone(layoutCache, zoneRoute.zoneIds.back())) {
            const auto exitCenter = polygonCenter(exitZone->area);
            if (distanceBetween(segmentStart, exitCenter) > kArrivalEpsilon) {
                const auto exitFloorId = exitZone->floorId;
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, exitFloorId);
                const auto& segment = cachedPath(exitFloorId, segmentLayout, segmentStart, exitCenter, kCandidateClearance);
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

    double hazardRoutePenalty(
        const ScenarioLayoutCacheResource& layoutCache,
        const RoutePlan& plan,
        const Point2D& start,
        const std::string& startFloorId,
        const ScenarioActiveEnvironmentHazardsResource& activeHazards) const {
        double penalty = 0.0;
        for (const auto& hazard : activeHazards.hazards) {
            bool routeTouchesHazard = false;

            Point2D segmentStart = start;
            std::string segmentFloorId = startFloorId;
            for (std::size_t waypointIndex = 0; waypointIndex < plan.waypoints.size(); ++waypointIndex) {
                const auto& segmentEnd = plan.waypoints[waypointIndex];
                if (sameFloor(segmentFloorId, hazard.floorId)
                    && distanceBetween(
                        hazard.draft.position,
                        closestPointOnSegment(hazard.draft.position, segmentStart, segmentEnd)) <= hazard.radiusMeters + 1e-9) {
                    routeTouchesHazard = true;
                    break;
                }

                if (waypointIndex < plan.waypointConnectionIds.size()) {
                    const auto* connection = findConnectionById(layoutCache, plan.waypointConnectionIds[waypointIndex]);
                    if (connection == nullptr) {
                        segmentStart = segmentEnd;
                        if (waypointIndex < plan.waypointFloorIds.size()
                            && !plan.waypointFloorIds[waypointIndex].empty()) {
                            segmentFloorId = plan.waypointFloorIds[waypointIndex];
                        }
                        continue;
                    }
                    const auto connectionFloorId = connection->floorId.empty()
                        ? cachedFloorIdForZone(layoutCache, connection->fromZoneId)
                        : connection->floorId;
                    if (sameFloor(connectionFloorId, hazard.floorId)
                        && distanceBetween(
                            hazard.draft.position,
                            closestPointOnSegment(
                                hazard.draft.position,
                                connection->centerSpan.start,
                                connection->centerSpan.end)) <= hazard.radiusMeters + 1e-9) {
                        routeTouchesHazard = true;
                        break;
                    }
                }

                segmentStart = segmentEnd;
                if (waypointIndex < plan.waypointFloorIds.size()
                    && !plan.waypointFloorIds[waypointIndex].empty()) {
                    segmentFloorId = plan.waypointFloorIds[waypointIndex];
                }
            }

            if (routeTouchesHazard) {
                penalty += hazard.routePenaltyMeters;
            }
        }
        return penalty;
    }

    RoutePlan routePlanToHazardAwareNearestExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& agentFloorId,
        const ScenarioActiveEnvironmentHazardsResource& activeHazards) const {
        std::string bestExitZoneId;
        double bestScore = std::numeric_limits<double>::max();
        double bestDistance = std::numeric_limits<double>::max();

        for (const auto& zone : layoutCache.layout.zones) {
            if (zone.kind != ZoneKind::Exit) {
                continue;
            }

            const auto result = cachedZoneRouteToExit(layoutCache, start, startZoneId, zone.id);
            if (!result.has_value() || result->route.empty()) {
                continue;
            }

            if (result->distance >= bestScore) {
                continue;
            }

            const auto plan = routePlanToExit(layoutCache, start, startZoneId, zone.id);
            if (plan.destinationZoneId.empty()) {
                continue;
            }

            const auto penalty = hazardRoutePenalty(layoutCache, plan, start, agentFloorId, activeHazards);
            const auto score = result->distance + penalty;
            if (score + 1e-9 < bestScore
                || (std::fabs(score - bestScore) <= 1e-9 && result->distance < bestDistance)) {
                bestScore = score;
                bestDistance = result->distance;
                bestExitZoneId = zone.id;
            }
        }

        if (bestExitZoneId.empty()) {
            return {};
        }
        return routePlanToExit(layoutCache, start, startZoneId, bestExitZoneId);
    }

    void replaceRouteWithPlan(EvacuationRoute& route, const RoutePlan& plan, const Point2D& start) const {
        route.destinationZoneId = plan.destinationZoneId;
        route.waypoints = plan.waypoints;
        route.waypointPassages = plan.waypointPassages;
        route.waypointFromZoneIds = plan.waypointFromZoneIds;
        route.waypointZoneIds = plan.waypointZoneIds;
        route.waypointFloorIds = plan.waypointFloorIds;
        route.waypointConnectionIds = plan.waypointConnectionIds;
        route.waypointVerticalTransitions = plan.waypointVerticalTransitions;
        route.nextWaypointIndex = 0;
        route.currentSegmentStart = start;
        route.displayFloorId = route.currentFloorId;
        route.previousDistanceToWaypoint = route.waypoints.empty()
            ? 0.0
            : distanceToRouteWaypoint(route, start);
        route.stalledSeconds = 0.0;
        route.noExitAvailable = false;
        route.holdingForClosure = false;
        route.closureHoldTarget = {};
        route.nextSegmentReplanSeconds = 0.0;
    }

    bool replanAfterVerticalTransition(
        const ScenarioLayoutCacheResource& layoutCache,
        EvacuationRoute& route,
        const Point2D& routeStartPoint,
        const Point2D& planningPoint) const {
        auto startPoint = planningPoint;
        auto startZoneId = zoneAt(layoutCache, startPoint, route.currentFloorId);
        if (startZoneId.empty()) {
            startPoint = routeStartPoint;
            startZoneId = zoneAt(layoutCache, startPoint, route.currentFloorId);
        }
        if (startZoneId.empty()) {
            return false;
        }

        RoutePlan plan;
        if (route.followsGuidance && !route.destinationZoneId.empty()) {
            plan = routePlanToExit(layoutCache, startPoint, startZoneId, route.destinationZoneId);
        }
        if (plan.destinationZoneId.empty()) {
            plan = routePlanToNearestExit(layoutCache, startPoint, startZoneId);
        }
        if (plan.destinationZoneId.empty()) {
            return false;
        }

        replaceRouteWithPlan(route, plan, routeStartPoint);
        return true;
    }

    RouteAdvanceResult advanceRouteWaypoint(
        const ScenarioLayoutCacheResource& layoutCache,
        EvacuationRoute& route,
        const Agent& agent,
        const Point2D& reachedPoint) const {
        if (route.nextWaypointIndex >= route.waypoints.size()) {
            return {.position = reachedPoint, .advanced = false};
        }

        const auto reachedIndex = route.nextWaypointIndex;
        const auto transitionStartPoint = route.currentSegmentStart;
        const auto completedVerticalTransition = reachedIndex < route.waypointVerticalTransitions.size()
            && route.waypointVerticalTransitions[reachedIndex];
        const auto landingPoint = verticalTransitionLandingPoint(
            layoutCache,
            route,
            reachedIndex,
            transitionStartPoint,
            reachedPoint);
        if (!landingPoint.has_value()) {
            return {.position = reachedPoint, .advanced = false};
        }
        const auto advancedPoint = *landingPoint;
        const auto verticalTargetFloorId = verticalTransitionTargetFloorId(layoutCache, route, reachedIndex);
        if (!verticalTargetFloorId.empty()) {
            route.currentFloorId = verticalTargetFloorId;
        } else if (reachedIndex < route.waypointFloorIds.size() && !route.waypointFloorIds[reachedIndex].empty()) {
            route.currentFloorId = route.waypointFloorIds[reachedIndex];
        }
        route.displayFloorId = route.currentFloorId;
        route.currentSegmentStart = advancedPoint;
        ++route.nextWaypointIndex;
        const auto planningPoint = completedVerticalTransition
            ? verticalTransitionPlanningPoint(
                layoutCache,
                route,
                reachedIndex,
                transitionStartPoint,
                advancedPoint,
                static_cast<double>(agent.radius)).value_or(advancedPoint)
            : advancedPoint;
        if (completedVerticalTransition && replanAfterVerticalTransition(layoutCache, route, advancedPoint, planningPoint)) {
            return {.position = advancedPoint, .advanced = true};
        }
        if (route.nextWaypointIndex < route.waypoints.size()) {
            route.previousDistanceToWaypoint =
                distanceToRouteWaypoint(route, route.currentSegmentStart);
        } else {
            route.previousDistanceToWaypoint = 0.0;
        }
        route.stalledSeconds = 0.0;
        route.nextSegmentReplanSeconds = 0.0;
        return {.position = advancedPoint, .advanced = true};
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

            auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
            const auto& velocity = query.get<Velocity>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            while (route.nextWaypointIndex < route.waypoints.size()) {
                const bool verticalTransition = currentWaypointIsVertical(route);
                const bool transitionWaypoint = waypointHasTransition(route);
                const bool passageCrossed = verticalTransition
                    ? verticalPassageCrossed(layoutCache, route, position.value, agent.radius)
                    : routePassageCrossed(cachedLayoutForFloor(layoutCache, route.currentFloorId), route, position.value, agent.radius);
                if (passageCrossed) {
                    const auto advance = advanceRouteWaypoint(layoutCache, route, agent, position.value);
                    position.value = advance.position;
                    if (advance.advanced) {
                        continue;
                    }
                    break;
                }

                const auto target = routeWaypointTarget(route, position.value);
                const auto segment = target - route.currentSegmentStart;
                const auto segmentLengthSquared = dot(segment, segment);
                const auto distance = distanceToRouteWaypoint(route, position.value);

                if (!verticalTransition && !transitionWaypoint && distance <= kArrivalEpsilon) {
                    const auto advance = advanceRouteWaypoint(layoutCache, route, agent, target);
                    position.value = advance.position;
                    if (advance.advanced) {
                        continue;
                    }
                    break;
                }

                if (segmentLengthSquared > 1e-9) {
                    const auto projection = dot(position.value - route.currentSegmentStart, segment);
                    if (!verticalTransition
                        && !transitionWaypoint
                        && projection >= segmentLengthSquared - kWaypointCrossingEpsilon) {
                        const auto advance = advanceRouteWaypoint(layoutCache, route, agent, position.value);
                        position.value = advance.position;
                        if (advance.advanced) {
                            continue;
                        }
                        break;
                    }
                    if (!verticalTransition && shouldAdvanceByBypassingWaypoint(
                            route,
                            position,
                            agent,
                            segment,
                            segmentLengthSquared,
                            projection)) {
                        const auto advance = advanceRouteWaypoint(layoutCache, route, agent, position.value);
                        position.value = advance.position;
                        if (advance.advanced) {
                            continue;
                        }
                        break;
                    }
                }

                if (route.previousDistanceToWaypoint <= 0.0
                    || distance < route.previousDistanceToWaypoint - kWaypointProgressEpsilon) {
                    route.previousDistanceToWaypoint = distance;
                    route.stalledSeconds = 0.0;
                    break;
                }

                if (movingOutOfBarrierKeepout(
                        cachedLayoutForFloor(layoutCache, route.currentFloorId),
                        position,
                        agent,
                        velocity,
                        std::max(
                            effectiveMaxSpeed(layoutCache, agent, route, position.value),
                            static_cast<double>(agent.maxSpeed) * 0.75))) {
                    route.stalledSeconds = 0.0;
                    route.previousDistanceToWaypoint = std::min(route.previousDistanceToWaypoint, distance);
                    break;
                }

                if (deltaSeconds > 0.0) {
                    route.stalledSeconds += deltaSeconds;
                }

                if (!verticalTransition
                    && !transitionWaypoint
                    && route.stalledSeconds >= kWaypointStallSeconds
                    && route.nextWaypointIndex + 1 < route.waypoints.size()
                    && segmentLengthSquared > 1e-9) {
                    const auto projection = dot(position.value - route.currentSegmentStart, segment);
                    if (projection > segmentLengthSquared * 0.45) {
                        const auto advance = advanceRouteWaypoint(layoutCache, route, agent, position.value);
                        position.value = advance.position;
                        if (advance.advanced) {
                            continue;
                        }
                        break;
                    }
                }

                route.previousDistanceToWaypoint = std::min(route.previousDistanceToWaypoint, distance);
                break;
            }
        }
    }

    void advanceVerticalRoutesAtPortal(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache) const {
        for (const auto entity : entities) {
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            while (route.nextWaypointIndex < route.waypoints.size() && currentWaypointIsVertical(route)) {
                if (!verticalPassageCrossed(layoutCache, route, position.value, agent.radius)) {
                    break;
                }

                const auto advance = advanceRouteWaypoint(layoutCache, route, agent, position.value);
                position.value = advance.position;
                if (!advance.advanced) {
                    break;
                }
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

            auto& position = query.get<Position>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            const auto& agent = query.get<Agent>(entity);
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

                if (currentWaypointIsVertical(route)) {
                    break;
                }
                if (matchedIndex == route.nextWaypointIndex
                    && waypointHasTransition(route)
                    && !routePassageCrossed(
                        cachedLayoutForFloor(layoutCache, route.currentFloorId),
                        route,
                        position.value,
                        agent.radius)) {
                    break;
                }
                while (route.nextWaypointIndex <= matchedIndex && route.nextWaypointIndex < route.waypoints.size()) {
                    const auto advance = advanceRouteWaypoint(layoutCache, route, agent, position.value);
                    position.value = advance.position;
                    if (!advance.advanced) {
                        break;
                    }
                }
            }
        }
    }

    void replanHazardAwareExitRoutes(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache,
        double elapsedSeconds,
        const ScenarioEnvironmentReactionResource* reactions,
        const ScenarioActiveEnvironmentHazardsResource* activeHazards) const {
        if (reactions == nullptr || activeHazards == nullptr || activeHazards->hazards.empty()) {
            return;
        }

        for (const auto entity : entities) {
            const auto* hazardState = activeHazardState(reactions, entity.index);
            if (hazardState == nullptr) {
                continue;
            }

            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            const auto& position = query.get<Position>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            if (elapsedSeconds + 1e-9 < route.nextExitReplanSeconds) {
                continue;
            }

            const auto startZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
            if (startZoneId.empty()) {
                route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
                continue;
            }

            const auto agentFloorId = !route.displayFloorId.empty() ? route.displayFloorId : route.currentFloorId;
            const auto plan =
                routePlanToHazardAwareNearestExit(layoutCache, position.value, startZoneId, agentFloorId, *activeHazards);
            if (plan.destinationZoneId.empty()) {
                route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
                continue;
            }
            if (plan.destinationZoneId == route.destinationZoneId && !route.waypoints.empty() && !route.noExitAvailable) {
                route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
                continue;
            }

            replaceRouteWithPlan(route, plan, position.value);
            route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
        }
    }

    bool closureReadyForBlockedConnection(
        ScenarioEnvironmentReactionResource* reactions,
        engine::Entity entity,
        const Agent& agent,
        const Connection2D& blockedConnection,
        double elapsedSeconds) const {
        if (reactions == nullptr) {
            return true;
        }

        auto& state = reactions->agentsById[entity.index];
        if (!state.closureDetected || state.blockedConnectionId != blockedConnection.id) {
            state.closureDetected = true;
            state.closureAware = false;
            state.blockedConnectionId = blockedConnection.id;
            state.closureDetectedAtSeconds = elapsedSeconds;
            state.closureReactionReadySeconds = elapsedSeconds + std::max(0.0, agent.closurePatienceSeconds);
        }

        state.closureAware = elapsedSeconds + 1e-9 >= state.closureReactionReadySeconds;
        return state.closureAware;
    }

    Point2D closureHoldTarget(
        const ScenarioLayoutCacheResource& layoutCache,
        const std::string& zoneId,
        const Point2D& currentPosition,
        engine::Entity entity,
        double clearance) const {
        const auto* zone = findCachedZone(layoutCache, zoneId);
        if (zone == nullptr) {
            return currentPosition;
        }

        auto center = representativePointInPolygon(zone->area).value_or(polygonCenter(zone->area));
        if (!pointInPolygon(zone->area, center)) {
            center = currentPosition;
        }

        constexpr double kTwoPi = 6.28318530717958647692;
        constexpr double kGoldenAngle = 2.39996322972865332223;
        const auto baseAngle =
            std::fmod(static_cast<double>((entity.index % 997U) + 1U) * kGoldenAngle, kTwoPi);
        const auto boundaryRadius =
            std::max(0.0, distanceToPolygonBoundary(zone->area, center) - clearance - 0.05);
        const auto radiusScale = 0.35 + (static_cast<double>(entity.index % 5U) * 0.10);

        for (int attempt = 0; attempt < 16; ++attempt) {
            const auto angle = baseAngle + (static_cast<double>(attempt) * 0.71);
            const auto candidateRadius =
                boundaryRadius * std::max(0.15, radiusScale - (static_cast<double>(attempt) * 0.04));
            const Point2D candidate{
                .x = center.x + (std::cos(angle) * candidateRadius),
                .y = center.y + (std::sin(angle) * candidateRadius),
            };
            if (pointInPolygon(zone->area, candidate)
                && distanceToPolygonBoundary(zone->area, candidate) + 1e-9 >= clearance * 0.75) {
                return candidate;
            }
        }

        if (pointInPolygon(zone->area, center)
            && distanceToPolygonBoundary(zone->area, center) + 1e-9 >= clearance * 0.5) {
            return center;
        }
        return currentPosition;
    }

    void replaceRouteWithClosureHold(
        EvacuationRoute& route,
        const Point2D& position,
        const Point2D& holdTarget) const {
        route.destinationZoneId.clear();
        route.waypoints.clear();
        route.waypointPassages.clear();
        route.waypointFromZoneIds.clear();
        route.waypointZoneIds.clear();
        route.waypointFloorIds.clear();
        route.waypointConnectionIds.clear();
        route.waypointVerticalTransitions.clear();
        if (distanceBetween(position, holdTarget) > kArrivalEpsilon) {
            route.waypoints.push_back(holdTarget);
            route.waypointPassages.push_back(pointPassage(holdTarget));
            route.waypointFromZoneIds.push_back({});
            route.waypointZoneIds.push_back({});
            route.waypointFloorIds.push_back(route.currentFloorId);
            route.waypointConnectionIds.push_back({});
            route.waypointVerticalTransitions.push_back(false);
        }
        route.nextWaypointIndex = 0;
        route.currentSegmentStart = position;
        route.displayFloorId = route.currentFloorId;
        route.previousDistanceToWaypoint = route.waypoints.empty()
            ? 0.0
            : distanceToRouteWaypoint(route, position);
        route.stalledSeconds = 0.0;
        route.noExitAvailable = true;
        route.holdingForClosure = true;
        route.closureHoldTarget = holdTarget;
        route.nextSegmentReplanSeconds = 0.0;
    }

    static bool routePlanUsesConnection(const RoutePlan& plan, const std::string& connectionId) {
        if (connectionId.empty()) {
            return false;
        }
        return std::any_of(plan.waypointConnectionIds.begin(), plan.waypointConnectionIds.end(), [&](const auto& id) {
            return id == connectionId;
        });
    }

    void replanBlockedExitRoutes(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache,
        double elapsedSeconds,
        std::uint64_t layoutRevision,
        ScenarioEnvironmentReactionResource* reactions) const {
        for (const auto entity : entities) {
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            auto& route = query.get<EvacuationRoute>(entity);
            const auto& agent = query.get<Agent>(entity);
            if (layoutRevision != route.observedLayoutRevision) {
                route.observedLayoutRevision = layoutRevision;
                route.nextExitReplanSeconds = 0.0;
                route.nextSegmentReplanSeconds = 0.0;
            }

            const auto* blockedConnection = nextBlockedConnection(layoutCache, route);
            if (blockedConnection == nullptr) {
                clearClosureReaction(reactions, entity.index);
            }
            if (blockedConnection == nullptr && !route.noExitAvailable) {
                continue;
            }
            if (elapsedSeconds + 1e-9 < route.nextExitReplanSeconds) {
                continue;
            }

            if (blockedConnection != nullptr
                && !closureReadyForBlockedConnection(reactions, entity, agent, *blockedConnection, elapsedSeconds)) {
                auto retryAt = elapsedSeconds + kExitReplanCooldownSeconds;
                if (reactions != nullptr) {
                    const auto stateIt = reactions->agentsById.find(entity.index);
                    if (stateIt != reactions->agentsById.end()) {
                        retryAt = std::min(retryAt, stateIt->second.closureReactionReadySeconds);
                    }
                }
                route.nextExitReplanSeconds = retryAt;
                continue;
            }

            const auto& position = query.get<Position>(entity);
            const auto startZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
            if (startZoneId.empty()) {
                route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
                continue;
            }

            RoutePlan plan;
            if (route.followsGuidance && !route.destinationZoneId.empty()) {
                plan = routePlanToExit(layoutCache, position.value, startZoneId, route.destinationZoneId);
            }
            if (plan.destinationZoneId.empty()) {
                plan = routePlanToNearestExit(layoutCache, position.value, startZoneId);
            }
            if (blockedConnection != nullptr && routePlanUsesConnection(plan, blockedConnection->id)) {
                plan = {};
            }
            if (plan.destinationZoneId.empty()) {
                const auto holdTarget = closureHoldTarget(
                    layoutCache,
                    startZoneId,
                    position.value,
                    entity,
                    static_cast<double>(agent.radius) + kPathClearance);
                replaceRouteWithClosureHold(route, position.value, holdTarget);
                route.nextExitReplanSeconds = elapsedSeconds + kNoExitReplanCooldownSeconds;
                continue;
            }

            replaceRouteWithPlan(route, plan, position.value);
            clearClosureReaction(reactions, entity.index);
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

            const auto& replacement = cachedPath(route.currentFloorId, floorLayout, position.value, target, clearance);
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
                    const auto& secondRoute = query.get<EvacuationRoute>(second);
                    if (agentCollisionFloorId(secondRoute) != firstCollisionFloorId) {
                        continue;
                    }
                    const auto delta = firstPosition.value - secondPosition.value;
                    const auto distance = lengthOf(delta);
                    const auto minimumDistance = static_cast<double>(firstAgent.radius + secondAgent.radius);
                    if (distance >= minimumDistance) {
                        continue;
                    }

                    const auto direction = normalizedOr(delta, deterministicFallbackDirection(first));
                    const auto push = std::min(0.08, (minimumDistance - distance) * 0.35);
                    const auto firstTarget = firstPosition.value + (direction * push);
                    const auto secondTarget = secondPosition.value - (direction * push);
                    firstPosition.value = constrainedMoveForCurrentWaypoint(
                        layoutCache,
                        firstRoute,
                        firstPosition.value,
                        firstTarget,
                        static_cast<double>(firstAgent.radius));
                    secondPosition.value = constrainedMoveForCurrentWaypoint(
                        layoutCache,
                        secondRoute,
                        secondPosition.value,
                        secondTarget,
                        static_cast<double>(secondAgent.radius));
                }
            }
        }
    }

    void advanceClock(
        engine::WorldQuery& query,
        ScenarioSimulationClockResource& clock,
        const std::vector<engine::Entity>& entities,
        double deltaSeconds,
        std::size_t pendingScheduledSpawns) const {
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
        clock.complete = pendingScheduledSpawns == 0 && totalAgentCount > 0 && evacuatedAgentCount >= totalAgentCount;
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
        const auto maxSpeed = static_cast<double>(agent.maxSpeed);
        return inStairZone || onVerticalTransition ? std::min(maxSpeed, kStairAgentSpeed) : maxSpeed;
    }

    void updateDisplayFloor(EvacuationRoute& route, const Point2D& position) const {
        (void)position;
        route.displayFloorId = route.currentFloorId;
    }

    std::optional<ScenarioLayoutCacheResource> layoutCache_{};
    std::vector<RouteGuidanceDraft> routeGuidances_{};
    std::string activeRouteGuidanceId_{};
    bool guidanceReplanPending_{false};
    std::size_t guidanceReplanCursor_{0};
    std::uint64_t guidanceReplanSeed_{0U};
    std::vector<engine::Entity> activeEntities_{};
    mutable std::uint64_t planningCacheRevision_{std::numeric_limits<std::uint64_t>::max()};
    mutable std::unordered_map<std::string, std::optional<ZoneRouteToExit>> nearestExitRouteCache_{};
    mutable std::unordered_map<std::string, std::optional<ZoneRouteResult>> exitRouteCache_{};
    mutable std::unordered_map<std::string, std::vector<Point2D>> pathCache_{};
};




}  // namespace

std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem() {
    return std::make_unique<ScenarioSimulationMotionSystem>();
}

std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem(FacilityLayout2D layout) {
    return std::make_unique<ScenarioSimulationMotionSystem>(std::move(layout));
}

std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem(
    FacilityLayout2D layout,
    std::vector<RouteGuidanceDraft> routeGuidances) {
    return std::make_unique<ScenarioSimulationMotionSystem>(std::move(layout), std::move(routeGuidances));
}

}  // namespace safecrowd::domain
