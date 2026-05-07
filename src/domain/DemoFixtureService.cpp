#include "domain/DemoFixtureService.h"

#include <utility>

#include "domain/DemoLayouts.h"

namespace safecrowd::domain {
namespace {

ScenarioDraft makeSprint1BlockedDoorAlternative(const ScenarioDraft& baseline) {
    auto alternative = duplicateScenarioDraft(
        baseline,
        "scenario-2",
        "Doorway blocked alternative");
    alternative.control.connectionBlocks.push_back({
        .id = "block-1",
        .connectionId = DemoLayouts::Sprint1FacilityIds::DoorwayConnectionId,
        .intervals = {{0.0, 60.0}},
    });
    alternative.variationDiffKeys = computeScenarioDiffKeys(baseline, alternative);
    return alternative;
}

DensityCellMetric densityCell(
    double centerX,
    double centerY,
    double minX,
    double minY,
    double maxX,
    double maxY,
    std::size_t agentCount,
    double densityPeoplePerSquareMeter) {
    return {
        .center = {centerX, centerY},
        .cellMin = {minX, minY},
        .cellMax = {maxX, maxY},
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .agentCount = agentCount,
        .densityPeoplePerSquareMeter = densityPeoplePerSquareMeter,
    };
}

EvacuationProgressSample progressSample(
    double timeSeconds,
    std::size_t evacuatedCount,
    double evacuatedRatio) {
    return {
        .timeSeconds = timeSeconds,
        .evacuatedCount = evacuatedCount,
        .totalCount = 100,
        .evacuatedRatio = evacuatedRatio,
    };
}

ScenarioRiskSnapshot makeBlockedDoorRiskSnapshot() {
    ScenarioRiskSnapshot risk;
    risk.completionRisk = ScenarioRiskLevel::High;
    risk.stalledAgentCount = 100;
    risk.hotspots = {
        {
            .center = {2.35, 2.35},
            .cellMin = {1.5, 1.5},
            .cellMax = {3.0, 3.0},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 25,
            .detectedAtSeconds = 0.0,
        },
        {
            .center = {2.35, 3.55},
            .cellMin = {1.5, 3.0},
            .cellMax = {3.0, 4.5},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 15,
            .detectedAtSeconds = 0.0,
        },
        {
            .center = {3.55, 2.35},
            .cellMin = {3.0, 1.5},
            .cellMax = {4.5, 3.0},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 15,
            .detectedAtSeconds = 0.0,
        },
        {
            .center = {2.35, 1.3},
            .cellMin = {1.5, 0.0},
            .cellMax = {3.0, 1.5},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 10,
            .detectedAtSeconds = 0.0,
        },
        {
            .center = {1.3, 2.35},
            .cellMin = {0.0, 1.5},
            .cellMax = {1.5, 3.0},
            .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
            .agentCount = 10,
            .detectedAtSeconds = 0.0,
        },
    };
    risk.bottlenecks = {{
        .connectionId = DemoLayouts::Sprint1FacilityIds::OpeningConnectionId,
        .label = "Main Demo Room -> Side Demo Room",
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .passage = {{12.0, 3.5}, {12.0, 6.5}},
        .nearbyAgentCount = 55,
        .stalledAgentCount = 50,
        .averageSpeed = 0.173202,
        .detectedAtSeconds = 18.8667,
    }};
    return risk;
}

ScenarioResultArtifacts makeBlockedDoorResultArtifacts(const SimulationFrame& finalFrame) {
    ScenarioResultArtifacts artifacts;
    artifacts.evacuationProgress = {
        progressSample(0.0, 0, 0.0),
        progressSample(30.0, 0, 0.0),
        progressSample(60.0, 0, 0.0),
        progressSample(66.9333, 2, 0.02),
        progressSample(67.8, 10, 0.10),
        progressSample(69.9667, 31, 0.31),
        progressSample(71.9333, 50, 0.50),
        progressSample(72.9667, 61, 0.61),
        progressSample(73.7667, 70, 0.70),
        progressSample(74.8333, 80, 0.80),
        progressSample(75.9667, 90, 0.90),
        progressSample(76.4667, 95, 0.95),
        progressSample(76.9667, 99, 0.99),
        progressSample(77.2333, 100, 1.0),
    };
    artifacts.replayFrames = {finalFrame};
    artifacts.timingSummary.t50Seconds = 71.9;
    artifacts.timingSummary.t90Seconds = 75.9333;
    artifacts.timingSummary.t95Seconds = 76.4333;
    artifacts.timingSummary.finalEvacuationTimeSeconds = 77.2;
    artifacts.timingSummary.targetTimeSeconds = 600.0;
    artifacts.timingSummary.marginSeconds = 522.8;

    auto peakCells = std::vector<DensityCellMetric>{
        densityCell(2.35, 2.35, 1.5, 1.5, 3.0, 3.0, 25, 11.1111),
        densityCell(3.55, 2.35, 3.0, 1.5, 4.5, 3.0, 15, 6.66667),
        densityCell(2.35, 3.55, 1.5, 3.0, 3.0, 4.5, 15, 6.66667),
        densityCell(9.84153, 3.70549, 9.0, 3.0, 10.5, 4.5, 13, 5.77778),
        densityCell(9.75901, 2.14418, 9.0, 1.5, 10.5, 3.0, 13, 5.77778),
    };

    artifacts.densitySummary.cellSizeMeters = 1.5;
    artifacts.densitySummary.highDensityThresholdPeoplePerSquareMeter = 4.0;
    artifacts.densitySummary.peakDensityPeoplePerSquareMeter = 11.1111;
    artifacts.densitySummary.peakAgentCount = 25;
    artifacts.densitySummary.peakAtSeconds = 0.0;
    artifacts.densitySummary.peakCell = peakCells.front();
    artifacts.densitySummary.highDensityDurationSeconds = 71.9667;
    artifacts.densitySummary.peakField = {
        .timeSeconds = finalFrame.elapsedSeconds,
        .cellSizeMeters = 1.5,
        .cells = peakCells,
    };
    artifacts.densitySummary.peakCells = std::move(peakCells);

    artifacts.exitUsage.push_back({
        .exitZoneId = DemoLayouts::Sprint1FacilityIds::ExitZoneId,
        .exitLabel = "Main Exit",
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .evacuatedCount = 100,
        .usageRatio = 1.0,
        .lastExitTimeSeconds = 77.2,
    });
    artifacts.zoneCompletion.push_back({
        .zoneId = DemoLayouts::Sprint1FacilityIds::MainRoomZoneId,
        .zoneLabel = "Main Demo Room",
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .initialCount = 100,
        .evacuatedCount = 100,
        .lastCompletionTimeSeconds = 77.2,
    });
    artifacts.placementCompletion.push_back({
        .placementId = "placement-1",
        .zoneId = DemoLayouts::Sprint1FacilityIds::MainRoomZoneId,
        .floorId = DemoLayouts::Sprint1FacilityIds::FloorId,
        .initialCount = 100,
        .evacuatedCount = 100,
        .lastCompletionTimeSeconds = 77.2,
    });
    return artifacts;
}

}  // namespace

DemoFixture DemoFixtureService::createSprint1DemoFixture() const {
    DemoFixture fixture;
    fixture.layout = DemoLayouts::demoFacility();

    fixture.population.initialPlacements.push_back({
        .id = "placement-1",
        .zoneId = DemoLayouts::Sprint1FacilityIds::MainRoomZoneId,
        .area = {
            .outline = {
                {1.0, 1.0},
                {4.0, 1.0},
                {4.0, 4.0},
                {1.0, 4.0},
            },
        },
        .targetAgentCount = 100,
    });

    fixture.baselineScenario.scenarioId = "scenario-1";
    fixture.baselineScenario.name = "Sprint 1 baseline";
    fixture.baselineScenario.role = ScenarioRole::Baseline;
    fixture.baselineScenario.population = fixture.population;
    fixture.baselineScenario.execution.timeLimitSeconds = 600.0;
    fixture.baselineScenario.execution.sampleIntervalSeconds = 1.0;
    fixture.baselineScenario.execution.repeatCount = 1;
    fixture.baselineScenario.execution.baseSeed = 1;
    fixture.baselineScenario.sourceTemplateId = "after-sprint-1-baseline";

    return fixture;
}

DemoScenarioResultFixture DemoFixtureService::createSprint1BlockedDoorResultFixture() const {
    const auto baselineFixture = createSprint1DemoFixture();

    DemoScenarioResultFixture fixture;
    fixture.layout = baselineFixture.layout;
    fixture.population = baselineFixture.population;
    fixture.baselineScenario = baselineFixture.baselineScenario;
    fixture.alternativeScenario = makeSprint1BlockedDoorAlternative(fixture.baselineScenario);
    fixture.frame = {
        .elapsedSeconds = 77.2333,
        .complete = true,
        .totalAgentCount = 100,
        .evacuatedAgentCount = 100,
    };
    fixture.risk = makeBlockedDoorRiskSnapshot();
    fixture.artifacts = makeBlockedDoorResultArtifacts(fixture.frame);

    return fixture;
}

}  // namespace safecrowd::domain
