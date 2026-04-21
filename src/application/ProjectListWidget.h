#pragma once

#include <functional>

#include <QFrame>
#include <QList>
#include <QString>

#include "application/ProjectMetadata.h"

namespace safecrowd::application {

class ProjectListWidget : public QFrame {
public:
    explicit ProjectListWidget(const QList<ProjectMetadata>& projects, QWidget* parent = nullptr);

    void setOpenProjectHandler(std::function<void(const ProjectMetadata&)> handler);

private:
    void addProjectRow(const ProjectMetadata& project);

    std::function<void(const ProjectMetadata&)> openProjectHandler_{};
};

}  // namespace safecrowd::application
