#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include <QString>
#include <QWidget>

#include "application/WorkspaceShell.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"

namespace safecrowd::application {

enum class ScenarioResultNavigationView {
    Bottleneck,
    Hotspot,
    Zone,
    Groups,
};

std::vector<WorkspaceNavigationTab> scenarioResultNavigationTabs();
QString scenarioResultNavigationTabId(ScenarioResultNavigationView view);
ScenarioResultNavigationView scenarioResultNavigationViewFromTabId(const QString& tabId);

QWidget* createScenarioResultNavigationPanel(
    ScenarioResultNavigationView view,
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    std::function<void(std::size_t)> bottleneckFocusHandler,
    std::function<void(std::size_t)> hotspotFocusHandler,
    QWidget* parent);

}  // namespace safecrowd::application
