#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include <QString>
#include <QWidget>

#include "application/ScenarioAuthoringWidget.h"
#include "application/ProjectWorkspaceState.h"
#include "application/ScenarioResultNavigation.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioSimulationFrame.h"

class QLabel;
class QCheckBox;
class QComboBox;
class QPushButton;
class QSlider;
class QTableWidget;
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
    SavedResultNavigationView currentSavedNavigationView() const noexcept;
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState() const;

private:
    enum class OverlayMode {
        Occupancy = 0,
        Density = 1,
        Pressure = 2,
        Hotspots = 3,
        Bottlenecks = 4,
        CrossFlow = 5,
        None = 6,
    };

    QWidget* createCanvasPanel();
    QWidget* createRightPanelContainer();
    QWidget* createDetailPanel();
    QWidget* createOverviewPanel();
    QWidget* createPanelToggleBar();
    void createRecommendedScenario(safecrowd::domain::ScenarioDraft recommendedScenario);
    void advanceReplay();
    void applyReplayFrame(int frameIndex);
    void applyReplayFrameData(const safecrowd::domain::SimulationFrame& frame, int sliderIndex);
    void applyOverlayModeToCanvas();
    void applySelectedResultStaticCanvasState();
    void loadReplayForSelectedResult();
    int nearestReplayFrameIndex(double seconds) const;
    void navigateToAuthoring();
    void pauseReplay();
    void refreshComparisonSelection();
    void refreshComparisonCountLabel();
    void refreshDetailPanel();
    void refreshExposureComparisonTable();
    void refreshOverviewPanel();
    void refreshPanelToggles();
    void refreshPressureComparisonTable();
    void refreshResultNavigationPanel();
    void refreshRightPanel();
    void refreshSelectedResult();
    void rerunBatch();
    void seekToTimingMarkerSeconds(double seconds);
    void selectAllComparisonScenarios();
    void selectBaselineAndCurrentComparisonScenarios();
    void clearComparisonScenarios();
    void clearDetailSelection();
    void setDetailSelection(ScenarioResultNavigationView view, std::size_t index);
    void setComparisonSelection(std::vector<int> indices);
    void setOverlayMode(OverlayMode mode);
    void showAuthoring(ScenarioAuthoringWidget::InitialState initialState);
    void showClosestReplayFrameAtSeconds(double seconds);
    void showReplayFrame(const safecrowd::domain::SimulationFrame& frame);
    void syncCompareCheckBoxes();
    QString detailTextForSelection(ScenarioResultNavigationView view, std::size_t index) const;
    QWidget* createBatchRecommendationNavigationPanel();
    int explicitBaselineResultIndex() const noexcept;
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
    QLabel* overviewScenarioLabel_{nullptr};
    QLabel* resultDetailLabel_{nullptr};
    QLabel* comparisonCountLabel_{nullptr};
    QTableWidget* exposureTable_{nullptr};
    QTableWidget* pressureTable_{nullptr};
    QPushButton* detailPanelToggleButton_{nullptr};
    QPushButton* overviewPanelToggleButton_{nullptr};
    std::vector<QCheckBox*> compareCheckBoxes_{};
    QWidget* remainingChart_{nullptr};
    QWidget* exitsChart_{nullptr};
    QTimer* replayTimer_{nullptr};
    OverlayMode overlayMode_{OverlayMode::Density};
    ScenarioResultNavigationView resultNavigationView_{ScenarioResultNavigationView::Bottleneck};
    std::optional<ScenarioResultNavigationView> detailSelectionView_{};
    std::optional<std::size_t> detailSelectionIndex_{};
    int detailSelectionResultIndex_{-1};
    bool detailPanelVisible_{true};
    bool overviewPanelVisible_{true};
};

}  // namespace safecrowd::application
