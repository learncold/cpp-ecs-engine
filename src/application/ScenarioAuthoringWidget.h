#pragma once

#include <functional>
#include <vector>

#include <QString>
#include <QWidget>

#include "application/ScenarioCanvasWidget.h"
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
        QString startText{};
        QString destinationText{};
        QString baseScenarioId{};
        bool stagedForRun{false};
    };

private:
    void addEventDraft(const QString& name, const QString& trigger, const QString& target);
    void createScenarioFromCurrent();
    void createScenarioWithName(const QString& name, int sourceIndex);
    void ensureInitialScenarioPrompt();
    void refreshCanvas();
    void refreshInspector();
    void refreshNavigationPanel();
    void refreshRightPanel();
    void refreshScenarioSwitcher();
    void setRightPanelMode(RightPanelMode mode);
    void stageCurrentScenario();
    void updateCurrentScenarioPlacements(const std::vector<ScenarioCrowdPlacement>& placements);
    void showEmptyCanvas();
    void showScenarioNameDialog(int sourceIndex);
    QWidget* createRunPanel();
    QWidget* createScenarioPanel();
    QWidget* createTopBarTogglePanel();
    ScenarioState* currentScenario();
    const ScenarioState* currentScenario() const;

    QString projectName_{};
    safecrowd::domain::FacilityLayout2D layout_{};
    std::vector<ScenarioState> scenarios_{};
    int currentScenarioIndex_{-1};
    NavigationView navigationView_{NavigationView::Layout};
    RightPanelMode rightPanelMode_{RightPanelMode::Scenario};
    WorkspaceShell* shell_{nullptr};
    ScenarioCanvasWidget* canvas_{nullptr};
    QPushButton* scenarioPanelButton_{nullptr};
    QPushButton* runPanelButton_{nullptr};
    QComboBox* scenarioSwitcher_{nullptr};
    QLabel* scenarioSummaryLabel_{nullptr};
    QLabel* changesLabel_{nullptr};
    QLabel* stagedScenariosLabel_{nullptr};
    QPushButton* newScenarioButton_{nullptr};
    QPushButton* stageScenarioButton_{nullptr};
    QPushButton* executeRunButton_{nullptr};
};

}  // namespace safecrowd::application
