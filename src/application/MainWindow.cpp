#include "application/MainWindow.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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

}  // namespace

namespace safecrowd::application {

MainWindow::MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent)
    : QMainWindow(parent),
      domain_(domain) {
    auto* centralWidget = new QWidget(this);
    auto* rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(16);

    auto* workspaceGroup = new QGroupBox("Project Workspace", centralWidget);
    auto* workspaceLayout = new QVBoxLayout(workspaceGroup);
    workspaceLayout->setSpacing(12);
    workspaceLayout->addWidget(createBodyLabel(
        "<b>1. Import &amp; Validate</b><br/>"
        "DXF and facility topology import, review, and manual correction will surface here.",
        workspaceGroup));
    workspaceLayout->addWidget(createBodyLabel(
        "<b>2. Scenario Editor</b><br/>"
        "Baseline and variation authoring stay in the same workspace but outside the run panel.",
        workspaceGroup));
    workspaceLayout->addWidget(createBodyLabel(
        "<b>3. Results &amp; Recommendation</b><br/>"
        "Run summaries, comparison, export, and recommendation remain downstream of persisted artifacts.",
        workspaceGroup));
    workspaceLayout->addStretch();

    auto* workspaceColumn = new QVBoxLayout();
    workspaceColumn->setSpacing(16);
    workspaceColumn->addWidget(createBodyLabel(
        "<b>SafeCrowd Workspace Prototype</b><br/>"
        "This shell now mirrors the documented workflow. Only playback control is wired live today; "
        "the rest of the workspace is reserved so future application features land in explicit sections.",
        centralWidget));

    auto* runControlGroup = new QGroupBox("Run Control Panel", centralWidget);
    auto* runControlLayout = new QVBoxLayout(runControlGroup);
    runControlLayout->setSpacing(10);
    runControlLayout->addWidget(createBodyLabel(
        "Playback control remains the active path into the current runtime prototype.",
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
        "<b>Planned next:</b> execution readiness checks, repeat runs, and variation selection.",
        runControlGroup));

    auto* runtimeStatusGroup = new QGroupBox("Runtime Status", centralWidget);
    auto* runtimeStatusLayout = new QFormLayout(runtimeStatusGroup);
    runtimeStatusLayout->setLabelAlignment(Qt::AlignLeft);
    runtimeStatusLayout->setFormAlignment(Qt::AlignTop | Qt::AlignLeft);

    runtimeStateValue_ = createValueLabel(runtimeStatusGroup);
    frameValue_ = createValueLabel(runtimeStatusGroup);
    fixedStepValue_ = createValueLabel(runtimeStatusGroup);
    alphaValue_ = createValueLabel(runtimeStatusGroup);
    runValue_ = createValueLabel(runtimeStatusGroup);
    variationValue_ = createValueLabel(runtimeStatusGroup);

    runtimeStatusLayout->addRow("Engine state", runtimeStateValue_);
    runtimeStatusLayout->addRow("Rendered frames", frameValue_);
    runtimeStatusLayout->addRow("Fixed steps", fixedStepValue_);
    runtimeStatusLayout->addRow("Interpolation alpha", alphaValue_);
    runtimeStatusLayout->addRow("Current run", runValue_);
    runtimeStatusLayout->addRow("Variation", variationValue_);

    auto* resultsGroup = new QGroupBox("Results Pipeline", centralWidget);
    auto* resultsLayout = new QVBoxLayout(resultsGroup);
    resultsLayout->setSpacing(12);
    resultsLayout->addWidget(createBodyLabel(
        "<b>Run Results Panel</b><br/>"
        "Single-run and variation summaries will read persisted artifacts first.",
        resultsGroup));
    resultsLayout->addWidget(createBodyLabel(
        "<b>Comparison View</b><br/>"
        "Baseline versus alternative comparisons stay separate from live runtime state.",
        resultsGroup));
    resultsLayout->addWidget(createBodyLabel(
        "<b>Export &amp; Recommendation</b><br/>"
        "Artifact export and recommendation evidence remain downstream consumers of saved results.",
        resultsGroup));

    workspaceColumn->addWidget(runControlGroup);
    workspaceColumn->addWidget(runtimeStatusGroup);
    workspaceColumn->addWidget(resultsGroup);
    workspaceColumn->addStretch();

    rootLayout->addWidget(workspaceGroup, 5);
    rootLayout->addLayout(workspaceColumn, 7);

    tickTimer_ = new QTimer(this);
    tickTimer_->setInterval(16);

    connect(startButton_, &QPushButton::clicked, this, [this]() { startSimulation(); });
    connect(pauseButton_, &QPushButton::clicked, this, [this]() { pauseSimulation(); });
    connect(stopButton_, &QPushButton::clicked, this, [this]() { stopSimulation(); });
    connect(tickTimer_, &QTimer::timeout, this, [this]() { tickSimulation(); });

    setCentralWidget(centralWidget);
    setWindowTitle("SafeCrowd Workspace");
    resize(980, 560);

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
    runtimeStateValue_->setText(stateToString(summary.state));
    frameValue_->setText(QString::number(summary.frameIndex));
    fixedStepValue_->setText(QString::number(summary.fixedStepIndex));
    alphaValue_->setText(QString::number(summary.alpha, 'f', 2));

    if (summary.state == EngineState::Running || summary.state == EngineState::Paused) {
        runValue_->setText("Prototype run 1 / 1");
    } else if (summary.frameIndex > 0 || summary.fixedStepIndex > 0) {
        runValue_->setText("Last prototype run retained");
    } else {
        runValue_->setText("Ready for first run");
    }

    variationValue_->setText("Baseline placeholder (domain wiring pending)");

    const bool isRunning = summary.state == EngineState::Running;
    const bool isPaused = summary.state == EngineState::Paused;
    startButton_->setEnabled(!isRunning);
    pauseButton_->setEnabled(isRunning);
    stopButton_->setEnabled(isRunning || isPaused || summary.frameIndex > 0 || summary.fixedStepIndex > 0);
}

}  // namespace safecrowd::application
