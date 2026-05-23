#include "TestSupport.h"

#include <cstdint>

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

SC_TEST(ScenarioBatchRunnerExpandsRepeatCountIntoSeededRuns) {
    auto repeated = scenarioDraft("scenario-1", "Repeated", 4.0, 1.0);
    repeated.execution.repeatCount = 3;
    repeated.execution.baseSeed = 10;

    safecrowd::domain::ScenarioBatchRunner batch(twoRoomExitLayout(), {repeated});

    SC_EXPECT_EQ(batch.size(), std::size_t{3});
    SC_EXPECT_EQ(batch.run(0).sourceScenarioIndex, std::size_t{0});
    SC_EXPECT_EQ(batch.run(1).sourceScenarioIndex, std::size_t{0});
    SC_EXPECT_EQ(batch.run(2).sourceScenarioIndex, std::size_t{0});
    SC_EXPECT_EQ(batch.run(0).repeatIndex, std::uint32_t{1});
    SC_EXPECT_EQ(batch.run(1).repeatIndex, std::uint32_t{2});
    SC_EXPECT_EQ(batch.run(2).repeatIndex, std::uint32_t{3});
    SC_EXPECT_EQ(batch.run(0).repeatCount, std::uint32_t{3});
    SC_EXPECT_EQ(batch.run(0).runSeed, std::uint32_t{10});
    SC_EXPECT_EQ(batch.run(1).runSeed, std::uint32_t{11});
    SC_EXPECT_EQ(batch.run(2).runSeed, std::uint32_t{12});
    SC_EXPECT_EQ(batch.run(0).scenario.execution.baseSeed, std::uint32_t{10});
    SC_EXPECT_EQ(batch.run(1).scenario.execution.baseSeed, std::uint32_t{11});
    SC_EXPECT_EQ(batch.run(2).scenario.execution.baseSeed, std::uint32_t{12});
    SC_EXPECT_EQ(batch.run(0).scenario.execution.repeatCount, std::uint32_t{1});
    SC_EXPECT_EQ(batch.run(0).scenario.scenarioId, std::string{"scenario-1-repeat-1"});
    SC_EXPECT_TRUE(batch.run(0).scenario.name.find("run 1/3") != std::string::npos);
}

SC_TEST(ScenarioBatchRunnerKeepsSingleRunIdentityWhenRepeatCountIsOne) {
    auto single = scenarioDraft("scenario-1", "Single", 4.0, 1.0);
    single.execution.repeatCount = 1;
    single.execution.baseSeed = 42;

    safecrowd::domain::ScenarioBatchRunner batch(twoRoomExitLayout(), {single});

    SC_EXPECT_EQ(batch.size(), std::size_t{1});
    SC_EXPECT_EQ(batch.run(0).scenario.scenarioId, std::string{"scenario-1"});
    SC_EXPECT_EQ(batch.run(0).scenario.name, std::string{"Single"});
    SC_EXPECT_EQ(batch.run(0).runSeed, std::uint32_t{42});
    SC_EXPECT_EQ(batch.run(0).repeatIndex, std::uint32_t{1});
    SC_EXPECT_EQ(batch.run(0).repeatCount, std::uint32_t{1});
}

SC_TEST(ScenarioBatchRunnerClampsRepeatCountAtDomainLimit) {
    auto repeated = scenarioDraft("scenario-1", "Repeated", 4.0, 1.0);
    repeated.execution.repeatCount = safecrowd::domain::kScenarioExecutionMaxRepeatCount + 5;
    repeated.execution.baseSeed = 100;

    safecrowd::domain::ScenarioBatchRunner batch(twoRoomExitLayout(), {repeated});

    SC_EXPECT_EQ(batch.size(), static_cast<std::size_t>(safecrowd::domain::kScenarioExecutionMaxRepeatCount));
    SC_EXPECT_EQ(batch.run(0).repeatCount, safecrowd::domain::kScenarioExecutionMaxRepeatCount);
    SC_EXPECT_EQ(batch.run(batch.size() - 1).repeatIndex, safecrowd::domain::kScenarioExecutionMaxRepeatCount);
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
