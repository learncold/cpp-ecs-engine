#include "application/ScenarioRunWidget.h"

#include <algorithm>
#include <utility>

#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "application/ScenarioAuthoringWidget.h"
#include "application/ScenarioResultWidget.h"
#include "application/SimulationCanvasWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"

namespace safecrowd::application {
namespace {

constexpr double kSimulationDeltaSeconds = 1.0 / 30.0;

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

QIcon makeTransportIcon(TransportIconKind kind, const QColor& color) {
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
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

    return QIcon(pixmap);
}

QPushButton* createIconButton(TransportIconKind icon, const QString& tooltip, QWidget* parent) {
    auto* button = new QPushButton(parent);
    button->setIcon(makeTransportIcon(icon, QColor("#16202b")));
    button->setIconSize(QSize(22, 22));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFixedSize(40, 36);
    button->setStyleSheet(ui::secondaryButtonStyleSheet());
    return button;
}

QString zoneLabel(const safecrowd::domain::Zone2D& zone) {
    const auto id = QString::fromStdString(zone.id);
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty() ? id : QString("%1  -  %2").arg(label, id);
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
        uiPlacement.area = placement.area.outline;
        uiPlacement.occupantCount = static_cast<int>(placement.targetAgentCount);
        uiPlacement.velocity = placement.initialVelocity;
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
    QWidget* parent)
    : QWidget(parent),
      projectName_(projectName),
      layout_(layout),
      scenario_(scenario),
      runner_(layout_, scenario_),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    shell_ = new WorkspaceShell(this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(saveProjectHandler_);
    shell_->setOpenProjectHandler(openProjectHandler_);
    shell_->setNavigationVisible(false);
    canvas_ = new SimulationCanvasWidget(layout_, shell_);
    canvas_->setFrame(runner_.frame());
    shell_->setCanvas(canvas_);
    shell_->setReviewPanel(createRunPanel());
    shell_->setReviewPanelVisible(true);
    rootLayout->addWidget(shell_);
    addBackToAuthoringButton();

    timer_ = new QTimer(this);
    timer_->setInterval(33);
    connect(timer_, &QTimer::timeout, this, [this]() {
        if (!paused_) {
            runner_.step(kSimulationDeltaSeconds);
            canvas_->setFrame(runner_.frame());
            refreshStatus();
            if (runner_.complete()) {
                timer_->stop();
            }
        }
    });
    refreshStatus();
    timer_->start();
}

QWidget* ScenarioRunWidget::createRunPanel() {
    auto* panel = new QWidget(shell_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(createLabel("Run", panel, ui::FontRole::Title));
    scenarioLabel_ = createLabel("", panel);
    scenarioLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    statusLabel_ = createLabel("", panel);
    statusLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    elapsedLabel_ = createLabel("", panel);
    elapsedLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    agentCountLabel_ = createLabel("", panel);
    agentCountLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    riskLabel_ = createLabel("", panel);
    riskLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    congestionLabel_ = createLabel("", panel);
    congestionLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    bottleneckLabel_ = createLabel("", panel);
    bottleneckLabel_->setStyleSheet(ui::mutedTextStyleSheet());

    layout->addWidget(scenarioLabel_);
    layout->addWidget(statusLabel_);
    layout->addWidget(elapsedLabel_);
    layout->addWidget(agentCountLabel_);
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
    connect(resultButton_, &QPushButton::clicked, this, [this]() {
        showResults();
    });

    return panel;
}

void ScenarioRunWidget::addBackToAuthoringButton() {
    if (canvas_ == nullptr) {
        return;
    }

    auto* button = new QPushButton("<", canvas_);
    button->setToolTip("Back to scenario editor");
    button->setAccessibleName("Back to scenario editor");
    button->setFixedSize(40, 36);
    button->move(16, 16);
    button->raise();
    button->setStyleSheet(
        "QPushButton {"
        " background: rgba(255, 255, 255, 232);"
        " border: 1px solid #c9d5e2;"
        " border-radius: 10px;"
        " color: #16202b;"
        " font-size: 18px;"
        " font-weight: 700;"
        " padding-bottom: 2px;"
        "}"
        "QPushButton:hover {"
        " background: #eef3f8;"
        " border-color: #b8c6d6;"
        "}");
    connect(button, &QPushButton::clicked, this, [this]() {
        returnToAuthoring();
    });
}

void ScenarioRunWidget::returnToAuthoring() {
    if (timer_ != nullptr) {
        timer_->stop();
    }

    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    ScenarioAuthoringWidget::InitialState initial;
    initial.scenarios.push_back(scenarioStateFromDraft(scenario_, layout_));
    initial.currentScenarioIndex = 0;
    initial.navigationView = ScenarioAuthoringWidget::NavigationView::Layout;
    initial.rightPanelMode = ScenarioAuthoringWidget::RightPanelMode::Scenario;

    auto* authoringWidget = new ScenarioAuthoringWidget(
        projectName_,
        layout_,
        std::move(initial),
        saveProjectHandler_,
        openProjectHandler_,
        this);

    rootLayout->replaceWidget(shell_, authoringWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
    canvas_ = nullptr;
}

void ScenarioRunWidget::refreshStatus() {
    const auto& frame = runner_.frame();
    if (scenarioLabel_ != nullptr) {
        scenarioLabel_->setText(QString("Scenario: %1").arg(QString::fromStdString(scenario_.name)));
    }
    if (statusLabel_ != nullptr) {
        statusLabel_->setText(QString("Status: %1").arg(frame.complete ? "Complete" : paused_ ? "Paused" : "Running"));
    }
    if (elapsedLabel_ != nullptr) {
        elapsedLabel_->setText(QString("Elapsed: %1 / %2 sec")
            .arg(frame.elapsedSeconds, 0, 'f', 1)
            .arg(runner_.timeLimitSeconds(), 0, 'f', 0));
    }
    if (agentCountLabel_ != nullptr) {
        agentCountLabel_->setText(QString("Evacuated: %1 / %2\nActive Agents: %3")
            .arg(static_cast<int>(frame.evacuatedAgentCount))
            .arg(static_cast<int>(frame.totalAgentCount))
            .arg(static_cast<int>(frame.agents.size())));
    }
    const auto& risk = runner_.riskSnapshot();
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
            bottleneckLabel_->setText(QString("Worst Bottleneck: %1\nNearby: %2, Stalled: %3")
                .arg(QString::fromStdString(bottleneck.label))
                .arg(static_cast<int>(bottleneck.nearbyAgentCount))
                .arg(static_cast<int>(bottleneck.stalledAgentCount)));
        }
    }
    if (pauseButton_ != nullptr) {
        pauseButton_->setIcon(makeTransportIcon(
            paused_ ? TransportIconKind::Play : TransportIconKind::Pause,
            QColor("#16202b")));
        pauseButton_->setToolTip(paused_ ? "Resume simulation" : "Pause simulation");
        pauseButton_->setAccessibleName(paused_ ? "Resume simulation" : "Pause simulation");
        pauseButton_->setEnabled(!frame.complete);
    }
    if (stopButton_ != nullptr) {
        stopButton_->setEnabled(frame.totalAgentCount > 0);
    }
    if (resultButton_ != nullptr) {
        const bool allAgentsEvacuated = frame.totalAgentCount > 0
            && frame.evacuatedAgentCount >= frame.totalAgentCount;
        resultButton_->setEnabled(allAgentsEvacuated);
    }
}

void ScenarioRunWidget::stopRun() {
    paused_ = true;
    runner_.reset(layout_, scenario_);
    canvas_->setFrame(runner_.frame());
    refreshStatus();
    timer_->start();
}

void ScenarioRunWidget::showResults() {
    const auto& frame = runner_.frame();
    if (frame.totalAgentCount == 0 || frame.evacuatedAgentCount < frame.totalAgentCount) {
        return;
    }
    if (timer_ != nullptr) {
        timer_->stop();
    }

    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    auto* resultWidget = new ScenarioResultWidget(
        projectName_,
        layout_,
        scenario_,
        runner_.frame(),
        runner_.resultRiskSnapshot(),
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
        this);
    rootLayout->replaceWidget(shell_, resultWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
    canvas_ = nullptr;
}

void ScenarioRunWidget::togglePaused() {
    if (runner_.complete()) {
        return;
    }
    paused_ = !paused_;
    if (!paused_ && timer_ != nullptr && !timer_->isActive()) {
        timer_->start();
    }
    refreshStatus();
}

}  // namespace safecrowd::application
