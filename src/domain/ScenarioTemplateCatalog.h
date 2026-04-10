#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/ScenarioAuthoring.h"

namespace safecrowd::domain {

struct ScenarioTemplateDescriptor {
    std::string templateId{};
    std::string name{};
    std::string intendedUse{};
    std::vector<std::string> requiredLayoutFeatures{};
    std::vector<std::string> focusRiskAxes{};
};

struct ScenarioTemplateDraft {
    ScenarioDraft scenario{};
    std::vector<std::string> highlightedFields{};
    std::vector<std::string> unmetRequirements{};
};

class ScenarioTemplateCatalog {
public:
    virtual ~ScenarioTemplateCatalog() = default;

    virtual std::vector<ScenarioTemplateDescriptor> listTemplates() const = 0;
    virtual std::optional<ScenarioTemplateDraft> instantiate(
        const std::string& templateId,
        const FacilityLayout2D& layout) const = 0;
};

}  // namespace safecrowd::domain
