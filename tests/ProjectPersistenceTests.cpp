#include "TestSupport.h"
#include "application/ProjectPersistence.h"
#include "application/ResultArtifactsCodec.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
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
