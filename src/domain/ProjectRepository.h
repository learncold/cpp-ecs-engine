#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/ScenarioAuthoring.h"

namespace safecrowd::domain {

struct ProjectArtifactReference {
    std::string artifactKind{};
    std::string artifactId{};
    std::string storageKey{};
};

struct ProjectWorkspaceRecord {
    ProjectWorkspaceSnapshot workspace{};
    std::vector<ProjectArtifactReference> artifactIndex{};
    std::vector<std::string> runIds{};
    std::vector<std::string> variationKeys{};
};

class ProjectRepository {
public:
    virtual ~ProjectRepository() = default;

    virtual std::optional<ProjectWorkspaceRecord> loadProject(const std::string& projectId) const = 0;
    virtual void saveProject(const ProjectWorkspaceRecord& record) = 0;
};

}  // namespace safecrowd::domain
