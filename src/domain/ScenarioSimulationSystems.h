#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"
#include "engine/EngineSystem.h"
#include "engine/Entity.h"

namespace safecrowd::domain {

struct ScenarioSimulationClockResource {
    double elapsedSeconds{0.0};
    double timeLimitSeconds{60.0};
    bool complete{false};
};

struct ScenarioSimulationFrameResource {
    SimulationFrame frame{};
};

struct ScenarioSimulationStepResource {
    double deltaSeconds{0.0};
};

struct ScenarioScheduledSpawnResource {
    std::size_t pendingCount{0};
};

struct ScenarioAgentSpatialIndexResource {
    double cellSize{1.0};
    std::unordered_map<std::string, std::unordered_map<long long, std::vector<engine::Entity>>> cellsByFloor{};
    std::unordered_map<std::string, std::unordered_map<long long, std::vector<engine::Entity>>> displayCellsByFloor{};
    std::unordered_map<std::string, std::unordered_map<long long, std::vector<std::size_t>>> barrierIndicesByFloor{};
};

struct ScenarioConnectionTraversal {
    std::string nextZoneId{};
    std::size_t connectionIndex{0};
};

struct ScenarioLayoutCacheResource {
    FacilityLayout2D layout{};
    std::unordered_map<std::string, FacilityLayout2D> floorLayouts{};
    std::unordered_map<std::string, std::size_t> zoneIndices{};
    std::unordered_map<std::string, std::string> zoneFloorIds{};
    std::unordered_map<std::string, std::size_t> connectionIndices{};
    std::unordered_map<std::string, double> floorElevations{};
    std::unordered_map<std::string, std::vector<ScenarioConnectionTraversal>> traversableConnectionsByZone{};
};

struct ScenarioLayoutRevisionResource {
    std::uint64_t revision{0};
};

struct ScenarioRiskMetricsResource {
    ScenarioRiskSnapshot snapshot{};
    ScenarioRiskSnapshot peakSnapshot{};
};

struct ScenarioPressureFeedbackAgentState {
    std::uint64_t agentId{0};
    Point2D position{};
    std::string floorId{};
    double compressionForce{0.0};
    double exposureSeconds{0.0};
    double feedbackLevel{0.0};
    double speedFactor{1.0};
    double avoidanceScale{1.0};
    double barrierScale{1.0};
    bool exposed{false};
    bool critical{false};
};

struct ScenarioPressureFeedbackResource {
    std::unordered_map<std::uint64_t, ScenarioPressureFeedbackAgentState> agentsById{};
    std::size_t exposedAgentCount{0};
    std::size_t criticalAgentCount{0};
};

struct ScenarioEnvironmentReactionAgentState {
    bool hazardDetected{false};
    bool hazardAware{false};
    bool hazardInRange{false};
    std::string hazardKey{};
    EnvironmentHazardKind hazardKind{EnvironmentHazardKind::Fire};
    ScenarioElementSeverity hazardSeverity{ScenarioElementSeverity::Medium};
    Point2D hazardPosition{};
    std::string hazardFloorId{};
    std::string hazardAffectedZoneId{};
    double hazardDistanceMeters{0.0};
    double hazardRadiusMeters{0.0};
    double hazardSpeedFactor{1.0};
    double hazardRoutePenaltyMeters{0.0};
    double hazardDetectedAtSeconds{0.0};
    double hazardReactionReadySeconds{0.0};
    bool closureDetected{false};
    bool closureAware{false};
    std::string blockedConnectionId{};
    double closureDetectedAtSeconds{0.0};
    double closureReactionReadySeconds{0.0};
};

struct ScenarioEnvironmentReactionResource {
    std::unordered_map<std::uint64_t, ScenarioEnvironmentReactionAgentState> agentsById{};
};

struct ScenarioActiveEnvironmentHazard {
    std::string key{};
    EnvironmentHazardDraft draft{};
    std::string floorId{};
    double radiusMeters{0.0};
    double speedFactor{1.0};
    double routePenaltyMeters{0.0};
    double severityWeight{1.0};
};

struct ScenarioActiveEnvironmentHazardsResource {
    std::vector<ScenarioActiveEnvironmentHazard> hazards{};
    std::string signature{};
    double cellSizeMeters{1.0};
    double maxRadiusMeters{0.0};
    std::unordered_map<std::string, std::unordered_map<long long, std::vector<std::size_t>>> hazardIndicesByFloor{};
};

struct ScenarioHazardExposureResource {
    std::unordered_map<std::string, HazardExposureMetric> hazardsById{};
};

struct ScenarioCrossFlowCellState {
    double startedAtSeconds{0.0};
    double exposureAgentSeconds{0.0};
    double peakCrossFlowScore{0.0};
    std::size_t peakAgentCount{0};
};

struct ScenarioCrossFlowResource {
    double totalCrossFlowExposureAgentSeconds{0.0};
    std::unordered_map<long long, ScenarioCrossFlowCellState> activeCellsByAddress{};
};

struct ScenarioResultArtifactsResource {
    ScenarioResultArtifacts artifacts{};
    std::size_t lastRecordedEvacuatedCount{static_cast<std::size_t>(-1)};
    double nextSampleTimeSeconds{0.0};
    double sampleIntervalSeconds{1.0};
    double nextReplaySampleTimeSeconds{0.0};
    double replaySampleIntervalSeconds{0.5};
    std::size_t maxReplayFrames{600};
    double nextOccupancySampleTimeSeconds{0.0};
    double occupancySampleIntervalSeconds{0.25};
    bool occupancyTrackingInitialized{false};
    double lastOccupancySampleTimeSeconds{0.0};
    bool densityTrackingInitialized{false};
    double lastDensitySampleTimeSeconds{0.0};
    std::unordered_map<long long, OccupancyHeatmapCell> occupancyHeatmapCellsByAddress{};
    std::unordered_map<long long, DensityCellMetric> peakDensityCellsByAddress{};
    std::unordered_map<long long, PressureCellMetric> peakPressureCellsByAddress{};
    std::unordered_map<long long, ScenarioPressureHotspot> peakPressureHotspotsByAddress{};
    std::unordered_map<std::uint64_t, ScenarioPressureAgentMetric> peakPressureAgentsById{};
    std::unordered_map<long long, ScenarioCriticalPressureEvent> peakCriticalPressureEventsByAddress{};
};

struct ScenarioTimingKeyframesResource {
    std::optional<SimulationFrame> t90Frame{};
    std::optional<SimulationFrame> t95Frame{};
};

struct ScenarioAgentSeed {
    Position position{};
    Agent agent{};
    Velocity velocity{};
    AvoidanceState avoidance{};
    EvacuationRoute route{};
    WayfindingState wayfinding{};
    EvacuationStatus status{};
};

struct ScheduledScenarioAgentSeed {
    double spawnSeconds{0.0};
    ScenarioAgentSeed seed{};
};

std::vector<engine::Entity> scenarioNearbyAgents(
    engine::WorldQuery& query,
    const ScenarioAgentSpatialIndexResource& index,
    const Point2D& point,
    const std::string& floorId,
    double radius);
std::vector<engine::Entity> scenarioNearbyDisplayAgents(
    engine::WorldQuery& query,
    const ScenarioAgentSpatialIndexResource& index,
    const Point2D& point,
    const std::string& floorId,
    double radius);
std::vector<const Barrier2D*> scenarioNearbyBarriers(
    const FacilityLayout2D& layout,
    const ScenarioAgentSpatialIndexResource& index,
    const Point2D& point,
    const std::string& floorId,
    double radius);
std::vector<std::size_t> scenarioNearbyHazardIndices(
    const ScenarioActiveEnvironmentHazardsResource& hazards,
    const Point2D& point,
    const std::string& floorId,
    double radius);
std::vector<std::size_t> scenarioHazardIndicesNearSegment(
    const ScenarioActiveEnvironmentHazardsResource& hazards,
    const Point2D& start,
    const Point2D& end,
    const std::string& floorId,
    double radius);

std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem();
std::unique_ptr<engine::EngineSystem> makeScenarioControlSystem(
    FacilityLayout2D baseLayout,
    std::vector<ConnectionBlockDraft> blocks);
std::unique_ptr<engine::EngineSystem> makeScenarioEnvironmentHazardSystem(
    FacilityLayout2D layout,
    std::vector<EnvironmentHazardDraft> hazards);
std::unique_ptr<engine::EngineSystem> makeScenarioWayfindingSystem(
    FacilityLayout2D layout,
    std::vector<EvacuationSignDraft> signs);
std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem(FacilityLayout2D layout);
std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem(
    FacilityLayout2D layout,
    std::vector<RouteGuidanceDraft> routeGuidances);
std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem(
    FacilityLayout2D layout,
    std::vector<RouteGuidanceDraft> routeGuidances,
    ScenarioWayfindingMode wayfindingMode);
std::unique_ptr<engine::EngineSystem> makeScenarioPressureFeedbackSystem(FacilityLayout2D layout);
std::unique_ptr<engine::EngineSystem> makeScenarioRiskMetricsSystem(FacilityLayout2D layout);

class ScenarioAgentSpawnSystem final : public engine::EngineSystem {
public:
    ScenarioAgentSpawnSystem(std::vector<ScenarioAgentSeed> seeds, double timeLimitSeconds);

    void configure(engine::EngineWorld& world) override;
    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override;

private:
    std::vector<ScenarioAgentSeed> seeds_{};
    double timeLimitSeconds_{60.0};
};

class ScenarioOccupantSourceSpawnSystem final : public engine::EngineSystem {
public:
    explicit ScenarioOccupantSourceSpawnSystem(std::vector<ScheduledScenarioAgentSeed> seeds);

    void configure(engine::EngineWorld& world) override;
    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override;

private:
    void spawnDueSeeds(engine::EngineWorld& world, double elapsedSeconds);

    std::vector<ScheduledScenarioAgentSeed> seeds_{};
    std::size_t nextSeedIndex_{0};
};

class ScenarioSpatialIndexSystem final : public engine::EngineSystem {
public:
    explicit ScenarioSpatialIndexSystem(double cellSize = 1.0);

    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override;

private:
    double cellSize_{1.0};
};

class ScenarioClockSystem final : public engine::EngineSystem {
public:
    explicit ScenarioClockSystem(double fixedDeltaSeconds);

    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override;

private:
    double fixedDeltaSeconds_{0.0};
};

class ScenarioFrameSyncSystem final : public engine::EngineSystem {
public:
    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override;
};

class ScenarioResultArtifactsSystem final : public engine::EngineSystem {
public:
    explicit ScenarioResultArtifactsSystem(double sampleIntervalSeconds = 1.0);

    void configure(engine::EngineWorld& world) override;
    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override;

private:
    double sampleIntervalSeconds_{1.0};
};

}  // namespace safecrowd::domain
