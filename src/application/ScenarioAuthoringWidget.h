#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QSet>
#include <QString>
#include <QWidget>

#include "application/ScenarioCanvasWidget.h"
#include "application/ProjectWorkspaceState.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"

class QComboBox;
class QLabel;
class QPushButton;

namespace safecrowd::application {

class WorkspaceShell;

class ScenarioAuthoringWidget : public QWidget {
public:
    explicit ScenarioAuthoringWidget(
        const QString& projectName,
        const safecrowd::domain::FacilityLayout2D& layout,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        std::function<void()> backToLayoutReviewHandler,
        QWidget* parent = nullptr);

    enum class NavigationView {
        Layout,
        Crowd,
        Events,
    };

    enum class RightPanelMode {
        None,
        Scenario,
        Run,
    };

    struct ScenarioState {
        safecrowd::domain::ScenarioDraft draft{};
        std::vector<safecrowd::domain::OperationalEventDraft> events{};
        std::vector<ScenarioCrowdPlacement> crowdPlacements{};
        QString baseScenarioId{};
        bool stagedForRun{false};
    };

    struct InitialState {
        std::vector<ScenarioState> scenarios{};
        int currentScenarioIndex{-1};
        NavigationView navigationView{NavigationView::Layout};
        bool inspectorPanelVisible{true};
        bool scenarioPanelVisible{true};
        RightPanelMode rightPanelMode{RightPanelMode::Scenario};
    };

    explicit ScenarioAuthoringWidget(
        const QString& projectName,
        const safecrowd::domain::FacilityLayout2D& layout,
        InitialState initialState,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        std::function<void()> backToLayoutReviewHandler,
        QWidget* parent = nullptr);

    SavedScenarioAuthoringState currentSavedState() const;
    InitialState currentInitialState() const;

private:
    struct OperationalEventHistoryEntry {
        std::vector<safecrowd::domain::OperationalEventDraft> events{};
        std::vector<safecrowd::domain::ConnectionBlockDraft> connectionBlocks{};
        std::vector<safecrowd::domain::EnvironmentHazardDraft> hazards{};
        std::vector<safecrowd::domain::RouteGuidanceDraft> routeGuidances{};
        QString selectedEventId{};
    };

    struct CrowdPlacementHistoryEntry {
        std::vector<ScenarioCrowdPlacement> placements{};
        QString selectedCrowdId{};
    };

    enum class ScenarioHistoryEntryKind {
        CrowdPlacement,
        OperationalEvent,
    };

    struct ScenarioHistoryEntry {
        ScenarioHistoryEntryKind kind{ScenarioHistoryEntryKind::CrowdPlacement};
        CrowdPlacementHistoryEntry crowdPlacement{};
        OperationalEventHistoryEntry operationalEvent{};
    };

    struct ScenarioHistory {
        QString scenarioId{};
        std::vector<ScenarioHistoryEntry> undo{};
        std::vector<ScenarioHistoryEntry> redo{};
    };

    void initializeUi(bool promptForScenario);
    void addEventDraft(const QString& name, const QString& trigger, const QString& target);
    void createScenarioFromCurrent();
    void createScenarioWithName(const QString& name, int sourceIndex);
    void ensureInitialScenarioPrompt();
    void refreshCanvas();
    void refreshInspector();
    void refreshNavigationPanel();
    void refreshRightPanel();
    void refreshScenarioSwitcher();
    void refreshPanelToggles();
    void recomputeDiffKeysAfterScenarioChanged(ScenarioState& scenario);
    void recomputeDependentVariationDiffKeys(const QString& baselineId);
    void recomputeVariationDiffKeysIfAlternative(ScenarioState& scenario) const;
    void runStagedScenarios();
    void stageCurrentScenario();
    void updateCurrentScenarioPlacements(
        const std::vector<ScenarioCrowdPlacement>& placements,
        std::optional<CrowdPlacementHistoryEntry> beforeChange = std::nullopt,
        const QString& selectedCrowdId = {});
    void showEmptyCanvas();
    void showScenarioNameDialog(int sourceIndex);
    QWidget* createRightPanelContainer();
    QWidget* createElementInspectorPanel();
    QWidget* createScenarioPanel();
    QWidget* createPanelToggleBar();
    void setInspectorSelectionFromCanvas(const ScenarioCanvasSelection& selection);
    void setInspectorSelectionFromEventId(const QString& rawId);
    void setInspectorSelectionNone();
    bool undoLastScenarioAuthoringEdit();
    bool redoLastScenarioAuthoringEdit();
    std::optional<OperationalEventHistoryEntry> currentOperationalEventHistoryEntry(const QString& selectedEventId = {}) const;
    void pushOperationalEventUndoEntry(OperationalEventHistoryEntry entry);
    void synchronizeOperationalEvents(ScenarioState& scenario);
    void restoreOperationalEventSelection(const QString& selectedEventId);
    bool restoreOperationalEventHistoryEntry(const OperationalEventHistoryEntry& entry);
    ScenarioHistory* currentScenarioHistory();
    std::optional<CrowdPlacementHistoryEntry> currentCrowdPlacementHistoryEntry(const QString& selectedCrowdId = {}) const;
    void pushCrowdPlacementUndoEntry(CrowdPlacementHistoryEntry entry);
    void synchronizeCrowdPlacements(ScenarioState& scenario);
    void restoreCrowdPlacementSelection(const QString& selectedCrowdId);
    bool restoreCrowdPlacementHistoryEntry(const CrowdPlacementHistoryEntry& entry);
    ScenarioState* currentScenario();
    const ScenarioState* currentScenario() const;
    std::vector<safecrowd::domain::ScenarioDraft> stagedRunnableScenarios() const;

    enum class InspectorSelectionKind {
        None,
        Layout,
        Crowd,
        ConnectionBlock,
        EnvironmentHazard,
        RouteGuidance,
        OperationalEvent,
    };

    QString projectName_{};
    safecrowd::domain::FacilityLayout2D layout_{};
    std::function<void()> saveProjectHandler_{};
    std::function<void()> openProjectHandler_{};
    std::function<void()> backToLayoutReviewHandler_{};
    std::vector<ScenarioState> scenarios_{};
    int currentScenarioIndex_{-1};
    NavigationView navigationView_{NavigationView::Layout};
    bool inspectorPanelVisible_{true};
    bool scenarioPanelVisible_{true};
    QSet<QString> layoutExpandedNodeIds_{};
    QString selectedLayoutElementId_{};
    QSet<QString> crowdExpandedNodeIds_{};
    QString selectedCrowdElementId_{};
    QSet<QString> eventExpandedNodeIds_{};
    QString selectedEventElementId_{};
    InspectorSelectionKind inspectorSelectionKind_{InspectorSelectionKind::None};
    QString inspectorSelectionId_{};
    WorkspaceShell* shell_{nullptr};
    ScenarioCanvasWidget* canvas_{nullptr};
    QComboBox* scenarioSwitcher_{nullptr};
    QWidget* elementInspectorPanel_{nullptr};
    QWidget* scenarioOverviewPanel_{nullptr};
    QWidget* scenarioDiffPanel_{nullptr};
    QPushButton* inspectorPanelToggleButton_{nullptr};
    QPushButton* scenarioPanelToggleButton_{nullptr};
    QLabel* stagedScenariosLabel_{nullptr};
    QPushButton* newScenarioButton_{nullptr};
    QPushButton* stageScenarioButton_{nullptr};
    QPushButton* executeRunButton_{nullptr};
    std::vector<ScenarioHistory> scenarioHistories_{};
};

}  // namespace safecrowd::application
