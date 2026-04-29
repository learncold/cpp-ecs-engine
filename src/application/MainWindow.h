#pragma once

#include <optional>

#include <QMainWindow>

#include "application/ProjectMetadata.h"
#include "domain/ImportResult.h"

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

    safecrowd::domain::SafeCrowdDomain& domain_;
    ProjectMetadata currentProject_{};
    bool hasCurrentProject_{false};
    std::optional<safecrowd::domain::ImportResult> lastApprovedImportResult_{};
};

}  // namespace safecrowd::application
