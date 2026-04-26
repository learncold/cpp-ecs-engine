#pragma once

#include <QList>

#include "application/ProjectMetadata.h"
#include "domain/ImportResult.h"

namespace safecrowd::application {

class ProjectPersistence {
public:
    static QList<ProjectMetadata> loadRecentProjects();
    static ProjectMetadata loadProject(const QString& folderPath);
    static bool loadProjectReview(const ProjectMetadata& metadata, safecrowd::domain::ImportResult* importResult);
    static bool saveProject(ProjectMetadata metadata, QString* errorMessage = nullptr);
    static bool saveProjectReview(
        const ProjectMetadata& metadata,
        const safecrowd::domain::ImportResult& importResult,
        QString* errorMessage = nullptr);
};

}  // namespace safecrowd::application
