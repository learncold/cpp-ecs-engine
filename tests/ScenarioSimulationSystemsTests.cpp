#include "TestSupport.h"

#include <memory>

#include "domain/AgentComponents.h"
#include "domain/ScenarioSimulationSystems.h"
#include "engine/EngineRuntime.h"

namespace {

class ConfigureScenarioAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.5,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 1.0, .y = 1.0}},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{.value = {.x = 0.5, .y = 0.0}},
            safecrowd::domain::EvacuationStatus{});
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 1.3, .y = 1.0}},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{.value = {.x = 0.0, .y = 0.0}},
            safecrowd::domain::EvacuationStatus{.evacuated = true});
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

}  // namespace

SC_TEST(ScenarioSpatialIndexSystem_BuildsNearbyAgentResource) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 3,
    });
    runtime.addSystem(std::make_unique<ConfigureScenarioAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    auto& resources = runtime.world().resources();
    SC_EXPECT_TRUE(resources.contains<safecrowd::domain::ScenarioAgentSpatialIndexResource>());
    const auto& index = resources.get<safecrowd::domain::ScenarioAgentSpatialIndexResource>();
    auto nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 1.0},
        0.4);
    SC_EXPECT_EQ(nearby.size(), std::size_t{1});
}

SC_TEST(ScenarioFrameSyncSystem_PublishesSimulationFrameResource) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 5,
    });
    runtime.addSystem(std::make_unique<ConfigureScenarioAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    auto& resources = runtime.world().resources();
    SC_EXPECT_TRUE(resources.contains<safecrowd::domain::ScenarioSimulationFrameResource>());
    const auto& frame = resources.get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
    SC_EXPECT_NEAR(frame.elapsedSeconds, 1.5, 1e-9);
    SC_EXPECT_EQ(frame.totalAgentCount, std::size_t{2});
    SC_EXPECT_EQ(frame.evacuatedAgentCount, std::size_t{1});
    SC_EXPECT_EQ(frame.agents.size(), std::size_t{1});
    SC_EXPECT_NEAR(frame.agents.front().velocity.x, 0.5, 1e-9);
}
