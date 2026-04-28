#pragma once

#include <functional>

#include <QList>
#include <QWidget>

#include "application/ProjectMetadata.h"

namespace safecrowd::application {

class ProjectNavigatorActions;
class ProjectListWidget;

class ProjectNavigatorWidget : public QWidget {
public:
    explicit ProjectNavigatorWidget(const QList<ProjectMetadata>& projects, QWidget* parent = nullptr);

    void setNewProjectHandler(std::function<void()> handler);
    void setOpenProjectHandler(std::function<void(const ProjectMetadata&)> handler);
    void setDeleteProjectHandler(std::function<void(const ProjectMetadata&)> handler);

private:
    ProjectNavigatorActions* actions_{nullptr};
    ProjectListWidget* projectList_{nullptr};
};

}  // namespace safecrowd::application
