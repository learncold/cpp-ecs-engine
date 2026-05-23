#include "TestSupport.h"
#include "application/ProjectPersistence.h"

#include <QFileInfo>
#include <QTemporaryDir>

using namespace safecrowd::application;
using namespace safecrowd::domain;

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
}

SC_TEST(ProjectPersistence_preservesRunningScenarioIndex) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    ScenarioDraft baseline;
    baseline.scenarioId = "baseline";
    baseline.name = "Baseline";
    baseline.execution.repeatCount = 3;

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
            .completionRisk = ScenarioRiskLevel::High,
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
    SC_EXPECT_TRUE(loadedRisk.completionRisk == ScenarioRiskLevel::High);
    SC_EXPECT_EQ(loadedRisk.pressureExposedAgentCount, std::size_t{5});
    SC_EXPECT_EQ(loadedRisk.criticalPressureAgentCount, std::size_t{2});
    SC_EXPECT_EQ(loadedRisk.pressureHotspots.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedRisk.pressureAgents.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedRisk.criticalPressureEvents.size(), std::size_t{1});
    SC_EXPECT_NEAR(loadedRisk.pressureHotspots.front().pressureScore, 8.7, 1e-9);
    SC_EXPECT_TRUE(loadedRisk.pressureHotspots.front().detectionFrame.has_value());
    SC_EXPECT_EQ(loadedRisk.pressureAgents.front().agentId, std::uint64_t{42});
    SC_EXPECT_TRUE(loadedRisk.pressureAgents.front().critical);
    SC_EXPECT_NEAR(loadedRisk.criticalPressureEvents.front().durationSeconds, 1.5, 1e-9);

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
    SC_EXPECT_EQ(loadedSummary.peakExposedAgentCount, std::size_t{5});
    SC_EXPECT_EQ(loadedSummary.peakCriticalAgentCount, std::size_t{2});
    SC_EXPECT_EQ(loadedSummary.peakCells.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedSummary.peakField.cells.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedSummary.peakHotspots.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedSummary.peakAgents.size(), std::size_t{1});
    SC_EXPECT_EQ(loadedSummary.criticalEvents.size(), std::size_t{1});
    SC_EXPECT_TRUE(loadedSummary.criticalEvents.front().detectionFrame.has_value());
    SC_EXPECT_NEAR(loadedSummary.criticalEvents.front().detectionFrame->elapsedSeconds, 7.5, 1e-9);
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
