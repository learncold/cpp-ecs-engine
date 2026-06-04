#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QString>
#include <QWidget>

#include "application/ProjectWorkspaceState.h"
#include "application/ScenarioAuthoringWidget.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioBatchRunner.h"

class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
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
        QWidget* parent = nullptr,
        std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState = std::nullopt);

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
        QWidget* parent = nullptr,
        std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState = std::nullopt);

    explicit ScenarioRunWidget(
        const QString& projectName,
        const safecrowd::domain::FacilityLayout2D& layout,
        std::vector<safecrowd::domain::ScenarioDraft> scenarios,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        std::function<void()> backToLayoutReviewHandler,
        std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState = std::nullopt,
        QWidget* parent = nullptr,
        int initialSelectedRunIndex = 0);

    explicit ScenarioRunWidget(
        const QString& projectName,
        const safecrowd::domain::FacilityLayout2D& layout,
        std::vector<safecrowd::domain::ScenarioDraft> scenarios,
        std::vector<SavedScenarioResultState> cachedResults,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        std::function<void()> backToLayoutReviewHandler,
        std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState = std::nullopt,
        QWidget* parent = nullptr,
        int initialSelectedRunIndex = 0);

    const safecrowd::domain::ScenarioDraft& scenario() const noexcept;
    const std::vector<safecrowd::domain::ScenarioDraft>& scenarios() const noexcept;
    const std::optional<ScenarioAuthoringWidget::InitialState>& returnAuthoringState() const noexcept;
    bool hasResultsForSave() const noexcept;
    int selectedRunIndex() const noexcept;
    std::vector<SavedScenarioResultState> resultsForSave();

private:
    QWidget* createRunCanvas();
    QWidget* createRunPanel();
    void completeRunsForResults();
    void cycleFastForwardMode();
    bool hasCachedResults() const noexcept;
    std::vector<SavedScenarioResultState> completedResults();
    void returnToAuthoring();
    void refreshStatus();
    void selectRun(int index);
    std::size_t selectedSourceScenarioIndex() const;
    void applyRunSettings();
    void syncReturnAuthoringScenarioExecution(std::size_t sourceIndex);
    void syncRunSettingsControls();
    void showResults();
    void setPlaybackSpeedMultiplier(int multiplier);
    void stopRun();
    void togglePaused();

    QString projectName_{};
    safecrowd::domain::FacilityLayout2D layout_{};
    safecrowd::domain::ScenarioDraft scenario_{};
    std::vector<safecrowd::domain::ScenarioDraft> scenarios_{};
    std::vector<SavedScenarioResultState> cachedResults_{};
    safecrowd::domain::ScenarioBatchRunner batchRunner_{};
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState_{};
    std::function<void()> saveProjectHandler_{};
    std::function<void()> openProjectHandler_{};
    std::function<void()> backToLayoutReviewHandler_{};
    WorkspaceShell* shell_{nullptr};
    SimulationCanvasWidget* canvas_{nullptr};
    std::vector<QPushButton*> previewButtons_{};
    std::vector<QLabel*> previewStatusLabels_{};
    std::vector<QProgressBar*> previewProgressBars_{};
    QTimer* timer_{nullptr};
    QLabel* scenarioLabel_{nullptr};
    QLabel* statusLabel_{nullptr};
    QLabel* elapsedLabel_{nullptr};
    QProgressBar* timeProgressBar_{nullptr};
    QLabel* agentCountLabel_{nullptr};
    QProgressBar* evacuationProgressBar_{nullptr};
    QLabel* stalledLabel_{nullptr};
    QLabel* congestionLabel_{nullptr};
    QLabel* bottleneckLabel_{nullptr};
    QPushButton* pauseButton_{nullptr};
    QPushButton* stopButton_{nullptr};
    QPushButton* fastForwardButton_{nullptr};
    QPushButton* speed2Button_{nullptr};
    QPushButton* speed3Button_{nullptr};
    QPushButton* speed5Button_{nullptr};
    QPushButton* resultButton_{nullptr};
    QDoubleSpinBox* timeLimitSpin_{nullptr};
    QDoubleSpinBox* sampleIntervalSpin_{nullptr};
    QSpinBox* repeatSpin_{nullptr};
    QSpinBox* seedSpin_{nullptr};
    QPushButton* applySettingsButton_{nullptr};
    int selectedRunIndex_{0};
    int playbackSpeedMultiplier_{1};
    bool paused_{false};
};

}  // namespace safecrowd::application
