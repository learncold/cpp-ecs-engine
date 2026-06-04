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

FacilityLayout2D makeCorridorRecommendationLayout() {
    auto layout = makeRecommendationLayout();
    layout.zones.push_back({
        .id = "hall-a",
        .floorId = "L1",
        .kind = ZoneKind::Room,
        .label = "Hall A",
    });
    layout.connections.push_back({
        .id = "hall-main",
        .floorId = "L1",
        .kind = ConnectionKind::Opening,
        .fromZoneId = "room-a",
        .toZoneId = "hall-a",
    });
    return layout;
}

FacilityLayout2D makeSingleExitRecommendationLayout() {
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
    layout.connections.push_back({
        .id = "door-main",
        .floorId = "L1",
        .kind = ConnectionKind::Exit,
        .fromZoneId = "room-a",
        .toZoneId = "exit-main",
    });
    return layout;
}

FacilityLayout2D makeUnreachableExitRecommendationLayout() {
    FacilityLayout2D layout = makeSingleExitRecommendationLayout();
    layout.zones.push_back({
        .id = "exit-east",
        .floorId = "L1",
        .kind = ZoneKind::Exit,
        .label = "East Exit",
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
    placement.floorId = "L1";
    placement.area.outline = {{.x = 0.5, .y = 0.5}};
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

ScenarioRiskSnapshot makeCrossFlowRisk() {
    ScenarioRiskSnapshot risk;
    risk.peakCrossFlowScore = 0.78;
    risk.totalCrossFlowExposureAgentSeconds = 22.5;
    risk.crossFlowAgentCount = 7;
    risk.crossFlowCells.push_back({
        .center = {.x = 1.0, .y = 0.5},
        .cellMin = {.x = 0.0, .y = 0.0},
        .cellMax = {.x = 2.0, .y = 2.0},
        .floorId = "L1",
        .movingAgentCount = 7,
        .peakAgentCount = 7,
        .primaryFlowCount = 4,
        .crossFlowCount = 3,
        .crossFlowRatio = 3.0 / 7.0,
        .averageSpeed = 0.55,
        .speedDropRatio = 0.58,
        .crossFlowScore = 0.78,
        .durationSeconds = 14.0,
        .exposureAgentSeconds = 22.5,
    });
    return risk;
}

ScenarioResultArtifacts makeCrossFlowArtifacts() {
    ScenarioResultArtifacts artifacts = makeCompletedArtifacts();
    artifacts.crossFlowSummary.peakCrossFlowScore = 0.78;
    artifacts.crossFlowSummary.totalCrossFlowExposureAgentSeconds = 22.5;
    artifacts.crossFlowSummary.longestCrossFlowDurationSeconds = 14.0;
    artifacts.crossFlowSummary.crossFlowHotspotCount = 1;
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

bool hasRiskSignalKind(
    const AlternativeRecommendationResult& result,
    AlternativeRecommendationRiskKind kind) {
    return std::any_of(result.riskSignals.begin(), result.riskSignals.end(), [&](const auto& signal) {
        return signal.kind == kind;
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

bool containsEvidenceSource(
    const AlternativeRecommendationRiskSignal& signal,
    const std::string& source) {
    return std::any_of(signal.evidence.begin(), signal.evidence.end(), [&](const auto& evidence) {
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

SC_TEST(AlternativeRecommendationService_skipsInactiveBlockedConnectionRelief) {
    auto scenario = makeScenario();
    scenario.control.connectionBlocks.push_back({
        .id = "block-main",
        .connectionId = "door-main",
        .intervals = {{.startSeconds = 0.0, .endSeconds = 5.0}},
    });

    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
        .detectedAtSeconds = 20.0,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .risk = risk,
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
    const auto& guidance = it->recommendedScenario.control.routeGuidances.front();
    SC_EXPECT_EQ(guidance.guidedExitZoneId, std::string{"exit-east"});
    SC_EXPECT_TRUE(guidance.installConnectionId.empty());
    SC_EXPECT_EQ(guidance.installFloorId, std::string{"L1"});
    SC_EXPECT_NEAR(guidance.installPosition.x, 0.5, 1e-9);
    SC_EXPECT_NEAR(guidance.installPosition.y, 0.5, 1e-9);
    SC_EXPECT_TRUE(guidance.baseComplianceRate > 0.5);
    SC_EXPECT_TRUE(guidance.baseComplianceRate <= 0.85);
    SC_EXPECT_TRUE(guidance.influenceRadiusMeters > 2.5);
    SC_EXPECT_TRUE(guidance.maxDetourMeters > 20.0);
    SC_EXPECT_TRUE(containsEvidenceLabel(*it, "Guidance install"));
    SC_EXPECT_TRUE(containsEvidenceLabel(*it, "Guidance tuning"));
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
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().installFloorId, std::string{"L1"});
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

SC_TEST(AlternativeRecommendationService_skipsExitBalancingForExistingGuidanceToTargetExit) {
    auto scenario = makeScenario();
    scenario.control.routeGuidances.push_back({
        .id = "manual-guidance-east",
        .guidedExitZoneId = "exit-east",
        .installConnectionId = "door-east",
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .artifacts = makeExitUsageArtifacts(),
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::ExitUsageBalancing));
}

SC_TEST(AlternativeRecommendationService_skipsUnreachableUnusedExitBalancing) {
    const auto artifacts = makeSingleExitUsageArtifacts("exit-main", "Main Exit", 20, 1.0);

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeUnreachableExitRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .artifacts = artifacts,
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::ExitUsageBalancing));
}

SC_TEST(AlternativeRecommendationService_addsBottleneckGuidanceAtExit) {
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
    SC_EXPECT_TRUE(it->recommendedScenario.control.routeGuidances.front().influenceRadiusMeters > 2.5);
    SC_EXPECT_TRUE(containsEvidenceLabel(*it, "Guidance install"));
    SC_EXPECT_TRUE(containsDiffKey(it->recommendedScenario, "control.routeGuidances"));
}

SC_TEST(AlternativeRecommendationService_skipsBottleneckGuidanceForSingleExitLayout) {
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeSingleExitRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = risk,
        .artifacts = makeSingleExitUsageArtifacts("exit-main", "Main Exit", 20, 1.0),
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::BottleneckBypassGuidance));
    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::ExitUsageBalancing));
}

SC_TEST(AlternativeRecommendationService_keepsInactiveTimedBlockReachableForBottleneckGuidance) {
    auto scenario = makeScenario();
    scenario.control.connectionBlocks.push_back({
        .id = "block-main-early",
        .connectionId = "door-main",
        .intervals = {{.startSeconds = 0.0, .endSeconds = 5.0}},
    });

    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
        .detectedAtSeconds = 20.0,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .risk = risk,
        .artifacts = makeExitUsageArtifacts(),
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::BlockedConnectionRelief));
    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::BottleneckBypassGuidance;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().guidedExitZoneId, std::string{"exit-east"});
}

SC_TEST(AlternativeRecommendationService_installsCorridorBottleneckGuidanceAtExitOnly) {
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "hall-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeCorridorRecommendationLayout(),
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
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.front().installConnectionId, std::string{"hall-main"});
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
    artifacts.pressureSummary.peakCell = PressureCellMetric{
        .center = {.x = 1.25, .y = 0.75},
        .floorId = "L1",
        .agentCount = 6,
        .pressureScore = 5.5,
    };

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
    const auto& guidance = it->recommendedScenario.control.routeGuidances.front();
    SC_EXPECT_EQ(guidance.guidedExitZoneId, std::string{"exit-east"});
    SC_EXPECT_TRUE(guidance.installConnectionId.empty());
    SC_EXPECT_EQ(guidance.installFloorId, std::string{"L1"});
    SC_EXPECT_NEAR(guidance.installPosition.x, 1.25, 1e-9);
    SC_EXPECT_NEAR(guidance.installPosition.y, 0.75, 1e-9);
    SC_EXPECT_TRUE(guidance.influenceRadiusMeters > 2.5);
    SC_EXPECT_TRUE(containsEvidenceSource(*it, "ScenarioResultArtifacts.pressureSummary.peakCell"));
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

SC_TEST(AlternativeRecommendationService_skipsPressureHotspotReliefForSingleExitLayout) {
    auto artifacts = makeSingleExitUsageArtifacts("exit-main", "Main Exit", 20, 1.0);
    artifacts.pressureSummary.hotspotScoreThreshold = 4.0;
    artifacts.pressureSummary.peakPressureScore = 8.7;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeSingleExitRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .artifacts = artifacts,
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::PressureHotspotRelief));
}

SC_TEST(AlternativeRecommendationService_skipsPressureHotspotReliefTowardBottleneckExit) {
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-east",
        .nearbyAgentCount = 10,
        .stalledAgentCount = 4,
        .averageSpeed = 0.2,
    });

    auto artifacts = makeExitUsageArtifacts(0.55, 0.45);
    artifacts.pressureSummary.hotspotScoreThreshold = 4.0;
    artifacts.pressureSummary.peakPressureScore = 8.7;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = risk,
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

SC_TEST(AlternativeRecommendationService_reportsExitAndCorridorBottleneckRiskSignals) {
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "door-main",
        .nearbyAgentCount = 8,
        .stalledAgentCount = 5,
        .averageSpeed = 0.2,
    });
    risk.bottlenecks.push_back({
        .connectionId = "hall-main",
        .nearbyAgentCount = 6,
        .stalledAgentCount = 4,
        .averageSpeed = 0.3,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeCorridorRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = risk,
        .artifacts = makeCompletedArtifacts(),
    });

    SC_EXPECT_TRUE(hasRiskSignalKind(result, AlternativeRecommendationRiskKind::ExitBottleneck));
    SC_EXPECT_TRUE(hasRiskSignalKind(result, AlternativeRecommendationRiskKind::CorridorBottleneck));
}

SC_TEST(AlternativeRecommendationService_doesNotAddOneWayOperationForCorridorBottleneckAlone) {
    ScenarioRiskSnapshot risk;
    risk.bottlenecks.push_back({
        .connectionId = "hall-main",
        .nearbyAgentCount = 6,
        .stalledAgentCount = 4,
        .averageSpeed = 0.3,
    });

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeCorridorRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = risk,
        .artifacts = makeCompletedArtifacts(),
    });

    SC_EXPECT_TRUE(hasRiskSignalKind(result, AlternativeRecommendationRiskKind::CorridorBottleneck));
    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::CorridorOneWayFlow));
}

SC_TEST(AlternativeRecommendationService_suppressesOneWayOperationForCrossFlow) {
    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = makeCrossFlowRisk(),
        .artifacts = makeCrossFlowArtifacts(),
    });

    SC_EXPECT_TRUE(hasRiskSignalKind(result, AlternativeRecommendationRiskKind::CrossFlow));
    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::CorridorOneWayFlow));
    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::CrossFlowSeparation));
}

SC_TEST(AlternativeRecommendationService_reportsCrossFlowRiskWithoutManualOnlyDraft) {
    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = makeCrossFlowRisk(),
        .artifacts = makeCrossFlowArtifacts(),
    });

    const auto signalIt = std::find_if(result.riskSignals.begin(), result.riskSignals.end(), [](const auto& signal) {
        return signal.kind == AlternativeRecommendationRiskKind::CrossFlow;
    });
    SC_EXPECT_TRUE(signalIt != result.riskSignals.end());
    SC_EXPECT_TRUE(containsEvidenceSource(*signalIt, "ScenarioRiskSnapshot.crossFlowCells"));
    SC_EXPECT_TRUE(containsEvidenceSource(*signalIt, "ScenarioResultArtifacts.crossFlowSummary"));
    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::CrossFlowSeparation));
}

SC_TEST(AlternativeRecommendationService_convertsCrossFlowToRouteGuidanceWhenExitDataAllows) {
    auto artifacts = makeCrossFlowArtifacts();
    artifacts.exitUsage = makeExitUsageArtifacts(0.55, 0.45).exitUsage;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = makeCrossFlowRisk(),
        .artifacts = artifacts,
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::CrossFlowSeparation;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_EQ(it->recommendedScenario.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_TRUE(it->recommendedScenario.control.events.empty());
    const auto& guidance = it->recommendedScenario.control.routeGuidances.front();
    SC_EXPECT_EQ(guidance.guidedExitZoneId, std::string{"exit-east"});
    SC_EXPECT_EQ(guidance.installFloorId, std::string{"L1"});
    SC_EXPECT_NEAR(guidance.installPosition.x, 1.0, 1e-9);
    SC_EXPECT_NEAR(guidance.installPosition.y, 0.5, 1e-9);
    SC_EXPECT_TRUE(containsEvidenceLabel(*it, "Guidance install"));
    SC_EXPECT_TRUE(containsDiffKey(it->recommendedScenario, "control.routeGuidances"));
}

SC_TEST(AlternativeRecommendationService_convertsCrossFlowToStagedReleaseWhenNoExitUsageExists) {
    auto scenario = makeScenario();
    auto second = scenario.population.initialPlacements.front();
    second.id = "group-b";
    second.targetAgentCount = 15;
    scenario.population.initialPlacements.push_back(second);

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .risk = makeCrossFlowRisk(),
        .artifacts = makeCrossFlowArtifacts(),
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::CrossFlowSeparation;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_TRUE(it->recommendedScenario.control.events.empty());
    SC_EXPECT_TRUE(it->recommendedScenario.population.initialPlacements.empty());
    SC_EXPECT_EQ(it->recommendedScenario.population.occupantSources.size(), std::size_t{2});
    SC_EXPECT_EQ(it->recommendedScenario.population.occupantSources.front().agentsPerSpawn, std::size_t{4});
    SC_EXPECT_TRUE(it->recommendedScenario.population.occupantSources.front().spawnIntervalSeconds > 9.0);
    SC_EXPECT_TRUE(containsDiffKey(it->recommendedScenario, "population.placements"));
}

SC_TEST(AlternativeRecommendationService_skipsManualCrossFlowFallbackWhenExitBalancingExists) {
    auto artifacts = makeCrossFlowArtifacts();
    artifacts.exitUsage = makeExitUsageArtifacts(0.90, 0.10).exitUsage;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = makeScenario(),
        .risk = makeCrossFlowRisk(),
        .artifacts = artifacts,
    });

    SC_EXPECT_EQ(result.candidates.size(), std::size_t{1});
    SC_EXPECT_TRUE(result.candidates.front().kind == AlternativeRecommendationKind::ExitUsageBalancing);
    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::CrossFlowSeparation));
}

SC_TEST(AlternativeRecommendationService_addsStagedEvacuationForMissedTimeLimit) {
    auto scenario = makeScenario();
    scenario.execution.timeLimitSeconds = 120.0;
    auto artifacts = makeCompletedArtifacts();
    artifacts.timingSummary.targetTimeSeconds = 120.0;
    artifacts.timingSummary.finalEvacuationTimeSeconds = 132.0;
    artifacts.timingSummary.marginSeconds = -12.0;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeRecommendationLayout(),
        .sourceScenario = scenario,
        .artifacts = artifacts,
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::StagedEvacuation;
    });
    SC_EXPECT_TRUE(hasRiskSignalKind(result, AlternativeRecommendationRiskKind::TimeLimitMissed));
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_TRUE(it->riskKind.has_value() && *it->riskKind == AlternativeRecommendationRiskKind::TimeLimitMissed);
    SC_EXPECT_TRUE(it->recommendedScenario.population.initialPlacements.empty());
    SC_EXPECT_EQ(it->recommendedScenario.population.occupantSources.size(), std::size_t{1});
    const auto& source = it->recommendedScenario.population.occupantSources.front();
    SC_EXPECT_EQ(source.targetAgentCount, std::size_t{20});
    SC_EXPECT_EQ(source.agentsPerSpawn, std::size_t{8});
    SC_EXPECT_NEAR(source.spawnIntervalSeconds, 3.525, 1e-9);
    SC_EXPECT_NEAR(source.startSeconds, 0.0, 1e-9);
    SC_EXPECT_NEAR(source.endSeconds, 10.575, 1e-9);
    SC_EXPECT_TRUE(containsDiffKey(it->recommendedScenario, "population.placements"));
}

SC_TEST(AlternativeRecommendationService_stagesLargeInitialPlacementForPressureHotspot) {
    auto scenario = makeScenario();
    scenario.population.initialPlacements.front().targetAgentCount = 100;

    auto artifacts = makeSingleExitUsageArtifacts("exit-main", "Main Exit", 100, 1.0);
    artifacts.pressureSummary.hotspotScoreThreshold = 4.0;
    artifacts.pressureSummary.peakPressureScore = 8.7;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeSingleExitRecommendationLayout(),
        .sourceScenario = scenario,
        .artifacts = artifacts,
    });

    SC_EXPECT_TRUE(!hasCandidateKind(result, AlternativeRecommendationKind::PressureHotspotRelief));
    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::StagedEvacuation;
    });
    SC_EXPECT_TRUE(hasRiskSignalKind(result, AlternativeRecommendationRiskKind::PressureHotspot));
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_TRUE(it->riskKind.has_value() && *it->riskKind == AlternativeRecommendationRiskKind::PressureHotspot);
    SC_EXPECT_TRUE(it->recommendedScenario.population.initialPlacements.empty());
    SC_EXPECT_EQ(it->recommendedScenario.population.occupantSources.size(), std::size_t{1});
    const auto& source = it->recommendedScenario.population.occupantSources.front();
    SC_EXPECT_EQ(source.zoneId, std::string{"room-a"});
    SC_EXPECT_EQ(source.floorId, std::string{"L1"});
    SC_EXPECT_EQ(source.targetAgentCount, std::size_t{100});
    SC_EXPECT_EQ(source.agentsPerSpawn, std::size_t{16});
    SC_EXPECT_NEAR(source.spawnIntervalSeconds, 6.80625, 1e-9);
    SC_EXPECT_NEAR(source.startSeconds, 0.0, 1e-9);
    SC_EXPECT_NEAR(source.endSeconds, 47.64375, 1e-9);
    SC_EXPECT_TRUE(containsEvidenceLabel(*it, "Release schedule"));
    SC_EXPECT_TRUE(containsDiffKey(it->recommendedScenario, "population.placements"));
}

SC_TEST(AlternativeRecommendationService_stagesAllInitialPlacementsInOrder) {
    auto scenario = makeScenario();
    scenario.population.initialPlacements.front().targetAgentCount = 25;
    InitialPlacement2D second = scenario.population.initialPlacements.front();
    second.id = "group-b";
    second.area.outline = {{.x = 1.5, .y = 0.5}};
    second.targetAgentCount = 25;
    scenario.population.initialPlacements.push_back(second);

    auto artifacts = makeSingleExitUsageArtifacts("exit-main", "Main Exit", 50, 1.0);
    artifacts.pressureSummary.hotspotScoreThreshold = 4.0;
    artifacts.pressureSummary.peakPressureScore = 8.7;

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = makeSingleExitRecommendationLayout(),
        .sourceScenario = scenario,
        .artifacts = artifacts,
    });

    const auto it = std::find_if(result.candidates.begin(), result.candidates.end(), [](const auto& candidate) {
        return candidate.kind == AlternativeRecommendationKind::StagedEvacuation;
    });
    SC_EXPECT_TRUE(it != result.candidates.end());
    SC_EXPECT_TRUE(it->recommendedScenario.population.initialPlacements.empty());
    SC_EXPECT_EQ(it->recommendedScenario.population.occupantSources.size(), std::size_t{2});
    const auto& firstSource = it->recommendedScenario.population.occupantSources.front();
    const auto& secondSource = it->recommendedScenario.population.occupantSources.back();
    SC_EXPECT_EQ(firstSource.id, std::string{"recommendation-source-group-a"});
    SC_EXPECT_EQ(secondSource.id, std::string{"recommendation-source-group-b"});
    SC_EXPECT_EQ(firstSource.targetAgentCount, std::size_t{25});
    SC_EXPECT_EQ(secondSource.targetAgentCount, std::size_t{25});
    SC_EXPECT_EQ(firstSource.agentsPerSpawn, std::size_t{8});
    SC_EXPECT_EQ(secondSource.agentsPerSpawn, std::size_t{8});
    SC_EXPECT_NEAR(firstSource.startSeconds, 0.0, 1e-9);
    SC_EXPECT_NEAR(firstSource.endSeconds, 27.225, 1e-9);
    SC_EXPECT_NEAR(secondSource.startSeconds, 27.225, 1e-9);
    SC_EXPECT_NEAR(secondSource.endSeconds, 54.45, 1e-9);
    SC_EXPECT_TRUE(containsEvidenceLabel(*it, "Release window"));
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
    auto layout = makeRecommendationLayout();
    layout.zones.push_back({
        .id = "exit-west",
        .floorId = "L1",
        .kind = ZoneKind::Exit,
        .label = "West Exit",
    });
    layout.connections.push_back({
        .id = "door-west",
        .floorId = "L1",
        .kind = ConnectionKind::Exit,
        .fromZoneId = "room-a",
        .toZoneId = "exit-west",
    });
    const auto artifacts = makeExitUsageArtifacts();

    const AlternativeRecommendationService service;
    const auto result = service.recommend({
        .layout = layout,
        .sourceScenario = scenario,
        .risk = risk,
        .artifacts = artifacts,
    });

    SC_EXPECT_TRUE(result.candidates.size() >= 2);
    SC_EXPECT_TRUE(result.candidates.front().kind == AlternativeRecommendationKind::BlockedConnectionRelief);
}
