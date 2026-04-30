#pragma once

#include <optional>

#include <QMainWindow>

#include "application/ProjectMetadata.h"
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
        const safecrowd::domain::ScenarioDraft& scenario);
    void showScenarioResult(
        const safecrowd::domain::FacilityLayout2D& layout,
        const safecrowd::domain::ScenarioDraft& scenario,
        const safecrowd::domain::SimulationFrame& frame,
        const safecrowd::domain::ScenarioRiskSnapshot& risk,
        const safecrowd::domain::ScenarioResultArtifacts& artifacts);

    safecrowd::domain::SafeCrowdDomain& domain_;
    ProjectMetadata currentProject_{};
    bool hasCurrentProject_{false};
    std::optional<safecrowd::domain::ImportResult> lastApprovedImportResult_{};
};

}  // namespace safecrowd::application
