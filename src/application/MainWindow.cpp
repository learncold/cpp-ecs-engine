#include "application/MainWindow.h"

#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "domain/SafeCrowdDomain.h"
#include "engine/EngineState.h"

namespace {

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

QString workspaceStageToString(const safecrowd::domain::SimulationSummary& summary) {
    using safecrowd::engine::EngineState;

    if (summary.state == EngineState::Running) {
        return "BatchRunning (runtime prototype)";
    }

    if (summary.state == EngineState::Paused) {
        return "BatchPaused (runtime prototype)";
    }

    if (summary.frameIndex > 0 || summary.fixedStepIndex > 0) {
        return "ResultsAvailable (aggregation placeholder)";
    }

    return "ScenarioReady (authoring placeholders)";
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

}  // namespace

namespace safecrowd::application {

MainWindow::MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent)
    : QMainWindow(parent),
      domain_(domain) {
    auto* centralWidget = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(centralWidget);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    rootLayout->addWidget(createBodyLabel(
        "<b>SafeCrowd Workspace IA Shell</b><br/>"
        "This window now mirrors the documented <b>Project</b>, <b>Authoring</b>, <b>Run</b>, and "
        "<b>Analysis</b> workspaces. Only playback control is wired live today; repository, template, "
        "and persisted-result flows remain placeholder sections for the next application/domain pass.",
        centralWidget));

    auto* workspaceTabs = new QTabWidget(centralWidget);
    workspaceTabs->setDocumentMode(true);
    rootLayout->addWidget(workspaceTabs, 1);

    auto* projectPage = new QWidget(workspaceTabs);
    auto* projectLayout = new QGridLayout(projectPage);
    projectLayout->setContentsMargins(0, 0, 0, 0);
    projectLayout->setHorizontalSpacing(12);
    projectLayout->setVerticalSpacing(12);
    projectLayout->setColumnStretch(0, 1);
    projectLayout->setColumnStretch(1, 1);
    projectLayout->addWidget(createInfoGroup(
        "Project Navigator",
        "<b>Scope</b><br/>"
        "Create, open, and review a workspace that keeps layout, scenario family, run metadata, and "
        "artifact indexes together.<br/><br/>"
        "<b>Planned actions</b><br/>"
        "Recent projects, import entry, reimport, and project-level navigation live here.",
        projectPage), 0, 0);
    projectLayout->addWidget(createInfoGroup(
        "Project Save/Open",
        "<b>ProjectRepository</b><br/>"
        "Workspace restore will load approved layout, scenario family, run/variation metadata, and the "
        "canonical artifact index through the project repository.<br/><br/>"
        "<b>Current shell</b><br/>"
        "Persistence is not wired yet; this section exists to keep project restore separate from analysis storage.",
        projectPage), 0, 1);
    projectLayout->addWidget(createInfoGroup(
        "Repository Boundaries",
        "<b>ProjectRepository</b> keeps project context, authoring drafts, and artifact indexes.<br/>"
        "<b>ResultRepository</b> remains analysis-only and feeds run summaries, comparison, export, and "
        "recommendation evidence after results have been persisted.",
        projectPage), 1, 0, 1, 2);

    auto* authoringPage = new QWidget(workspaceTabs);
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

    auto* runPage = new QWidget(workspaceTabs);
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
        "Playback control remains the only live path in the current prototype. The documented batch queue, repeat runs, "
        "and variation selection stay visible here as placeholders until domain orchestration is wired.",
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

    auto* analysisPage = new QWidget(workspaceTabs);
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

    workspaceTabs->addTab(projectPage, "Project");
    workspaceTabs->addTab(authoringPage, "Authoring");
    workspaceTabs->addTab(runPage, "Run");
    workspaceTabs->addTab(analysisPage, "Analysis");
    workspaceTabs->setCurrentWidget(runPage);

    rootLayout->addWidget(createBodyLabel(
        "Current prototype note: persistence, template instantiation, and persisted analysis remain design-level placeholders. "
        "Only the playback buttons and runtime counters below are wired to the domain runtime today.",
        centralWidget));

    tickTimer_ = new QTimer(this);
    tickTimer_->setInterval(16);

    connect(startButton_, &QPushButton::clicked, this, [this]() { startSimulation(); });
    connect(pauseButton_, &QPushButton::clicked, this, [this]() { pauseSimulation(); });
    connect(stopButton_, &QPushButton::clicked, this, [this]() { stopSimulation(); });
    connect(tickTimer_, &QTimer::timeout, this, [this]() { tickSimulation(); });

    setCentralWidget(centralWidget);
    setWindowTitle("SafeCrowd Workspace");
    resize(1200, 760);

    refreshRuntimePanel();
}

void MainWindow::startSimulation() {
    domain_.start();
    tickTimer_->start();
    refreshRuntimePanel();
}

void MainWindow::pauseSimulation() {
    domain_.pause();
    tickTimer_->stop();
    refreshRuntimePanel();
}

void MainWindow::stopSimulation() {
    domain_.stop();
    tickTimer_->stop();
    refreshRuntimePanel();
}

void MainWindow::tickSimulation() {
    domain_.update(1.0 / 60.0);
    refreshRuntimePanel();
}

void MainWindow::refreshRuntimePanel() {
    using safecrowd::engine::EngineState;

    const auto summary = domain_.summary();
    workspaceStageValue_->setText(workspaceStageToString(summary));
    runtimeStateValue_->setText(stateToString(summary.state));
    frameValue_->setText(QString::number(summary.frameIndex));
    fixedStepValue_->setText(QString::number(summary.fixedStepIndex));
    alphaValue_->setText(QString::number(summary.alpha, 'f', 2));

    if (summary.state == EngineState::Running || summary.state == EngineState::Paused) {
        runValue_->setText("Prototype run 1 / repeat placeholder");
    } else if (summary.frameIndex > 0 || summary.fixedStepIndex > 0) {
        runValue_->setText("Last prototype run retained");
    } else {
        runValue_->setText("Queue not started");
    }

    variationValue_->setText("Baseline placeholder (domain wiring pending)");

    const bool isRunning = summary.state == EngineState::Running;
    const bool isPaused = summary.state == EngineState::Paused;
    startButton_->setEnabled(!isRunning);
    pauseButton_->setEnabled(isRunning);
    stopButton_->setEnabled(isRunning || isPaused || summary.frameIndex > 0 || summary.fixedStepIndex > 0);
}

}  // namespace safecrowd::application
