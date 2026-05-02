#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "domain/ScenarioAuthoring.h"
#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
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

struct ScenarioLayoutResource {
    FacilityLayout2D layout{};
};

struct ScenarioLayoutRevisionResource {
    std::uint64_t revision{0};
};

struct ScenarioAgentSpatialIndexResource {
    double cellSize{1.0};
    std::unordered_map<long long, std::vector<engine::Entity>> cells{};
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
    std::unordered_map<std::string, std::vector<ScenarioConnectionTraversal>> traversableConnectionsByZone{};
};

struct ScenarioRiskMetricsResource {
    ScenarioRiskSnapshot snapshot{};
    ScenarioRiskSnapshot peakSnapshot{};
};

struct ScenarioResultArtifactsResource {
    ScenarioResultArtifacts artifacts{};
    std::size_t lastRecordedEvacuatedCount{static_cast<std::size_t>(-1)};
    double nextSampleTimeSeconds{0.0};
    double sampleIntervalSeconds{1.0};
};

struct ScenarioAgentSeed {
    Position position{};
    Agent agent{};
    Velocity velocity{};
    EvacuationRoute route{};
    EvacuationStatus status{};
};

std::vector<engine::Entity> scenarioNearbyAgents(
    engine::WorldQuery& query,
    const ScenarioAgentSpatialIndexResource& index,
    const Point2D& point,
    double radius);

std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem();
std::unique_ptr<engine::EngineSystem> makeScenarioControlSystem(
    FacilityLayout2D baseLayout,
    std::vector<ConnectionBlockDraft> blocks);
std::unique_ptr<engine::EngineSystem> makeScenarioSimulationMotionSystem(FacilityLayout2D layout);
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
