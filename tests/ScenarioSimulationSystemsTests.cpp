#include "TestSupport.h"

#include <memory>

#include "domain/AgentComponents.h"
#include "domain/ScenarioSimulationInternal.h"
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

safecrowd::domain::FacilityLayout2D straightExitLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{1.0, -1.0}, {2.0, -1.0}, {2.0, 1.0}, {1.0, 1.0}}},
    });
    return layout;
}

}  // namespace

SC_TEST(ScenarioAgentSpawnSystem_ConfiguresClockAndSpawnsAgentSeeds) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 2.0, .y = 3.0}},
        .agent = {.radius = 0.3f, .maxSpeed = 1.2f},
        .velocity = {.value = {.x = 0.2, .y = 0.1}},
        .route = {},
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 2,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 15.0));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    SC_EXPECT_NEAR(clock.timeLimitSeconds, 15.0, 1e-9);
    const auto& frame = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
    SC_EXPECT_EQ(frame.totalAgentCount, std::size_t{1});
    SC_EXPECT_EQ(frame.agents.size(), std::size_t{1});
    SC_EXPECT_NEAR(frame.agents.front().position.x, 2.0, 1e-9);
}

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

SC_TEST(ScenarioClockSystem_AdvancesClockResourceOnFixedSteps) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 11,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{},
        0.5));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioClockSystem>(0.25),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::FixedStep});

    runtime.play();
    runtime.stepFrame(0.25);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    SC_EXPECT_NEAR(clock.elapsedSeconds, 0.25, 1e-9);
    SC_EXPECT_TRUE(!clock.complete);

    runtime.stepFrame(0.25);
    SC_EXPECT_NEAR(clock.elapsedSeconds, 0.5, 1e-9);
    SC_EXPECT_TRUE(clock.complete);
}

SC_TEST(ScenarioSimulationMotionSystem_AdvancesAgentsFromStepResource) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 0.0, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 0.0}},
            .waypointPassages = {{{.x = 1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"exit"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.0, .y = 0.0},
            .previousDistanceToWaypoint = 1.0,
            .stalledSeconds = 0.0,
            .destinationZoneId = "exit",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 13,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.5});
    runtime.stepFrame(0.0);

    const auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    SC_EXPECT_NEAR(clock.elapsedSeconds, 0.5, 1e-9);
    const auto& frame = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
    SC_EXPECT_EQ(frame.agents.size(), std::size_t{1});
    SC_EXPECT_NEAR(frame.agents.front().position.x, 0.5, 1e-9);
    SC_EXPECT_NEAR(frame.agents.front().velocity.x, 1.0, 1e-9);
}

SC_TEST(ScenarioRoutePassageCrossed_UsesDoorPlaneNearEndpoint) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "corridor",
        .kind = safecrowd::domain::ZoneKind::Corridor,
        .label = "Corridor",
        .area = {.outline = {{1.0, 0.2}, {2.0, 0.2}, {2.0, 4.0}, {1.0, 4.0}}},
    });

    safecrowd::domain::EvacuationRoute route;
    route.waypoints = {{.x = 1.0, .y = 0.9}};
    route.waypointPassages = {{{.x = 1.0, .y = 0.8}, {.x = 1.0, .y = 1.0}}};
    route.waypointFromZoneIds = {"room"};
    route.waypointZoneIds = {"corridor"};
    route.nextWaypointIndex = 0;

    const safecrowd::domain::Point2D crossedNearEndpoint{.x = 1.05, .y = 0.75};
    SC_EXPECT_TRUE(safecrowd::domain::simulation_internal::routePassageCrossed(
        layout,
        route,
        crossedNearEndpoint,
        0.25));
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
