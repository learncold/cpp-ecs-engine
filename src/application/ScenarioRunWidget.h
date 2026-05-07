#pragma once

#include <functional>
#include <optional>

#include <QString>
#include <QWidget>

#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioSimulationRunner.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

namespace safecrowd::application {

class SimulationCanvasWidget;
class WorkspaceShell;

class ScenarioRunWidget : public QWidget {
public:
    explicit ScenarioRunWidget(
        const QString& projectName,
        const safecrowd::domain::FacilityLayout2D& layout,
        const safecrowd::domain::ScenarioDraft& scenario,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        std::function<void()> backToLayoutReviewHandler,
        QWidget* parent = nullptr);
    explicit ScenarioRunWidget(
        const QString& projectName,
        const safecrowd::domain::FacilityLayout2D& layout,
        const safecrowd::domain::ScenarioDraft& scenario,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        std::function<void()> backToLayoutReviewHandler,
        safecrowd::domain::SimulationFrame cachedResultFrame,
        safecrowd::domain::ScenarioRiskSnapshot cachedResultRisk,
        safecrowd::domain::ScenarioResultArtifacts cachedResultArtifacts,
        QWidget* parent = nullptr);

    const safecrowd::domain::ScenarioDraft& scenario() const noexcept;

private:
    QWidget* createRunPanel();
    void returnToAuthoring();
    bool hasCachedResult() const noexcept;
    void refreshStatus();
    void advanceFastForwardToResult();
    void startFastForwardToResult();
    void storeResultCache(const safecrowd::domain::ScenarioSimulationRunner& runner);
    void setupUi();
    void showResults();
    void stopRun();
    void togglePaused();

    QString projectName_{};
    safecrowd::domain::FacilityLayout2D layout_{};
    safecrowd::domain::ScenarioDraft scenario_{};
    safecrowd::domain::ScenarioSimulationRunner runner_{};
    std::optional<safecrowd::domain::SimulationFrame> cachedResultFrame_{};
    std::optional<safecrowd::domain::ScenarioRiskSnapshot> cachedResultRisk_{};
    std::optional<safecrowd::domain::ScenarioResultArtifacts> cachedResultArtifacts_{};
    std::function<void()> saveProjectHandler_{};
    std::function<void()> openProjectHandler_{};
    std::function<void()> backToLayoutReviewHandler_{};
    WorkspaceShell* shell_{nullptr};
    SimulationCanvasWidget* canvas_{nullptr};
    QTimer* timer_{nullptr};
    QLabel* scenarioLabel_{nullptr};
    QLabel* statusLabel_{nullptr};
    QLabel* elapsedLabel_{nullptr};
    QProgressBar* timeProgressBar_{nullptr};
    QLabel* agentCountLabel_{nullptr};
    QProgressBar* evacuationProgressBar_{nullptr};
    QLabel* riskLabel_{nullptr};
    QLabel* congestionLabel_{nullptr};
    QLabel* bottleneckLabel_{nullptr};
    QPushButton* pauseButton_{nullptr};
    QPushButton* stopButton_{nullptr};
    QPushButton* fastForwardButton_{nullptr};
    QPushButton* resultButton_{nullptr};
    bool fastForwardingToResult_{false};
    bool paused_{false};
};

}  // namespace safecrowd::application
