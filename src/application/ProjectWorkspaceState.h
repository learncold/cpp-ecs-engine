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

enum class SavedResultNavigationView {
    Bottleneck = 0,
    Hotspot = 1,
    Zone = 2,
    Groups = 3,
    Recommendations = 4,
    HazardExposure = 5,
    CrossFlow = 6,
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
    bool inspectorPanelVisible{true};
    bool scenarioPanelVisible{true};
    SavedRightPanelMode rightPanelMode{SavedRightPanelMode::Scenario};
};

struct SavedScenarioResultState {
    safecrowd::domain::ScenarioDraft scenario{};
    safecrowd::domain::SimulationFrame frame{};
    safecrowd::domain::ScenarioRiskSnapshot risk{};
    safecrowd::domain::ScenarioResultArtifacts artifacts{};
    SavedResultNavigationView navigationView{SavedResultNavigationView::Bottleneck};
};

struct SavedScenarioBatchResultState {
    std::vector<SavedScenarioResultState> results{};
    int currentResultIndex{0};
};

struct ProjectWorkspaceState {
    ProjectWorkspaceView activeView{ProjectWorkspaceView::LayoutReview};
    std::optional<SavedScenarioAuthoringState> authoring{};
    std::optional<safecrowd::domain::ScenarioDraft> runningScenario{};
    std::vector<safecrowd::domain::ScenarioDraft> runningScenarios{};
    int runningScenarioIndex{-1};
    std::optional<SavedScenarioResultState> result{};
    std::optional<SavedScenarioBatchResultState> batchResult{};
};

}  // namespace safecrowd::application
