#include "TestSupport.h"

#include <cmath>
#include <memory>
#include <string>

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

class ConfigureOverlappingFloorAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 0.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 1.0, .y = 1.0}},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{.value = {.x = 0.0, .y = 0.0}},
            safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
            safecrowd::domain::EvacuationStatus{});
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 1.0, .y = 1.0}},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{.value = {.x = 0.0, .y = 0.0}},
            safecrowd::domain::EvacuationRoute{.currentFloorId = "L2", .displayFloorId = "L2"},
            safecrowd::domain::EvacuationStatus{});
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureEvacuatedAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 7.0,
            .timeLimitSeconds = 10.0,
            .complete = true,
        });
        spawnEvacuatedAgent(world, 2.0);
        spawnEvacuatedAgent(world, 5.0);
        spawnEvacuatedAgent(world, 7.0);
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }

private:
    static void spawnEvacuatedAgent(safecrowd::engine::EngineWorld& world, double completionTimeSeconds) {
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = completionTimeSeconds, .y = 0.0}},
            safecrowd::domain::Agent{
                .radius = 0.25f,
                .maxSpeed = 1.5f,
                .sourcePlacementId = "group-a",
                .sourceZoneId = "room-a",
            },
            safecrowd::domain::Velocity{.value = {}},
            safecrowd::domain::EvacuationRoute{.destinationZoneId = "exit-a"},
            safecrowd::domain::EvacuationStatus{
                .evacuated = true,
                .completionTimeSeconds = completionTimeSeconds,
            });
    }
};

class ConfigureDenseActiveAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        for (int index = 0; index < 10; ++index) {
            world.commands().spawnEntity(
                safecrowd::domain::Position{.value = {.x = 0.1 + (0.04 * static_cast<double>(index)), .y = 0.1}},
                safecrowd::domain::Agent{
                    .radius = 0.25f,
                    .maxSpeed = 1.5f,
                    .sourcePlacementId = "dense-group",
                    .sourceZoneId = "room-a",
                },
                safecrowd::domain::Velocity{.value = {}},
                safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
                safecrowd::domain::EvacuationStatus{});
        }
        for (int index = 0; index < 6; ++index) {
            world.commands().spawnEntity(
                safecrowd::domain::Position{.value = {.x = 3.0 + (2.0 * static_cast<double>(index)), .y = 0.1}},
                safecrowd::domain::Agent{
                    .radius = 0.25f,
                    .maxSpeed = 1.5f,
                    .sourcePlacementId = "spread-group",
                    .sourceZoneId = "room-a",
                },
                safecrowd::domain::Velocity{.value = {}},
                safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
                safecrowd::domain::EvacuationStatus{});
        }
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
    layout.connections.push_back({
        .id = "room-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "exit",
        .effectiveWidth = 0.8,
        .centerSpan = {{1.0, -0.4}, {1.0, 0.4}},
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
        .route = {.stalledSeconds = 1.0},
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
    SC_EXPECT_TRUE(frame.agents.front().stalled);
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
        std::string{},
        0.4);
    SC_EXPECT_EQ(nearby.size(), std::size_t{1});
}

SC_TEST(ScenarioSpatialIndexSystem_SeparatesNearbyAgentsByFloor) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 31,
    });
    runtime.addSystem(std::make_unique<ConfigureOverlappingFloorAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    const auto& index = runtime.world().resources().get<safecrowd::domain::ScenarioAgentSpatialIndexResource>();
    auto l1Nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 1.0},
        "L1",
        0.4);
    auto l2Nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 1.0},
        "L2",
        0.4);

    SC_EXPECT_EQ(l1Nearby.size(), std::size_t{1});
    SC_EXPECT_EQ(l2Nearby.size(), std::size_t{1});
    SC_EXPECT_TRUE(l1Nearby.front() != l2Nearby.front());
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
    SC_EXPECT_TRUE(!frame.agents.front().stalled);
}

SC_TEST(ScenarioSimulationMotionSystem_SkipsIntermediateWaypointWhenCrowdPushesAgentPastApproachArea) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 0.82, .y = 0.45}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 0.0}, {.x = 2.0, .y = 0.0}},
            .waypointPassages = {
                {{.x = 1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}},
                {{.x = 2.0, .y = 0.0}, {.x = 2.0, .y = 0.0}},
            },
            .waypointFromZoneIds = {"", ""},
            .waypointZoneIds = {"", "missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.0, .y = 0.0},
            .previousDistanceToWaypoint = 0.5,
            .destinationZoneId = "missing",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 14,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem({}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto entity = entities.front();
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entity);
    const auto& velocity = runtime.world().query().get<safecrowd::domain::Velocity>(entity);

    SC_EXPECT_EQ(route.nextWaypointIndex, std::size_t{1});
    SC_EXPECT_TRUE(velocity.value.x > 0.0);
}

SC_TEST(ScenarioSimulationMotionSystem_HeadOnAgentsSeparateLaterally) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = -0.4, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 0.0}},
            .waypointPassages = {{{.x = 1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = -0.4, .y = 0.0},
            .previousDistanceToWaypoint = 1.4,
            .destinationZoneId = "missing",
        },
        .status = {},
    });
    seeds.push_back({
        .position = {.value = {.x = 0.4, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = -1.0, .y = 0.0}},
            .waypointPassages = {{{.x = -1.0, .y = 0.0}, {.x = -1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.4, .y = 0.0},
            .previousDistanceToWaypoint = 1.4,
            .destinationZoneId = "missing",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 17,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem({}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.2});
    runtime.stepFrame(0.0);

    // Step the simulation a few times so the social-force tangent bias has
    // room to translate into measurable lateral velocity.
    for (int frame = 0; frame < 10; ++frame) {
        runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.2});
        runtime.stepFrame(0.0);
    }

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{2});

    const auto first = entities[0];
    const auto second = entities[1];
    const auto& firstVelocity = runtime.world().query().get<safecrowd::domain::Velocity>(first);
    const auto& secondVelocity = runtime.world().query().get<safecrowd::domain::Velocity>(second);
    const auto& firstPosition = runtime.world().query().get<safecrowd::domain::Position>(first);
    const auto& secondPosition = runtime.world().query().get<safecrowd::domain::Position>(second);

    // Both agents must keep heading toward their goals (positive/negative x).
    SC_EXPECT_TRUE(firstVelocity.value.x > 0.0);
    SC_EXPECT_TRUE(secondVelocity.value.x < 0.0);
    // Symmetry-breaker should drive them onto opposite sides of the original axis.
    SC_EXPECT_TRUE(firstVelocity.value.y * secondVelocity.value.y < 0.0);
    // No body overlap: distance must stay above the sum of radii.
    const auto dx = firstPosition.value.x - secondPosition.value.x;
    const auto dy = firstPosition.value.y - secondPosition.value.y;
    SC_EXPECT_TRUE(std::sqrt(dx * dx + dy * dy) >= 0.5);
}

SC_TEST(ScenarioControlSystem_BlocksConnectionsUsingScenarioClock) {
    auto layout = straightExitLayout();
    safecrowd::domain::ConnectionBlockDraft block;
    block.id = "block-1";
    block.connectionId = "room-exit";
    block.intervals = {
        {.startSeconds = 0.0, .endSeconds = 2.0},
    };

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 21,
    });
    runtime.addSystem(
        safecrowd::domain::makeScenarioControlSystem(layout, {block}),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();

    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationClockResource{
        .elapsedSeconds = 1.0,
        .timeLimitSeconds = 10.0,
        .complete = false,
    });
    runtime.stepFrame(0.0);

    {
        const auto& layoutCache =
            runtime.world().resources().get<safecrowd::domain::ScenarioLayoutCacheResource>();
        SC_EXPECT_EQ(layoutCache.layout.connections.size(), std::size_t{1});
        SC_EXPECT_EQ(
            layoutCache.layout.connections.front().directionality,
            safecrowd::domain::TravelDirection::Closed);
        SC_EXPECT_EQ(layoutCache.layout.barriers.size(), std::size_t{1});
    }

    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 3.0;
    runtime.stepFrame(0.0);

    {
        const auto& layoutCache =
            runtime.world().resources().get<safecrowd::domain::ScenarioLayoutCacheResource>();
        SC_EXPECT_EQ(layoutCache.layout.connections.size(), std::size_t{1});
        SC_EXPECT_EQ(
            layoutCache.layout.connections.front().directionality,
            safecrowd::domain::TravelDirection::Bidirectional);
        SC_EXPECT_EQ(layoutCache.layout.barriers.size(), std::size_t{0});
    }
}

SC_TEST(ScenarioRiskMetricsSystem_PublishesStalledHotspotAndBottleneckMetrics) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (int index = 0; index < 5; ++index) {
        seeds.push_back({
            .position = {.value = {.x = 0.75 + (static_cast<double>(index) * 0.03), .y = 0.0}},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 1.0, .y = 0.0}},
                .waypointPassages = {{{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = {.x = 0.75, .y = 0.0},
                .previousDistanceToWaypoint = 0.25,
                .stalledSeconds = 1.0,
                .destinationZoneId = "exit",
            },
            .status = {},
        });
    }

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 17,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& snapshot =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
    SC_EXPECT_EQ(snapshot.stalledAgentCount, std::size_t{5});
    SC_EXPECT_TRUE(!snapshot.hotspots.empty());
    SC_EXPECT_NEAR(snapshot.hotspots.front().cellMin.x, 0.0, 1e-9);
    SC_EXPECT_NEAR(snapshot.hotspots.front().cellMin.y, 0.0, 1e-9);
    SC_EXPECT_NEAR(snapshot.hotspots.front().cellMax.x, 1.5, 1e-9);
    SC_EXPECT_NEAR(snapshot.hotspots.front().cellMax.y, 1.5, 1e-9);
    SC_EXPECT_TRUE(!snapshot.bottlenecks.empty());
    SC_EXPECT_EQ(snapshot.bottlenecks.front().label, std::string{"Room -> Exit"});
    SC_EXPECT_EQ(snapshot.completionRisk, safecrowd::domain::ScenarioRiskLevel::High);
}

SC_TEST(ScenarioRiskMetricsSystem_PreservesPeakMetricsAfterAllAgentsEvacuate) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (int index = 0; index < 5; ++index) {
        seeds.push_back({
            .position = {.value = {.x = 0.75 + (static_cast<double>(index) * 0.03), .y = 0.0}},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 1.0, .y = 0.0}},
                .waypointPassages = {{{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = {.x = 0.75, .y = 0.0},
                .previousDistanceToWaypoint = 0.25,
                .stalledSeconds = 1.0,
                .destinationZoneId = "exit",
            },
            .status = {},
        });
    }

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 19,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& query = runtime.world().query();
    for (const auto entity : query.view<safecrowd::domain::EvacuationStatus>()) {
        query.get<safecrowd::domain::EvacuationStatus>(entity).evacuated = true;
    }

    runtime.stepFrame(0.0);

    const auto& metrics =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>();
    SC_EXPECT_TRUE(metrics.snapshot.hotspots.empty());
    SC_EXPECT_TRUE(metrics.snapshot.bottlenecks.empty());
    SC_EXPECT_TRUE(!metrics.peakSnapshot.hotspots.empty());
    SC_EXPECT_TRUE(!metrics.peakSnapshot.bottlenecks.empty());
    SC_EXPECT_EQ(metrics.peakSnapshot.stalledAgentCount, std::size_t{5});
    SC_EXPECT_EQ(metrics.peakSnapshot.completionRisk, safecrowd::domain::ScenarioRiskLevel::High);
}

SC_TEST(ScenarioResultArtifactsSystem_PublishesEvacuationCurveAndPercentiles) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 23,
    });
    runtime.addSystem(std::make_unique<ConfigureEvacuatedAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& artifacts =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts;
    SC_EXPECT_EQ(artifacts.evacuationProgress.size(), std::size_t{1});
    SC_EXPECT_NEAR(artifacts.evacuationProgress.front().timeSeconds, 7.0, 1e-9);
    SC_EXPECT_EQ(artifacts.evacuationProgress.front().evacuatedCount, std::size_t{3});
    SC_EXPECT_EQ(artifacts.evacuationProgress.front().totalCount, std::size_t{3});
    SC_EXPECT_NEAR(artifacts.evacuationProgress.front().evacuatedRatio, 1.0, 1e-9);
    SC_EXPECT_TRUE(artifacts.timingSummary.t50Seconds.has_value());
    SC_EXPECT_TRUE(artifacts.timingSummary.t90Seconds.has_value());
    SC_EXPECT_TRUE(artifacts.timingSummary.t95Seconds.has_value());
    SC_EXPECT_TRUE(artifacts.timingSummary.finalEvacuationTimeSeconds.has_value());
    SC_EXPECT_NEAR(*artifacts.timingSummary.t50Seconds, 5.0, 1e-9);
    SC_EXPECT_NEAR(*artifacts.timingSummary.t90Seconds, 7.0, 1e-9);
    SC_EXPECT_NEAR(*artifacts.timingSummary.t95Seconds, 7.0, 1e-9);
    SC_EXPECT_NEAR(*artifacts.timingSummary.finalEvacuationTimeSeconds, 7.0, 1e-9);
    SC_EXPECT_NEAR(artifacts.timingSummary.targetTimeSeconds, 10.0, 1e-9);
    SC_EXPECT_TRUE(artifacts.timingSummary.marginSeconds.has_value());
    SC_EXPECT_NEAR(*artifacts.timingSummary.marginSeconds, 3.0, 1e-9);
    SC_EXPECT_EQ(artifacts.exitUsage.size(), std::size_t{1});
    SC_EXPECT_EQ(artifacts.exitUsage.front().exitZoneId, std::string{"exit-a"});
    SC_EXPECT_EQ(artifacts.exitUsage.front().evacuatedCount, std::size_t{3});
    SC_EXPECT_NEAR(artifacts.exitUsage.front().usageRatio, 1.0, 1e-9);
    SC_EXPECT_EQ(artifacts.zoneCompletion.size(), std::size_t{1});
    SC_EXPECT_EQ(artifacts.zoneCompletion.front().zoneId, std::string{"room-a"});
    SC_EXPECT_EQ(artifacts.zoneCompletion.front().evacuatedCount, std::size_t{3});
    SC_EXPECT_EQ(artifacts.placementCompletion.size(), std::size_t{1});
    SC_EXPECT_EQ(artifacts.placementCompletion.front().placementId, std::string{"group-a"});
}

SC_TEST(ScenarioResultArtifactsSystem_PublishesDensitySummary) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 24,
    });
    runtime.addSystem(std::make_unique<ConfigureDenseActiveAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);

    const auto& artifacts =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts;
    SC_EXPECT_TRUE(artifacts.densitySummary.peakCell.has_value());
    SC_EXPECT_EQ(artifacts.densitySummary.peakAgentCount, std::size_t{10});
    SC_EXPECT_TRUE(artifacts.densitySummary.peakDensityPeoplePerSquareMeter >= 4.0);
    SC_EXPECT_TRUE(artifacts.densitySummary.highDensityDurationSeconds >= 1.0);
    SC_EXPECT_TRUE(!artifacts.densitySummary.peakCells.empty());
    SC_EXPECT_EQ(artifacts.densitySummary.peakCells.size(), std::size_t{5});
    SC_EXPECT_TRUE(artifacts.densitySummary.peakField.cells.size() > artifacts.densitySummary.peakCells.size());
    SC_EXPECT_NEAR(artifacts.densitySummary.peakField.timeSeconds, 1.0, 1e-9);
    SC_EXPECT_NEAR(artifacts.densitySummary.peakField.cellSizeMeters, artifacts.densitySummary.cellSizeMeters, 1e-9);
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
        .id = "passage",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Passage",
        .area = {.outline = {{1.0, 0.2}, {2.0, 0.2}, {2.0, 4.0}, {1.0, 4.0}}},
    });

    safecrowd::domain::EvacuationRoute route;
    route.waypoints = {{.x = 1.0, .y = 0.9}};
    route.waypointPassages = {{{.x = 1.0, .y = 0.8}, {.x = 1.0, .y = 1.0}}};
    route.waypointFromZoneIds = {"room"};
    route.waypointZoneIds = {"passage"};
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
