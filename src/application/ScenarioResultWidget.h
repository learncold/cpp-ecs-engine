#pragma once

#include <functional>

#include <QString>
#include <QWidget>

#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"

namespace safecrowd::application {

class WorkspaceShell;

class ScenarioResultWidget : public QWidget {
public:
    explicit ScenarioResultWidget(
        QString projectName,
        safecrowd::domain::FacilityLayout2D layout,
        safecrowd::domain::ScenarioDraft scenario,
        safecrowd::domain::SimulationFrame frame,
        safecrowd::domain::ScenarioRiskSnapshot risk,
        safecrowd::domain::ScenarioResultArtifacts artifacts,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        std::function<void()> backToLayoutReviewHandler,
        QWidget* parent = nullptr);

    const safecrowd::domain::ScenarioDraft& scenario() const noexcept;
    const safecrowd::domain::SimulationFrame& frame() const noexcept;
    const safecrowd::domain::ScenarioRiskSnapshot& risk() const noexcept;
    const safecrowd::domain::ScenarioResultArtifacts& artifacts() const noexcept;

private:
    void rerunScenario();
    void navigateToAuthoring(bool showRunPanel);

    QString projectName_{};
    safecrowd::domain::FacilityLayout2D layout_{};
    safecrowd::domain::ScenarioDraft scenario_{};
    safecrowd::domain::SimulationFrame frame_{};
    safecrowd::domain::ScenarioRiskSnapshot risk_{};
    safecrowd::domain::ScenarioResultArtifacts artifacts_{};
    std::function<void()> saveProjectHandler_{};
    std::function<void()> openProjectHandler_{};
    std::function<void()> backToLayoutReviewHandler_{};
    WorkspaceShell* shell_{nullptr};
};

}  // namespace safecrowd::application
