#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "domain/FacilityLayout2D.h"
#include "domain/PopulationSpec.h"

namespace safecrowd::domain {

enum class ScenarioRole {
    Baseline,
    Alternative,
    Recommended,
};

struct EnvironmentState {
    bool reducedVisibility{false};
    std::string familiarityProfile{};
    std::string guidanceProfile{};
};

struct OperationalEventDraft {
    std::string id{};
    std::string name{};
    std::string triggerSummary{};
    std::string targetSummary{};
};

struct ControlPlan {
    std::vector<OperationalEventDraft> events{};
};

struct ExecutionConfig {
    double timeLimitSeconds{0.0};
    double sampleIntervalSeconds{0.0};
    std::uint32_t repeatCount{1};
    std::uint32_t baseSeed{0};
    bool recordOccupantHistory{false};
};

struct ScenarioDraft {
    std::string scenarioId{};
    std::string name{};
    ScenarioRole role{ScenarioRole::Alternative};
    PopulationSpec population{};
    EnvironmentState environment{};
    ControlPlan control{};
    ExecutionConfig execution{};
    std::string sourceTemplateId{};
    std::vector<std::string> variationDiffKeys{};
    std::vector<std::string> blockingIssues{};
};

struct ProjectWorkspaceSnapshot {
    std::string projectId{};
    std::string displayName{};
    FacilityLayout2D layout{};
    std::vector<ScenarioDraft> scenarios{};
};

}  // namespace safecrowd::domain
