#include "TestSupport.h"

#include <cmath>
#include "domain/DemoLayouts.h"
#include "domain/ScenarioSimulationRunner.h"

namespace {

safecrowd::domain::FacilityLayout2D blockedDoorLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "left",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Left",
        .area = {.outline = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {1.0, 1.0}}},
    });
    layout.connections.push_back({
        .id = "conn-left-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "left",
        .toZoneId = "exit",
        .effectiveWidth = 1.0,
        .centerSpan = {{1.0, 0.4}, {1.0, 0.6}},
    });
    layout.barriers.push_back({
        .id = "blocking-wall",
        .geometry = {.vertices = {{1.0, 0.0}, {1.0, 1.0}}},
        .blocksMovement = true,
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D wideDoorLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{4.0, 0.0}, {8.0, 0.0}, {8.0, 4.0}, {4.0, 4.0}}},
    });
    layout.connections.push_back({
        .id = "wide-door",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "exit",
        .effectiveWidth = 3.0,
        .centerSpan = {{4.0, 0.5}, {4.0, 3.5}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D wideDoorToPassageLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "passage",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Passage",
        .area = {.outline = {{4.0, 0.0}, {8.0, 0.0}, {8.0, 4.0}, {4.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{8.0, 1.0}, {10.0, 1.0}, {10.0, 3.0}, {8.0, 3.0}}},
    });
    layout.connections.push_back({
        .id = "wide-door",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "room",
        .toZoneId = "passage",
        .effectiveWidth = 3.0,
        .centerSpan = {{4.0, 0.5}, {4.0, 3.5}},
    });
    layout.connections.push_back({
        .id = "exit-door",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "passage",
        .toZoneId = "exit",
        .effectiveWidth = 2.0,
        .centerSpan = {{8.0, 1.0}, {8.0, 3.0}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D twoFloorStairExitLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "two-floor";
    layout.levelId = "L1";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "room-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L1",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "exit-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L2",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "stair-l1-l2",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "room-l1",
        .toZoneId = "exit-l2",
        .effectiveWidth = 1.2,
        .isStair = true,
        .centerSpan = {{2.0, 0.6}, {2.0, 1.4}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D overlappingTwoFloorExitLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "overlapping-two-floor";
    layout.levelId = "L1";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "room-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L1",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "exit-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L1",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "room-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L2",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "exit-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L2",
        .area = {.outline = {{-2.0, 0.0}, {0.0, 0.0}, {0.0, 2.0}, {-2.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "door-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room-l1",
        .toZoneId = "exit-l1",
        .effectiveWidth = 1.2,
        .centerSpan = {{2.0, 0.4}, {2.0, 1.6}},
    });
    layout.connections.push_back({
        .id = "door-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room-l2",
        .toZoneId = "exit-l2",
        .effectiveWidth = 1.2,
        .centerSpan = {{0.0, 0.4}, {0.0, 1.6}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D directedStairExitLayout(
    safecrowd::domain::StairEntryDirection lowerEntryDirection,
    safecrowd::domain::StairEntryDirection upperEntryDirection) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "directed-stair";
    layout.levelId = "L1";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "room-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L1",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "stair-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Stair L1",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "stair-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Stair L2",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "exit-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L2",
        .area = {.outline = {{4.0, 0.0}, {6.0, 0.0}, {6.0, 2.0}, {4.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "room-to-stair",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Opening,
        .fromZoneId = "room-l1",
        .toZoneId = "stair-l1",
        .effectiveWidth = 1.2,
        .centerSpan = {{2.0, 0.4}, {2.0, 1.6}},
    });
    layout.connections.push_back({
        .id = "stair-l1-l2",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "stair-l1",
        .toZoneId = "stair-l2",
        .effectiveWidth = 1.2,
        .isStair = true,
        .lowerEntryDirection = lowerEntryDirection,
        .upperEntryDirection = upperEntryDirection,
        .centerSpan = lowerEntryDirection == safecrowd::domain::StairEntryDirection::North
                || lowerEntryDirection == safecrowd::domain::StairEntryDirection::South
            ? safecrowd::domain::LineSegment2D{{2.8, 1.0}, {3.2, 1.0}}
            : safecrowd::domain::LineSegment2D{{3.0, 0.4}, {3.0, 1.6}},
    });
    layout.connections.push_back({
        .id = "stair-to-exit",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "stair-l2",
        .toZoneId = "exit-l2",
        .effectiveWidth = 1.2,
        .centerSpan = {{4.0, 0.4}, {4.0, 1.6}},
    });
    return layout;
}

safecrowd::domain::InitialPlacement2D groupPlacement() {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "group-1";
    placement.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    placement.targetAgentCount = 4;
    placement.initialVelocity = {.x = 1.0, .y = 0.5};
    placement.area.outline = {
        {.x = 1.0, .y = 4.0},
        {.x = 3.0, .y = 4.0},
        {.x = 3.0, .y = 6.0},
        {.x = 1.0, .y = 6.0},
    };
    return placement;
}

}  // namespace

SC_TEST(ScenarioSimulationRunnerInitializesAndRoutesAgentsThroughLayoutConnections) {
    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(groupPlacement());

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(4));

    runner.step(2.0);

    SC_EXPECT_NEAR(runner.frame().elapsedSeconds, 2.0, 1e-9);
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x > 2.0);
    SC_EXPECT_TRUE(runner.frame().agents.front().position.y > 4.0);
    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, static_cast<std::size_t>(0));
    SC_EXPECT_TRUE(!runner.complete());
}

SC_TEST(ScenarioSimulationRunnerCompletesAtTimeLimit) {
    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 1.0;
    scenario.population.initialPlacements.push_back(groupPlacement());

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    runner.step(2.0);

    SC_EXPECT_NEAR(runner.frame().elapsedSeconds, 1.0, 1e-9);
    SC_EXPECT_TRUE(runner.complete());
}

SC_TEST(ScenarioSimulationRunnerMarksEvacuationSuccessAtExitZone) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::ExitPassageZoneId;
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 3.0, .y = 0.0};
    placement.area.outline = {{.x = 23.5, .y = 6.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    for (int i = 0; i < 20 && !runner.complete(); ++i) {
        runner.step(0.5);
    }

    SC_EXPECT_EQ(runner.frame().totalAgentCount, static_cast<std::size_t>(1));
    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, static_cast<std::size_t>(1));
    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(0));
    SC_EXPECT_TRUE(runner.complete());
}

SC_TEST(ScenarioSimulationRunnerSeparatesOverlappingAgents) {
    safecrowd::domain::InitialPlacement2D first;
    first.id = "agent-1";
    first.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    first.targetAgentCount = 1;
    first.initialVelocity = {.x = 1.0, .y = 0.0};
    first.area.outline = {{.x = 2.0, .y = 5.0}};

    safecrowd::domain::InitialPlacement2D second = first;
    second.id = "agent-2";

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(first);
    scenario.population.initialPlacements.push_back(second);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    // Social-force repulsion separates coincident agents over physical time
    // rather than instantly via a relaxation pass; step long enough for the
    // contact-range exponential force to push them apart by a body diameter.
    runner.step(0.6);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(2));
    const auto dx = runner.frame().agents[0].position.x - runner.frame().agents[1].position.x;
    const auto dy = runner.frame().agents[0].position.y - runner.frame().agents[1].position.y;
    SC_EXPECT_TRUE(std::hypot(dx, dy) >= 0.49);
}

SC_TEST(ScenarioSimulationRunnerAvoidanceDoesNotReverseSharedRouteDirection) {
    safecrowd::domain::InitialPlacement2D first;
    first.id = "agent-1";
    first.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    first.targetAgentCount = 1;
    first.initialVelocity = {.x = 1.5, .y = 0.0};
    first.area.outline = {{.x = 2.0, .y = 5.0}};

    safecrowd::domain::InitialPlacement2D second = first;
    second.id = "agent-2";
    second.area.outline = {{.x = 2.18, .y = 5.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(first);
    scenario.population.initialPlacements.push_back(second);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    for (int i = 0; i < 6; ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(2));
    for (const auto& agent : runner.frame().agents) {
        SC_EXPECT_TRUE(agent.velocity.x >= -1e-6);
    }
}

SC_TEST(ScenarioSimulationRunnerAdvancesDoorWaypointAfterAgentEntersNextZone) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 12.4, .y = 5.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    runner.step(0.2);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x > 12.4);
    SC_EXPECT_TRUE(runner.frame().agents.front().velocity.x > 0.0);
}

SC_TEST(ScenarioSimulationRunnerUsesDoorSpanInsteadOfOnlyCenterPoint) {
    safecrowd::domain::InitialPlacement2D lower;
    lower.id = "agent-lower";
    lower.zoneId = "room";
    lower.targetAgentCount = 1;
    lower.initialVelocity = {.x = 2.0, .y = 0.0};
    lower.area.outline = {{.x = 1.0, .y = 1.0}};

    safecrowd::domain::InitialPlacement2D upper = lower;
    upper.id = "agent-upper";
    upper.area.outline = {{.x = 1.0, .y = 3.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 5.0;
    scenario.population.initialPlacements.push_back(lower);
    scenario.population.initialPlacements.push_back(upper);

    safecrowd::domain::ScenarioSimulationRunner runner(wideDoorLayout(), scenario);
    runner.step(0.25);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(2));
    for (const auto& agent : runner.frame().agents) {
        SC_EXPECT_TRUE(agent.velocity.x > 0.0);
        SC_EXPECT_TRUE(std::fabs(agent.velocity.y) < 0.05);
    }
}

SC_TEST(ScenarioSimulationRunnerAdvancesWideDoorPassageAfterCrossingNearEndpoint) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-near-endpoint";
    placement.zoneId = "room";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 2.0, .y = 0.0};
    placement.area.outline = {{.x = 4.08, .y = 3.25}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 5.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(wideDoorToPassageLayout(), scenario);
    runner.step(0.1);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    SC_EXPECT_TRUE(runner.frame().agents.front().velocity.x > 0.0);
}

SC_TEST(ScenarioSimulationRunnerSkipsPassedDoorwaysWhenAgentIsAlreadyInLaterZone) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-pushed-ahead";
    placement.zoneId = "room";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 2.0, .y = 0.0};
    placement.area.outline = {{.x = 4.4, .y = 3.25}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 5.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(wideDoorToPassageLayout(), scenario);
    runner.step(0.1);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    SC_EXPECT_TRUE(runner.frame().agents.front().velocity.x > 0.0);
}

SC_TEST(ScenarioSimulationRunnerTraversesStairConnectionBetweenFloors) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "room-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 0.5, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 10.0;

    safecrowd::domain::ScenarioSimulationRunner runner(twoFloorStairExitLayout(), scenario);
    for (int i = 0; i < 80 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, std::size_t{1});
}

SC_TEST(ScenarioSimulationRunnerUsesPlacementFloorForOverlappingCoordinates) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.floorId = "L2";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 1.0, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 5.0;

    safecrowd::domain::ScenarioSimulationRunner runner(overlappingTwoFloorExitLayout(), scenario);
    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L2"});

    runner.step(0.25);

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L2"});
    SC_EXPECT_TRUE(runner.frame().agents.front().velocity.x < 0.0);
}

SC_TEST(ScenarioSimulationRunnerDoesNotSlowAgentsOnDifferentFloorsAtSameCoordinates) {
    safecrowd::domain::InitialPlacement2D lower;
    lower.id = "agent-l1";
    lower.floorId = "L1";
    lower.zoneId = "room-l1";
    lower.targetAgentCount = 1;
    lower.initialVelocity = {.x = 1.5, .y = 0.0};
    lower.area.outline = {{.x = 1.0, .y = 1.0}};

    safecrowd::domain::InitialPlacement2D upper;
    upper.id = "agent-l2";
    upper.floorId = "L2";
    upper.zoneId = "room-l2";
    upper.targetAgentCount = 1;
    upper.initialVelocity = {.x = 1.5, .y = 0.0};
    upper.area.outline = {{.x = 1.0, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(lower);
    scenario.population.initialPlacements.push_back(upper);
    scenario.execution.timeLimitSeconds = 5.0;

    safecrowd::domain::ScenarioSimulationRunner runner(overlappingTwoFloorExitLayout(), scenario);
    // Social-force driving accelerates over the relaxation time tau (~0.5 s).
    // Step long enough for both agents to settle above 1 m/s, but short enough
    // that neither reaches its exit zone (which would zero the velocity).
    runner.step(0.8);

    const auto& agents = runner.frame().agents;
    SC_EXPECT_EQ(agents.size(), std::size_t{2});
    const auto& first = agents[0].floorId == "L1" ? agents[0] : agents[1];
    const auto& second = agents[0].floorId == "L2" ? agents[0] : agents[1];
    SC_EXPECT_EQ(first.floorId, std::string{"L1"});
    SC_EXPECT_EQ(second.floorId, std::string{"L2"});
    SC_EXPECT_TRUE(first.velocity.x > 1.0);
    SC_EXPECT_TRUE(second.velocity.x < -1.0);
}

SC_TEST(ScenarioSimulationRunnerHonorsAllowedStairEntryDirection) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "room-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 0.5, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 12.0;

    safecrowd::domain::ScenarioSimulationRunner runner(directedStairExitLayout(
        safecrowd::domain::StairEntryDirection::West,
        safecrowd::domain::StairEntryDirection::East), scenario);
    for (int i = 0; i < 120 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, std::size_t{1});
}

SC_TEST(ScenarioSimulationRunnerBlocksDisallowedStairEntryDirection) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "room-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 0.5, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 4.0;

    safecrowd::domain::ScenarioSimulationRunner runner(directedStairExitLayout(
        safecrowd::domain::StairEntryDirection::East,
        safecrowd::domain::StairEntryDirection::East), scenario);
    for (int i = 0; i < 40 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, std::size_t{0});
    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x < 2.0);
}

SC_TEST(ScenarioSimulationRunnerDisplaysNextFloorAfterCrossingStairMidpoint) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "stair-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 2.2, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 4.0;

    safecrowd::domain::ScenarioSimulationRunner runner(directedStairExitLayout(
        safecrowd::domain::StairEntryDirection::West,
        safecrowd::domain::StairEntryDirection::East), scenario);
    runner.step(0.55);

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L1"});

    runner.step(0.5);

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L2"});
}

SC_TEST(ScenarioSimulationRunnerBlocksMovementAcrossBarrierSegments) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "left";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 2.0, .y = 0.0};
    placement.area.outline = {{.x = 0.5, .y = 0.5}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 5.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(blockedDoorLayout(), scenario);
    runner.step(1.0);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x < 1.0);
}

SC_TEST(ScenarioSimulationRunnerRoutesAroundClosedObstructions) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 3.0, .y = 0.0};
    placement.area.outline = {{.x = 2.0, .y = 4.2}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 40.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    for (int i = 0; i < 160 && !runner.complete(); ++i) {
        runner.step(0.25);
    }

    SC_EXPECT_EQ(runner.frame().totalAgentCount, static_cast<std::size_t>(1));
    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, static_cast<std::size_t>(1));
}
