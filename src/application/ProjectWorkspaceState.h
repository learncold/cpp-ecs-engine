#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"

namespace safecrowd::application {

enum class ProjectWorkspaceView {
    LayoutReview,
    ScenarioAuthoring,
    ScenarioRun,
    ScenarioResult,
};

enum class SavedNavigationView {
    Layout,
    Crowd,
    Events,
};

enum class SavedRightPanelMode {
    None,
    Scenario,
    Run,
};

struct SavedScenarioState {
    safecrowd::domain::ScenarioDraft draft{};
    std::string baseScenarioId{};
    bool stagedForRun{false};
};

struct SavedScenarioAuthoringState {
    std::vector<SavedScenarioState> scenarios{};
    int currentScenarioIndex{-1};
    SavedNavigationView navigationView{SavedNavigationView::Layout};
    SavedRightPanelMode rightPanelMode{SavedRightPanelMode::Scenario};
};

struct SavedScenarioResultState {
    safecrowd::domain::ScenarioDraft scenario{};
    safecrowd::domain::SimulationFrame frame{};
    safecrowd::domain::ScenarioRiskSnapshot risk{};
    safecrowd::domain::ScenarioResultArtifacts artifacts{};
};

struct ProjectWorkspaceState {
    ProjectWorkspaceView activeView{ProjectWorkspaceView::LayoutReview};
    std::optional<SavedScenarioAuthoringState> authoring{};
    std::optional<safecrowd::domain::ScenarioDraft> runningScenario{};
    std::optional<SavedScenarioResultState> result{};
};

}  // namespace safecrowd::application
