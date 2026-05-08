#include "TestSupport.h"

#include "domain/ScenarioBatchRunner.h"

namespace {

safecrowd::domain::FacilityLayout2D twoRoomExitLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "room-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "exit",
        .effectiveWidth = 1.2,
        .centerSpan = {{2.0, 0.5}, {2.0, 1.5}},
    });
    return layout;
}

safecrowd::domain::ScenarioDraft scenarioDraft(
    std::string id,
    std::string name,
    double timeLimitSeconds,
    double speed) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = id + "-placement";
    placement.zoneId = "room";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = speed, .y = 0.0};
    placement.area.outline = {{.x = 0.5, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.scenarioId = std::move(id);
    scenario.name = std::move(name);
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = timeLimitSeconds;
    scenario.execution.sampleIntervalSeconds = 0.5;
    return scenario;
}

}  // namespace

SC_TEST(ScenarioBatchRunnerAdvancesMultipleScenariosOnSameTick) {
    auto first = scenarioDraft("scenario-1", "Fast", 4.0, 2.0);
    auto second = scenarioDraft("scenario-2", "Slow", 4.0, 0.8);

    safecrowd::domain::ScenarioBatchRunner batch(twoRoomExitLayout(), {first, second});
    batch.step(0.25);

    SC_EXPECT_EQ(batch.size(), std::size_t{2});
    SC_EXPECT_TRUE(batch.run(0).frame.elapsedSeconds > 0.0);
    SC_EXPECT_TRUE(batch.run(1).frame.elapsedSeconds > 0.0);
    SC_EXPECT_EQ(batch.run(0).scenario.scenarioId, std::string{"scenario-1"});
    SC_EXPECT_EQ(batch.run(1).scenario.scenarioId, std::string{"scenario-2"});
}

SC_TEST(ScenarioBatchRunnerContinuesUnfinishedRunsAfterOneCompletes) {
    auto shortRun = scenarioDraft("scenario-1", "Short", 0.2, 0.5);
    auto longRun = scenarioDraft("scenario-2", "Long", 1.0, 0.5);

    safecrowd::domain::ScenarioBatchRunner batch(twoRoomExitLayout(), {shortRun, longRun});
    batch.step(0.3);

    SC_EXPECT_TRUE(batch.run(0).complete);
    SC_EXPECT_TRUE(!batch.run(1).complete);
    const auto elapsedBefore = batch.run(1).frame.elapsedSeconds;

    batch.step(0.3);

    SC_EXPECT_TRUE(batch.run(1).frame.elapsedSeconds > elapsedBefore);
}

SC_TEST(ScenarioBatchRunnerKeepsResultArtifactsForCompletedRuns) {
    auto first = scenarioDraft("scenario-1", "First", 1.0, 2.0);
    auto second = scenarioDraft("scenario-2", "Second", 1.0, 1.5);

    safecrowd::domain::ScenarioBatchRunner batch(twoRoomExitLayout(), {first, second});
    for (int index = 0; index < 20 && !batch.complete(); ++index) {
        batch.step(0.1);
    }

    SC_EXPECT_TRUE(batch.complete());
    SC_EXPECT_TRUE(!batch.run(0).artifacts.evacuationProgress.empty());
    SC_EXPECT_TRUE(!batch.run(1).artifacts.evacuationProgress.empty());
}
