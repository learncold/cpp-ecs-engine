#pragma once

#include <QMainWindow>
#include <QString>

#include <filesystem>
#include <optional>
#include <vector>

#include "domain/DxfImportService.h"
#include "domain/ImportResult.h"

namespace safecrowd::domain {
class SafeCrowdDomain;
}

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QTimer;

namespace safecrowd::application {

enum class WorkspaceStage {
    NoProject,
    LayoutNeedsReview,
    LayoutReady,
    ScenarioDraftInvalid,
    ScenarioReady,
    BatchRunning,
    BatchPaused,
    AggregationPending,
    ResultsAvailable,
    ComparisonReady,
    RecommendationReady,
};

struct RecentProjectEntry {
    QString projectId{};
    QString displayName{};
    QString stageSummary{};
    QString detailSummary{};
    WorkspaceStage stage{WorkspaceStage::NoProject};
    int scenarioCount{0};
    int artifactCount{0};
    bool canRestore{false};
};

struct WorkspaceSession {
    QString projectId{};
    QString displayName{};
    QString restoreSummary{};
    WorkspaceStage stage{WorkspaceStage::NoProject};
    int scenarioCount{0};
    int artifactCount{0};
};

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent = nullptr);

private:
    void applyTheme();
    void populateSampleRecentProjects();
    void rebuildRecentProjectsList();
    void refreshNavigator();
    void resetImportWorkflow();
    void refreshImportWorkflow();
    void refreshImportIssueSelection();
    void refreshWorkspaceChrome();
    void browseImportFile();
    void importSelectedFile();
    void reimportSelectedFile();
    void approveImportReview();
    void rejectImportReview();
    void openLayoutCorrectionEntry();
    void startSimulation();
    void pauseSimulation();
    void stopSimulation();
    void tickSimulation();
    void refreshRuntimePanel();
    void createNewWorkspace();
    void beginImportWorkspace();
    void openSelectedRecentProject();
    void closeWorkspace();

    safecrowd::domain::SafeCrowdDomain& domain_;
    QStackedWidget* rootStack_{nullptr};
    QWidget* navigatorPage_{nullptr};
    QWidget* workspacePage_{nullptr};
    QTabWidget* workspaceTabs_{nullptr};
    QListWidget* recentProjectsList_{nullptr};
    QLabel* recentProjectsEmptyValue_{nullptr};
    QLabel* navigatorFeedbackValue_{nullptr};
    QLabel* selectedRecentTitleValue_{nullptr};
    QLabel* selectedRecentStageValue_{nullptr};
    QLabel* selectedRecentSummaryValue_{nullptr};
    QLabel* selectedRecentRestoreValue_{nullptr};
    QLabel* selectedRecentCountsValue_{nullptr};
    QPushButton* openRecentButton_{nullptr};
    QPushButton* clearRecentButton_{nullptr};
    QPushButton* restoreSampleRecentButton_{nullptr};
    QPushButton* newWorkspaceButton_{nullptr};
    QPushButton* importLayoutButton_{nullptr};
    QLabel* currentProjectValue_{nullptr};
    QLabel* currentWorkspaceStageValue_{nullptr};
    QLabel* currentRestoreValue_{nullptr};
    QLabel* currentScenarioCountValue_{nullptr};
    QLabel* currentArtifactCountValue_{nullptr};
    QPushButton* closeWorkspaceButton_{nullptr};
    QLabel* importSourceValue_{nullptr};
    QLabel* importSummaryValue_{nullptr};
    QLabel* importReviewStatusValue_{nullptr};
    QLabel* importCountsValue_{nullptr};
    QLabel* importAuthoringGateValue_{nullptr};
    QLabel* importIssueDetailValue_{nullptr};
    QLabel* layoutCorrectionTargetValue_{nullptr};
    QListWidget* blockingIssuesList_{nullptr};
    QListWidget* warningIssuesList_{nullptr};
    QPushButton* browseImportButton_{nullptr};
    QPushButton* importSelectedFileButton_{nullptr};
    QPushButton* reimportButton_{nullptr};
    QPushButton* approveImportButton_{nullptr};
    QPushButton* rejectImportButton_{nullptr};
    QPushButton* openLayoutCorrectionButton_{nullptr};
    QPushButton* startButton_{nullptr};
    QPushButton* pauseButton_{nullptr};
    QPushButton* stopButton_{nullptr};
    QLabel* workspaceStageValue_{nullptr};
    QLabel* runtimeStateValue_{nullptr};
    QLabel* frameValue_{nullptr};
    QLabel* fixedStepValue_{nullptr};
    QLabel* alphaValue_{nullptr};
    QLabel* runValue_{nullptr};
    QLabel* variationValue_{nullptr};
    QTimer* tickTimer_{nullptr};
    std::vector<RecentProjectEntry> recentProjects_{};
    std::optional<WorkspaceSession> currentWorkspace_{};
    std::filesystem::path importSourcePath_{};
    std::optional<safecrowd::domain::ImportResult> currentImportResult_{};
    safecrowd::domain::DxfImportService importService_{};
};

}  // namespace safecrowd::application
