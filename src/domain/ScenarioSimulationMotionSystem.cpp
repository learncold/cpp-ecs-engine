#include "domain/ScenarioSimulationSystems.h"
#include "domain/ScenarioSimulationRouteGuidance.h"

#include "domain/GeometryQueries.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
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

    ScenarioSimulationMotionSystem(FacilityLayout2D layout, std::vector<RouteGuidanceDraft> routeGuidances)
        : layoutCache_(buildScenarioLayoutCache(std::move(layout))),
          routeGuidance_(std::move(routeGuidances)) {
    }

    ScenarioSimulationMotionSystem(
        FacilityLayout2D layout,
        std::vector<RouteGuidanceDraft> routeGuidances,
        ScenarioWayfindingMode wayfindingMode)
        : layoutCache_(buildScenarioLayoutCache(std::move(layout))),
          routeGuidance_(std::move(routeGuidances)),
          wayfindingMode_(wayfindingMode) {
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
        routeGuidance_.refreshPlanningCache(layoutRevision);
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

        if (wayfindingMode_ == ScenarioWayfindingMode::FullKnowledge) {
            routeGuidance_.apply(
                query,
                entities,
                layoutCache,
                clock.elapsedSeconds,
                step.derivedSeed,
                reactions,
                activeHazards,
                sharedSpatialIndex);
        }
        advanceRoutesForCurrentZones(query, activeEntities_, layoutCache);
        if (wayfindingMode_ == ScenarioWayfindingMode::FullKnowledge) {
            replanBlockedExitRoutes(query, activeEntities_, layoutCache, clock.elapsedSeconds, layoutRevision, reactions);
        }
        advanceRoutesForWaypointProgress(
            query,
            0.0,
            activeEntities_,
            layoutCache);
        replanBlockedRouteSegments(query, activeEntities_, layoutCache, clock.elapsedSeconds, layoutRevision);
        if (wayfindingMode_ == ScenarioWayfindingMode::FullKnowledge) {
            replanHazardAwareExitRoutes(query, activeEntities_, layoutCache, clock.elapsedSeconds, reactions, activeHazards);
        }
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
            const auto currentZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
            const auto* currentZone = findCachedZone(layoutCache, currentZoneId);
            if (currentZone != nullptr && currentZone->kind == ZoneKind::Exit) {
                status.evacuated = true;
                status.completionTimeSeconds = clock.elapsedSeconds;
                status.exitZoneId = currentZone->id;
                velocity.value = {};
                ++newlyEvacuatedCount;
                continue;
            }

            if (route.destinationZoneId.empty()) {
                continue;
            }

            const auto& floorLayout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
            const auto* destinationZone = findZone(floorLayout, route.destinationZoneId);
            if (destinationZone != nullptr
                && destinationZone->kind == ZoneKind::Exit
                && pointInRing(destinationZone->area.outline, position.value)) {
                status.evacuated = true;
                status.completionTimeSeconds = clock.elapsedSeconds;
                status.exitZoneId = destinationZone->id;
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
                const auto advance = advanceRouteWaypoint(
                    layoutCache,
                    route,
                    agent,
                    target);
                position.value = advance.position;
                velocity.value = {};
                continue;
            }
            if (verticalTransition && distance <= kGeometryEpsilon) {
                velocity.value = {};
                continue;
            }

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
            const auto& physicsLayout = cachedLayoutForFloor(layoutCache, collisionFloorId);
            const auto neighborCandidates = nearbyAgentsForRoute(
                query,
                sharedSpatialIndex,
                localNeighborIndex ? &(*localNeighborIndex) : nullptr,
                position.value,
                route,
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
            const auto barrierVelocity = !route.physicsFloorId.empty()
                ? barrierSeparationVelocity(physicsLayout, position, agent, barrierReferenceSpeed)
                : (sharedSpatialIndex != nullptr
                ? barrierSeparationVelocity(
                    scenarioNearbyBarriers(
                        layoutCache.layout,
                        *sharedSpatialIndex,
                        position.value,
                        collisionFloorId,
                        static_cast<double>(agent.radius) + kBarrierAvoidanceBuffer),
                    position,
                    agent,
                    barrierReferenceSpeed)
                : barrierSeparationVelocity(physicsLayout, position, agent, barrierReferenceSpeed));
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

        advanceVerticalRoutesAtPortal(
            query,
            activeEntities_,
            layoutCache);
        updateAgentPhysicsFloorIds(query, layoutCache, activeEntities_);
        resolveAgentOverlaps(query, activeEntities_, layoutCache);
        advanceRoutesForCurrentZones(query, activeEntities_, layoutCache);
        advanceRoutesForWaypointProgress(
            query,
            clampedDelta,
            activeEntities_,
            layoutCache);
        updateAgentPhysicsFloorIds(query, layoutCache, activeEntities_);
        resolveAgentOverlaps(query, activeEntities_, layoutCache);
        advanceVerticalRoutesAtPortal(
            query,
            activeEntities_,
            layoutCache);
        updateAgentPhysicsFloorIds(query, layoutCache, activeEntities_);
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
    static constexpr double kHazardSafeRoutePenaltyMeters = 1.0;
    static constexpr double kHazardRoutePenaltyToleranceMeters = 1.0;
    static constexpr double kHazardRouteOppositionDotThreshold = -0.25;

    struct RouteAdvanceResult {
        Point2D position{};
        bool advanced{false};
    };

    static void appendUniqueEntities(std::vector<engine::Entity>& target, const std::vector<engine::Entity>& source) {
        for (const auto entity : source) {
            if (std::find(target.begin(), target.end(), entity) == target.end()) {
                target.push_back(entity);
            }
        }
    }

    std::vector<engine::Entity> nearbyAgentsForRoute(
        engine::WorldQuery& query,
        const ScenarioAgentSpatialIndexResource* sharedSpatialIndex,
        const AgentSpatialIndex* localSpatialIndex,
        const Point2D& position,
        const EvacuationRoute& route,
        double radius) const {
        std::vector<engine::Entity> candidates;
        const auto collisionFloorId = agentCollisionFloorId(route);
        const auto displayFloorId = agentDisplayFloorId(route);
        if (sharedSpatialIndex != nullptr) {
            candidates = scenarioNearbyAgents(query, *sharedSpatialIndex, position, collisionFloorId, radius);
            appendUniqueEntities(
                candidates,
                scenarioNearbyDisplayAgents(query, *sharedSpatialIndex, position, displayFloorId, radius));
            return candidates;
        }
        if (localSpatialIndex == nullptr) {
            return candidates;
        }

        candidates = nearbyAgents(query, *localSpatialIndex, position, collisionFloorId, radius);
        appendUniqueEntities(
            candidates,
            nearbyDisplayAgents(query, *localSpatialIndex, position, displayFloorId, radius));
        return candidates;
    }

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

    bool hazardAvoidanceOpposesCurrentRoute(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route,
        const Agent& agent,
        const Point2D& position,
        const ScenarioEnvironmentReactionAgentState& state) const {
        if (route.nextWaypointIndex >= route.waypoints.size()) {
            return false;
        }

        const auto target = movementTargetForCurrentWaypoint(
            layoutCache,
            route,
            position,
            static_cast<double>(agent.radius));
        const auto distance = distanceBetween(position, target);
        if (distance <= kArrivalEpsilon) {
            return false;
        }

        const auto routeDirection = (target - position) * (1.0 / distance);
        const auto away = normalizedOr(position - state.hazardPosition, perpendicularLeft(routeDirection));
        return dot(away, routeDirection) <= kHazardRouteOppositionDotThreshold;
    }

    static bool routePlanDiffersFromRoute(const EvacuationRoute& route, const ScenarioRoutePlan& plan) {
        if (route.destinationZoneId != plan.destinationZoneId) {
            return true;
        }

        const auto remainingWaypointCount = route.nextWaypointIndex >= route.waypoints.size()
            ? std::size_t{0}
            : route.waypoints.size() - route.nextWaypointIndex;
        if (remainingWaypointCount != plan.waypoints.size()) {
            return true;
        }

        for (std::size_t index = 0; index < plan.waypoints.size(); ++index) {
            const auto routeIndex = route.nextWaypointIndex + index;
            if (distanceBetween(route.waypoints[routeIndex], plan.waypoints[index]) > 1e-6) {
                return true;
            }
            const auto routeConnectionId = routeIndex < route.waypointConnectionIds.size()
                ? route.waypointConnectionIds[routeIndex]
                : std::string{};
            const auto planConnectionId = index < plan.waypointConnectionIds.size()
                ? plan.waypointConnectionIds[index]
                : std::string{};
            if (routeConnectionId != planConnectionId) {
                return true;
            }
        }
        return false;
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
        if (currentWaypointIsVertical(route)
            && route.nextWaypointIndex < route.waypointPassages.size()) {
            const auto& passage = route.waypointPassages[route.nextWaypointIndex];
            if (lengthSquaredOf(passage) > 1e-9) {
                const auto normal = passageNormalTowardCurrentWaypoint(layoutCache, route);
                if (normal.has_value()) {
                    const auto portalTarget = closestPointOnSegment(position, passage.start, passage.end);
                    const auto offset = std::max(kPortalCrossingEpsilon * 2.0, clearance * 0.20);
                    return portalTarget + (*normal * offset);
                }
            }
        }

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
            return pointInRing(endpointZone->area.outline, candidate)
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
        const auto& layout = cachedLayoutForFloor(
            layoutCache,
            !route.physicsFloorId.empty() && !currentWaypointIsVertical(route)
                ? route.currentFloorId
                : agentCollisionFloorId(route));
        const auto constrained = constrainedMove(layout, from, to, clearance);
        if (distanceBetween(constrained, from) > kGeometryEpsilon) {
            return constrained;
        }
        if (route.physicsFloorId.empty() && !currentWaypointIsVertical(route)) {
            return constrainedMoveOutOfVerticalPortalContact(layoutCache, route, from, to, clearance);
        }
        return constrained;
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

        const auto landingOnPassage = closestPointOnSegment(reachedPoint, passage.start, passage.end);
        const auto portalDistance = dot(reachedPoint - midpoint(passage), *normal);
        if (portalDistance > kGeometryEpsilon) {
            return landingOnPassage + (*normal * kGeometryEpsilon);
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
            return pointInRing(toZone->area.outline, candidate)
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

    bool replanAfterVerticalTransition(
        const ScenarioLayoutCacheResource& layoutCache,
        EvacuationRoute& route,
        const Point2D& routeStartPoint,
        const Point2D& planningPoint) const {
        if (wayfindingMode_ == ScenarioWayfindingMode::LocalWayfinding) {
            return false;
        }

        auto startPoint = planningPoint;
        auto startZoneId = zoneAt(layoutCache, startPoint, route.currentFloorId);
        if (startZoneId.empty()) {
            startPoint = routeStartPoint;
            startZoneId = zoneAt(layoutCache, startPoint, route.currentFloorId);
        }
        if (startZoneId.empty()) {
            return false;
        }

        ScenarioRoutePlan plan;
        if (route.followsGuidance && !route.destinationZoneId.empty()) {
            plan = routeGuidance_.routePlanToExit(layoutCache, startPoint, startZoneId, route.destinationZoneId);
        }
        if (plan.destinationZoneId.empty()) {
            plan = routeGuidance_.routePlanToNearestExit(layoutCache, startPoint, startZoneId);
        }
        if (plan.destinationZoneId.empty()) {
            return false;
        }

        routeGuidance_.replaceRouteWithPlan(route, plan, routeStartPoint);
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
        auto advancedPoint = *landingPoint;
        const auto planningPoint = completedVerticalTransition
            ? verticalTransitionPlanningPoint(
                layoutCache,
                route,
                reachedIndex,
                transitionStartPoint,
                advancedPoint,
                static_cast<double>(agent.radius)).value_or(advancedPoint)
            : advancedPoint;
        if (completedVerticalTransition && wayfindingMode_ == ScenarioWayfindingMode::LocalWayfinding) {
            advancedPoint = planningPoint;
        }
        const auto verticalTargetFloorId = verticalTransitionTargetFloorId(layoutCache, route, reachedIndex);
        if (!verticalTargetFloorId.empty()) {
            route.currentFloorId = verticalTargetFloorId;
        } else if (reachedIndex < route.waypointFloorIds.size() && !route.waypointFloorIds[reachedIndex].empty()) {
            route.currentFloorId = route.waypointFloorIds[reachedIndex];
        }
        route.displayFloorId = route.currentFloorId;
        route.currentSegmentStart = advancedPoint;
        ++route.nextWaypointIndex;
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
                    const auto advance = advanceRouteWaypoint(
                        layoutCache,
                        route,
                        agent,
                        position.value);
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
                    const auto advance = advanceRouteWaypoint(
                        layoutCache,
                        route,
                        agent,
                        target);
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
                        const auto advance = advanceRouteWaypoint(
                            layoutCache,
                            route,
                            agent,
                            position.value);
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
                        const auto advance = advanceRouteWaypoint(
                            layoutCache,
                            route,
                            agent,
                            position.value);
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
                        const auto advance = advanceRouteWaypoint(
                            layoutCache,
                            route,
                            agent,
                            position.value);
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

                const auto advance = advanceRouteWaypoint(
                    layoutCache,
                    route,
                    agent,
                    position.value);
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
                    const auto advance = advanceRouteWaypoint(
                        layoutCache,
                        route,
                        agent,
                        position.value);
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

        if (entities.empty()) {
            hazardReplanCursor_ = 0;
            return;
        }

        constexpr std::size_t kHazardReplanEntityBudgetPerFrame = 50;
        if (hazardReplanCursor_ >= entities.size()) {
            hazardReplanCursor_ = 0;
        }
        const auto startCursor = hazardReplanCursor_;
        const auto visitCount = std::min<std::size_t>(entities.size(), kHazardReplanEntityBudgetPerFrame);
        for (std::size_t offset = 0; offset < visitCount; ++offset) {
            const auto entity = entities[(startCursor + offset) % entities.size()];
            const auto* hazardState = activeHazardState(reactions, entity.index);
            if (hazardState == nullptr) {
                continue;
            }

            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            const auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
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
            const auto currentPenalty = routeGuidance_.remainingRouteHazardPenalty(
                layoutCache,
                route,
                position.value,
                agentFloorId,
                *activeHazards);
            const auto currentRouteDifficult =
                route.stalledSeconds >= kWaypointStallSeconds
                && (currentPenalty > kHazardSafeRoutePenaltyMeters
                    || hazardAvoidanceOpposesCurrentRoute(layoutCache, route, agent, position.value, *hazardState));

            auto plan =
                routeGuidance_.routePlanToHazardAwareNearestExit(layoutCache, position.value, startZoneId, agentFloorId, *activeHazards);
            if (plan.destinationZoneId.empty()) {
                route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
                continue;
            }
            auto planPenalty = routeGuidance_.hazardRoutePenalty(layoutCache, plan, position.value, agentFloorId, *activeHazards);
            if (currentRouteDifficult
                && plan.destinationZoneId == route.destinationZoneId
                && (!routePlanDiffersFromRoute(route, plan)
                    || planPenalty > currentPenalty + kHazardRoutePenaltyToleranceMeters)) {
                auto alternatePlan = routeGuidance_.routePlanToHazardAwareNearestExitExcluding(
                    layoutCache,
                    position.value,
                    startZoneId,
                    agentFloorId,
                    *activeHazards,
                    route.destinationZoneId);
                if (!alternatePlan.destinationZoneId.empty()) {
                    const auto alternatePenalty = routeGuidance_.hazardRoutePenalty(
                        layoutCache,
                        alternatePlan,
                        position.value,
                        agentFloorId,
                        *activeHazards);
                    if (alternatePenalty <= currentPenalty + kHazardRoutePenaltyToleranceMeters
                        || alternatePenalty + kHazardRoutePenaltyToleranceMeters < planPenalty) {
                        plan = std::move(alternatePlan);
                        planPenalty = alternatePenalty;
                    }
                }
            }

            if (plan.destinationZoneId == route.destinationZoneId && !route.waypoints.empty() && !route.noExitAvailable) {
                const auto replacesDifficultRoute =
                    currentRouteDifficult
                    && routePlanDiffersFromRoute(route, plan)
                    && planPenalty <= currentPenalty + kHazardRoutePenaltyToleranceMeters;
                if (replacesDifficultRoute) {
                    routeGuidance_.replaceRouteWithPlan(route, plan, position.value);
                    route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
                    continue;
                }
                route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
                continue;
            }

            routeGuidance_.replaceRouteWithPlan(route, plan, position.value);
            route.nextExitReplanSeconds = elapsedSeconds + kExitReplanCooldownSeconds;
        }
        hazardReplanCursor_ = (startCursor + visitCount) % entities.size();
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

            ScenarioRoutePlan plan;
            if (route.followsGuidance && !route.destinationZoneId.empty()) {
                plan = routeGuidance_.routePlanToExit(layoutCache, position.value, startZoneId, route.destinationZoneId);
            }
            if (plan.destinationZoneId.empty()) {
                plan = routeGuidance_.routePlanToNearestExit(layoutCache, position.value, startZoneId);
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

            routeGuidance_.replaceRouteWithPlan(route, plan, position.value);
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

            const auto& replacement = routeGuidance_.cachedPath(route.currentFloorId, floorLayout, position.value, target, clearance);
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
        std::unordered_set<unsigned long long> checkedPairs;
        checkedPairs.reserve(entities.size() * 4);
        for (int iteration = 0; iteration < kOverlapRelaxationIterations; ++iteration) {
            const auto spatialIndex = buildAgentSpatialIndex(query, entities, 1.0);
            checkedPairs.clear();
            bool resolvedAnyOverlap = false;
            for (const auto first : entities) {
                auto& firstStatus = query.get<EvacuationStatus>(first);
                if (firstStatus.evacuated) {
                    continue;
                }

                auto& firstPosition = query.get<Position>(first);
                const auto& firstAgent = query.get<Agent>(first);
                const auto& firstRoute = query.get<EvacuationRoute>(first);
                const auto candidates = nearbyAgentsForRoute(
                    query,
                    nullptr,
                    &spatialIndex,
                    firstPosition.value,
                    firstRoute,
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
                    if (!agentCollisionScopesOverlap(secondRoute, firstRoute)) {
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
                    const auto firstNext = constrainedMoveForCurrentWaypoint(
                        layoutCache,
                        firstRoute,
                        firstPosition.value,
                        firstTarget,
                        static_cast<double>(firstAgent.radius));
                    const auto secondNext = constrainedMoveForCurrentWaypoint(
                        layoutCache,
                        secondRoute,
                        secondPosition.value,
                        secondTarget,
                        static_cast<double>(secondAgent.radius));
                    if (distanceBetween(firstNext, firstPosition.value) > kGeometryEpsilon
                        || distanceBetween(secondNext, secondPosition.value) > kGeometryEpsilon) {
                        resolvedAnyOverlap = true;
                    }
                    firstPosition.value = firstNext;
                    secondPosition.value = secondNext;
                }
            }
            if (!resolvedAnyOverlap) {
                break;
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
    ScenarioRouteGuidanceController routeGuidance_{};
    ScenarioWayfindingMode wayfindingMode_{ScenarioWayfindingMode::FullKnowledge};
    std::vector<engine::Entity> activeEntities_{};
    mutable std::size_t hazardReplanCursor_{0};
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

std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem(
    FacilityLayout2D layout,
    std::vector<RouteGuidanceDraft> routeGuidances,
    ScenarioWayfindingMode wayfindingMode) {
    return std::make_unique<ScenarioSimulationMotionSystem>(
        std::move(layout),
        std::move(routeGuidances),
        wayfindingMode);
}

}  // namespace safecrowd::domain
