#pragma once

#include <QList>

#include "application/ProjectMetadata.h"

namespace safecrowd::application {

class ProjectPersistence {
public:
    static QList<ProjectMetadata> loadRecentProjects();
    static ProjectMetadata loadProject(const QString& folderPath);
    static bool saveProject(ProjectMetadata metadata, QString* errorMessage = nullptr);
};

}  // namespace safecrowd::application
