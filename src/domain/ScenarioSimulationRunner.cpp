#include "domain/ScenarioSimulationRunner.h"

#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <deque>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "engine/TriggerPolicy.h"
#include "engine/UpdatePhase.h"

namespace safecrowd::domain {
using namespace simulation_internal;

ScenarioSimulationRunner::ScenarioSimulationRunner(FacilityLayout2D layout, ScenarioDraft scenario) {
    reset(std::move(layout), std::move(scenario));
}

void ScenarioSimulationRunner::reset(FacilityLayout2D layout, ScenarioDraft scenario) {
    layout_ = std::move(layout);
    layoutCache_ = buildScenarioLayoutCache(layout_);
    scenario_ = std::move(scenario);
    frame_ = {};
    riskSnapshot_ = {};
    resultRiskSnapshot_ = {};
    resultArtifacts_ = {};
    timeLimitSeconds_ = scenario_.execution.timeLimitSeconds > 0.0
        ? scenario_.execution.timeLimitSeconds
        : kDefaultTimeLimitSeconds;
    initializeRuntime();
}

void ScenarioSimulationRunner::step(double deltaSeconds) {
    if (runtime_ == nullptr || frame_.complete || deltaSeconds <= 0.0) {
        return;
    }

    const auto remaining = std::max(0.0, timeLimitSeconds_ - frame_.elapsedSeconds);
    const auto clampedDelta = std::min(deltaSeconds, remaining);
    auto& resources = runtime_->world().resources();
    auto& clock = resources.get<ScenarioSimulationClockResource>();
    if (clampedDelta <= 0.0) {
        clock.complete = true;
    } else {
        resources.set(ScenarioSimulationStepResource{.deltaSeconds = clampedDelta});
    }
    runtime_->stepFrame(0.0);
    syncFrameFromRuntime();
}

const SimulationFrame& ScenarioSimulationRunner::frame() const noexcept {
    return frame_;
}

const ScenarioRiskSnapshot& ScenarioSimulationRunner::riskSnapshot() const noexcept {
    return riskSnapshot_;
}

const ScenarioRiskSnapshot& ScenarioSimulationRunner::resultRiskSnapshot() const noexcept {
    return resultRiskSnapshot_;
}

const ScenarioResultArtifacts& ScenarioSimulationRunner::resultArtifacts() const noexcept {
    return resultArtifacts_;
}

double ScenarioSimulationRunner::timeLimitSeconds() const noexcept {
    return timeLimitSeconds_;
}

bool ScenarioSimulationRunner::complete() const noexcept {
    return frame_.complete;
}

std::vector<ScenarioAgentSeed> ScenarioSimulationRunner::createAgentSeeds() const {
    std::vector<ScenarioAgentSeed> seeds;
    for (const auto& placement : scenario_.population.initialPlacements) {
        const auto count = placement.explicitPositions.empty()
            ? placement.targetAgentCount
            : placement.explicitPositions.size();
        seeds.reserve(seeds.size() + count);
        for (std::size_t index = 0; index < count; ++index) {
            const auto position = placementPoint(placement, index);
            auto placementFloorId = placement.floorId;
            if (placementFloorId.empty() && !placement.zoneId.empty()) {
                placementFloorId = cachedFloorIdForZone(layoutCache_, placement.zoneId);
            }
            auto startZoneId = placement.zoneId;
            if (!startZoneId.empty() && !placementFloorId.empty()) {
                const auto zoneFloorId = cachedFloorIdForZone(layoutCache_, startZoneId);
                if (!zoneFloorId.empty() && zoneFloorId != placementFloorId) {
                    startZoneId.clear();
                }
            }
            if (startZoneId.empty()) {
                startZoneId = zoneAt(position, placementFloorId);
            }
            if (placementFloorId.empty()) {
                placementFloorId = cachedFloorIdForZone(layoutCache_, startZoneId);
            }
            const auto route = routePlan(position, startZoneId);
            const auto speed = speedOf(placement.initialVelocity);
            auto evacuationRoute = EvacuationRoute{
                .waypoints = route.waypoints,
                .waypointPassages = route.waypointPassages,
                .waypointFromZoneIds = route.waypointFromZoneIds,
                .waypointZoneIds = route.waypointZoneIds,
                .waypointFloorIds = route.waypointFloorIds,
                .waypointConnectionIds = route.waypointConnectionIds,
                .waypointVerticalTransitions = route.waypointVerticalTransitions,
                .nextWaypointIndex = 0,
                .currentSegmentStart = position,
                .previousDistanceToWaypoint = 0.0,
                .stalledSeconds = 0.0,
                .destinationZoneId = route.destinationZoneId,
                .currentFloorId = placementFloorId,
            };
            evacuationRoute.displayFloorId = evacuationRoute.currentFloorId;
            evacuationRoute.previousDistanceToWaypoint = route.waypoints.empty()
                ? 0.0
                : distanceToRouteWaypoint(evacuationRoute, position);
            seeds.push_back({
                .position = {.value = position},
                .agent = {
                    .radius = static_cast<float>(kDefaultAgentRadius),
                    .maxSpeed = static_cast<float>(speed),
                    .sourcePlacementId = placement.id,
                    .sourceZoneId = startZoneId,
                },
                .velocity = {.value = {}},
                .route = std::move(evacuationRoute),
                .status = {},
            });
        }
    }
    return seeds;
}

void ScenarioSimulationRunner::initializeRuntime() {
    runtime_ = std::make_unique<engine::EngineRuntime>(engine::EngineConfig{
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 1,
    });
    runtime_->addSystem(std::make_unique<ScenarioAgentSpawnSystem>(createAgentSeeds(), timeLimitSeconds_));
    runtime_->addSystem(
        makeScenarioControlSystem(layout_, scenario_.control.connectionBlocks),
        {.phase = engine::UpdatePhase::PreSimulation,
         .triggerPolicy = engine::TriggerPolicy::EveryFrame});
    runtime_->addSystem(
        std::make_unique<ScenarioSpatialIndexSystem>(1.0),
        {.phase = engine::UpdatePhase::PreSimulation,
         .triggerPolicy = engine::TriggerPolicy::EveryFrame});
    runtime_->addSystem(
        makeScenarioSimulationMotionSystem(layout_),
        {.phase = engine::UpdatePhase::PostSimulation,
         .triggerPolicy = engine::TriggerPolicy::EveryFrame});
    runtime_->addSystem(
        makeScenarioRiskMetricsSystem(layout_),
        {.phase = engine::UpdatePhase::PostSimulation,
         .order = 10,
         .triggerPolicy = engine::TriggerPolicy::EveryFrame});
    runtime_->addSystem(
        std::make_unique<ScenarioResultArtifactsSystem>(scenario_.execution.sampleIntervalSeconds),
        {.phase = engine::UpdatePhase::PostSimulation,
         .order = 20,
         .triggerPolicy = engine::TriggerPolicy::EveryFrame});
    runtime_->addSystem(
        std::make_unique<ScenarioFrameSyncSystem>(),
        {.phase = engine::UpdatePhase::RenderSync,
         .triggerPolicy = engine::TriggerPolicy::EveryFrame});
    runtime_->play();
    runtime_->stepFrame(0.0);
    syncFrameFromRuntime();
}

void ScenarioSimulationRunner::syncFrameFromRuntime() {
    if (runtime_ == nullptr) {
        return;
    }
    auto& resources = runtime_->world().resources();
    if (resources.contains<ScenarioSimulationFrameResource>()) {
        frame_ = resources.get<ScenarioSimulationFrameResource>().frame;
    }
    if (resources.contains<ScenarioRiskMetricsResource>()) {
        const auto& metrics = resources.get<ScenarioRiskMetricsResource>();
        riskSnapshot_ = metrics.snapshot;
        resultRiskSnapshot_ = metrics.peakSnapshot;
    }
    if (resources.contains<ScenarioResultArtifactsResource>()) {
        resultArtifacts_ = resources.get<ScenarioResultArtifactsResource>().artifacts;
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
        if (const auto* connection = findCachedConnectionBetween(layoutCache_, fromZoneId, toZoneId)) {
            const auto passage = passageWithClearance(*connection, kCandidateClearance);
            const auto fromFloorId = cachedFloorIdForZone(layoutCache_, fromZoneId);
            const auto toFloorId = cachedFloorIdForZone(layoutCache_, toZoneId);
            const auto& segmentLayout = cachedLayoutForFloor(layoutCache_, fromFloorId);
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
    if (const auto* exitZone = findCachedZone(layoutCache_, zoneRoute->back())) {
        const auto exitCenter = polygonCenter(exitZone->area);
        if (distanceBetween(segmentStart, exitCenter) > kArrivalEpsilon) {
            const auto exitFloorId = exitZone->floorId;
            const auto& segmentLayout = cachedLayoutForFloor(layoutCache_, exitFloorId);
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

std::optional<std::vector<std::string>> ScenarioSimulationRunner::zoneRouteToExit(const std::string& startZoneId) const {
    return zoneRouteToNearestExit(layoutCache_, startZoneId);
}

std::string ScenarioSimulationRunner::zoneAt(const Point2D& point, const std::string& floorId) const {
    return simulation_internal::zoneAt(layoutCache_, point, floorId);
}

Point2D ScenarioSimulationRunner::placementPoint(const InitialPlacement2D& placement, std::size_t index) const {
    if (index < placement.explicitPositions.size()) {
        return placement.explicitPositions[index];
    }
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


