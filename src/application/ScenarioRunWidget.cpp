#include "application/ScenarioRunWidget.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <utility>

#include <QColor>
#include <QCoreApplication>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

#include "application/ScenarioAuthoringWidget.h"
#include "application/ScenarioBatchResultWidget.h"
#include "application/SimulationCanvasWidget.h"
#include "application/ToolIconResources.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"

namespace safecrowd::application {
namespace {

constexpr double kSimulationDeltaSeconds = 1.0 / 30.0;
constexpr double kResultCalculationChunkSeconds = 1.0;
constexpr int kPlaybackTimerIntervalMs = 33;

int normalizedRunIndex(int index, std::size_t runCount) {
    if (runCount == 0) {
        return 0;
    }
    return std::clamp(index, 0, static_cast<int>(runCount) - 1);
}

enum class TransportIconKind {
    Play,
    Pause,
    Stop,
};

class BusySpinnerWidget final : public QWidget {
public:
    explicit BusySpinnerWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setFixedSize(44, 44);
        timer_.setInterval(16);
        connect(&timer_, &QTimer::timeout, this, [this]() {
            angleDegrees_ = (angleDegrees_ + 348) % 360;
            update();
        });
        timer_.start();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF bounds(7, 7, width() - 14, height() - 14);
        painter.setPen(QPen(QColor("#d8e2ec"), 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(bounds, 0, 360 * 16);
        painter.setPen(QPen(QColor("#1f5fae"), 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(bounds, angleDegrees_ * 16, -115 * 16);
    }

private:
    QTimer timer_{this};
    int angleDegrees_{0};
};

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
}

QProgressBar* createProgressBar(const QString& tooltip, QWidget* parent) {
    auto* progress = new QProgressBar(parent);
    progress->setRange(0, 100);
    progress->setValue(0);
    progress->setTextVisible(true);
    progress->setFixedHeight(14);
    progress->setToolTip(tooltip);
    progress->setAccessibleName(tooltip);
    progress->setStyleSheet(
        "QProgressBar {"
        " background: #e8eef5;"
        " border: 0;"
        " border-radius: 7px;"
        " color: #4f5d6b;"
        " font-size: 9px;"
        " text-align: center;"
        "}"
        "QProgressBar::chunk {"
        " background: #1f5fae;"
        " border-radius: 7px;"
        "}");
    return progress;
}

QIcon makeTransportIcon(TransportIconKind kind, const QColor& color) {
    if (kind == TransportIconKind::Play) {
        return makeSvgToolIcon(QStringLiteral(":/tool-icons/etc/transport-play.svg"), color, QSize(22, 22));
    }
    if (kind == TransportIconKind::Pause) {
        return makeSvgToolIcon(QStringLiteral(":/tool-icons/etc/transport-pause.svg"), color, QSize(22, 22));
    }
    return makeSvgToolIcon(QStringLiteral(":/tool-icons/etc/transport-stop.svg"), color, QSize(22, 22));
}

QIcon makeFastForwardIcon(bool active, const QColor& color) {
    return makeSvgToolIcon(
        active
            ? QStringLiteral(":/tool-icons/etc/fast-forward-filled.svg")
            : QStringLiteral(":/tool-icons/etc/fast-forward.svg"),
        color,
        QSize(22, 22));
}

QString speedIconResourcePath(int multiplier, bool active) {
    if (multiplier == 2) {
        return active
            ? QStringLiteral(":/tool-icons/etc/speed-2-filled.svg")
            : QStringLiteral(":/tool-icons/etc/speed-2.svg");
    }
    if (multiplier == 3) {
        return active
            ? QStringLiteral(":/tool-icons/etc/speed-3-filled.svg")
            : QStringLiteral(":/tool-icons/etc/speed-3.svg");
    }
    return active
        ? QStringLiteral(":/tool-icons/etc/speed-5-filled.svg")
        : QStringLiteral(":/tool-icons/etc/speed-5.svg");
}

QIcon makeSpeedIcon(int multiplier, bool active, const QColor& color) {
    return makeSvgToolIcon(speedIconResourcePath(multiplier, active), color, QSize(22, 22));
}

QString runIconButtonStyleSheet() {
    return QStringLiteral(
        "QPushButton {"
        " background: transparent;"
        " border: 0;"
        " border-radius: 10px;"
        " outline: none;"
        " padding: 4px;"
        "}"
        "QPushButton:hover {"
        " background: #eef3f8;"
        "}"
        "QPushButton:checked {"
        " background: #e6eef8;"
        "}"
        "QPushButton:focus {"
        " outline: none;"
        "}"
        "QPushButton:disabled {"
        " background: transparent;"
        "}");
}

QString resultButtonStyleSheet(bool complete) {
    const auto background = complete ? QStringLiteral("#1f7a4d") : QStringLiteral("#b42318");
    const auto hover = complete ? QStringLiteral("#16643f") : QStringLiteral("#922018");
    return QString(
        "QPushButton {"
        " background: %1;"
        " border: 1px solid %1;"
        " border-radius: 12px;"
        " color: white;"
        " font-weight: 600;"
        " padding: 10px 18px;"
        "}"
        "QPushButton:hover {"
        " background: %2;"
        " border-color: %2;"
        "}"
        "QPushButton:disabled {"
        " background: #c3cfdb;"
        " border-color: #c3cfdb;"
        " color: #f7f9fb;"
        "}"
    ).arg(background, hover);
}

void setTransportIcon(QPushButton* button, TransportIconKind kind, const QColor& color) {
    if (button == nullptr) {
        return;
    }
    const auto iconKind = static_cast<int>(kind);
    const auto previousKind = button->property("transportIconKind");
    if (previousKind.isValid() && previousKind.toInt() == iconKind) {
        return;
    }
    button->setIcon(makeTransportIcon(kind, color));
    button->setProperty("transportIconKind", iconKind);
}

QPushButton* createIconButton(TransportIconKind icon, const QString& tooltip, QWidget* parent) {
    auto* button = new QPushButton(parent);
    button->setIconSize(QSize(22, 22));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(40, 36);
    button->setStyleSheet(runIconButtonStyleSheet());
    setTransportIcon(button, icon, QColor("#16202b"));
    return button;
}

QPushButton* createToggleIconButton(const QIcon& icon, const QString& tooltip, QWidget* parent) {
    auto* button = new QPushButton(parent);
    button->setCheckable(true);
    button->setIcon(icon);
    button->setIconSize(QSize(22, 22));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(40, 36);
    button->setStyleSheet(runIconButtonStyleSheet());
    return button;
}

int percentValue(double numerator, double denominator) {
    if (denominator <= 0.0) {
        return 0;
    }
    const auto percent = std::clamp((numerator / denominator) * 100.0, 0.0, 100.0);
    return static_cast<int>(std::round(percent));
}

double remainingSimulationSeconds(const safecrowd::domain::ScenarioBatchRunner& batchRunner) {
    double remainingSeconds = 0.0;
    for (const auto& run : batchRunner.runs()) {
        if (!run.complete) {
            remainingSeconds = std::max(
                remainingSeconds,
                std::max(0.0, run.timeLimitSeconds - run.frame.elapsedSeconds));
        }
    }
    return remainingSeconds;
}

QDialog* createResultCalculationDialog(QWidget* parent) {
    auto* dialog = new QDialog(parent);
    dialog->setModal(true);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setWindowTitle("Calculating Results");
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    dialog->setFixedSize(260, 150);
    dialog->setStyleSheet(
        "QDialog {"
        " background: #ffffff;"
        " border-radius: 12px;"
        "}"
        "QLabel {"
        " color: #16202b;"
        "}");

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(14);
    layout->setAlignment(Qt::AlignCenter);

    auto* spinner = new BusySpinnerWidget(dialog);
    layout->addWidget(spinner, 0, Qt::AlignHCenter);

    auto* label = createLabel("Calculating Results...", dialog, ui::FontRole::Body);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    return dialog;
}

QString simulationStatusText(bool complete, bool paused, int playbackSpeedMultiplier) {
    if (complete) {
        return "Complete";
    }
    if (paused) {
        return "Paused";
    }
    switch (playbackSpeedMultiplier) {
    case 2:
        return "x2 fastforward";
    case 3:
        return "x3 fastforward";
    case 5:
        return "x5 fastforward";
    default:
        return "running";
    }
}

ScenarioAuthoringWidget::ScenarioState scenarioStateFromDraft(
    const safecrowd::domain::ScenarioDraft& scenario) {
    ScenarioAuthoringWidget::ScenarioState state;
    state.draft = scenario;
    state.events = scenario.control.events;
    state.stagedForRun = true;

    for (const auto& placement : scenario.population.initialPlacements) {
        ScenarioCrowdPlacement uiPlacement;
        uiPlacement.id = QString::fromStdString(placement.id);
        uiPlacement.name = uiPlacement.id;
        uiPlacement.kind = (placement.targetAgentCount <= 1 && placement.area.outline.size() <= 1)
            ? ScenarioCrowdPlacementKind::Individual
            : ScenarioCrowdPlacementKind::Group;
        uiPlacement.zoneId = QString::fromStdString(placement.zoneId);
        uiPlacement.floorId = QString::fromStdString(placement.floorId);
        uiPlacement.area = placement.area.outline;
        uiPlacement.occupantCount = static_cast<int>(placement.targetAgentCount);
        uiPlacement.velocity = placement.initialVelocity;
        uiPlacement.distribution = placement.distribution;
        uiPlacement.generatedPositions = placement.explicitPositions;
        state.crowdPlacements.push_back(std::move(uiPlacement));
    }
    for (const auto& source : scenario.population.occupantSources) {
        ScenarioCrowdPlacement uiPlacement;
        uiPlacement.id = QString::fromStdString(source.id);
        uiPlacement.name = uiPlacement.id;
        uiPlacement.kind = ScenarioCrowdPlacementKind::Source;
        uiPlacement.zoneId = QString::fromStdString(source.zoneId);
        uiPlacement.floorId = QString::fromStdString(source.floorId);
        uiPlacement.area = {source.position};
        uiPlacement.occupantCount = static_cast<int>(source.targetAgentCount);
        uiPlacement.velocity = source.initialVelocity;
        uiPlacement.sourceAgentsPerSpawn = std::max(1, static_cast<int>(source.agentsPerSpawn));
        uiPlacement.sourceStartSeconds = source.startSeconds;
        uiPlacement.sourceEndSeconds = source.endSeconds;
        uiPlacement.sourceIntervalSeconds = source.spawnIntervalSeconds;
        state.crowdPlacements.push_back(std::move(uiPlacement));
    }

    return state;
}

}  // namespace

ScenarioRunWidget::ScenarioRunWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::ScenarioDraft& scenario,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    QWidget* parent,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState)
    : ScenarioRunWidget(
          projectName,
          layout,
          std::vector<safecrowd::domain::ScenarioDraft>{scenario},
          std::move(saveProjectHandler),
          std::move(openProjectHandler),
          std::move(backToLayoutReviewHandler),
          std::move(returnAuthoringState),
          parent,
          0) {}

ScenarioRunWidget::ScenarioRunWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::ScenarioDraft& scenario,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    safecrowd::domain::SimulationFrame cachedResultFrame,
    safecrowd::domain::ScenarioRiskSnapshot cachedResultRisk,
    safecrowd::domain::ScenarioResultArtifacts cachedResultArtifacts,
    QWidget* parent,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState)
    : ScenarioRunWidget(
          projectName,
          layout,
          std::vector<safecrowd::domain::ScenarioDraft>{scenario},
          std::vector<SavedScenarioResultState>{{
              .scenario = scenario,
              .frame = std::move(cachedResultFrame),
              .risk = std::move(cachedResultRisk),
              .artifacts = std::move(cachedResultArtifacts),
          }},
          std::move(saveProjectHandler),
          std::move(openProjectHandler),
          std::move(backToLayoutReviewHandler),
          std::move(returnAuthoringState),
          parent,
          0) {}

ScenarioRunWidget::ScenarioRunWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<safecrowd::domain::ScenarioDraft> scenarios,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState,
    QWidget* parent,
    int initialSelectedRunIndex)
    : ScenarioRunWidget(
          projectName,
          layout,
          std::move(scenarios),
          {},
          std::move(saveProjectHandler),
          std::move(openProjectHandler),
          std::move(backToLayoutReviewHandler),
          std::move(returnAuthoringState),
          parent,
          initialSelectedRunIndex) {}

ScenarioRunWidget::ScenarioRunWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<safecrowd::domain::ScenarioDraft> scenarios,
    std::vector<SavedScenarioResultState> cachedResults,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState,
    QWidget* parent,
    int initialSelectedRunIndex)
    : QWidget(parent),
      projectName_(projectName),
      layout_(layout),
      scenario_({}),
      scenarios_(std::move(scenarios)),
      cachedResults_(std::move(cachedResults)),
      batchRunner_(layout_, scenarios_),
      returnAuthoringState_(std::move(returnAuthoringState)),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)),
      backToLayoutReviewHandler_(std::move(backToLayoutReviewHandler)) {
    selectedRunIndex_ = normalizedRunIndex(initialSelectedRunIndex, batchRunner_.size());
    if (!batchRunner_.empty()) {
        scenario_ = batchRunner_.run(static_cast<std::size_t>(selectedRunIndex_)).scenario;
    } else if (!scenarios_.empty()) {
        scenario_ = scenarios_.front();
    }

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    shell_ = new WorkspaceShell(WorkspaceShellOptions{
        .showTopBar = true,
        .navigationMode = WorkspaceNavigationMode::RailOnly,
        .showReviewPanel = true,
        .reviewPanelWidth = 280,
    }, this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(saveProjectHandler_);
    shell_->setOpenProjectHandler(openProjectHandler_);
    shell_->setBackHandler([this]() {
        returnToAuthoring();
    });
    shell_->setCanvas(createRunCanvas());
    shell_->setReviewPanel(createRunPanel());
    shell_->setReviewPanelVisible(true);
    rootLayout->addWidget(shell_);

    timer_ = new QTimer(this);
    timer_->setInterval(kPlaybackTimerIntervalMs);
    connect(timer_, &QTimer::timeout, this, [this]() {
        if (!paused_) {
            const auto stepsPerTick = std::max(playbackSpeedMultiplier_, 1);
            for (int step = 0; step < stepsPerTick && !batchRunner_.complete(); ++step) {
                batchRunner_.step(kSimulationDeltaSeconds);
            }
            if (batchRunner_.complete()) {
                playbackSpeedMultiplier_ = 1;
                timer_->stop();
            }
            refreshStatus();
        }
    });
    refreshStatus();
    timer_->start();
}

const safecrowd::domain::ScenarioDraft& ScenarioRunWidget::scenario() const noexcept {
    return scenario_;
}

const std::vector<safecrowd::domain::ScenarioDraft>& ScenarioRunWidget::scenarios() const noexcept {
    return scenarios_;
}

const std::optional<ScenarioAuthoringWidget::InitialState>& ScenarioRunWidget::returnAuthoringState() const noexcept {
    return returnAuthoringState_;
}

bool ScenarioRunWidget::hasResultsForSave() const noexcept {
    return hasCachedResults() || (batchRunner_.complete() && !batchRunner_.empty());
}

int ScenarioRunWidget::selectedRunIndex() const noexcept {
    return selectedRunIndex_;
}

std::vector<SavedScenarioResultState> ScenarioRunWidget::resultsForSave() {
    if (batchRunner_.complete() && !batchRunner_.empty()) {
        return completedResults();
    }
    return cachedResults_;
}

bool ScenarioRunWidget::hasCachedResults() const noexcept {
    return !cachedResults_.empty();
}

std::vector<SavedScenarioResultState> ScenarioRunWidget::completedResults() {
    batchRunner_.syncResultArtifacts();

    std::vector<SavedScenarioResultState> results;
    results.reserve(batchRunner_.size());
    for (const auto& run : batchRunner_.runs()) {
        results.push_back({
            .scenario = run.scenario,
            .frame = run.frame,
            .risk = run.resultRisk,
            .artifacts = run.artifacts,
        });
    }
    return results;
}

std::size_t ScenarioRunWidget::selectedSourceScenarioIndex() const {
    if (scenarios_.empty()) {
        return 0;
    }
    if (!batchRunner_.empty()
        && selectedRunIndex_ >= 0
        && selectedRunIndex_ < static_cast<int>(batchRunner_.size())) {
        return std::min(
            batchRunner_.run(static_cast<std::size_t>(selectedRunIndex_)).sourceScenarioIndex,
            scenarios_.size() - 1);
    }
    return static_cast<std::size_t>(normalizedRunIndex(selectedRunIndex_, scenarios_.size()));
}

QWidget* ScenarioRunWidget::createRunCanvas() {
    auto* container = new QWidget(shell_);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    canvas_ = new SimulationCanvasWidget(layout_, container);
    canvas_->setMinimumHeight(360);
    if (!batchRunner_.empty()) {
        const auto& run = batchRunner_.run(static_cast<std::size_t>(
            normalizedRunIndex(selectedRunIndex_, batchRunner_.size())));
        canvas_->setConnectionBlocks(run.scenario.control.connectionBlocks);
        canvas_->setEnvironmentHazards(run.scenario.environment.hazards);
        canvas_->setRouteGuidances(run.scenario.control.routeGuidances);
        canvas_->setFrame(run.frame);
    }
    layout->addWidget(canvas_, 1);

    auto* cardGrid = new QGridLayout();
    cardGrid->setContentsMargins(0, 0, 0, 0);
    cardGrid->setSpacing(8);
    const auto count = static_cast<int>(batchRunner_.size());
    const auto columns = count <= 1 ? 1 : 2;
    previewButtons_.clear();
    previewStatusLabels_.clear();
    previewProgressBars_.clear();
    previewButtons_.reserve(static_cast<std::size_t>(count));
    previewStatusLabels_.reserve(static_cast<std::size_t>(count));
    previewProgressBars_.reserve(static_cast<std::size_t>(count));

    for (int index = 0; index < count; ++index) {
        const auto& run = batchRunner_.run(static_cast<std::size_t>(index));
        auto* card = new QWidget(container);
        card->setStyleSheet(ui::panelStyleSheet());
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(10, 10, 10, 10);
        cardLayout->setSpacing(8);

        auto* button = new QPushButton(QString::fromStdString(run.scenario.name), card);
        button->setFont(ui::font(ui::FontRole::Body));
        button->setStyleSheet(ui::secondaryButtonStyleSheet());
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        cardLayout->addWidget(button);

        auto* status = createLabel("Pending", card, ui::FontRole::Caption);
        status->setStyleSheet(ui::mutedTextStyleSheet());
        cardLayout->addWidget(status);

        auto* progress = createProgressBar("Scenario progress against its time limit.", card);
        cardLayout->addWidget(progress);

        const int row = index / columns;
        const int column = index % columns;
        cardGrid->addWidget(card, row, column);
        previewButtons_.push_back(button);
        previewStatusLabels_.push_back(status);
        previewProgressBars_.push_back(progress);

        connect(button, &QPushButton::clicked, this, [this, index]() {
            selectRun(index);
        });
    }

    layout->addLayout(cardGrid);
    return container;
}

QWidget* ScenarioRunWidget::createRunPanel() {
    auto* panel = new QWidget(shell_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(shell_ != nullptr ? shell_->createPanelHeader("Run", panel, false) : createLabel("Run", panel, ui::FontRole::Title));

    scenarioLabel_ = createLabel("", panel);
    scenarioLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    statusLabel_ = createLabel("", panel);
    statusLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    elapsedLabel_ = createLabel("", panel);
    elapsedLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    timeProgressBar_ = createProgressBar("Time progress against the scenario time limit.", panel);
    agentCountLabel_ = createLabel("", panel);
    agentCountLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    evacuationProgressBar_ = createProgressBar("Evacuation progress based on evacuated agents divided by total agents.", panel);
    stalledLabel_ = createLabel("", panel);
    stalledLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    stalledLabel_->setToolTip(safecrowd::domain::scenarioStalledDefinition());
    congestionLabel_ = createLabel("", panel);
    congestionLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    congestionLabel_->setToolTip(safecrowd::domain::scenarioHotspotDefinition());
    bottleneckLabel_ = createLabel("", panel);
    bottleneckLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    bottleneckLabel_->setToolTip(safecrowd::domain::scenarioBottleneckDefinition());

    layout->addWidget(scenarioLabel_);
    layout->addWidget(statusLabel_);
    layout->addWidget(elapsedLabel_);
    layout->addWidget(timeProgressBar_);
    layout->addWidget(agentCountLabel_);
    layout->addWidget(evacuationProgressBar_);
    layout->addWidget(stalledLabel_);
    layout->addWidget(congestionLabel_);
    layout->addWidget(bottleneckLabel_);

    auto* settingsTitle = createLabel("Run settings", panel, ui::FontRole::Caption);
    settingsTitle->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(settingsTitle);

    auto* settingsGroup = new QWidget(panel);
    auto* settingsForm = new QFormLayout(settingsGroup);
    settingsForm->setContentsMargins(0, 4, 0, 0);
    settingsForm->setSpacing(6);

    timeLimitSpin_ = new QDoubleSpinBox(settingsGroup);
    timeLimitSpin_->setRange(1.0, 3600.0);
    timeLimitSpin_->setDecimals(0);
    timeLimitSpin_->setSuffix(" s");
    timeLimitSpin_->setToolTip("Simulation time limit");
    settingsForm->addRow("Time limit", timeLimitSpin_);

    sampleIntervalSpin_ = new QDoubleSpinBox(settingsGroup);
    sampleIntervalSpin_->setRange(0.1, 10.0);
    sampleIntervalSpin_->setSingleStep(0.1);
    sampleIntervalSpin_->setDecimals(2);
    sampleIntervalSpin_->setSuffix(" s");
    sampleIntervalSpin_->setToolTip("Result sample interval");
    settingsForm->addRow("Sample interval", sampleIntervalSpin_);

    repeatSpin_ = new QSpinBox(settingsGroup);
    repeatSpin_->setRange(1, static_cast<int>(safecrowd::domain::kScenarioExecutionMaxRepeatCount));
    repeatSpin_->setSuffix(" runs");
    repeatSpin_->setToolTip("Repeat count");
    settingsForm->addRow("Repeats", repeatSpin_);

    seedSpin_ = new QSpinBox(settingsGroup);
    seedSpin_->setRange(1, 1000000);
    seedSpin_->setToolTip("Base random seed");
    settingsForm->addRow("Seed", seedSpin_);

    applySettingsButton_ = new QPushButton("Apply & restart", settingsGroup);
    applySettingsButton_->setFont(ui::font(ui::FontRole::Caption));
    applySettingsButton_->setStyleSheet(ui::secondaryButtonStyleSheet());
    settingsForm->addRow(applySettingsButton_);

    layout->addWidget(settingsGroup);
    syncRunSettingsControls();

    layout->addStretch(1);

    auto* transportLayout = new QHBoxLayout();
    transportLayout->setContentsMargins(0, 0, 0, 0);
    transportLayout->setSpacing(8);
    pauseButton_ = createIconButton(TransportIconKind::Pause, "Pause simulation", panel);
    stopButton_ = createIconButton(TransportIconKind::Stop, "Stop and reset run", panel);
    fastForwardButton_ = createToggleIconButton(makeFastForwardIcon(false, QColor("#16202b")), "Fast forward", panel);
    speed2Button_ = createToggleIconButton(makeSpeedIcon(2, false, QColor("#16202b")), "2x speed", panel);
    speed3Button_ = createToggleIconButton(makeSpeedIcon(3, false, QColor("#16202b")), "3x speed", panel);
    speed5Button_ = createToggleIconButton(makeSpeedIcon(5, false, QColor("#16202b")), "5x speed", panel);
    transportLayout->addWidget(pauseButton_);
    transportLayout->addWidget(stopButton_);
    transportLayout->addWidget(fastForwardButton_);
    transportLayout->addWidget(speed2Button_);
    transportLayout->addWidget(speed3Button_);
    transportLayout->addWidget(speed5Button_);
    transportLayout->addStretch(1);
    layout->addLayout(transportLayout);

    resultButton_ = new QPushButton("View Results", panel);
    resultButton_->setFont(ui::font(ui::FontRole::Body));
    resultButton_->setStyleSheet(ui::primaryButtonStyleSheet());
    resultButton_->setEnabled(false);
    layout->addWidget(resultButton_);

    connect(pauseButton_, &QPushButton::clicked, this, [this]() {
        togglePaused();
    });
    connect(stopButton_, &QPushButton::clicked, this, [this]() {
        stopRun();
    });
    connect(fastForwardButton_, &QPushButton::clicked, this, [this]() {
        cycleFastForwardMode();
    });
    connect(speed2Button_, &QPushButton::clicked, this, [this]() {
        setPlaybackSpeedMultiplier(2);
    });
    connect(speed3Button_, &QPushButton::clicked, this, [this]() {
        setPlaybackSpeedMultiplier(3);
    });
    connect(speed5Button_, &QPushButton::clicked, this, [this]() {
        setPlaybackSpeedMultiplier(5);
    });
    connect(resultButton_, &QPushButton::clicked, this, [this]() {
        showResults();
    });
    connect(applySettingsButton_, &QPushButton::clicked, this, [this]() {
        applyRunSettings();
    });

    return panel;
}

void ScenarioRunWidget::returnToAuthoring() {
    playbackSpeedMultiplier_ = 1;
    if (timer_ != nullptr) {
        timer_->stop();
    }
    const auto sourceScenarioIndex = selectedSourceScenarioIndex();

    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    auto initial = returnAuthoringState_.value_or(ScenarioAuthoringWidget::InitialState{});
    if (initial.scenarios.empty()) {
        for (const auto& scenario : scenarios_) {
            initial.scenarios.push_back(scenarioStateFromDraft(scenario));
        }
        initial.navigationView = ScenarioAuthoringWidget::NavigationView::Layout;
    }
    if (sourceScenarioIndex < scenarios_.size()) {
        const auto& selectedScenarioId = scenarios_[sourceScenarioIndex].scenarioId;
        const auto selectedIt = std::find_if(initial.scenarios.begin(), initial.scenarios.end(), [&](const auto& scenario) {
            return scenario.draft.scenarioId == selectedScenarioId;
        });
        if (selectedIt != initial.scenarios.end()) {
            initial.currentScenarioIndex = static_cast<int>(std::distance(initial.scenarios.begin(), selectedIt));
        } else if (sourceScenarioIndex < initial.scenarios.size()) {
            initial.currentScenarioIndex = static_cast<int>(sourceScenarioIndex);
        }
    }
    initial.rightPanelMode = ScenarioAuthoringWidget::RightPanelMode::Scenario;
    initial.inspectorPanelVisible = true;
    initial.scenarioPanelVisible = true;

    auto* authoringWidget = new ScenarioAuthoringWidget(
        projectName_,
        layout_,
        std::move(initial),
        saveProjectHandler_,
        openProjectHandler_,
        backToLayoutReviewHandler_,
        this);

    rootLayout->replaceWidget(shell_, authoringWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
    canvas_ = nullptr;
}

void ScenarioRunWidget::refreshStatus() {
    if (batchRunner_.empty()) {
        return;
    }
    if (selectedRunIndex_ < 0 || selectedRunIndex_ >= static_cast<int>(batchRunner_.size())) {
        selectedRunIndex_ = 0;
    }
    const auto& selectedRun = batchRunner_.run(static_cast<std::size_t>(selectedRunIndex_));
    const auto& frame = selectedRun.frame;
    if (canvas_ != nullptr) {
        canvas_->setConnectionBlocks(selectedRun.scenario.control.connectionBlocks);
        canvas_->setEnvironmentHazards(selectedRun.scenario.environment.hazards);
        canvas_->setRouteGuidances(selectedRun.scenario.control.routeGuidances);
        canvas_->setFrame(selectedRun.frame);
    }
    for (std::size_t index = 0; index < batchRunner_.size(); ++index) {
        const auto& run = batchRunner_.run(index);
        if (index < previewButtons_.size() && previewButtons_[index] != nullptr) {
            previewButtons_[index]->setText(QString("%1%2")
                .arg(QString::fromStdString(run.scenario.name))
                .arg(run.complete ? "  -  Complete" : "  -  Running"));
            previewButtons_[index]->setStyleSheet(index == static_cast<std::size_t>(selectedRunIndex_)
                ? ui::primaryButtonStyleSheet()
                : ui::secondaryButtonStyleSheet());
        }
        if (index < previewStatusLabels_.size() && previewStatusLabels_[index] != nullptr) {
            const auto repeatText = run.repeatCount > 1
                ? QString("Run %1/%2  -  Seed %3\n")
                    .arg(run.repeatIndex)
                    .arg(run.repeatCount)
                    .arg(run.runSeed)
                : QString{};
            previewStatusLabels_[index]->setText(QString("%1%2  -  %3 / %4 evacuated")
                .arg(repeatText)
                .arg(simulationStatusText(run.complete, paused_, playbackSpeedMultiplier_))
                .arg(static_cast<int>(run.frame.evacuatedAgentCount))
                .arg(static_cast<int>(run.frame.totalAgentCount)));
        }
        if (index < previewProgressBars_.size() && previewProgressBars_[index] != nullptr) {
            previewProgressBars_[index]->setValue(percentValue(run.frame.elapsedSeconds, run.timeLimitSeconds));
        }
    }
    if (scenarioLabel_ != nullptr) {
        const auto completedRuns = static_cast<int>(std::count_if(batchRunner_.runs().begin(), batchRunner_.runs().end(), [](const auto& run) {
            return run.complete;
        }));
        const auto runCount = static_cast<int>(batchRunner_.size());
        scenarioLabel_->setText(QString("Running %1 run%2\nSelected: %3\nBatch: %4 / %5 complete")
            .arg(runCount)
            .arg(runCount == 1 ? "" : "s")
            .arg(QString::fromStdString(selectedRun.scenario.name))
            .arg(completedRuns)
            .arg(runCount));
    }
    if (statusLabel_ != nullptr) {
        statusLabel_->setText(QString("Status: %1")
            .arg(simulationStatusText(frame.complete, paused_, playbackSpeedMultiplier_)));
    }
    if (elapsedLabel_ != nullptr) {
        elapsedLabel_->setText(QString("Elapsed: %1 / %2 sec")
            .arg(frame.elapsedSeconds, 0, 'f', 1)
            .arg(selectedRun.timeLimitSeconds, 0, 'f', 0));
    }
    if (timeProgressBar_ != nullptr) {
        timeProgressBar_->setValue(percentValue(frame.elapsedSeconds, selectedRun.timeLimitSeconds));
    }
    if (agentCountLabel_ != nullptr) {
        agentCountLabel_->setText(QString("Evacuated: %1 / %2\nActive Agents: %3")
            .arg(static_cast<int>(frame.evacuatedAgentCount))
            .arg(static_cast<int>(frame.totalAgentCount))
            .arg(static_cast<int>(frame.agents.size())));
    }
    if (evacuationProgressBar_ != nullptr) {
        evacuationProgressBar_->setValue(percentValue(
            static_cast<double>(frame.evacuatedAgentCount),
            static_cast<double>(frame.totalAgentCount)));
    }
    const auto& risk = selectedRun.risk;
    if (stalledLabel_ != nullptr) {
        stalledLabel_->setText(QString("Stalled Agents: %1")
            .arg(static_cast<int>(risk.stalledAgentCount)));
    }
    if (congestionLabel_ != nullptr) {
        const auto hotspotCount = risk.hotspots.empty() ? 0 : static_cast<int>(risk.hotspots.front().agentCount);
        congestionLabel_->setText(QString("Hotspots: %1%2")
            .arg(static_cast<int>(risk.hotspots.size()))
            .arg(risk.hotspots.empty() ? QString{} : QString(" (max %1 agents)").arg(hotspotCount)));
    }
    if (bottleneckLabel_ != nullptr) {
        if (risk.bottlenecks.empty()) {
            bottleneckLabel_->setText("Bottlenecks: 0");
        } else {
            const auto& bottleneck = risk.bottlenecks.front();
            const auto label = QString::fromStdString(bottleneck.label);
            const auto id = QString::fromStdString(bottleneck.connectionId);
            const auto idLine = (!id.isEmpty() && id != label) ? QString("\nID: %1").arg(id) : QString{};
            bottleneckLabel_->setText(QString("Worst Bottleneck: %1%2\nNearby: %3, Stalled: %4")
                .arg(label, idLine)
                .arg(static_cast<int>(bottleneck.nearbyAgentCount))
                .arg(static_cast<int>(bottleneck.stalledAgentCount)));
        }
    }
    if (pauseButton_ != nullptr) {
        setTransportIcon(
            pauseButton_,
            paused_ ? TransportIconKind::Play : TransportIconKind::Pause,
            QColor("#16202b"));
        pauseButton_->setToolTip(paused_ ? "Resume simulation" : "Pause simulation");
        pauseButton_->setAccessibleName(paused_ ? "Resume simulation" : "Pause simulation");
        pauseButton_->setEnabled(!batchRunner_.complete());
    }
    if (stopButton_ != nullptr) {
        stopButton_->setEnabled(frame.totalAgentCount > 0 || hasCachedResults());
    }
    const bool canFastForward = !batchRunner_.complete() && !batchRunner_.empty();
    const bool fastForwardActive = playbackSpeedMultiplier_ > 1 && canFastForward;
    if (fastForwardButton_ != nullptr) {
        fastForwardButton_->blockSignals(true);
        fastForwardButton_->setChecked(fastForwardActive);
        fastForwardButton_->setIcon(makeFastForwardIcon(fastForwardActive, QColor("#16202b")));
        fastForwardButton_->blockSignals(false);
        fastForwardButton_->setToolTip(fastForwardActive ? "Turn off fast forward" : "Fast forward");
        fastForwardButton_->setAccessibleName(fastForwardButton_->toolTip());
        fastForwardButton_->setEnabled(canFastForward);
    }
    const auto updateSpeedButton = [this, canFastForward, fastForwardActive](QPushButton* button, int multiplier) {
        if (button == nullptr) {
            return;
        }
        const bool selected = fastForwardActive && playbackSpeedMultiplier_ == multiplier;
        button->blockSignals(true);
        button->setChecked(selected);
        button->setIcon(makeSpeedIcon(multiplier, selected, QColor("#16202b")));
        button->blockSignals(false);
        button->setVisible(fastForwardActive);
        button->setEnabled(canFastForward && fastForwardActive);
    };
    updateSpeedButton(speed2Button_, 2);
    updateSpeedButton(speed3Button_, 3);
    updateSpeedButton(speed5Button_, 5);
    if (resultButton_ != nullptr) {
        const bool hasRuns = !batchRunner_.empty();
        const bool complete = batchRunner_.complete();
        resultButton_->setEnabled(hasRuns);
        resultButton_->setText(complete ? "View Results" : "Skip to Results");
        resultButton_->setStyleSheet(resultButtonStyleSheet(complete));
        resultButton_->setToolTip(complete
            ? "Open simulation results."
            : "Finish the simulation without playback and open results.");
        resultButton_->setAccessibleName(resultButton_->toolTip());
    }
}

void ScenarioRunWidget::selectRun(int index) {
    if (index < 0 || index >= static_cast<int>(batchRunner_.size())) {
        return;
    }
    selectedRunIndex_ = index;
    scenario_ = batchRunner_.run(static_cast<std::size_t>(selectedRunIndex_)).scenario;
    syncRunSettingsControls();
    refreshStatus();
}

void ScenarioRunWidget::syncRunSettingsControls() {
    if (scenarios_.empty()) {
        return;
    }
    const auto sourceIndex = selectedSourceScenarioIndex();
    if (sourceIndex >= scenarios_.size()) {
        return;
    }
    const auto& execution = scenarios_[sourceIndex].execution;
    if (timeLimitSpin_ != nullptr) {
        timeLimitSpin_->setValue(execution.timeLimitSeconds > 0.0 ? execution.timeLimitSeconds : 600.0);
    }
    if (sampleIntervalSpin_ != nullptr) {
        sampleIntervalSpin_->setValue(execution.sampleIntervalSeconds > 0.0 ? execution.sampleIntervalSeconds : 0.5);
    }
    if (repeatSpin_ != nullptr) {
        repeatSpin_->setValue(std::clamp<int>(
            static_cast<int>(execution.repeatCount),
            1,
            static_cast<int>(safecrowd::domain::kScenarioExecutionMaxRepeatCount)));
    }
    if (seedSpin_ != nullptr) {
        seedSpin_->setValue(execution.baseSeed == 0
            ? 1
            : static_cast<int>(std::min<unsigned int>(execution.baseSeed, 1000000U)));
    }
}

void ScenarioRunWidget::applyRunSettings() {
    if (scenarios_.empty()
        || timeLimitSpin_ == nullptr
        || sampleIntervalSpin_ == nullptr
        || repeatSpin_ == nullptr
        || seedSpin_ == nullptr) {
        return;
    }

    const auto timeLimitSeconds = timeLimitSpin_->value();
    const auto sampleIntervalSeconds = sampleIntervalSpin_->value();
    const auto repeatCount = static_cast<std::uint32_t>(repeatSpin_->value());
    const auto baseSeed = static_cast<std::uint32_t>(seedSpin_->value());
    for (auto& scenario : scenarios_) {
        scenario.execution.timeLimitSeconds = timeLimitSeconds;
        scenario.execution.sampleIntervalSeconds = sampleIntervalSeconds;
        scenario.execution.repeatCount = repeatCount;
        scenario.execution.baseSeed = baseSeed;
    }

    playbackSpeedMultiplier_ = 1;
    paused_ = false;
    cachedResults_.clear();
    if (timer_ != nullptr) {
        timer_->stop();
    }
    batchRunner_.reset(layout_, scenarios_);
    selectedRunIndex_ = normalizedRunIndex(selectedRunIndex_, batchRunner_.size());
    if (!batchRunner_.empty()) {
        scenario_ = batchRunner_.run(static_cast<std::size_t>(selectedRunIndex_)).scenario;
    }
    if (shell_ != nullptr) {
        shell_->setCanvas(createRunCanvas());
    }
    refreshStatus();
    if (timer_ != nullptr) {
        timer_->start();
    }
}

void ScenarioRunWidget::cycleFastForwardMode() {
    if (batchRunner_.empty()) {
        refreshStatus();
        return;
    }
    if (batchRunner_.complete()) {
        batchRunner_.syncResultArtifacts();
        refreshStatus();
        return;
    }
    if (playbackSpeedMultiplier_ == 1) {
        setPlaybackSpeedMultiplier(2);
    } else {
        setPlaybackSpeedMultiplier(1);
    }
}

void ScenarioRunWidget::setPlaybackSpeedMultiplier(int multiplier) {
    if (batchRunner_.empty() || batchRunner_.complete()) {
        refreshStatus();
        return;
    }
    if (multiplier != 2 && multiplier != 3 && multiplier != 5) {
        multiplier = 1;
    }
    playbackSpeedMultiplier_ = multiplier;
    paused_ = false;
    if (timer_ != nullptr && !timer_->isActive()) {
        timer_->start();
    }
    refreshStatus();
}

void ScenarioRunWidget::stopRun() {
    playbackSpeedMultiplier_ = 1;
    paused_ = true;
    cachedResults_.clear();
    if (timer_ != nullptr) {
        timer_->stop();
    }
    batchRunner_.reset(layout_, scenarios_);
    selectedRunIndex_ = normalizedRunIndex(selectedRunIndex_, batchRunner_.size());
    if (!batchRunner_.empty()) {
        scenario_ = batchRunner_.run(static_cast<std::size_t>(selectedRunIndex_)).scenario;
    }
    refreshStatus();
    if (timer_ != nullptr) {
        timer_->start();
    }
}

void ScenarioRunWidget::completeRunsForResults() {
    if (batchRunner_.empty() || batchRunner_.complete()) {
        return;
    }

    const auto wasPaused = paused_;
    const auto previousPlaybackSpeedMultiplier = playbackSpeedMultiplier_;
    const bool timerWasActive = timer_ != nullptr && timer_->isActive();

    if (timer_ != nullptr) {
        timer_->stop();
    }
    paused_ = true;
    playbackSpeedMultiplier_ = 1;

    if (resultButton_ != nullptr) {
        resultButton_->setEnabled(false);
        resultButton_->setText("Calculating Results...");
        resultButton_->repaint();
    }

    auto* calculationDialog = createResultCalculationDialog(this);
    calculationDialog->adjustSize();
    const auto dialogPosition = mapToGlobal(rect().center())
        - QPoint(calculationDialog->width() / 2, calculationDialog->height() / 2);
    calculationDialog->move(dialogPosition);
    calculationDialog->show();
    calculationDialog->raise();
    calculationDialog->activateWindow();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    while (!batchRunner_.complete()) {
        const auto remainingSeconds = remainingSimulationSeconds(batchRunner_);
        if (remainingSeconds <= 1e-9) {
            break;
        }
        batchRunner_.step(std::min(remainingSeconds, kResultCalculationChunkSeconds));
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    if (!batchRunner_.complete()) {
        batchRunner_.step(kSimulationDeltaSeconds);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    batchRunner_.syncResultArtifacts();

    calculationDialog->close();
    calculationDialog->deleteLater();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    if (!batchRunner_.complete()) {
        paused_ = wasPaused;
        playbackSpeedMultiplier_ = previousPlaybackSpeedMultiplier;
        if (timer_ != nullptr && timerWasActive) {
            timer_->start();
        }
        refreshStatus();
    }
}

void ScenarioRunWidget::showResults() {
    if (batchRunner_.empty()) {
        return;
    }

    completeRunsForResults();

    std::vector<SavedScenarioResultState> results;
    if (batchRunner_.complete() && !batchRunner_.empty()) {
        results = completedResults();
    } else {
        return;
    }

    if (timer_ != nullptr) {
        timer_->stop();
    }

    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    auto* resultWidget = new ScenarioBatchResultWidget(
        projectName_,
        layout_,
        std::move(results),
        saveProjectHandler_,
        openProjectHandler_,
        backToLayoutReviewHandler_,
        returnAuthoringState_,
        selectedRunIndex_,
        this);
    rootLayout->replaceWidget(shell_, resultWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
    canvas_ = nullptr;
}

void ScenarioRunWidget::togglePaused() {
    if (batchRunner_.complete()) {
        return;
    }
    paused_ = !paused_;
    if (!paused_ && timer_ != nullptr && !timer_->isActive()) {
        timer_->start();
    }
    refreshStatus();
}

}  // namespace safecrowd::application
