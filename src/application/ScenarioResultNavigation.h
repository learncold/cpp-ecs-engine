#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include <QString>
#include <QWidget>

#include "application/WorkspaceShell.h"
#include "domain/AlternativeRecommendationService.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"

namespace safecrowd::application {

enum class ScenarioResultNavigationView {
    Bottleneck,
    OperationalConflict,
    Hotspot,
    HazardExposure,
    Zone,
    Groups,
    Recommendations,
};

std::vector<WorkspaceNavigationTab> scenarioResultNavigationTabs();
QString scenarioResultNavigationTabId(ScenarioResultNavigationView view);
ScenarioResultNavigationView scenarioResultNavigationViewFromTabId(const QString& tabId);

QWidget* createScenarioResultNavigationPanel(
    ScenarioResultNavigationView view,
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    std::function<void(std::size_t)> bottleneckFocusHandler,
    std::function<void(std::size_t)> operationalConflictCellFocusHandler,
    std::function<void(std::size_t)> operationalConflictConnectionFocusHandler,
    std::function<void(std::size_t)> hotspotFocusHandler,
    std::function<void(ScenarioResultNavigationView, std::size_t)> itemSelectionHandler,
    QWidget* parent);

QWidget* createScenarioRecommendationNavigationPanel(
    const safecrowd::domain::AlternativeRecommendationResult& recommendation,
    std::function<void(safecrowd::domain::ScenarioDraft)> createScenarioHandler,
    QWidget* parent);

}  // namespace safecrowd::application
