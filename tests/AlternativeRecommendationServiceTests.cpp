#include "TestSupport.h"
#include "domain/AlternativeRecommendationService.h"

#include <algorithm>
#include <utility>

using namespace safecrowd::domain;

namespace {

FacilityLayout2D makeRecommendationLayout() {
    FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room-a",
        .floorId = "L1",
        .kind = ZoneKind::Room,
        .label = "Room A",
    });
    layout.zones.push_back({
        .id = "exit-main",
        .floorId = "L1",
        .kind = ZoneKind::Exit,
        .label = "Main Exit",
    });
    layout.zones.push_back({
        .id = "exit-east",
        .floorId = "L1",
        .kind = ZoneKind::Exit,
        .label = "East Exit",
    });
    layout.connections.push_back({
        .id = "door-main",
        .floorId = "L1",
        .kind = ConnectionKind::Exit,
        .fromZoneId = "room-a",
        .toZoneId = "exit-main",
    });
    layout.connections.push_back({
        .id = "door-east",
        .floorId = "L1",
        .kind = ConnectionKind::Exit,
        .fromZoneId = "room-a",
        .toZoneId = "exit-east",
    });
    return layout;
}

ScenarioDraft makeScenario() {
    ScenarioDraft scenario;
    scenario.scenarioId = "scenario-1";
    scenario.name = "Scenario";
    scenario.role = ScenarioRole::Alternative;
    InitialPlacement2D placement;
    placement.id = "group-a";
    placement.zoneId = "room-a";
    placement.targetAgentCount = 20;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 120.0;
    scenario.execution.sampleIntervalSeconds = 1.0;
    return scenario;
}

ScenarioResultArtifacts makeCompletedArtifacts() {
    ScenarioResultArtifacts artifacts;
    artifacts.timingSummary.finalEvacuationTimeSeconds = 72.0;
    return artifacts;
}

ScenarioResultArtifacts makeExitUsageArtifacts(double mainRatio = 0.85, double eastRatio = 0.15) {
    ScenarioResultArtifacts artifacts = makeCompletedArtifacts();
    artifacts.exitUsage.push_back({
        .exitZoneId = "exit-main",
        .exitLabel = "Main Exit",
        .evacuatedCount = static_cast<std::size_t>(mainRatio * 20.0),
        .usageRatio = mainRatio,
    });
    artifacts.exitUsage.push_back({
        .exitZoneId = "exit-east",
        .exitLabel = "East Exit",
        .evacuatedCount = static_cast<std::size_t>(eastRatio * 20.0),
        .usageRatio = eastRatio,
    });
    return artifacts;
}

ScenarioResultArtifacts makeSingleExitUsageArtifacts(
    std::string exitZoneId,
    std::string exitLabel,
    std::size_t evacuatedCount,
    double usageRatio) {
    ScenarioResultArtifacts artifacts = makeCompletedArtifacts();
    artifacts.exitUsage.push_back({
        .exitZoneId = std::move(exitZoneId),
        .exitLabel = std::move(exitLabel),
        .evacuatedCount = evacuatedCount,
        .usageRatio = usageRatio,
    });
    return artifacts;
}

bool hasCandidateKind(
    const AlternativeRecommendationResult& result,
    AlternativeRecommendationKind kind) {
    return std::any_of(result.candidates.begin(), result.candidates.end(), [&](const auto& candidate) {
        return candidate.kind == kind;
    });
}

bool containsEvidenceLabel(
    const AlternativeRecommendationCandidate& candidate,
    const std::string& label) {
    return std::any_of(candidate.evidence.begin(), candidate.evidence.end(), [&](const auto& evidence) {
        return evidence.label == label;
    });
}

bool containsEvidenceSource(
    const AlternativeRecommendationCandidate& candidate,
    const std::string& source) {
    return std::any_of(candidate.evidence.begin(), candidate.evidence.end(), [&](const auto& evidence) {
        return evidence.source == source;
    });
}

bool containsDiffKey(const ScenarioDraft& scenario, const std::string& key) {
    return std::find(scenario.variationDiffKeys.begin(), scenario.variationDiffKeys.end(), key)
        != scenario.variationDiffKeys.end();
}

}  // namespace

SC_TEST(AlternativeRecommendationService_requiresCompletedResultEvidence) {
    auto scenario = makeScenario();
    ConnectionBlockDraft block;
    block.id = "block-main";
    block.connectionId = "door-main";
    scenario.control.connectionBlocks.push_back(block);

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
    });

    SC_EXPECT_TRUE(result.candidates.empty());
    SC_EXPECT_TRUE(!result.blockingReasons.empty());
}

SC_TEST(AlternativeRecommendationService_rejectsRiskSnapshotWithoutCompletedArtifacts) {
    auto scenario = makeScenario();
    scenario.control.connectionBlocks.push_back({
        .id = "block-main",
        .connectionId = "door-main",
    });
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .risk = risk,
    });

    SC_EXPECT_TRUE(result.candidates.empty());
    SC_EXPECT_TRUE(!result.blockingReasons.empty());
}

SC_TEST(AlternativeRecommendationService_removesBlockedConnectionInRecommendedDraft) {
    auto scenario = makeScenario();
    ConnectionBlockDraft block;
    block.id = "block-main";
    block.connectionId = "door-main";
    scenario.control.connectionBlocks.push_back(block);

    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .label = "Main Door",
        .floorId = "L1",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .risk = risk,
        .artifacts = makeCompletedArtifacts(),
    });

    SC_EXPECT_TRUE(!result.candidates.empty());
    const auto& candidate = result.candidates.front();
    SC_EXPECT_TRUE(candidate.kind == AlternativeRecommendationKind::BlockedConnectionRelief);
    SC_EXPECT_TRUE(candidate.recommendedScenario.role == ScenarioRole::Recommended);
    SC_EXPECT_TRUE(candidate.recommendedScenario.control.connectionBlocks.empty());
    SC_EXPECT_EQ(
        candidate.recommendedScenario.sourceTemplateId,
        std::string{"recommendation:blocked-connection-relief:scenario-1"});
    SC_EXPECT_TRUE(containsDiffKey(candidate.recommendedScenario, "control.connectionBlocks"));
}

SC_TEST(AlternativeRecommendationService_skipsBlockedConnectionWithoutBottleneckEvidence) {
    auto scenario = makeScenario();
    scenario.control.connectionBlocks.push_back({
        .id = "block-main",
        .connectionId = "door-main",
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .artifacts = makeCompletedArtifacts(),
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::BlockedConnectionRelief));
}

SC_TEST(AlternativeRecommendationService_reopensWorstBlockedBottleneck) {
    auto scenario = makeScenario();
    scenario.control.connectionBlocks.push_back({
        .id = "block-main",
        .connectionId = "door-main",
    });
    scenario.control.connectionBlocks.push_back({
        .id = "block-east",
        .connectionId = "door-east",
    });

    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-east",
        .nearbyAgentCount = 4,
        .stalledAgentCount = 1,
    });
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .risk = risk,
        .artifacts = makeCompletedArtifacts(),
    });

    SC_EXPECT_TRUE(!result.candidates.empty());
    const auto& candidate = result.candidates.front();
    SC_EXPECT_TRUE(candidate.kind == AlternativeRecommendationKind::BlockedConnectionRelief);
    SC_EXPECT_EQ(candidate.recommendedScenario.control.connectionBlocks.size(), std::size_t{1});
    SC_EXPECT_EQ(candidate.recommendedScenario.control.connectionBlocks.front().connectionId, std::string{"door-east"});
}

SC_TEST(AlternativeRecommendationService_addsRouteGuidanceForExitImbalance) {
    auto scenario = makeScenario();
    const auto artifacts = makeExitUsageArtifacts();

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .artifacts = artifacts,
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::ExitUsageBalancing;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().guidedExitZoneId, std::string{"exit-east"});
    SC_EXPECT_TRUE(it->recommendedScenario.control.routeGuidances.front().installConnectionId.empty());
    SC_EXPECT_NEAR(it->recommendedScenario.control.routeGuidances.front().baseComplianceRate, 0.5, 1e-9);
    SC_EXPECT_NEAR(it->recommendedScenario.control.routeGuidances.front().guidanceStrength, 0.55, 1e-9);
    SC_EXPECT_NEAR(it->recommendedScenario.control.routeGuidances.front().maxDetourMeters, 20.0, 1e-9);
    SC_EXPECT_TRUE(containsDiffKey(it->recommendedScenario, "control.routeGuidances"));
}

SC_TEST(AlternativeRecommendationService_balancesExitUsageTowardUnusedLayoutExit) {
    const auto artifacts = makeSingleExitUsageArtifacts("exit-main", "Main Exit", 20, 1.0);

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .artifacts = artifacts,
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::ExitUsageBalancing;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().guidedExitZoneId, std::string{"exit-east"});
    SC_EXPECT_TRUE(containsEvidenceSource(*it, "FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage"));
}

SC_TEST(AlternativeRecommendationService_skipsExitBalancingBelowThreshold) {
    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .artifacts = makeExitUsageArtifacts(0.60, 0.40),
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::ExitUsageBalancing));
}

SC_TEST(AlternativeRecommendationService_addsBottleneckGuidanceAtBottleneckConnection) {
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = risk,
        .artifacts = makeExitUsageArtifacts(),
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::BottleneckBypassGuidance;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().guidedExitZoneId, std::string{"exit-east"});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().installConnectionId, std::string{"door-main"});
    SC_EXPECT_TRUE(containsDiffKey(it->recommendedScenario, "control.routeGuidances"));
}

SC_TEST(AlternativeRecommendationService_guidesBottleneckAwayFromAdjacentLeastUsedExit) {
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-east",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = risk,
        .artifacts = makeExitUsageArtifacts(),
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::BottleneckBypassGuidance;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().guidedExitZoneId, std::string{"exit-main"});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().installConnectionId, std::string{"door-east"});
    SC_EXPECT_TRUE(containsEvidenceLabel(*it, "Excluded adjacent exits"));
    SC_EXPECT_TRUE(containsEvidenceSource(*it, "least-used non-adjacent exit from FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage"));
}

SC_TEST(AlternativeRecommendationService_guidesBottleneckTowardUnusedNonAdjacentLayoutExit) {
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-east",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });

    const auto artifacts = makeSingleExitUsageArtifacts("exit-east", "East Exit", 20, 1.0);

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = risk,
        .artifacts = artifacts,
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::BottleneckBypassGuidance;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().guidedExitZoneId, std::string{"exit-main"});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().installConnectionId, std::string{"door-east"});
    SC_EXPECT_TRUE(containsEvidenceSource(*it, "least-used non-adjacent exit from FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage"));
}

SC_TEST(AlternativeRecommendationService_requiresExitUsageForBottleneckGuidance) {
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = risk,
        .artifacts = makeCompletedArtifacts(),
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::BottleneckBypassGuidance));
}

SC_TEST(AlternativeRecommendationService_addsPressureHotspotReliefWithExitUsage) {
    auto artifacts = makeExitUsageArtifacts(0.55, 0.45);
    artifacts.pressureSummary.hotspotScoreThreshold = 4.0;
    artifacts.pressureSummary.peakPressureScore = 5.5;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .artifacts = artifacts,
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::PressureHotspotRelief;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().guidedExitZoneId, std::string{"exit-east"});
    SC_EXPECT_TRUE(it->recommendedScenario.control.routeGuidances.front().installConnectionId.empty());
    SC_EXPECT_TRUE(containsDiffKey(it->recommendedScenario, "control.routeGuidances"));
}

SC_TEST(AlternativeRecommendationService_prefersExitBalancingOverDuplicatePressureRelief) {
    auto artifacts = makeExitUsageArtifacts();
    artifacts.pressureSummary.hotspotScoreThreshold = 4.0;
    artifacts.pressureSummary.peakPressureScore = 5.5;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .artifacts = artifacts,
    });

    SC_EXPECT_TRUE(hasCandidateKind(result, AlternativeRecommendationKind::ExitUsageBalancing));
    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::PressureHotspotRelief));
}

SC_TEST(AlternativeRecommendationService_requiresExitUsageForPressureHotspotRelief) {
    auto artifacts = makeCompletedArtifacts();
    artifacts.pressureSummary.hotspotScoreThreshold = 4.0;
    artifacts.pressureSummary.peakPressureScore = 5.5;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .artifacts = artifacts,
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::PressureHotspotRelief));
}

SC_TEST(AlternativeRecommendationService_usesRiskPressureEvidenceWhenArtifactPeakMissing) {
    ScenarioRiskSnapshot risk;
    risk.criticalPressureAgentCount = 4;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = risk,
        .artifacts = makeExitUsageArtifacts(0.55, 0.45),
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::PressureHotspotRelief;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_TRUE(containsEvidenceLabel(*it, "Critical pressure agents"));
    SC_EXPECT_TRUE(!containsEvidenceLabel(*it, "Peak pressure"));
}

SC_TEST(AlternativeRecommendationService_sortsBlockedReliefBeforeGuidance) {
    auto scenario = makeScenario();
    scenario.control.connectionBlocks.push_back({
        .id = "block-main",
        .connectionId = "door-main",
    });
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });
    const auto artifacts = makeExitUsageArtifacts();

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .risk = risk,
        .artifacts = artifacts,
    });

    SC_EXPECT_TRUE(result.candidates.size() >= 2);
    SC_EXPECT_TRUE(result.candidates.front().kind == AlternativeRecommendationKind::BlockedConnectionRelief);
}
