#include "TestSupport.h"
#include "application/LayoutReviewCodec.h"
#include "application/ProjectPersistence.h"
#include "application/ResultArtifactsCodec.h"
#include "domain/ImportValidationService.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <string>

using namespace safecrowd::application;
using namespace safecrowd::domain;

SC_TEST(ProjectMetadata_recognizesBuiltInDemo2FProject) {
    const auto metadata = makeBuiltInDemo2FProject();

    SC_EXPECT_EQ(metadata.name.toStdString(), std::string{"Demo - 2F"});
    SC_EXPECT_EQ(metadata.layoutPath.toStdString(), std::string{"safecrowd://demo/demo-2f"});
    SC_EXPECT_TRUE(metadata.isBuiltInDemo());
    SC_EXPECT_TRUE(metadata.isBuiltInDemo2F());
    SC_EXPECT_TRUE(metadata.isValid());
}

SC_TEST(ProjectPersistence_loadsBuiltInDemo2FLayoutResource) {
    QFile file(":/demo-layouts/demo-2f-layout-review.json");
    SC_EXPECT_TRUE(file.open(QIODevice::ReadOnly));

    const auto document = QJsonDocument::fromJson(file.readAll());
    SC_EXPECT_TRUE(document.isObject());
    SC_EXPECT_TRUE(document.object().value("layout").isObject());

    const auto layout = layoutFromJson(document.object().value("layout").toObject());
    SC_EXPECT_EQ(layout.name, std::string{"Demo - 2F"});
    SC_EXPECT_EQ(layout.floors.size(), std::size_t{2});
    SC_EXPECT_EQ(layout.zones.size(), std::size_t{19});
    SC_EXPECT_EQ(layout.connections.size(), std::size_t{20});
    SC_EXPECT_EQ(layout.barriers.size(), std::size_t{43});

    ImportValidationService validator;
    const auto issues = validator.validate(layout);
    SC_EXPECT_TRUE(!hasBlockingImportIssue(issues));
}

SC_TEST(LayoutReviewCodec_preservesDoorLeafDirectionAndDefaultsLegacyConnections) {
    FacilityLayout2D layout;
    layout.id = "layout";
    layout.name = "Door Leaf Layout";
    layout.floors.push_back({
        .id = "L1",
        .label = "Level 1",
    });
    layout.zones.push_back({
        .id = "room-a",
        .floorId = "L1",
        .kind = ZoneKind::Room,
        .label = "Room A",
        .area = {.outline = {{.x = 0.0, .y = 0.0}, {.x = 4.0, .y = 0.0}, {.x = 4.0, .y = 4.0}, {.x = 0.0, .y = 4.0}}},
    });
    layout.zones.push_back({
        .id = "room-b",
        .floorId = "L1",
        .kind = ZoneKind::Room,
        .label = "Room B",
        .area = {.outline = {{.x = 4.0, .y = 0.0}, {.x = 8.0, .y = 0.0}, {.x = 8.0, .y = 4.0}, {.x = 4.0, .y = 4.0}}},
    });
    layout.connections.push_back({
        .id = "door-a",
        .floorId = "L1",
        .kind = ConnectionKind::Doorway,
        .fromZoneId = "room-a",
        .toZoneId = "room-b",
        .effectiveWidth = 1.2,
        .directionality = TravelDirection::Bidirectional,
        .centerSpan = {.start = {.x = 4.0, .y = 1.4}, .end = {.x = 4.0, .y = 2.6}},
        .doorLeafDirection = DoorLeafDirection::East,
    });

    const auto json = layoutToJson(layout);
    SC_EXPECT_TRUE(json.value("connections").toArray().at(0).toObject().contains("doorLeafDirection"));

    const auto loaded = layoutFromJson(json);
    SC_EXPECT_EQ(loaded.connections.size(), std::size_t{1});
    SC_EXPECT_TRUE(loaded.connections.front().doorLeafDirection == DoorLeafDirection::East);

    auto legacyJson = json;
    auto legacyConnections = legacyJson.value("connections").toArray();
    auto legacyConnection = legacyConnections.at(0).toObject();
    legacyConnection.remove("doorLeafDirection");
    legacyConnections.replace(0, legacyConnection);
    legacyJson["connections"] = legacyConnections;

    const auto legacyLoaded = layoutFromJson(legacyJson);
    SC_EXPECT_EQ(legacyLoaded.connections.size(), std::size_t{1});
    SC_EXPECT_TRUE(legacyLoaded.connections.front().doorLeafDirection == DoorLeafDirection::None);
}

SC_TEST(ProjectPersistence_preservesRecommendedScenarioDraftState) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    ScenarioDraft draft;
    draft.scenarioId = "recommended-1";
    draft.name = "Recommended: balance exits";
    draft.role = ScenarioRole::Recommended;
    draft.sourceTemplateId = "recommendation:exit-usage-balancing:scenario-1";
    draft.variationDiffKeys = {"control.routeGuidances"};

    RouteGuidanceDraft guidance;
    guidance.id = "guidance-east";
    guidance.guidedExitZoneId = "exit-east";
    guidance.baseComplianceRate = 0.5;
    guidance.influenceRadiusMeters = 2.5;
    guidance.maxDetourMeters = 20.0;
    draft.control.routeGuidances.push_back(guidance);

    EnvironmentHazardDraft hazard;
    hazard.id = "fire-a";
    hazard.kind = EnvironmentHazardKind::Fire;
    hazard.name = "Lobby fire";
    hazard.affectedZoneId = "lobby";
    hazard.floorId = "L1";
    hazard.position = {.x = 3.0, .y = 4.0};
    hazard.severity = ScenarioElementSeverity::High;
    hazard.radiusMeters = 7.5;
    draft.environment.hazards.push_back(hazard);

    SavedScenarioState scenario;
    scenario.draft = draft;
    scenario.baseScenarioId = "baseline-1";
    scenario.stagedForRun = false;

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::ScenarioAuthoring;
    workspace.authoring = SavedScenarioAuthoringState{
        .scenarios = {scenario},
        .currentScenarioIndex = 0,
        .rightPanelMode = SavedRightPanelMode::Scenario,
    };

    const ProjectMetadata metadata{
        .name = "Persistence Test",
        .folderPath = projectDir.path(),
    };

    QString errorMessage;
    SC_EXPECT_TRUE(ProjectPersistence::saveProjectWorkspace(metadata, workspace, &errorMessage));

    ProjectWorkspaceState loaded;
    SC_EXPECT_TRUE(ProjectPersistence::loadProjectWorkspace(metadata, &loaded));
    SC_EXPECT_TRUE(loaded.authoring.has_value());
    SC_EXPECT_EQ(loaded.authoring->scenarios.size(), std::size_t{1});

    const auto& loadedScenario = loaded.authoring->scenarios.front();
    SC_EXPECT_TRUE(loadedScenario.draft.role == ScenarioRole::Recommended);
    SC_EXPECT_EQ(loadedScenario.draft.sourceTemplateId, draft.sourceTemplateId);
    SC_EXPECT_EQ(loadedScenario.draft.variationDiffKeys, draft.variationDiffKeys);
    SC_EXPECT_EQ(loadedScenario.baseScenarioId, std::string{"baseline-1"});
    SC_EXPECT_TRUE(!loadedScenario.stagedForRun);
    SC_EXPECT_EQ(loadedScenario.draft.control.routeGuidances.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedScenario.draft.control.routeGuidances.front().guidedExitZoneId, std::string{"exit-east"});
    SC_EXPECT_NEAR(loadedScenario.draft.control.routeGuidances.front().baseComplianceRate, 0.5, 1e-9);
    SC_EXPECT_NEAR(loadedScenario.draft.control.routeGuidances.front().influenceRadiusMeters, 2.5, 1e-9);
    SC_EXPECT_NEAR(loadedScenario.draft.control.routeGuidances.front().maxDetourMeters, 20.0, 1e-9);
    SC_EXPECT_EQ(loadedScenario.draft.environment.hazards.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedScenario.draft.environment.hazards.front().id, std::string{"fire-a"});
    SC_EXPECT_NEAR(loadedScenario.draft.environment.hazards.front().radiusMeters, 7.5, 1e-9);
}

SC_TEST(ProjectPersistence_preservesRunningScenarioIndex) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    ScenarioDraft baseline;
    baseline.scenarioId = "baseline";
    baseline.name = "Baseline";
    baseline.execution.repeatCount = 3;
    baseline.execution.wayfindingMode = ScenarioWayfindingMode::LocalWayfinding;

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::ScenarioRun;
    workspace.runningScenario = baseline;
    workspace.runningScenarios = {baseline};
    workspace.runningScenarioIndex = 2;

    const ProjectMetadata metadata{
        .name = "Running Index Test",
        .folderPath = projectDir.path(),
    };

    QString errorMessage;
    SC_EXPECT_TRUE(ProjectPersistence::saveProjectWorkspace(metadata, workspace, &errorMessage));

    ProjectWorkspaceState loaded;
    SC_EXPECT_TRUE(ProjectPersistence::loadProjectWorkspace(metadata, &loaded));
    SC_EXPECT_TRUE(loaded.activeView == ProjectWorkspaceView::ScenarioRun);
    SC_EXPECT_EQ(loaded.runningScenarios.size(), std::size_t{1});
    SC_EXPECT_EQ(loaded.runningScenarioIndex, 2);
    SC_EXPECT_EQ(loaded.runningScenarios.front().execution.repeatCount, std::uint32_t{3});
    SC_EXPECT_TRUE(loaded.runningScenarios.front().execution.wayfindingMode == ScenarioWayfindingMode::LocalWayfinding);
}

SC_TEST(ProjectPersistence_preservesPressureResultArtifactsAndRiskSnapshot) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    ScenarioDraft scenario;
    scenario.scenarioId = "pressure-result";
    scenario.name = "Pressure Result";

    SimulationFrame detectionFrame;
    detectionFrame.elapsedSeconds = 7.5;
    detectionFrame.complete = false;
    detectionFrame.totalAgentCount = 8;
    detectionFrame.evacuatedAgentCount = 2;
    detectionFrame.agents.push_back({
        .id = 42,
        .position = {.x = 4.0, .y = 5.0},
        .velocity = {.x = 0.1, .y = 0.2},
        .radius = 0.3,
        .floorId = "L1",
        .stalled = true,
    });

    PressureCellMetric peakCell{
        .center = {.x = 4.5, .y = 5.5},
        .cellMin = {.x = 4.0, .y = 5.0},
        .cellMax = {.x = 5.0, .y = 6.0},
        .floorId = "L1",
        .agentCount = 6,
        .intrudingPairCount = 3,
        .densityPeoplePerSquareMeter = 3.8,
        .pressureScore = 8.7,
    };

    PressureCellMetric fieldCell{
        .center = {.x = 6.5, .y = 7.5},
        .cellMin = {.x = 6.0, .y = 7.0},
        .cellMax = {.x = 7.0, .y = 8.0},
        .floorId = "L2",
        .agentCount = 4,
        .intrudingPairCount = 2,
        .densityPeoplePerSquareMeter = 2.9,
        .pressureScore = 5.4,
    };

    ScenarioPressureHotspot hotspot{
        .center = {.x = 4.5, .y = 5.5},
        .cellMin = {.x = 4.0, .y = 5.0},
        .cellMax = {.x = 5.0, .y = 6.0},
        .floorId = "L1",
        .agentCount = 6,
        .intrudingPairCount = 3,
        .densityPeoplePerSquareMeter = 3.8,
        .pressureScore = 8.7,
        .detectedAtSeconds = 7.5,
        .detectionFrame = detectionFrame,
    };

    ScenarioPressureAgentMetric agent{
        .agentId = 42,
        .position = {.x = 4.2, .y = 5.2},
        .floorId = "L1",
        .compressionForce = 1.2,
        .exposureSeconds = 2.5,
        .critical = true,
    };

    ScenarioCriticalPressureEvent event{
        .center = {.x = 4.5, .y = 5.5},
        .cellMin = {.x = 4.0, .y = 5.0},
        .cellMax = {.x = 5.0, .y = 6.0},
        .floorId = "L1",
        .exposedAgentCount = 5,
        .criticalAgentCount = 2,
        .pressureScore = 9.1,
        .startedAtSeconds = 6.0,
        .durationSeconds = 1.5,
        .detectedAtSeconds = 7.5,
        .detectionFrame = detectionFrame,
    };

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::ScenarioResult;
    workspace.result = SavedScenarioResultState{
        .scenario = scenario,
        .risk = {
            .stalledAgentCount = 3,
            .pressureExposedAgentCount = 5,
            .criticalPressureAgentCount = 2,
            .pressureHotspots = {hotspot},
            .pressureAgents = {agent},
            .criticalPressureEvents = {event},
        },
        .artifacts = {
            .pressureSummary = {
                .cellSizeMeters = 1.5,
                .hotspotScoreThreshold = 4.0,
                .criticalCompressionForceThreshold = 1.0,
                .criticalExposureThresholdSeconds = 2.0,
                .criticalEventDurationThresholdSeconds = 1.0,
                .criticalEventAgentThreshold = 2,
                .peakPressureScore = 8.7,
                .peakAtSeconds = 7.5,
                .peakCell = peakCell,
                .peakExposedAgentCount = 5,
                .peakCriticalAgentCount = 2,
                .peakCells = {peakCell},
                .peakField = {
                    .timeSeconds = 7.5,
                    .cellSizeMeters = 1.5,
                    .cells = {fieldCell},
                },
                .peakHotspots = {hotspot},
                .peakAgents = {agent},
                .criticalEvents = {event},
            },
        },
    };

    const ProjectMetadata metadata{
        .name = "Pressure Persistence Test",
        .folderPath = projectDir.path(),
    };

    QString errorMessage;
    SC_EXPECT_TRUE(ProjectPersistence::saveProjectWorkspace(metadata, workspace, &errorMessage));

    ProjectWorkspaceState loaded;
    SC_EXPECT_TRUE(ProjectPersistence::loadProjectWorkspace(metadata, &loaded));
    SC_EXPECT_TRUE(loaded.result.has_value());

    const auto& loadedRisk = loaded.result->risk;
    SC_EXPECT_EQ(loadedRisk.pressureExposedAgentCount, std::size_t{5});
    SC_EXPECT_EQ(loadedRisk.criticalPressureAgentCount, std::size_t{2});
    SC_EXPECT_EQ(loadedRisk.pressureHotspots.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedRisk.pressureAgents.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedRisk.criticalPressureEvents.size(), std::size_t{1});
    SC_EXPECT_NEAR(loadedRisk.pressureHotspots.front().pressureScore, 8.7, 1e-9);
    SC_EXPECT_TRUE(loadedRisk.pressureHotspots.front().detectionFrame.has_value());
    SC_EXPECT_EQ(loadedRisk.pressureAgents.front().agentId, std::uint64_t{42});
    SC_EXPECT_TRUE(loadedRisk.pressureAgents.front().critical);
    SC_EXPECT_NEAR(loadedRisk.pressureAgents.front().position.x, 4.2, 1e-9);
    SC_EXPECT_NEAR(loadedRisk.pressureAgents.front().compressionForce, 1.2, 1e-9);
    SC_EXPECT_NEAR(loadedRisk.pressureAgents.front().exposureSeconds, 2.5, 1e-9);
    SC_EXPECT_NEAR(loadedRisk.criticalPressureEvents.front().durationSeconds, 1.5, 1e-9);
    SC_EXPECT_TRUE(loadedRisk.criticalPressureEvents.front().detectionFrame.has_value());
    SC_EXPECT_EQ(loadedRisk.criticalPressureEvents.front().detectionFrame->agents.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedRisk.criticalPressureEvents.front().detectionFrame->agents.front().id, std::uint64_t{42});

    const auto& loadedSummary = loaded.result->artifacts.pressureSummary;
    SC_EXPECT_NEAR(loadedSummary.cellSizeMeters, 1.5, 1e-9);
    SC_EXPECT_NEAR(loadedSummary.hotspotScoreThreshold, 4.0, 1e-9);
    SC_EXPECT_NEAR(loadedSummary.criticalCompressionForceThreshold, 1.0, 1e-9);
    SC_EXPECT_NEAR(loadedSummary.criticalExposureThresholdSeconds, 2.0, 1e-9);
    SC_EXPECT_NEAR(loadedSummary.criticalEventDurationThresholdSeconds, 1.0, 1e-9);
    SC_EXPECT_EQ(loadedSummary.criticalEventAgentThreshold, std::size_t{2});
    SC_EXPECT_NEAR(loadedSummary.peakPressureScore, 8.7, 1e-9);
    SC_EXPECT_TRUE(loadedSummary.peakAtSeconds.has_value());
    SC_EXPECT_NEAR(*loadedSummary.peakAtSeconds, 7.5, 1e-9);
    SC_EXPECT_TRUE(loadedSummary.peakCell.has_value());
    SC_EXPECT_NEAR(loadedSummary.peakCell->center.x, 4.5, 1e-9);
    SC_EXPECT_NEAR(loadedSummary.peakCell->densityPeoplePerSquareMeter, 3.8, 1e-9);
    SC_EXPECT_EQ(loadedSummary.peakCell->intrudingPairCount, std::size_t{3});
    SC_EXPECT_EQ(loadedSummary.peakExposedAgentCount, std::size_t{5});
    SC_EXPECT_EQ(loadedSummary.peakCriticalAgentCount, std::size_t{2});
    SC_EXPECT_EQ(loadedSummary.peakCells.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedSummary.peakField.cells.size(), std::size_t{1});
    SC_EXPECT_NEAR(loadedSummary.peakField.timeSeconds, 7.5, 1e-9);
    SC_EXPECT_NEAR(loadedSummary.peakField.cells.front().pressureScore, 5.4, 1e-9);
    SC_EXPECT_EQ(loadedSummary.peakHotspots.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedSummary.peakAgents.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedSummary.criticalEvents.size(), std::size_t{1});
    SC_EXPECT_TRUE(loadedSummary.criticalEvents.front().detectionFrame.has_value());
    SC_EXPECT_NEAR(loadedSummary.criticalEvents.front().detectionFrame->elapsedSeconds, 7.5, 1e-9);
}

SC_TEST(ProjectPersistence_preservesBatchPressureResultArtifacts) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    SavedScenarioResultState first;
    first.scenario.scenarioId = "pressure-batch-low";
    first.scenario.name = "Pressure Batch Low";
    first.risk.pressureExposedAgentCount = 1;
    first.artifacts.pressureSummary.peakPressureScore = 2.5;
    first.artifacts.pressureSummary.peakExposedAgentCount = 1;

    ScenarioCriticalPressureEvent event{
        .center = {.x = 8.0, .y = 9.0},
        .cellMin = {.x = 7.5, .y = 8.5},
        .cellMax = {.x = 8.5, .y = 9.5},
        .floorId = "L3",
        .exposedAgentCount = 7,
        .criticalAgentCount = 3,
        .pressureScore = 11.0,
        .startedAtSeconds = 12.0,
        .durationSeconds = 2.0,
        .detectedAtSeconds = 14.0,
    };

    SavedScenarioResultState second;
    second.scenario.scenarioId = "pressure-batch-critical";
    second.scenario.name = "Pressure Batch Critical";
    second.risk.pressureExposedAgentCount = 7;
    second.risk.criticalPressureAgentCount = 3;
    second.risk.criticalPressureEvents = {event};
    second.artifacts.pressureSummary.peakPressureScore = 11.0;
    second.artifacts.pressureSummary.peakExposedAgentCount = 7;
    second.artifacts.pressureSummary.peakCriticalAgentCount = 3;
    second.artifacts.pressureSummary.criticalEvents = {event};

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::ScenarioResult;
    workspace.batchResult = SavedScenarioBatchResultState{
        .results = {first, second},
        .currentResultIndex = 1,
    };

    const ProjectMetadata metadata{
        .name = "Pressure Batch Persistence Test",
        .folderPath = projectDir.path(),
    };

    QString errorMessage;
    SC_EXPECT_TRUE(ProjectPersistence::saveProjectWorkspace(metadata, workspace, &errorMessage));

    ProjectWorkspaceState loaded;
    SC_EXPECT_TRUE(ProjectPersistence::loadProjectWorkspace(metadata, &loaded));
    SC_EXPECT_TRUE(loaded.batchResult.has_value());
    SC_EXPECT_EQ(loaded.batchResult->results.size(), std::size_t{2});
    SC_EXPECT_EQ(loaded.batchResult->currentResultIndex, 1);

    const auto& loadedFirst = loaded.batchResult->results.front();
    SC_EXPECT_EQ(loadedFirst.scenario.scenarioId, std::string{"pressure-batch-low"});
    SC_EXPECT_EQ(loadedFirst.risk.pressureExposedAgentCount, std::size_t{1});
    SC_EXPECT_NEAR(loadedFirst.artifacts.pressureSummary.peakPressureScore, 2.5, 1e-9);

    const auto& loadedSecond = loaded.batchResult->results.back();
    SC_EXPECT_EQ(loadedSecond.scenario.scenarioId, std::string{"pressure-batch-critical"});
    SC_EXPECT_EQ(loadedSecond.risk.criticalPressureAgentCount, std::size_t{3});
    SC_EXPECT_EQ(loadedSecond.risk.criticalPressureEvents.size(), std::size_t{1});
    SC_EXPECT_NEAR(loadedSecond.risk.criticalPressureEvents.front().pressureScore, 11.0, 1e-9);
    SC_EXPECT_EQ(loadedSecond.artifacts.pressureSummary.peakCriticalAgentCount, std::size_t{3});
    SC_EXPECT_EQ(loadedSecond.artifacts.pressureSummary.criticalEvents.size(), std::size_t{1});
    SC_EXPECT_NEAR(loadedSecond.artifacts.pressureSummary.criticalEvents.front().durationSeconds, 2.0, 1e-9);
}

SC_TEST(ResultArtifactsCodec_readsNumericAgentIdsForLegacyJson) {
    const auto pointJson = [](double x, double y) {
        QJsonArray point;
        point.append(x);
        point.append(y);
        return point;
    };

    QJsonObject frameAgent;
    frameAgent["id"] = 77;
    frameAgent["position"] = pointJson(1.0, 2.0);
    frameAgent["velocity"] = pointJson(0.1, 0.2);
    frameAgent["radius"] = 0.25;
    frameAgent["floorId"] = "L1";
    frameAgent["stalled"] = false;

    QJsonArray frameAgents;
    frameAgents.append(frameAgent);

    QJsonObject frameObject;
    frameObject["elapsedSeconds"] = 1.0;
    frameObject["complete"] = false;
    frameObject["totalAgentCount"] = 1;
    frameObject["evacuatedAgentCount"] = 0;
    frameObject["agents"] = frameAgents;

    const auto frame = simulationFrameFromJson(frameObject);
    SC_EXPECT_EQ(frame.agents.size(), std::size_t{1});
    SC_EXPECT_EQ(frame.agents.front().id, std::uint64_t{77});

    QJsonObject pressureAgent;
    pressureAgent["agentId"] = 88;
    pressureAgent["position"] = pointJson(3.0, 4.0);
    pressureAgent["floorId"] = "L2";
    pressureAgent["compressionForce"] = 1.25;
    pressureAgent["exposureSeconds"] = 2.5;
    pressureAgent["critical"] = true;

    QJsonArray pressureAgents;
    pressureAgents.append(pressureAgent);

    QJsonObject riskObject;
    riskObject["stalledAgentCount"] = 0;
    riskObject["pressureAgents"] = pressureAgents;

    const auto risk = riskSnapshotFromJson(riskObject);
    SC_EXPECT_EQ(risk.pressureAgents.size(), std::size_t{1});
    SC_EXPECT_EQ(risk.pressureAgents.front().agentId, std::uint64_t{88});
    SC_EXPECT_TRUE(risk.pressureAgents.front().critical);
}

SC_TEST(ProjectPersistence_preservesHazardExposureResultArtifacts) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    ScenarioDraft scenario;
    scenario.scenarioId = "hazard-result";
    scenario.name = "Hazard Exposure Result";

    HazardExposureMetric exposure;
    exposure.hazardId = "fire-a";
    exposure.hazardName = "Lobby fire";
    exposure.kind = EnvironmentHazardKind::Fire;
    exposure.severity = ScenarioElementSeverity::High;
    exposure.affectedZoneId = "lobby";
    exposure.floorId = "L1";
    exposure.position = {.x = 2.0, .y = 3.0};
    exposure.exposedAgentSeconds = 12.5;
    exposure.peakExposedAgentCount = 4;
    exposure.firstExposureSeconds = 1.5;
    exposure.peakAtSeconds = 3.0;
    exposure.exposureScore = 25.0;

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::ScenarioResult;
    workspace.result = SavedScenarioResultState{
        .scenario = scenario,
        .artifacts = {
            .hazardExposureSummary = {
                .totalExposureScore = 25.0,
                .hazards = {exposure},
            },
        },
        .navigationView = SavedResultNavigationView::HazardExposure,
    };

    const ProjectMetadata metadata{
        .name = "Hazard Exposure Persistence Test",
        .folderPath = projectDir.path(),
    };

    QString errorMessage;
    SC_EXPECT_TRUE(ProjectPersistence::saveProjectWorkspace(metadata, workspace, &errorMessage));

    ProjectWorkspaceState loaded;
    SC_EXPECT_TRUE(ProjectPersistence::loadProjectWorkspace(metadata, &loaded));
    SC_EXPECT_TRUE(loaded.result.has_value());
    SC_EXPECT_TRUE(loaded.result->navigationView == SavedResultNavigationView::HazardExposure);
    const auto& loadedSummary = loaded.result->artifacts.hazardExposureSummary;
    SC_EXPECT_NEAR(loadedSummary.totalExposureScore, 25.0, 1e-9);
    SC_EXPECT_EQ(loadedSummary.hazards.size(), std::size_t{1});
    const auto& loadedExposure = loadedSummary.hazards.front();
    SC_EXPECT_EQ(loadedExposure.hazardId, std::string{"fire-a"});
    SC_EXPECT_EQ(loadedExposure.hazardName, std::string{"Lobby fire"});
    SC_EXPECT_TRUE(loadedExposure.kind == EnvironmentHazardKind::Fire);
    SC_EXPECT_TRUE(loadedExposure.severity == ScenarioElementSeverity::High);
    SC_EXPECT_EQ(loadedExposure.affectedZoneId, std::string{"lobby"});
    SC_EXPECT_EQ(loadedExposure.floorId, std::string{"L1"});
    SC_EXPECT_NEAR(loadedExposure.position.x, 2.0, 1e-9);
    SC_EXPECT_NEAR(loadedExposure.position.y, 3.0, 1e-9);
    SC_EXPECT_NEAR(loadedExposure.exposedAgentSeconds, 12.5, 1e-9);
    SC_EXPECT_EQ(loadedExposure.peakExposedAgentCount, std::size_t{4});
    SC_EXPECT_TRUE(loadedExposure.firstExposureSeconds.has_value());
    SC_EXPECT_NEAR(*loadedExposure.firstExposureSeconds, 1.5, 1e-9);
    SC_EXPECT_TRUE(loadedExposure.peakAtSeconds.has_value());
    SC_EXPECT_NEAR(*loadedExposure.peakAtSeconds, 3.0, 1e-9);
    SC_EXPECT_NEAR(loadedExposure.exposureScore, 25.0, 1e-9);
}

SC_TEST(ProjectPersistence_preservesBatchHazardExposureResultArtifacts) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    HazardExposureMetric fireExposure;
    fireExposure.hazardId = "fire-b";
    fireExposure.hazardName = "Atrium fire";
    fireExposure.kind = EnvironmentHazardKind::Fire;
    fireExposure.severity = ScenarioElementSeverity::High;
    fireExposure.exposedAgentSeconds = 18.0;
    fireExposure.peakExposedAgentCount = 6;
    fireExposure.peakAtSeconds = 4.5;
    fireExposure.exposureScore = 36.0;

    HazardExposureMetric smokeExposure;
    smokeExposure.hazardId = "smoke-b";
    smokeExposure.hazardName = "Upper smoke";
    smokeExposure.kind = EnvironmentHazardKind::Smoke;
    smokeExposure.severity = ScenarioElementSeverity::Medium;
    smokeExposure.exposedAgentSeconds = 9.0;
    smokeExposure.peakExposedAgentCount = 3;
    smokeExposure.peakAtSeconds = 6.0;
    smokeExposure.exposureScore = 13.5;

    SavedScenarioResultState baseline;
    baseline.scenario.scenarioId = "hazard-batch-baseline";
    baseline.scenario.name = "Hazard Batch Baseline";
    baseline.artifacts.hazardExposureSummary = {
        .totalExposureScore = 36.0,
        .hazards = {fireExposure},
    };

    SavedScenarioResultState alternative;
    alternative.scenario.scenarioId = "hazard-batch-alternative";
    alternative.scenario.name = "Hazard Batch Alternative";
    alternative.navigationView = SavedResultNavigationView::HazardExposure;
    alternative.artifacts.hazardExposureSummary = {
        .totalExposureScore = 49.5,
        .hazards = {fireExposure, smokeExposure},
    };

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::ScenarioResult;
    workspace.batchResult = SavedScenarioBatchResultState{
        .results = {baseline, alternative},
        .currentResultIndex = 1,
    };

    const ProjectMetadata metadata{
        .name = "Hazard Batch Persistence Test",
        .folderPath = projectDir.path(),
    };

    QString errorMessage;
    SC_EXPECT_TRUE(ProjectPersistence::saveProjectWorkspace(metadata, workspace, &errorMessage));

    ProjectWorkspaceState loaded;
    SC_EXPECT_TRUE(ProjectPersistence::loadProjectWorkspace(metadata, &loaded));
    SC_EXPECT_TRUE(loaded.batchResult.has_value());
    SC_EXPECT_EQ(loaded.batchResult->results.size(), std::size_t{2});
    SC_EXPECT_EQ(loaded.batchResult->currentResultIndex, 1);

    const auto& loadedAlternative = loaded.batchResult->results.back();
    SC_EXPECT_EQ(loadedAlternative.scenario.scenarioId, std::string{"hazard-batch-alternative"});
    SC_EXPECT_TRUE(loadedAlternative.navigationView == SavedResultNavigationView::HazardExposure);
    const auto& loadedSummary = loadedAlternative.artifacts.hazardExposureSummary;
    SC_EXPECT_NEAR(loadedSummary.totalExposureScore, 49.5, 1e-9);
    SC_EXPECT_EQ(loadedSummary.hazards.size(), std::size_t{2});
    SC_EXPECT_TRUE(loadedSummary.hazards.back().kind == EnvironmentHazardKind::Smoke);
    SC_EXPECT_TRUE(loadedSummary.hazards.back().severity == ScenarioElementSeverity::Medium);
    SC_EXPECT_NEAR(loadedSummary.hazards.back().exposedAgentSeconds, 9.0, 1e-9);
}

SC_TEST(ResultArtifactsCodec_readsHazardExposureEnumsDefensively) {
    QJsonArray position;
    position.append(1.0);
    position.append(2.0);

    QJsonObject hazard;
    hazard["hazardId"] = "legacy-smoke";
    hazard["kind"] = "Smoke";
    hazard["severity"] = "High";
    hazard["position"] = position;

    QJsonArray hazards;
    hazards.append(hazard);

    QJsonObject numericHazard;
    numericHazard["hazardId"] = "legacy-numeric";
    numericHazard["kind"] = static_cast<int>(EnvironmentHazardKind::Smoke);
    numericHazard["severity"] = static_cast<int>(ScenarioElementSeverity::Low);
    numericHazard["position"] = position;
    hazards.append(numericHazard);

    QJsonObject invalidHazard;
    invalidHazard["hazardId"] = "legacy-invalid";
    invalidHazard["kind"] = 99;
    invalidHazard["severity"] = 99;
    invalidHazard["position"] = position;
    hazards.append(invalidHazard);

    QJsonObject summary;
    summary["hazards"] = hazards;

    QJsonObject artifactsObject;
    artifactsObject["hazardExposureSummary"] = summary;

    const auto artifacts = resultArtifactsFromJson(artifactsObject);
    SC_EXPECT_EQ(artifacts.hazardExposureSummary.hazards.size(), std::size_t{3});
    SC_EXPECT_TRUE(artifacts.hazardExposureSummary.hazards[0].kind == EnvironmentHazardKind::Smoke);
    SC_EXPECT_TRUE(artifacts.hazardExposureSummary.hazards[0].severity == ScenarioElementSeverity::High);
    SC_EXPECT_TRUE(artifacts.hazardExposureSummary.hazards[1].kind == EnvironmentHazardKind::Smoke);
    SC_EXPECT_TRUE(artifacts.hazardExposureSummary.hazards[1].severity == ScenarioElementSeverity::Low);
    SC_EXPECT_TRUE(artifacts.hazardExposureSummary.hazards[2].kind == EnvironmentHazardKind::Fire);
    SC_EXPECT_TRUE(artifacts.hazardExposureSummary.hazards[2].severity == ScenarioElementSeverity::Medium);
}

SC_TEST(ProjectPersistence_preservesImportArtifactsBesideLayoutReview) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    const ProjectMetadata metadata{
        .name = "Import Artifact Test",
        .folderPath = projectDir.path(),
    };

    ImportResult importResult;
    importResult.layout = FacilityLayout2D{
        .id = "layout-L1",
        .name = "Imported Layout L1",
        .levelId = "L1",
        .floors = {{
            .id = "L1",
            .label = "Floor 1",
        }},
    };
    importResult.reviewStatus = ImportReviewStatus::Pending;
    importResult.traceRefs.push_back({
        .targetId = "barrier-1",
        .sourceIds = {"line-1"},
        .canonicalIds = {"wall-1"},
    });
    importResult.artifacts.source = {
        .sourcePath = "source.dxf",
        .fileSizeBytes = 42,
        .modifiedTimeTicks = 1234,
        .exists = true,
    };
    importResult.artifacts.selectedRules = {
        .rules = {{
            .semantic = ImportElementSemantic::Wall,
            .tokens = {"A-WALL"},
            .confidence = 0.9,
        }},
    };
    importResult.artifacts.summary = {
        .rawEntityCount = 3,
        .canonicalElementCount = 2,
        .layoutElementCount = 1,
        .issueCount = 1,
    };
    importResult.artifacts.reimport = {
        .hasComparison = true,
        .addedElements = 1,
        .removedElements = 2,
        .changedElements = 3,
    };

    QString errorMessage;
    SC_EXPECT_TRUE(ProjectPersistence::saveProjectReview(metadata, importResult, &errorMessage));
    SC_EXPECT_TRUE(QFileInfo::exists(projectDir.filePath("import-artifacts.json")));

    ImportResult loaded;
    SC_EXPECT_TRUE(ProjectPersistence::loadProjectReview(metadata, &loaded));
    SC_EXPECT_EQ(loaded.traceRefs.size(), std::size_t{1});
    SC_EXPECT_EQ(loaded.traceRefs.front().sourceIds.front(), std::string{"line-1"});
    SC_EXPECT_EQ(loaded.artifacts.source.sourcePath, std::string{"source.dxf"});
    SC_EXPECT_EQ(loaded.artifacts.summary.rawEntityCount, std::size_t{3});
    SC_EXPECT_TRUE(loaded.artifacts.reimport.hasComparison);
    SC_EXPECT_EQ(loaded.artifacts.reimport.changedElements, std::size_t{3});
    SC_EXPECT_EQ(loaded.artifacts.selectedRules.rules.size(), std::size_t{1});
    SC_EXPECT_EQ(loaded.artifacts.selectedRules.rules.front().tokens.front(), std::string{"A-WALL"});
}

SC_TEST(ProjectPersistence_preservesCrossFlowResultState) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    ScenarioDraft scenario;
    scenario.scenarioId = "scenario-cross-flow";
    scenario.name = "Cross Flow Scenario";

    ScenarioRiskSnapshot risk;
    risk.peakCrossFlowScore = 0.74;
    risk.totalCrossFlowExposureAgentSeconds = 19.5;
    risk.crossFlowCells.push_back({
        .center = {.x = 1.0, .y = 1.0},
        .cellMin = {.x = 0.0, .y = 0.0},
        .cellMax = {.x = 2.0, .y = 2.0},
        .floorId = "L1",
        .movingAgentCount = 6,
        .peakAgentCount = 6,
        .primaryFlowCount = 3,
        .crossFlowCount = 3,
        .crossFlowRatio = 0.5,
        .averageSpeed = 0.58,
        .speedDropRatio = 0.55,
        .crossFlowScore = 0.74,
        .durationSeconds = 10.0,
        .exposureAgentSeconds = 19.5,
    });

    ScenarioResultArtifacts artifacts;
    artifacts.crossFlowSummary.peakCrossFlowScore = 0.74;
    artifacts.crossFlowSummary.totalCrossFlowExposureAgentSeconds = 19.5;
    artifacts.crossFlowSummary.longestCrossFlowDurationSeconds = 10.0;
    artifacts.crossFlowSummary.crossFlowHotspotCount = 1;
    artifacts.crossFlowTimeline.push_back({
        .timeSeconds = 12.0,
        .peakCrossFlowScore = 0.74,
        .activeCrossFlowCellCount = 1,
    });

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::ScenarioResult;
    workspace.result = SavedScenarioResultState{
        .scenario = scenario,
        .risk = risk,
        .artifacts = artifacts,
        .navigationView = SavedResultNavigationView::CrossFlow,
    };

    const ProjectMetadata metadata{
        .name = "Cross Flow Persistence",
        .folderPath = projectDir.path(),
    };

    QString errorMessage;
    SC_EXPECT_TRUE(ProjectPersistence::saveProjectWorkspace(metadata, workspace, &errorMessage));

    ProjectWorkspaceState loaded;
    SC_EXPECT_TRUE(ProjectPersistence::loadProjectWorkspace(metadata, &loaded));
    SC_EXPECT_TRUE(loaded.result.has_value());
    SC_EXPECT_TRUE(loaded.result->navigationView == SavedResultNavigationView::CrossFlow);
    SC_EXPECT_EQ(loaded.result->risk.crossFlowCells.size(), std::size_t{1});
    SC_EXPECT_NEAR(loaded.result->artifacts.crossFlowSummary.peakCrossFlowScore, 0.74, 1e-9);
    SC_EXPECT_EQ(loaded.result->artifacts.crossFlowSummary.crossFlowHotspotCount, std::size_t{1});
    SC_EXPECT_EQ(loaded.result->artifacts.crossFlowTimeline.size(), std::size_t{1});
    SC_EXPECT_EQ(loaded.result->artifacts.crossFlowTimeline.front().activeCrossFlowCellCount, std::size_t{1});
}
