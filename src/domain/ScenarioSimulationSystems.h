#pragma once

#include <unordered_map>
#include <vector>

#include "domain/AgentComponents.h"
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

struct ScenarioAgentSpatialIndexResource {
    double cellSize{1.0};
    std::unordered_map<long long, std::vector<engine::Entity>> cells{};
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

}  // namespace safecrowd::domain
