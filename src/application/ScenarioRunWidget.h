#pragma once

#include <functional>

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

private:
    QWidget* createRunPanel();
    void addBackToAuthoringButton();
    void returnToAuthoring();
    void refreshStatus();
    void showResults();
    void stopRun();
    void togglePaused();

    QString projectName_{};
    safecrowd::domain::FacilityLayout2D layout_{};
    safecrowd::domain::ScenarioDraft scenario_{};
    safecrowd::domain::ScenarioSimulationRunner runner_{};
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
    QPushButton* resultButton_{nullptr};
    bool paused_{false};
};

}  // namespace safecrowd::application
