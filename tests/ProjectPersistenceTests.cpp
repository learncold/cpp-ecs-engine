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

SC_TEST(ProjectPersistence_preservesOperationalConflictResultState) {
    QTemporaryDir projectDir;
    SC_EXPECT_TRUE(projectDir.isValid());

    ScenarioDraft scenario;
    scenario.scenarioId = "scenario-conflict";
    scenario.name = "Operational Conflict Scenario";

    ScenarioRiskSnapshot risk;
    risk.completionRisk = ScenarioRiskLevel::Medium;
    risk.peakConflictScore = 0.74;
    risk.totalConflictExposureAgentSeconds = 19.5;
    risk.operationalConflictCells.push_back({
        .center = {.x = 1.0, .y = 1.0},
        .cellMin = {.x = 0.0, .y = 0.0},
        .cellMax = {.x = 2.0, .y = 2.0},
        .floorId = "L1",
        .movingAgentCount = 6,
        .peakAgentCount = 6,
        .forwardCount = 3,
        .reverseCount = 3,
        .counterflowRatio = 0.5,
        .averageSpeed = 0.58,
        .speedDropRatio = 0.55,
        .conflictScore = 0.74,
        .durationSeconds = 10.0,
        .exposureAgentSeconds = 19.5,
        .nearestConnectionId = "door-main",
        .nearestConnectionLabel = "Main Door",
    });
    risk.operationalConflictConnections.push_back({
        .connectionId = "door-main",
        .label = "Main Door",
        .floorId = "L1",
        .nearbyAgentCount = 6,
        .movingAgentCount = 6,
        .queueAgentCount = 2,
        .forwardCount = 3,
        .reverseCount = 3,
        .counterflowRatio = 0.5,
        .averageSpeed = 0.58,
        .speedDropRatio = 0.55,
        .conflictScore = 0.74,
        .durationSeconds = 10.0,
        .exposureAgentSeconds = 19.5,
    });

    ScenarioResultArtifacts artifacts;
    artifacts.operationalConflictSummary.peakConflictScore = 0.74;
    artifacts.operationalConflictSummary.totalConflictExposureAgentSeconds = 19.5;
    artifacts.operationalConflictSummary.longestConflictDurationSeconds = 10.0;
    artifacts.operationalConflictSummary.conflictConnectionCount = 1;
    artifacts.operationalConflictSummary.connectionConcentrationIndex = 0.61;
    artifacts.operationalConflictSummary.topConflictConnectionId = "door-main";
    artifacts.operationalConflictSummary.topConflictConnectionLabel = "Main Door";
    artifacts.connectionUsage.push_back({
        .connectionId = "door-main",
        .label = "Main Door",
        .floorId = "L1",
        .traversalCount = 8,
        .usageRatio = 0.61,
        .peakWindowCount = 4,
        .forwardTraversals = 5,
        .reverseTraversals = 3,
        .queueExposureAgentSeconds = 3.5,
        .peakQueuedAgents = 2,
        .averageObservedSpeed = 0.64,
        .peakConflictScore = 0.74,
        .longestConflictDurationSeconds = 10.0,
        .counterflowEventCount = 2,
    });
    artifacts.operationalConflictTimeline.push_back({
        .timeSeconds = 12.0,
        .peakConflictScore = 0.74,
        .activeConflictCellCount = 1,
        .activeConflictConnectionCount = 1,
        .queuedAgentsNearConnections = 2,
    });

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::ScenarioResult;
    workspace.result = SavedScenarioResultState{
        .scenario = scenario,
        .risk = risk,
        .artifacts = artifacts,
        .navigationView = SavedResultNavigationView::OperationalConflict,
    };

    const ProjectMetadata metadata{
        .name = "Operational Conflict Persistence",
        .folderPath = projectDir.path(),
    };

    QString errorMessage;
    SC_EXPECT_TRUE(ProjectPersistence::saveProjectWorkspace(metadata, workspace, &errorMessage));

    ProjectWorkspaceState loaded;
    SC_EXPECT_TRUE(ProjectPersistence::loadProjectWorkspace(metadata, &loaded));
    SC_EXPECT_TRUE(loaded.result.has_value());
    SC_EXPECT_TRUE(loaded.result->navigationView == SavedResultNavigationView::OperationalConflict);
    SC_EXPECT_EQ(loaded.result->risk.operationalConflictCells.size(), std::size_t{1});
    SC_EXPECT_EQ(loaded.result->risk.operationalConflictConnections.size(), std::size_t{1});
    SC_EXPECT_NEAR(loaded.result->artifacts.operationalConflictSummary.peakConflictScore, 0.74, 1e-9);
    SC_EXPECT_EQ(loaded.result->artifacts.connectionUsage.size(), std::size_t{1});
    SC_EXPECT_EQ(loaded.result->artifacts.operationalConflictTimeline.size(), std::size_t{1});
}
