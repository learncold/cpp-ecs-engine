#include "application/ScenarioRunWidget.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <utility>

#include <QColor>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSizePolicy>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

#include "application/ScenarioAuthoringWidget.h"
#include "application/ScenarioBatchResultWidget.h"
#include "application/ScenarioResultWidget.h"
#include "application/SimulationCanvasWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"

namespace safecrowd::application {
namespace {

constexpr double kSimulationDeltaSeconds = 1.0 / 30.0;
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
    QImage image(QSize(40, 40), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    if (kind == TransportIconKind::Play) {
        QPolygonF triangle;
        triangle << QPointF(16, 12) << QPointF(16, 28) << QPointF(28, 20);
        painter.drawPolygon(triangle);
    } else if (kind == TransportIconKind::Pause) {
        painter.drawRoundedRect(QRectF(13, 11, 5, 18), 1.5, 1.5);
        painter.drawRoundedRect(QRectF(22, 11, 5, 18), 1.5, 1.5);
    } else {
        painter.drawRoundedRect(QRectF(13, 13, 14, 14), 2, 2);
    }

    painter.end();
    return QIcon(QPixmap::fromImage(image));
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
    button->setFixedSize(40, 36);
    button->setStyleSheet(ui::secondaryButtonStyleSheet());
    setTransportIcon(button, icon, QColor("#16202b"));
    return button;
}

QString zoneLabel(const safecrowd::domain::Zone2D& zone) {
    const auto id = QString::fromStdString(zone.id);
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty() ? id : QString("%1  -  %2").arg(label, id);
}

int percentValue(double numerator, double denominator) {
    if (denominator <= 0.0) {
        return 0;
    }
    const auto percent = std::clamp((numerator / denominator) * 100.0, 0.0, 100.0);
    return static_cast<int>(std::round(percent));
}

const safecrowd::domain::Zone2D* firstStartZone(const safecrowd::domain::FacilityLayout2D& layout) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Room || zone.kind == safecrowd::domain::ZoneKind::Unknown;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

const safecrowd::domain::Zone2D* firstDestinationZone(const safecrowd::domain::FacilityLayout2D& layout) {
    const auto exitIt = std::find_if(layout.zones.begin(), layout.zones.end(), [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Exit;
    });
    if (exitIt != layout.zones.end()) {
        return &(*exitIt);
    }
    return layout.zones.empty() ? nullptr : &layout.zones.back();
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
    default:
        return "running";
    }
}

QString fastForwardButtonText(int playbackSpeedMultiplier) {
    switch (playbackSpeedMultiplier) {
    case 2:
        return "Fast Forward x3";
    case 3:
        return "Fast Forward Off";
    default:
        return "Fast Forward x2";
    }
}

ScenarioAuthoringWidget::ScenarioState scenarioStateFromDraft(
    const safecrowd::domain::ScenarioDraft& scenario,
    const safecrowd::domain::FacilityLayout2D& layout) {
    ScenarioAuthoringWidget::ScenarioState state;
    state.draft = scenario;
    state.events = scenario.control.events;
    state.stagedForRun = true;

    if (const auto* startZone = firstStartZone(layout); startZone != nullptr) {
        state.startText = zoneLabel(*startZone);
    }
    if (const auto* destinationZone = firstDestinationZone(layout); destinationZone != nullptr) {
        state.destinationText = zoneLabel(*destinationZone);
    }

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
            refreshStatus();
            if (batchRunner_.complete()) {
                timer_->stop();
            }
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
    riskLabel_ = createLabel("", panel);
    riskLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    riskLabel_->setToolTip(QString("%1\n\n%2")
        .arg(safecrowd::domain::scenarioRiskDefinition(), safecrowd::domain::scenarioStalledDefinition()));
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
    layout->addWidget(riskLabel_);
    layout->addWidget(congestionLabel_);
    layout->addWidget(bottleneckLabel_);

    auto* transportLayout = new QHBoxLayout();
    transportLayout->setContentsMargins(0, 0, 0, 0);
    transportLayout->setSpacing(8);
    pauseButton_ = createIconButton(TransportIconKind::Pause, "Pause simulation", panel);
    stopButton_ = createIconButton(TransportIconKind::Stop, "Stop and reset run", panel);
    transportLayout->addWidget(pauseButton_);
    transportLayout->addWidget(stopButton_);
    transportLayout->addStretch(1);
    layout->addLayout(transportLayout);

    layout->addStretch(1);

    fastForwardButton_ = new QPushButton("Fast Forward to Result", panel);
    fastForwardButton_->setFont(ui::font(ui::FontRole::Body));
    fastForwardButton_->setStyleSheet(ui::secondaryButtonStyleSheet());
    fastForwardButton_->setEnabled(false);
    layout->addWidget(fastForwardButton_);

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
    connect(resultButton_, &QPushButton::clicked, this, [this]() {
        showResults();
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
            initial.scenarios.push_back(scenarioStateFromDraft(scenario, layout_));
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
    if (riskLabel_ != nullptr) {
        riskLabel_->setText(QString("Completion Risk: %1\nStalled Agents: %2")
            .arg(safecrowd::domain::scenarioRiskLevelLabel(risk.completionRisk))
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
    if (fastForwardButton_ != nullptr) {
        fastForwardButton_->setText(fastForwardButtonText(playbackSpeedMultiplier_));
        fastForwardButton_->setEnabled(
            !batchRunner_.complete()
            && !batchRunner_.empty());
    }
    if (resultButton_ != nullptr) {
        resultButton_->setEnabled(
            batchRunner_.complete() && !batchRunner_.empty());
    }
}

void ScenarioRunWidget::selectRun(int index) {
    if (index < 0 || index >= static_cast<int>(batchRunner_.size())) {
        return;
    }
    selectedRunIndex_ = index;
    scenario_ = batchRunner_.run(static_cast<std::size_t>(selectedRunIndex_)).scenario;
    refreshStatus();
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
        playbackSpeedMultiplier_ = 2;
    } else if (playbackSpeedMultiplier_ == 2) {
        playbackSpeedMultiplier_ = 3;
    } else {
        playbackSpeedMultiplier_ = 1;
    }
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

void ScenarioRunWidget::showResults() {
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

    QWidget* resultWidget = nullptr;
    if (results.size() == 1) {
        const auto& result = results.front();
        resultWidget = new ScenarioResultWidget(
            projectName_,
            layout_,
            result.scenario,
            result.frame,
            result.risk,
            result.artifacts,
            [this]() {
                if (saveProjectHandler_) {
                    saveProjectHandler_();
                }
            },
            [this]() {
                if (openProjectHandler_) {
                    openProjectHandler_();
                }
            },
            backToLayoutReviewHandler_,
            SavedResultNavigationView::Bottleneck,
            returnAuthoringState_,
            this);
    } else {
        resultWidget = new ScenarioBatchResultWidget(
            projectName_,
            layout_,
            std::move(results),
            [this]() {
                if (saveProjectHandler_) {
                    saveProjectHandler_();
                }
            },
            [this]() {
                if (openProjectHandler_) {
                    openProjectHandler_();
                }
            },
            backToLayoutReviewHandler_,
            returnAuthoringState_,
            selectedRunIndex_,
            this);
    }
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
