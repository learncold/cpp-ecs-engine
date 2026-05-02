#pragma once

#include <QList>

#include "application/ProjectMetadata.h"
#include "application/ProjectWorkspaceState.h"
#include "domain/ImportResult.h"

namespace safecrowd::application {

class ProjectPersistence {
public:
    static QList<ProjectMetadata> loadRecentProjects();
    static ProjectMetadata loadProject(const QString& folderPath);
    static bool deleteProject(const ProjectMetadata& metadata, QString* errorMessage = nullptr);
    static bool loadProjectReview(const ProjectMetadata& metadata, safecrowd::domain::ImportResult* importResult);
    static bool loadProjectWorkspace(const ProjectMetadata& metadata, ProjectWorkspaceState* state);
    static bool saveProject(ProjectMetadata metadata, QString* errorMessage = nullptr);
    static bool saveProjectReview(
        const ProjectMetadata& metadata,
        const safecrowd::domain::ImportResult& importResult,
        QString* errorMessage = nullptr);
    static bool saveProjectWorkspace(
        const ProjectMetadata& metadata,
        const ProjectWorkspaceState& state,
        QString* errorMessage = nullptr);
};

}  // namespace safecrowd::application
