#include "TestSupport.h"
#include "domain/ScenarioAuthoring.h"

#include <algorithm>

using namespace safecrowd::domain;
using safecrowd::tests::TestFailure;

namespace {

ScenarioDraft makeBaselineDraft() {
    ScenarioDraft draft;
    draft.scenarioId = "scenario-baseline";
    draft.name = "Baseline";
    draft.role = ScenarioRole::Baseline;
    draft.execution.timeLimitSeconds = 600.0;
    draft.execution.sampleIntervalSeconds = 1.0;
    draft.execution.repeatCount = 1;
    draft.execution.baseSeed = 42;
    draft.environment.familiarityProfile = "office";
    draft.environment.guidanceProfile = "trained";
    InitialPlacement2D placement;
    placement.id = "placement-1";
    placement.zoneId = "zone-a";
    placement.targetAgentCount = 100;
    draft.population.initialPlacements.push_back(placement);
    OperationalEventDraft event;
    event.id = "event-1";
    event.name = "Exit closed";
    draft.control.events.push_back(event);
    return draft;
}

bool containsKey(const std::vector<std::string>& keys, const std::string& key) {
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

EnvironmentHazardDraft makeSmokeHazard() {
    EnvironmentHazardDraft hazard;
    hazard.id = "hazard-1";
    hazard.kind = EnvironmentHazardKind::Smoke;
    hazard.name = "Smoke near lobby";
    hazard.affectedZoneId = "zone-a";
    hazard.floorId = "L1";
    hazard.position = {.x = 1.0, .y = 2.0};
    hazard.startSeconds = 5.0;
    hazard.endSeconds = 60.0;
    hazard.severity = ScenarioElementSeverity::High;
    hazard.note = "Visibility concept only";
    return hazard;
}

}  // namespace

SC_TEST(duplicateScenarioDraft_setsAlternativeRoleAndIdentity) {
    const auto baseline = makeBaselineDraft();

    const auto variant = duplicateScenarioDraft(baseline, "scenario-2", "My Alternative");

    SC_EXPECT_TRUE(variant.role == ScenarioRole::Alternative);
    SC_EXPECT_EQ(variant.scenarioId, std::string("scenario-2"));
    SC_EXPECT_EQ(variant.name, std::string("My Alternative"));
    SC_EXPECT_TRUE(variant.variationDiffKeys.empty());
    SC_EXPECT_TRUE(variant.blockingIssues.empty());
    SC_EXPECT_EQ(variant.population.initialPlacements.size(), baseline.population.initialPlacements.size());
    SC_EXPECT_EQ(variant.control.events.size(), baseline.control.events.size());
    SC_EXPECT_EQ(variant.execution.baseSeed, baseline.execution.baseSeed);
}

SC_TEST(duplicateScenarioDraft_doesNotMutateSource) {
    auto baseline = makeBaselineDraft();
    baseline.environment.hazards.push_back(makeSmokeHazard());
    const auto originalEventCount = baseline.control.events.size();
    const auto originalPlacementCount = baseline.population.initialPlacements.size();
    const auto originalHazardCount = baseline.environment.hazards.size();
    const auto originalRole = baseline.role;
    const auto originalId = baseline.scenarioId;

    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.control.events.clear();
    variant.population.initialPlacements.clear();
    variant.environment.hazards.clear();
    variant.execution.timeLimitSeconds = 1.0;

    SC_EXPECT_EQ(baseline.control.events.size(), originalEventCount);
    SC_EXPECT_EQ(baseline.population.initialPlacements.size(), originalPlacementCount);
    SC_EXPECT_EQ(baseline.environment.hazards.size(), originalHazardCount);
    SC_EXPECT_TRUE(baseline.role == originalRole);
    SC_EXPECT_EQ(baseline.scenarioId, originalId);
    SC_EXPECT_NEAR(baseline.execution.timeLimitSeconds, 600.0, 1e-9);
}

SC_TEST(environmentHazardRuntimeProfile_UsesSharedSeverityAndScheduleRules) {
    auto hazard = makeSmokeHazard();
    hazard.startSeconds = 10.0;
    hazard.endSeconds = 10.0;

    const auto profile = environmentHazardRuntimeProfile(hazard);

    SC_EXPECT_NEAR(profile.radiusMeters, 5.0, 1e-9);
    SC_EXPECT_NEAR(profile.speedFactor, 0.65 / 1.5, 1e-9);
    SC_EXPECT_NEAR(profile.routePenaltyMeters, 150.0, 1e-9);
    SC_EXPECT_NEAR(profile.severityWeight, 3.0, 1e-9);
    SC_EXPECT_TRUE(environmentHazardHasOpenEndedSchedule(hazard));
    SC_EXPECT_TRUE(!environmentHazardActiveAt(hazard, 9.9));
    SC_EXPECT_TRUE(environmentHazardActiveAt(hazard, 10.0));
    SC_EXPECT_TRUE(environmentHazardActiveAt(hazard, 120.0));

    hazard.endSeconds = 20.0;
    SC_EXPECT_TRUE(!environmentHazardHasOpenEndedSchedule(hazard));
}

SC_TEST(environmentHazardSmokeSpeed_UsesVisibilityBasedPathfinderRule) {
    auto hazard = makeSmokeHazard();
    hazard.severity = ScenarioElementSeverity::High;

    SC_EXPECT_NEAR(environmentHazardSmokeVisibilityMetersAt(hazard, 0.0), 0.5, 1e-9);
    SC_EXPECT_NEAR(environmentHazardSmokeVisibilityMetersAt(hazard, 5.0), 3.0, 1e-9);
    SC_EXPECT_NEAR(environmentHazardSmokeSpeedMetersPerSecond(1.5, 0.5), 0.65, 1e-9);
    SC_EXPECT_NEAR(environmentHazardSpeedFactorAt(hazard, 0.0, 1.5), 0.65 / 1.5, 1e-9);
    SC_EXPECT_NEAR(environmentHazardSpeedFactorAt(hazard, 5.0, 1.5), 1.0, 1e-9);
}

SC_TEST(environmentHazardFloorId_FallsBackToAffectedZoneFloor) {
    FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "zone-a",
        .floorId = "L2",
    });
    auto hazard = makeSmokeHazard();
    hazard.floorId.clear();

    SC_EXPECT_EQ(environmentHazardFloorId(layout, hazard), std::string{"L2"});

    hazard.floorId = "Manual";
    SC_EXPECT_EQ(environmentHazardFloorId(layout, hazard), std::string{"Manual"});
}

SC_TEST(computeScenarioDiffKeys_returnsEmptyForFreshDuplicate) {
    const auto baseline = makeBaselineDraft();
    const auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_TRUE(keys.empty());
}

SC_TEST(computeScenarioDiffKeys_detectsPopulationChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.population.initialPlacements[0].targetAgentCount = 250;

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "population.placements"));
}

SC_TEST(computeScenarioDiffKeys_detectsPlacementAreaChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.population.initialPlacements[0].area.outline.push_back({1.0, 2.0});

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "population.placements"));
}

SC_TEST(computeScenarioDiffKeys_detectsPlacementVelocityChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.population.initialPlacements[0].initialVelocity = {0.25, 0.5};

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "population.placements"));
}

SC_TEST(computeScenarioDiffKeys_detectsEnvironmentChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.environment.reducedVisibility = true;

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "environment.reducedVisibility"));
}

SC_TEST(computeScenarioDiffKeys_detectsFamiliarityProfileChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.environment.familiarityProfile = "visitor";

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "environment.familiarityProfile"));
}

SC_TEST(computeScenarioDiffKeys_detectsGuidanceProfileChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.environment.guidanceProfile = "untrained";

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "environment.guidanceProfile"));
}

SC_TEST(computeScenarioDiffKeys_detectsEnvironmentHazardsChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.environment.hazards.push_back(makeSmokeHazard());

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "environment.hazards"));
}

SC_TEST(computeScenarioDiffKeys_detectsEnvironmentHazardDetailChange) {
    auto baseline = makeBaselineDraft();
    baseline.environment.hazards.push_back(makeSmokeHazard());
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.environment.hazards[0].position = {.x = 3.0, .y = 4.0};
    variant.environment.hazards[0].severity = ScenarioElementSeverity::Medium;
    variant.environment.hazards[0].note = "Edited";

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "environment.hazards"));
}

SC_TEST(computeScenarioDiffKeys_detectsControlEventsChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    OperationalEventDraft extra;
    extra.id = "event-extra";
    extra.name = "Counterflow guidance";
    variant.control.events.push_back(extra);

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "control.events"));
}

SC_TEST(computeScenarioDiffKeys_detectsExecutionChanges) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.execution.timeLimitSeconds = 900.0;
    variant.execution.repeatCount = 5;
    variant.execution.baseSeed = baseline.execution.baseSeed + 1;

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_TRUE(containsKey(keys, "execution.timeLimit"));
    SC_EXPECT_TRUE(containsKey(keys, "execution.repeatCount"));
    SC_EXPECT_TRUE(containsKey(keys, "execution.baseSeed"));
    SC_EXPECT_EQ(keys.size(), std::size_t{3});
}

SC_TEST(computeScenarioDiffKeys_detectsSampleIntervalChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.execution.sampleIntervalSeconds = 0.5;

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "execution.sampleInterval"));
}

SC_TEST(computeScenarioDiffKeys_detectsRecordOccupantHistoryChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    variant.execution.recordOccupantHistory = true;

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "execution.recordOccupantHistory"));
}

SC_TEST(computeScenarioDiffKeys_detectsConnectionBlockChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    ConnectionBlockDraft block;
    block.id = "block-1";
    block.connectionId = "door-east";
    block.intervals.push_back({0.0, 60.0});
    variant.control.connectionBlocks.push_back(block);

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "control.connectionBlocks"));
}

SC_TEST(computeScenarioDiffKeys_detectsRouteGuidanceChange) {
    const auto baseline = makeBaselineDraft();
    auto variant = duplicateScenarioDraft(baseline, "scenario-2", "Variant");
    RouteGuidanceDraft guidance;
    guidance.id = "guidance-1";
    guidance.guidedExitZoneId = "exit-east";
    variant.control.routeGuidances.push_back(guidance);

    const auto keys = computeScenarioDiffKeys(baseline, variant);

    SC_EXPECT_EQ(keys.size(), std::size_t{1});
    SC_EXPECT_TRUE(containsKey(keys, "control.routeGuidances"));
}
