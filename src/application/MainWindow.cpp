#include "application/MainWindow.h"

#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "domain/SafeCrowdDomain.h"
#include "engine/EngineState.h"

namespace {

using safecrowd::application::RecentProjectEntry;
using safecrowd::application::WorkspaceStage;

QString stateToString(safecrowd::engine::EngineState state) {
    using safecrowd::engine::EngineState;

    switch (state) {
    case EngineState::Stopped:
        return "Stopped";
    case EngineState::Ready:
        return "Ready";
    case EngineState::Running:
        return "Running";
    case EngineState::Paused:
        return "Paused";
    }

    return "Unknown";
}

QString workspaceStageToString(WorkspaceStage stage) {
    switch (stage) {
    case WorkspaceStage::NoProject:
        return "No Project";
    case WorkspaceStage::LayoutNeedsReview:
        return "Layout Review";
    case WorkspaceStage::LayoutReady:
        return "Layout Ready";
    case WorkspaceStage::ScenarioDraftInvalid:
        return "Scenario Draft Needs Input";
    case WorkspaceStage::ScenarioReady:
        return "Scenario Ready";
    case WorkspaceStage::BatchRunning:
        return "Batch Running";
    case WorkspaceStage::BatchPaused:
        return "Batch Paused";
    case WorkspaceStage::AggregationPending:
        return "Aggregation Pending";
    case WorkspaceStage::ResultsAvailable:
        return "Results Available";
    case WorkspaceStage::ComparisonReady:
        return "Comparison Ready";
    case WorkspaceStage::RecommendationReady:
        return "Recommendation Ready";
    }

    return "Unknown";
}

QString workspaceStageAccent(WorkspaceStage stage) {
    switch (stage) {
    case WorkspaceStage::NoProject:
        return "#6b7280";
    case WorkspaceStage::LayoutNeedsReview:
        return "#c05621";
    case WorkspaceStage::LayoutReady:
        return "#2f855a";
    case WorkspaceStage::ScenarioDraftInvalid:
        return "#d97706";
    case WorkspaceStage::ScenarioReady:
        return "#0f766e";
    case WorkspaceStage::BatchRunning:
        return "#2563eb";
    case WorkspaceStage::BatchPaused:
        return "#7c3aed";
    case WorkspaceStage::AggregationPending:
        return "#8b5cf6";
    case WorkspaceStage::ResultsAvailable:
        return "#0f766e";
    case WorkspaceStage::ComparisonReady:
        return "#166534";
    case WorkspaceStage::RecommendationReady:
        return "#9a3412";
    }

    return "#6b7280";
}

bool canOpenRunWorkspace(WorkspaceStage stage) {
    switch (stage) {
    case WorkspaceStage::ScenarioReady:
    case WorkspaceStage::BatchRunning:
    case WorkspaceStage::BatchPaused:
    case WorkspaceStage::AggregationPending:
    case WorkspaceStage::ResultsAvailable:
    case WorkspaceStage::ComparisonReady:
    case WorkspaceStage::RecommendationReady:
        return true;
    case WorkspaceStage::NoProject:
    case WorkspaceStage::LayoutNeedsReview:
    case WorkspaceStage::LayoutReady:
    case WorkspaceStage::ScenarioDraftInvalid:
        return false;
    }

    return false;
}

bool canOpenAnalysisWorkspace(WorkspaceStage stage) {
    switch (stage) {
    case WorkspaceStage::ResultsAvailable:
    case WorkspaceStage::ComparisonReady:
    case WorkspaceStage::RecommendationReady:
        return true;
    case WorkspaceStage::NoProject:
    case WorkspaceStage::LayoutNeedsReview:
    case WorkspaceStage::LayoutReady:
    case WorkspaceStage::ScenarioDraftInvalid:
    case WorkspaceStage::ScenarioReady:
    case WorkspaceStage::BatchRunning:
    case WorkspaceStage::BatchPaused:
    case WorkspaceStage::AggregationPending:
        return false;
    }

    return false;
}

QString stageDetailSummary(WorkspaceStage stage) {
    switch (stage) {
    case WorkspaceStage::NoProject:
        return "Open, import, or create a workspace before authoring starts.";
    case WorkspaceStage::LayoutNeedsReview:
        return "Import review is still blocking scenario readiness and run entry.";
    case WorkspaceStage::LayoutReady:
        return "The layout is approved, but a valid scenario has not been opened yet.";
    case WorkspaceStage::ScenarioDraftInvalid:
        return "A scenario draft exists, but required inputs are still missing.";
    case WorkspaceStage::ScenarioReady:
        return "A valid scenario is selected and the run workspace can open.";
    case WorkspaceStage::BatchRunning:
        return "Run control and live playback are active in the current batch.";
    case WorkspaceStage::BatchPaused:
        return "The batch is paused and can be resumed or stopped.";
    case WorkspaceStage::AggregationPending:
        return "Playback has finished, but persisted summaries are still pending.";
    case WorkspaceStage::ResultsAvailable:
        return "Run results and variation summaries are available for analysis.";
    case WorkspaceStage::ComparisonReady:
        return "Baseline and alternative summaries are ready for comparison.";
    case WorkspaceStage::RecommendationReady:
        return "Comparison artifacts are ready for recommendation and export.";
    }

    return "Workspace state unknown.";
}

QLabel* createBodyLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    return label;
}

QLabel* createValueLabel(QWidget* parent) {
    auto* label = new QLabel("-", parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

QGroupBox* createInfoGroup(const QString& title, const QString& body, QWidget* parent) {
    auto* group = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(8);
    layout->addWidget(createBodyLabel(body, group));
    layout->addStretch();
    return group;
}

QLabel* createRoleLabel(const QString& text, const char* role, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setProperty("role", role);
    return label;
}

QLabel* createStageBadge(const QString& text, WorkspaceStage stage, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setProperty("role", "stageBadge");
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(
        QString("background:%1; color:white; border-radius:11px; padding:5px 10px; font-weight:600;")
            .arg(workspaceStageAccent(stage)));
    return label;
}

void repolish(QWidget* widget) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

std::vector<RecentProjectEntry> makeSampleRecentProjects() {
    return {
        {
            .projectId = "results-ready-concourse",
            .displayName = "West Concourse Review",
            .stageSummary = "Approved layout, baseline open, persisted run summaries restored.",
            .detailSummary = "Restored through ProjectRepository with 3 scenarios and 6 artifact references.",
            .stage = WorkspaceStage::ResultsAvailable,
            .scenarioCount = 3,
            .artifactCount = 6,
            .canRestore = true,
        },
        {
            .projectId = "pending-layout-review",
            .displayName = "Expo Hall Draft Import",
            .stageSummary = "Layout restored in review state with unresolved topology blockers.",
            .detailSummary = "Authoring should reopen on import review before any scenario is considered run-ready.",
            .stage = WorkspaceStage::LayoutNeedsReview,
            .scenarioCount = 1,
            .artifactCount = 0,
            .canRestore = true,
        },
        {
            .projectId = "broken-artifact-index",
            .displayName = "Arena Egress Archive",
            .stageSummary = "Recent entry kept for visibility, but restore validation fails.",
            .detailSummary = "Project metadata exists, but the canonical artifact index cannot be resolved.",
            .stage = WorkspaceStage::NoProject,
            .scenarioCount = 0,
            .artifactCount = 0,
            .canRestore = false,
        },
    };
}

QWidget* createRecentProjectCard(const RecentProjectEntry& entry, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("RecentProjectCard");

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(10);

    auto* titleLabel = createRoleLabel(entry.displayName, "cardTitle", card);
    auto* stageLabel = createStageBadge(workspaceStageToString(entry.stage), entry.stage, card);
    headerLayout->addWidget(titleLabel, 1);
    headerLayout->addWidget(stageLabel, 0, Qt::AlignTop);

    auto* summaryLabel = createRoleLabel(entry.stageSummary, "cardSummary", card);
    const QString metaText = QString("%1  |  %2 scenarios  |  %3 artifact refs")
                                 .arg(entry.canRestore ? "Restorable" : "Restore blocked")
                                 .arg(entry.scenarioCount)
                                 .arg(entry.artifactCount);
    auto* metaLabel = createRoleLabel(metaText, "cardMeta", card);

    layout->addLayout(headerLayout);
    layout->addWidget(summaryLabel);
    layout->addWidget(metaLabel);

    return card;
}

}  // namespace

namespace safecrowd::application {

MainWindow::MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent)
    : QMainWindow(parent),
      domain_(domain) {
    auto* centralWidget = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    auto* heroCard = new QFrame(centralWidget);
    heroCard->setObjectName("HeroCard");
    auto* heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(22, 20, 22, 20);
    heroLayout->setSpacing(8);
    heroLayout->addWidget(createRoleLabel("APPLICATION WORKSPACE", "heroEyebrow", heroCard));
    heroLayout->addWidget(createRoleLabel("SafeCrowd Project Workspace", "heroTitle", heroCard));
    heroLayout->addWidget(createRoleLabel(
        "The app now starts in the documented No Project state. "
        "Restore, new workspace entry, and import entry are visible first so authoring, run, and analysis do not open out of order.",
        "heroBody",
        heroCard));
    rootLayout->addWidget(heroCard);

    rootStack_ = new QStackedWidget(centralWidget);
    rootLayout->addWidget(rootStack_, 1);

    navigatorPage_ = new QWidget(rootStack_);
    auto* navigatorLayout = new QVBoxLayout(navigatorPage_);
    navigatorLayout->setContentsMargins(0, 0, 0, 0);
    navigatorLayout->setSpacing(12);
    navigatorLayout->addWidget(createRoleLabel("Project Navigator", "sectionTitle", navigatorPage_));
    navigatorLayout->addWidget(createRoleLabel(
        "Choose a recent workspace, start a new shell, or jump into layout import. "
        "The navigator is the only entry point while the app is in No Project.",
        "sectionBody",
        navigatorPage_));

    navigatorFeedbackValue_ = createRoleLabel(
        "Select a recent project, create a new workspace, or import a layout to enter the workspace.",
        "feedback",
        navigatorPage_);
    navigatorLayout->addWidget(navigatorFeedbackValue_);

    auto* navigatorGrid = new QGridLayout();
    navigatorGrid->setHorizontalSpacing(12);
    navigatorGrid->setVerticalSpacing(12);
    navigatorGrid->setColumnStretch(0, 3);
    navigatorGrid->setColumnStretch(1, 2);

    auto* recentGroup = new QGroupBox("Recent Projects", navigatorPage_);
    auto* recentLayout = new QVBoxLayout(recentGroup);
    recentLayout->setSpacing(8);
    recentLayout->addWidget(createRoleLabel(
        "Recent entries reopen workspace context through ProjectRepository. "
        "Broken restores stay visible instead of silently disappearing.",
        "groupBody",
        recentGroup));

    recentProjectsList_ = new QListWidget(recentGroup);
    recentProjectsList_->setSpacing(10);
    recentProjectsList_->setFrameShape(QFrame::NoFrame);
    recentProjectsList_->setSelectionMode(QAbstractItemView::SingleSelection);
    recentProjectsList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    recentLayout->addWidget(recentProjectsList_, 1);

    recentProjectsEmptyValue_ = createRoleLabel(
        "No recent projects are available yet. Use New Workspace or Import Layout to create the first project context.",
        "emptyState",
        recentGroup);
    recentLayout->addWidget(recentProjectsEmptyValue_);

    auto* recentButtonLayout = new QHBoxLayout();
    openRecentButton_ = new QPushButton("Open Selected Recent", recentGroup);
    clearRecentButton_ = new QPushButton("Clear Recent Samples", recentGroup);
    restoreSampleRecentButton_ = new QPushButton("Restore Sample Recents", recentGroup);
    recentButtonLayout->addWidget(clearRecentButton_);
    recentButtonLayout->addWidget(restoreSampleRecentButton_);
    recentLayout->addLayout(recentButtonLayout);

    auto* detailGroup = new QGroupBox("Selected Project", navigatorPage_);
    auto* detailLayout = new QVBoxLayout(detailGroup);
    detailLayout->setSpacing(10);
    selectedRecentTitleValue_ = createRoleLabel("No recent project selected", "detailTitle", detailGroup);
    selectedRecentStageValue_ = createStageBadge("No Project", WorkspaceStage::NoProject, detailGroup);
    selectedRecentSummaryValue_ = createRoleLabel(
        "Pick a recent entry to inspect restore state, artifact coverage, and next workspace gate.",
        "detailBody",
        detailGroup);
    selectedRecentCountsValue_ = createRoleLabel("0 scenarios  |  0 artifact refs", "detailMeta", detailGroup);
    selectedRecentRestoreValue_ = createRoleLabel("Restore status will appear here.", "detailMeta", detailGroup);
    detailLayout->addWidget(selectedRecentTitleValue_);
    detailLayout->addWidget(selectedRecentStageValue_, 0, Qt::AlignLeft);
    detailLayout->addWidget(selectedRecentSummaryValue_);
    detailLayout->addWidget(selectedRecentCountsValue_);
    detailLayout->addWidget(selectedRecentRestoreValue_);
    detailLayout->addStretch();
    detailLayout->addWidget(openRecentButton_);

    auto* entryGroup = new QGroupBox("Quick Actions", navigatorPage_);
    auto* entryLayout = new QVBoxLayout(entryGroup);
    entryLayout->setSpacing(8);
    entryLayout->addWidget(createRoleLabel(
        "New Workspace opens a shell without an approved layout. "
        "Import Layout jumps directly into review and keeps Run locked until blockers clear.",
        "groupBody",
        entryGroup));

    newWorkspaceButton_ = new QPushButton("New Workspace", entryGroup);
    importLayoutButton_ = new QPushButton("Import Layout", entryGroup);
    entryLayout->addWidget(newWorkspaceButton_);
    entryLayout->addWidget(importLayoutButton_);
    entryLayout->addStretch();

    navigatorGrid->addWidget(recentGroup, 0, 0);
    navigatorGrid->addWidget(detailGroup, 0, 1);
    navigatorGrid->addWidget(entryGroup, 1, 1);
    navigatorGrid->addWidget(createInfoGroup(
        "Repository Boundaries",
        "<b>ProjectRepository</b> restores layout, scenario family, run metadata, and artifact indexes.<br/>"
        "<b>ResultRepository</b> is analysis-only storage and should never replace project restore as the entry path.",
        navigatorPage_), 1, 0);
    navigatorLayout->addLayout(navigatorGrid, 1);

    workspacePage_ = new QWidget(rootStack_);
    auto* workspaceLayout = new QVBoxLayout(workspacePage_);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(12);

    auto* workspaceSummaryGroup = new QGroupBox("Current Workspace", workspacePage_);
    auto* workspaceSummaryLayout = new QFormLayout(workspaceSummaryGroup);
    workspaceSummaryLayout->setLabelAlignment(Qt::AlignLeft);
    workspaceSummaryLayout->setFormAlignment(Qt::AlignTop | Qt::AlignLeft);
    currentProjectValue_ = createValueLabel(workspaceSummaryGroup);
    currentWorkspaceStageValue_ = createValueLabel(workspaceSummaryGroup);
    currentRestoreValue_ = createValueLabel(workspaceSummaryGroup);
    currentScenarioCountValue_ = createValueLabel(workspaceSummaryGroup);
    currentArtifactCountValue_ = createValueLabel(workspaceSummaryGroup);
    workspaceSummaryLayout->addRow("Project", currentProjectValue_);
    workspaceSummaryLayout->addRow("Workspace stage", currentWorkspaceStageValue_);
    workspaceSummaryLayout->addRow("Restore status", currentRestoreValue_);
    workspaceSummaryLayout->addRow("Scenario family", currentScenarioCountValue_);
    workspaceSummaryLayout->addRow("Artifact index", currentArtifactCountValue_);

    closeWorkspaceButton_ = new QPushButton("Close Workspace", workspaceSummaryGroup);
    workspaceSummaryLayout->addRow("", closeWorkspaceButton_);
    workspaceLayout->addWidget(workspaceSummaryGroup);

    workspaceTabs_ = new QTabWidget(workspacePage_);
    workspaceTabs_->setDocumentMode(true);
    workspaceLayout->addWidget(workspaceTabs_, 1);

    auto* projectPage = new QWidget(workspaceTabs_);
    auto* projectLayout = new QGridLayout(projectPage);
    projectLayout->setContentsMargins(0, 0, 0, 0);
    projectLayout->setHorizontalSpacing(12);
    projectLayout->setVerticalSpacing(12);
    projectLayout->setColumnStretch(0, 1);
    projectLayout->setColumnStretch(1, 1);
    projectLayout->addWidget(createInfoGroup(
        "Project Navigator",
        "<b>Scope</b><br/>"
        "Project remains the top-level navigator for restore status, recent/open flow, and workspace transitions.<br/><br/>"
        "<b>Current implementation</b><br/>"
        "This tab keeps top-level project context distinct from authoring, run, and analysis details.",
        projectPage), 0, 0);
    projectLayout->addWidget(createInfoGroup(
        "Project Save/Open",
        "<b>ProjectRepository</b><br/>"
        "Workspace restore is described here as layout, scenario family, run metadata, and artifact index recovery.<br/><br/>"
        "<b>Current shell</b><br/>"
        "Persistence is still a placeholder, but entry and restore responsibilities are now explicit.",
        projectPage), 0, 1);
    projectLayout->addWidget(createInfoGroup(
        "Repository Boundaries",
        "<b>ProjectRepository</b> restores project context and authoring state.<br/>"
        "<b>ResultRepository</b> stays analysis-only and is consumed after persisted results exist.",
        projectPage), 1, 0, 1, 2);

    auto* authoringPage = new QWidget(workspaceTabs_);
    auto* authoringLayout = new QGridLayout(authoringPage);
    authoringLayout->setContentsMargins(0, 0, 0, 0);
    authoringLayout->setHorizontalSpacing(12);
    authoringLayout->setVerticalSpacing(12);
    authoringLayout->setColumnStretch(0, 1);
    authoringLayout->setColumnStretch(1, 1);
    authoringLayout->addWidget(createInfoGroup(
        "Import Workflow UI",
        "DXF import, reimport, and source selection enter here before any scenario authoring begins.",
        authoringPage), 0, 0);
    authoringLayout->addWidget(createInfoGroup(
        "Issue Review Panel",
        "Blocking topology issues, warnings, approval state, and traceable problem locations surface here before run is enabled.",
        authoringPage), 0, 1);
    authoringLayout->addWidget(createInfoGroup(
        "Layout Canvas + Inspector",
        "Manual correction stays a 2D topology editor with inspector support, not a full CAD environment.",
        authoringPage), 1, 0);
    authoringLayout->addWidget(createInfoGroup(
        "Scenario Library",
        "Baseline, alternatives, and recommended drafts remain distinct so lineage is clear before comparison and scenarioize flows.",
        authoringPage), 1, 1);
    authoringLayout->addWidget(createInfoGroup(
        "Scenario Template Picker",
        "Template cards will expose intended use, risk axis, and layout prerequisites through ScenarioTemplateCatalog quick starts.",
        authoringPage), 2, 0);
    authoringLayout->addWidget(createInfoGroup(
        "Scenario Editor Tabs",
        "Population, Environment, Control, and Execution contracts stay separated here so authoring does not collapse into one form.",
        authoringPage), 2, 1);
    authoringLayout->addWidget(createInfoGroup(
        "Readiness Panel",
        "Required field gaps, remaining blockers, and run gating are centralized here instead of being hidden behind disabled run buttons.",
        authoringPage), 3, 0);
    authoringLayout->addWidget(createInfoGroup(
        "Variation Diff List",
        "Changed items versus baseline track route cost assumptions, control changes, inflow settings, visibility conditions, and template origin.",
        authoringPage), 3, 1);

    auto* runPage = new QWidget(workspaceTabs_);
    auto* runLayout = new QGridLayout(runPage);
    runLayout->setContentsMargins(0, 0, 0, 0);
    runLayout->setHorizontalSpacing(12);
    runLayout->setVerticalSpacing(12);
    runLayout->setColumnStretch(0, 1);
    runLayout->setColumnStretch(1, 1);

    auto* runQueueGroup = createInfoGroup(
        "Run Queue",
        "Selected variations, repeat count, and seed contract will be staged here before the batch runner starts or replays a scenario family.",
        runPage);

    auto* runControlGroup = new QGroupBox("Run Control Panel", runPage);
    auto* runControlLayout = new QVBoxLayout(runControlGroup);
    runControlLayout->setSpacing(10);
    runControlLayout->addWidget(createBodyLabel(
        "Playback control remains the only live path in the current prototype. "
        "The run workspace is now gated behind project + scenario readiness instead of being the default entry screen.",
        runControlGroup));

    auto* buttonLayout = new QHBoxLayout();
    startButton_ = new QPushButton("Start Playback", runControlGroup);
    pauseButton_ = new QPushButton("Pause Playback", runControlGroup);
    stopButton_ = new QPushButton("Stop Playback", runControlGroup);
    buttonLayout->addWidget(startButton_);
    buttonLayout->addWidget(pauseButton_);
    buttonLayout->addWidget(stopButton_);
    runControlLayout->addLayout(buttonLayout);
    runControlLayout->addWidget(createBodyLabel(
        "<b>Planned next</b><br/>"
        "Run queue handoff, repeat execution, variation selection, and readiness-aware enablement.",
        runControlGroup));
    runControlLayout->addStretch();

    auto* batchProgressGroup = new QGroupBox("Batch Progress", runPage);
    auto* batchProgressLayout = new QFormLayout(batchProgressGroup);
    batchProgressLayout->setLabelAlignment(Qt::AlignLeft);
    batchProgressLayout->setFormAlignment(Qt::AlignTop | Qt::AlignLeft);
    workspaceStageValue_ = createValueLabel(batchProgressGroup);
    runValue_ = createValueLabel(batchProgressGroup);
    variationValue_ = createValueLabel(batchProgressGroup);
    batchProgressLayout->addRow("Workspace stage", workspaceStageValue_);
    batchProgressLayout->addRow("Current run", runValue_);
    batchProgressLayout->addRow("Variation", variationValue_);

    auto* runtimeStatusGroup = new QGroupBox("Runtime Status", runPage);
    auto* runtimeStatusLayout = new QFormLayout(runtimeStatusGroup);
    runtimeStatusLayout->setLabelAlignment(Qt::AlignLeft);
    runtimeStatusLayout->setFormAlignment(Qt::AlignTop | Qt::AlignLeft);
    runtimeStateValue_ = createValueLabel(runtimeStatusGroup);
    frameValue_ = createValueLabel(runtimeStatusGroup);
    fixedStepValue_ = createValueLabel(runtimeStatusGroup);
    alphaValue_ = createValueLabel(runtimeStatusGroup);
    runtimeStatusLayout->addRow("Engine state", runtimeStateValue_);
    runtimeStatusLayout->addRow("Rendered frames", frameValue_);
    runtimeStatusLayout->addRow("Fixed steps", fixedStepValue_);
    runtimeStatusLayout->addRow("Interpolation alpha", alphaValue_);

    auto* liveViewportGroup = createInfoGroup(
        "Live Viewport",
        "Runtime snapshots belong here. This surface is intentionally separate from persisted comparison and recommendation evidence.",
        runPage);
    auto* heatmapOverlayGroup = createInfoGroup(
        "Heatmap Overlay",
        "Live overlays will toggle here during playback. Persisted heatmap layers move to Analysis once summaries are stored.",
        runPage);

    runLayout->addWidget(runQueueGroup, 0, 0);
    runLayout->addWidget(runControlGroup, 0, 1);
    runLayout->addWidget(batchProgressGroup, 1, 0);
    runLayout->addWidget(runtimeStatusGroup, 1, 1);
    runLayout->addWidget(liveViewportGroup, 2, 0);
    runLayout->addWidget(heatmapOverlayGroup, 2, 1);

    auto* analysisPage = new QWidget(workspaceTabs_);
    auto* analysisLayout = new QGridLayout(analysisPage);
    analysisLayout->setContentsMargins(0, 0, 0, 0);
    analysisLayout->setHorizontalSpacing(12);
    analysisLayout->setVerticalSpacing(12);
    analysisLayout->setColumnStretch(0, 1);
    analysisLayout->setColumnStretch(1, 1);
    analysisLayout->addWidget(createInfoGroup(
        "Run Results Panel",
        "Single-run summaries will read persisted RunResult artifacts first, not transient runtime state.",
        analysisPage), 0, 0);
    analysisLayout->addWidget(createInfoGroup(
        "Variation Summary",
        "Repeated-run aggregates, seed-aware variation context, and visibility condition traces will summarize here.",
        analysisPage), 0, 1);
    analysisLayout->addWidget(createInfoGroup(
        "Comparison View",
        "Baseline versus alternatives stays a persisted ScenarioComparison and CumulativeArtifact reader, not an ad hoc delta calculator.",
        analysisPage), 1, 0, 1, 2);
    analysisLayout->addWidget(createInfoGroup(
        "Recommendation Drawer",
        "Recommendation evidence and scenarioize actions remain downstream of persisted comparison artifacts.",
        analysisPage), 2, 0);
    analysisLayout->addWidget(createInfoGroup(
        "Export Dialog",
        "Canonical artifact bundle export will stay disabled until comparison-ready persisted results exist.",
        analysisPage), 2, 1);

    workspaceTabs_->addTab(projectPage, "Project");
    workspaceTabs_->addTab(authoringPage, "Authoring");
    workspaceTabs_->addTab(runPage, "Run");
    workspaceTabs_->addTab(analysisPage, "Analysis");

    rootStack_->addWidget(navigatorPage_);
    rootStack_->addWidget(workspacePage_);

    tickTimer_ = new QTimer(this);
    tickTimer_->setInterval(16);

    connect(newWorkspaceButton_, &QPushButton::clicked, this, [this]() { createNewWorkspace(); });
    connect(importLayoutButton_, &QPushButton::clicked, this, [this]() { beginImportWorkspace(); });
    connect(openRecentButton_, &QPushButton::clicked, this, [this]() { openSelectedRecentProject(); });
    connect(clearRecentButton_, &QPushButton::clicked, this, [this]() {
        recentProjects_.clear();
        rebuildRecentProjectsList();
        navigatorFeedbackValue_->setText(
            "Recent project history is empty. New workspace and import entry remain available.");
    });
    connect(restoreSampleRecentButton_, &QPushButton::clicked, this, [this]() {
        populateSampleRecentProjects();
        rebuildRecentProjectsList();
        navigatorFeedbackValue_->setText(
            "Sample recent projects restored. Open one to enter the workspace.");
    });
    connect(recentProjectsList_, &QListWidget::currentRowChanged, this, [this](int) { refreshNavigator(); });
    connect(recentProjectsList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) {
        openSelectedRecentProject();
    });
    connect(closeWorkspaceButton_, &QPushButton::clicked, this, [this]() { closeWorkspace(); });

    connect(startButton_, &QPushButton::clicked, this, [this]() { startSimulation(); });
    connect(pauseButton_, &QPushButton::clicked, this, [this]() { pauseSimulation(); });
    connect(stopButton_, &QPushButton::clicked, this, [this]() { stopSimulation(); });
    connect(tickTimer_, &QTimer::timeout, this, [this]() { tickSimulation(); });

    setCentralWidget(centralWidget);
    setWindowTitle("SafeCrowd Workspace");
    resize(1280, 820);

    applyTheme();
    populateSampleRecentProjects();
    rebuildRecentProjectsList();
    refreshWorkspaceChrome();
    refreshRuntimePanel();
}

void MainWindow::applyTheme() {
    setStyleSheet(R"(
        QMainWindow {
            background: #f4efe8;
            color: #1f2937;
        }
        QFrame#HeroCard {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #fff7ed, stop:1 #f3e8d7);
            border: 1px solid #e4d5c3;
            border-radius: 22px;
        }
        QGroupBox {
            background: #fffdfa;
            border: 1px solid #ded3c6;
            border-radius: 18px;
            margin-top: 16px;
            padding-top: 12px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 16px;
            padding: 0 6px;
            color: #4b5563;
        }
        QLabel[role="heroEyebrow"] {
            color: #9a3412;
            font-size: 11px;
            font-weight: 700;
            letter-spacing: 0.18em;
        }
        QLabel[role="heroTitle"] {
            color: #111827;
            font-size: 26px;
            font-weight: 700;
        }
        QLabel[role="heroBody"], QLabel[role="sectionBody"], QLabel[role="groupBody"],
        QLabel[role="detailBody"], QLabel[role="feedback"] {
            color: #4b5563;
            font-size: 13px;
            line-height: 1.4;
        }
        QLabel[role="sectionTitle"] {
            color: #111827;
            font-size: 20px;
            font-weight: 700;
        }
        QLabel[role="detailTitle"], QLabel[role="cardTitle"] {
            color: #111827;
            font-size: 17px;
            font-weight: 700;
        }
        QLabel[role="detailMeta"], QLabel[role="cardMeta"], QLabel[role="emptyState"] {
            color: #6b7280;
            font-size: 12px;
        }
        QLabel[role="cardSummary"] {
            color: #374151;
            font-size: 13px;
        }
        QLabel[role="feedback"] {
            background: #fff7ed;
            border: 1px solid #f1d5b6;
            border-radius: 14px;
            padding: 10px 12px;
        }
        QLabel[role="stageBadge"] {
            font-size: 11px;
        }
        QListWidget {
            background: transparent;
            border: none;
            outline: 0;
        }
        QListWidget::item {
            border: none;
            padding: 0px;
        }
        QFrame#RecentProjectCard {
            background: #ffffff;
            border: 1px solid #e6ddd2;
            border-radius: 16px;
        }
        QFrame#RecentProjectCard[selected="true"] {
            background: #fff7ed;
            border: 2px solid #c56b2c;
        }
        QPushButton {
            background: #fbf7f2;
            border: 1px solid #d9ccbd;
            border-radius: 12px;
            padding: 10px 16px;
            min-height: 20px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #f6ecdf;
        }
        QPushButton:pressed {
            background: #efdcc6;
        }
        QPushButton:disabled {
            color: #9ca3af;
            background: #f3f4f6;
            border-color: #e5e7eb;
        }
        QTabWidget::pane {
            border: 1px solid #ded3c6;
            border-radius: 16px;
            background: #fffdfa;
            top: -1px;
        }
        QTabBar::tab {
            background: #ece4d8;
            border: 1px solid #d5c7b7;
            border-bottom: none;
            border-top-left-radius: 12px;
            border-top-right-radius: 12px;
            padding: 10px 18px;
            margin-right: 4px;
            font-weight: 600;
            color: #4b5563;
        }
        QTabBar::tab:selected {
            background: #fffdfa;
            color: #111827;
        }
        QTabBar::tab:!selected:hover {
            background: #f4ecdf;
        }
    )");
}

void MainWindow::populateSampleRecentProjects() {
    recentProjects_ = makeSampleRecentProjects();
}

void MainWindow::rebuildRecentProjectsList() {
    recentProjectsList_->clear();

    for (const auto& entry : recentProjects_) {
        auto* item = new QListWidgetItem();
        item->setToolTip(entry.detailSummary);
        item->setSizeHint(QSize(0, 104));
        recentProjectsList_->addItem(item);
        recentProjectsList_->setItemWidget(item, createRecentProjectCard(entry, recentProjectsList_));
    }

    if (!recentProjects_.empty()) {
        recentProjectsList_->setCurrentRow(0);
    }

    refreshNavigator();
}

void MainWindow::refreshNavigator() {
    const bool hasRecents = !recentProjects_.empty();

    recentProjectsList_->setVisible(hasRecents);
    recentProjectsEmptyValue_->setVisible(!hasRecents);
    clearRecentButton_->setEnabled(hasRecents);
    restoreSampleRecentButton_->setVisible(!hasRecents);

    const int selectedRow = recentProjectsList_->currentRow();
    openRecentButton_->setEnabled(hasRecents && selectedRow >= 0 && selectedRow < static_cast<int>(recentProjects_.size()));

    if (!hasRecents || selectedRow < 0 || selectedRow >= static_cast<int>(recentProjects_.size())) {
        selectedRecentTitleValue_->setText("No recent project selected");
        selectedRecentStageValue_->setText("No Project");
        selectedRecentStageValue_->setStyleSheet(
            QString("background:%1; color:white; border-radius:11px; padding:5px 10px; font-weight:600;")
                .arg(workspaceStageAccent(WorkspaceStage::NoProject)));
        selectedRecentSummaryValue_->setText(
            "Pick a recent entry to inspect restore state, artifact coverage, and next workspace gate.");
        selectedRecentCountsValue_->setText("0 scenarios  |  0 artifact refs");
        selectedRecentRestoreValue_->setText("Restore status will appear here.");
    } else {
        const auto& entry = recentProjects_[selectedRow];
        selectedRecentTitleValue_->setText(entry.displayName);
        selectedRecentStageValue_->setText(workspaceStageToString(entry.stage));
        selectedRecentStageValue_->setStyleSheet(
            QString("background:%1; color:white; border-radius:11px; padding:5px 10px; font-weight:600;")
                .arg(workspaceStageAccent(entry.stage)));
        selectedRecentSummaryValue_->setText(entry.detailSummary);
        selectedRecentCountsValue_->setText(
            QString("%1 scenarios  |  %2 artifact refs").arg(entry.scenarioCount).arg(entry.artifactCount));
        selectedRecentRestoreValue_->setText(
            entry.canRestore ? "Restore ready through ProjectRepository." : "Restore blocked. Keep entry visible and explain the failure.");
    }

    for (int index = 0; index < recentProjectsList_->count(); ++index) {
        auto* item = recentProjectsList_->item(index);
        auto* widget = recentProjectsList_->itemWidget(item);
        if (widget == nullptr) {
            continue;
        }

        widget->setProperty("selected", index == selectedRow);
        repolish(widget);
    }
}

void MainWindow::refreshWorkspaceChrome() {
    if (!currentWorkspace_.has_value()) {
        rootStack_->setCurrentWidget(navigatorPage_);
        currentProjectValue_->setText("No workspace open");
        currentWorkspaceStageValue_->setText(workspaceStageToString(WorkspaceStage::NoProject));
        currentRestoreValue_->setText("Project entry is still at NoProject.");
        currentScenarioCountValue_->setText("0 drafts");
        currentArtifactCountValue_->setText("0 indexed artifacts");
        workspaceTabs_->setTabEnabled(0, false);
        workspaceTabs_->setTabEnabled(1, false);
        workspaceTabs_->setTabEnabled(2, false);
        workspaceTabs_->setTabEnabled(3, false);
        setWindowTitle("SafeCrowd Workspace");
        return;
    }

    rootStack_->setCurrentWidget(workspacePage_);

    const auto& workspace = *currentWorkspace_;
    currentProjectValue_->setText(QString("%1 (%2)").arg(workspace.displayName, workspace.projectId));
    currentWorkspaceStageValue_->setText(workspaceStageToString(workspace.stage));
    currentRestoreValue_->setText(workspace.restoreSummary);
    currentScenarioCountValue_->setText(QString("%1 scenarios in family").arg(workspace.scenarioCount));
    currentArtifactCountValue_->setText(QString("%1 artifact references").arg(workspace.artifactCount));

    workspaceTabs_->setTabEnabled(0, true);
    workspaceTabs_->setTabEnabled(1, true);
    workspaceTabs_->setTabEnabled(2, canOpenRunWorkspace(workspace.stage));
    workspaceTabs_->setTabEnabled(3, canOpenAnalysisWorkspace(workspace.stage));

    if (!workspaceTabs_->isTabEnabled(workspaceTabs_->currentIndex())) {
        workspaceTabs_->setCurrentIndex(0);
    }

    setWindowTitle(QString("SafeCrowd Workspace - %1").arg(workspace.displayName));
}

void MainWindow::createNewWorkspace() {
    stopSimulation();

    currentWorkspace_ = WorkspaceSession{
        .projectId = "new-workspace",
        .displayName = "New Workspace",
        .restoreSummary = "Workspace created. Import a layout before any scenario or run workflow can continue.",
        .stage = WorkspaceStage::LayoutNeedsReview,
        .scenarioCount = 0,
        .artifactCount = 0,
    };

    workspaceTabs_->setCurrentIndex(0);
    navigatorFeedbackValue_->setText(
        "A new workspace shell has been created. Layout review is now the active gate.");
    refreshWorkspaceChrome();
    refreshRuntimePanel();
}

void MainWindow::beginImportWorkspace() {
    stopSimulation();

    currentWorkspace_ = WorkspaceSession{
        .projectId = "import-workspace",
        .displayName = "Imported Layout Draft",
        .restoreSummary = "Import entry opened. Review blockers and approval state before scenario authoring proceeds.",
        .stage = WorkspaceStage::LayoutNeedsReview,
        .scenarioCount = 0,
        .artifactCount = 0,
    };

    workspaceTabs_->setCurrentIndex(1);
    navigatorFeedbackValue_->setText(
        "Import entry moved the workspace into LayoutNeedsReview. Run stays blocked until review clears.");
    refreshWorkspaceChrome();
    refreshRuntimePanel();
}

void MainWindow::openSelectedRecentProject() {
    const int selectedRow = recentProjectsList_->currentRow();
    if (selectedRow < 0 || selectedRow >= static_cast<int>(recentProjects_.size())) {
        navigatorFeedbackValue_->setText(
            "Select a recent project first. Recent restore and new/import entry stay separate on purpose.");
        return;
    }

    const auto& entry = recentProjects_[selectedRow];
    if (!entry.canRestore) {
        navigatorFeedbackValue_->setText(
            QString("<b>Restore failed:</b> %1<br/>%2")
                .arg(entry.displayName, entry.detailSummary));
        return;
    }

    stopSimulation();

    currentWorkspace_ = WorkspaceSession{
        .projectId = entry.projectId,
        .displayName = entry.displayName,
        .restoreSummary = entry.detailSummary,
        .stage = entry.stage,
        .scenarioCount = entry.scenarioCount,
        .artifactCount = entry.artifactCount,
    };

    if (entry.stage == WorkspaceStage::LayoutNeedsReview || entry.stage == WorkspaceStage::LayoutReady) {
        workspaceTabs_->setCurrentIndex(1);
    } else {
        workspaceTabs_->setCurrentIndex(0);
    }

    navigatorFeedbackValue_->setText(
        QString("Restored <b>%1</b> through the Project Navigator. Top-level tabs now reflect the restored workspace gate.")
            .arg(entry.displayName));
    refreshWorkspaceChrome();
    refreshRuntimePanel();
}

void MainWindow::closeWorkspace() {
    stopSimulation();
    currentWorkspace_.reset();
    navigatorFeedbackValue_->setText(
        "Workspace closed. Recent/open/import entry points are available again from NoProject.");
    refreshWorkspaceChrome();
    refreshRuntimePanel();
}

void MainWindow::startSimulation() {
    if (!currentWorkspace_.has_value()) {
        navigatorFeedbackValue_->setText(
            "Run is gated behind an open workspace and a scenario-ready state.");
        return;
    }

    if (!canOpenRunWorkspace(currentWorkspace_->stage)) {
        navigatorFeedbackValue_->setText(
            "Run is still blocked. Clear layout/scenario gates before opening playback.");
        return;
    }

    domain_.start();
    currentWorkspace_->stage = WorkspaceStage::BatchRunning;
    tickTimer_->start();
    refreshWorkspaceChrome();
    refreshRuntimePanel();
}

void MainWindow::pauseSimulation() {
    domain_.pause();

    if (currentWorkspace_.has_value() && currentWorkspace_->stage == WorkspaceStage::BatchRunning) {
        currentWorkspace_->stage = WorkspaceStage::BatchPaused;
    }

    tickTimer_->stop();
    refreshWorkspaceChrome();
    refreshRuntimePanel();
}

void MainWindow::stopSimulation() {
    domain_.stop();

    if (currentWorkspace_.has_value() &&
        (currentWorkspace_->stage == WorkspaceStage::BatchRunning
         || currentWorkspace_->stage == WorkspaceStage::BatchPaused)) {
        currentWorkspace_->stage = WorkspaceStage::ScenarioReady;
    }

    tickTimer_->stop();
    refreshWorkspaceChrome();
    refreshRuntimePanel();
}

void MainWindow::tickSimulation() {
    domain_.update(1.0 / 60.0);
    refreshRuntimePanel();
}

void MainWindow::refreshRuntimePanel() {
    using safecrowd::engine::EngineState;

    const auto summary = domain_.summary();
    const WorkspaceStage workspaceStage =
        currentWorkspace_.has_value() ? currentWorkspace_->stage : WorkspaceStage::NoProject;

    workspaceStageValue_->setText(workspaceStageToString(workspaceStage));
    runtimeStateValue_->setText(stateToString(summary.state));
    frameValue_->setText(QString::number(summary.frameIndex));
    fixedStepValue_->setText(QString::number(summary.fixedStepIndex));
    alphaValue_->setText(QString::number(summary.alpha, 'f', 2));

    if (!currentWorkspace_.has_value()) {
        runValue_->setText("Run unavailable until a workspace is opened.");
        variationValue_->setText("No scenario family in context.");
    } else if (!canOpenRunWorkspace(workspaceStage)) {
        runValue_->setText(stageDetailSummary(workspaceStage));
        variationValue_->setText("Run queue remains gated by the current workspace stage.");
    } else if (summary.state == EngineState::Running || summary.state == EngineState::Paused) {
        runValue_->setText("Prototype run 1 / repeat placeholder");
        variationValue_->setText("Baseline placeholder (domain queue wiring pending)");
    } else if (summary.frameIndex > 0 || summary.fixedStepIndex > 0) {
        runValue_->setText("Last prototype playback retained in runtime counters");
        variationValue_->setText("Persisted batch metadata is still placeholder-only");
    } else {
        runValue_->setText("Queue ready but no playback has started yet");
        variationValue_->setText("Selected scenario family placeholder");
    }

    const bool runAllowed = currentWorkspace_.has_value() && canOpenRunWorkspace(workspaceStage);
    const bool isRunning = summary.state == EngineState::Running;
    const bool isPaused = summary.state == EngineState::Paused;

    startButton_->setEnabled(runAllowed && !isRunning);
    pauseButton_->setEnabled(runAllowed && isRunning);
    stopButton_->setEnabled(runAllowed && (isRunning || isPaused || summary.frameIndex > 0 || summary.fixedStepIndex > 0));
}

}  // namespace safecrowd::application
