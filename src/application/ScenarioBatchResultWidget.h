#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QString>
#include <QWidget>

#include "application/ScenarioAuthoringWidget.h"
#include "application/ProjectWorkspaceState.h"
#include "domain/FacilityLayout2D.h"

class QLabel;
class QCheckBox;
class QComboBox;
class QPushButton;
class QSlider;
class QTimer;

namespace safecrowd::application {

class SimulationCanvasWidget;
class WorkspaceShell;

class ScenarioBatchResultWidget : public QWidget {
public:
    explicit ScenarioBatchResultWidget(
        QString projectName,
        safecrowd::domain::FacilityLayout2D layout,
        std::vector<SavedScenarioResultState> results,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        std::function<void()> backToLayoutReviewHandler,
        std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState = std::nullopt,
        int currentResultIndex = 0,
        QWidget* parent = nullptr);

    const std::vector<SavedScenarioResultState>& results() const noexcept;
    int currentResultIndex() const noexcept;
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState() const;

private:
    enum class OverlayMode {
        Density = 0,
        Hotspots = 1,
        Bottlenecks = 2,
        None = 3,
    };

    QWidget* createCanvasPanel();
    QWidget* createSummaryPanel();
    void advanceReplay();
    void applyReplayFrame(int frameIndex);
    void loadReplayForSelectedResult();
    void navigateToAuthoring();
    void pauseReplay();
    void refreshComparisonSelection();
    void refreshSelectedResult();
    void rerunBatch();
    int baselineResultIndex() const noexcept;

    QString projectName_{};
    safecrowd::domain::FacilityLayout2D layout_{};
    std::vector<SavedScenarioResultState> results_{};
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState_{};
    std::function<void()> saveProjectHandler_{};
    std::function<void()> openProjectHandler_{};
    std::function<void()> backToLayoutReviewHandler_{};
    int currentResultIndex_{0};
    std::vector<int> selectedCompareIndices_{};
    std::vector<safecrowd::domain::SimulationFrame> replayFrames_{};
    int replayFrameIndex_{0};
    WorkspaceShell* shell_{nullptr};
    SimulationCanvasWidget* canvas_{nullptr};
    QComboBox* displayScenarioCombo_{nullptr};
    QComboBox* overlayCombo_{nullptr};
    QPushButton* playButton_{nullptr};
    QSlider* replaySlider_{nullptr};
    QLabel* replayTimeLabel_{nullptr};
    QLabel* detailLabel_{nullptr};
    std::vector<QCheckBox*> compareCheckBoxes_{};
    QWidget* remainingChart_{nullptr};
    QWidget* exitsChart_{nullptr};
    QTimer* replayTimer_{nullptr};
    OverlayMode overlayMode_{OverlayMode::Density};
};

}  // namespace safecrowd::application
