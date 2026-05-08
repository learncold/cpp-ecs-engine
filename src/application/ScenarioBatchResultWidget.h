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
class QButtonGroup;
class QTableWidget;

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
    };

    QWidget* createSummaryPanel();
    void navigateToAuthoring();
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
    WorkspaceShell* shell_{nullptr};
    SimulationCanvasWidget* canvas_{nullptr};
    QButtonGroup* overlayButtonGroup_{nullptr};
    QTableWidget* table_{nullptr};
    QLabel* detailLabel_{nullptr};
    QWidget* progressChart_{nullptr};
    OverlayMode overlayMode_{OverlayMode::Density};
};

}  // namespace safecrowd::application
