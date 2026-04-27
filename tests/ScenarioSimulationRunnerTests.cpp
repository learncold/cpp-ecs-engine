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
    placement.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::ExitCorridorZoneId;
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
    runner.step(0.1);

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
