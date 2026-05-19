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
