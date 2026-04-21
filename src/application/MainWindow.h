#pragma once

#include <QMainWindow>

#include "application/ProjectMetadata.h"

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

    safecrowd::domain::SafeCrowdDomain& domain_;
    ProjectMetadata currentProject_{};
    bool hasCurrentProject_{false};
};

}  // namespace safecrowd::application
