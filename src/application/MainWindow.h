#pragma once

#include <optional>
#include <vector>

#include <QMainWindow>

#include "application/ProjectMetadata.h"
#include "application/ProjectWorkspaceState.h"
#include "application/ScenarioAuthoringWidget.h"
#include "domain/ImportResult.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"

namespace safecrowd::domain {
class SafeCrowdDomain;
}

class QWidget;

namespace safecrowd::application {

struct NewProjectRequest;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent = nullptr);

private:
    void showProjectNavigator();
    void showNewProject();
    void createProject(const NewProjectRequest& request);
    void openProject(const ProjectMetadata& metadata);
    void saveCurrentProject();
    void showLayoutReview(const ProjectMetadata& metadata);
    void showLayoutReview(const ProjectMetadata& metadata, safecrowd::domain::ImportResult importResult);
    void showScenarioAuthoring(const safecrowd::domain::ImportResult& importResult);
    void showScenarioAuthoring(
        const safecrowd::domain::ImportResult& importResult,
        ScenarioAuthoringWidget::InitialState initialState);
    void showScenarioRun(
        const safecrowd::domain::FacilityLayout2D& layout,
        const safecrowd::domain::ScenarioDraft& scenario,
        std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState = std::nullopt);
    void showScenarioRun(
        const safecrowd::domain::FacilityLayout2D& layout,
        std::vector<safecrowd::domain::ScenarioDraft> scenarios,
        std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState = std::nullopt,
        int initialSelectedRunIndex = 0);
    void showScenarioBatchResult(
        const safecrowd::domain::FacilityLayout2D& layout,
        std::vector<SavedScenarioResultState> results,
        int currentResultIndex,
        std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState = std::nullopt);
    void showScenarioResult(
        const safecrowd::domain::FacilityLayout2D& layout,
        const safecrowd::domain::ScenarioDraft& scenario,
        const safecrowd::domain::SimulationFrame& frame,
        const safecrowd::domain::ScenarioRiskSnapshot& risk,
        const safecrowd::domain::ScenarioResultArtifacts& artifacts,
        SavedResultNavigationView savedNavigationView = SavedResultNavigationView::Bottleneck,
        std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState = std::nullopt);

    safecrowd::domain::SafeCrowdDomain& domain_;
    ProjectMetadata currentProject_{};
    bool hasCurrentProject_{false};
    std::optional<safecrowd::domain::ImportResult> lastApprovedImportResult_{};
};

}  // namespace safecrowd::application
