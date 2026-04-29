#include "application/ScenarioRunWidget.h"

#include <utility>

#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStringList>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "application/ScenarioResultWidget.h"
#include "application/SimulationCanvasWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"

namespace safecrowd::application {
namespace {

constexpr double kSimulationDeltaSeconds = 1.0 / 30.0;

bool allAgentsEvacuated(const safecrowd::domain::SimulationFrame& frame) {
    return frame.totalAgentCount > 0 && frame.evacuatedAgentCount >= frame.totalAgentCount;
}

QString completionOutcome(const safecrowd::domain::SimulationFrame& frame) {
    if (!frame.complete) {
        return "In progress";
    }
    return allAgentsEvacuated(frame) ? "Evacuation complete" : "Time limit reached";
}

QString runStatusText(const safecrowd::domain::SimulationFrame& frame, bool paused) {
    if (frame.complete) {
        return completionOutcome(frame);
    }
    if (paused && frame.elapsedSeconds <= 0.0 && frame.evacuatedAgentCount == 0) {
        return "Reset to start";
    }
    return paused ? "Paused" : "Running";
}

QString hotspotSummary(const safecrowd::domain::ScenarioRiskSnapshot& risk) {
    if (risk.hotspots.empty()) {
        return "Hotspots: 0";
    }

    const auto& hotspot = risk.hotspots.front();
    return QString("Hotspots: %1\nWorst: %2 agents at (%3, %4)")
        .arg(static_cast<int>(risk.hotspots.size()))
        .arg(static_cast<int>(hotspot.agentCount))
        .arg(hotspot.center.x, 0, 'f', 1)
        .arg(hotspot.center.y, 0, 'f', 1);
}

QString configuredEventSummary(const safecrowd::domain::ScenarioDraft& scenario) {
    if (scenario.control.events.empty()) {
        return "Configured Events: none";
    }

    QStringList names;
    for (const auto& event : scenario.control.events) {
        names << QString::fromStdString(event.name);
    }
    return QString("Configured Events: %1\n%2")
        .arg(static_cast<int>(scenario.control.events.size()))
        .arg(names.join(", "));
}

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
}

QPushButton* createIconButton(QStyle::StandardPixmap icon, const QString& tooltip, QWidget* parent) {
    auto* button = new QPushButton(parent);
    button->setIcon(parent->style()->standardIcon(icon));
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setFixedSize(40, 36);
    button->setStyleSheet(ui::secondaryButtonStyleSheet());
    return button;
}

}  // namespace

ScenarioRunWidget::ScenarioRunWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::ScenarioDraft& scenario,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void(bool)> returnToAuthoringHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(projectName),
      layout_(layout),
      scenario_(scenario),
      runner_(layout_, scenario_),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)),
      returnToAuthoringHandler_(std::move(returnToAuthoringHandler)) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    shell_ = new WorkspaceShell(this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(saveProjectHandler_);
    shell_->setOpenProjectHandler(openProjectHandler_);
    canvas_ = new SimulationCanvasWidget(layout_, shell_);
    canvas_->setFrame(runner_.frame());
    shell_->setCanvas(canvas_);
    shell_->setReviewPanel(createRunPanel());
    shell_->setReviewPanelVisible(true);
    rootLayout->addWidget(shell_);

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

    layout->addWidget(createLabel("Baseline Run", panel, ui::FontRole::Title));
    scenarioLabel_ = createLabel("", panel);
    scenarioLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    statusLabel_ = createLabel("", panel);
    statusLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    elapsedLabel_ = createLabel("", panel);
    elapsedLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    agentCountLabel_ = createLabel("", panel);
    agentCountLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    eventLabel_ = createLabel("", panel);
    eventLabel_->setStyleSheet(ui::mutedTextStyleSheet());
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
    layout->addWidget(eventLabel_);
    layout->addWidget(riskLabel_);
    layout->addWidget(congestionLabel_);
    layout->addWidget(bottleneckLabel_);

    auto* transportLayout = new QHBoxLayout();
    transportLayout->setContentsMargins(0, 0, 0, 0);
    transportLayout->setSpacing(8);
    pauseButton_ = createIconButton(QStyle::SP_MediaPause, "Pause simulation", panel);
    stopButton_ = createIconButton(QStyle::SP_MediaStop, "Reset run to start; no result is created", panel);
    transportLayout->addWidget(pauseButton_);
    transportLayout->addWidget(stopButton_);
    transportLayout->addStretch(1);
    layout->addLayout(transportLayout);

    layout->addStretch(1);

    resultButton_ = new QPushButton("View Results", panel);
    resultButton_->setFont(ui::font(ui::FontRole::Body));
    resultButton_->setStyleSheet(ui::primaryButtonStyleSheet());
    resultButton_->setEnabled(false);
    resultButton_->setToolTip("Available after evacuation completes or the time limit is reached");
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

void ScenarioRunWidget::refreshStatus() {
    const auto& frame = runner_.frame();
    if (scenarioLabel_ != nullptr) {
        scenarioLabel_->setText(QString("Staged baseline: %1").arg(QString::fromStdString(scenario_.name)));
    }
    if (statusLabel_ != nullptr) {
        statusLabel_->setText(QString("Status: %1").arg(runStatusText(frame, paused_)));
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
    if (eventLabel_ != nullptr) {
        eventLabel_->setText(configuredEventSummary(scenario_));
    }
    const auto& risk = runner_.riskSnapshot();
    if (riskLabel_ != nullptr) {
        riskLabel_->setText(QString("Completion Risk: %1\nStalled Agents: %2")
            .arg(safecrowd::domain::scenarioRiskLevelLabel(risk.completionRisk))
            .arg(static_cast<int>(risk.stalledAgentCount)));
    }
    if (congestionLabel_ != nullptr) {
        congestionLabel_->setText(hotspotSummary(risk));
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
        pauseButton_->setIcon(style()->standardIcon(paused_ ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
        pauseButton_->setToolTip(paused_ ? "Resume simulation" : "Pause simulation");
        pauseButton_->setAccessibleName(paused_ ? "Resume simulation" : "Pause simulation");
        pauseButton_->setEnabled(!frame.complete);
    }
    if (stopButton_ != nullptr) {
        stopButton_->setToolTip("Reset run to start; no result is created");
        stopButton_->setAccessibleName("Reset run to start");
        stopButton_->setEnabled(frame.totalAgentCount > 0);
    }
    if (resultButton_ != nullptr) {
        resultButton_->setEnabled(frame.complete);
        resultButton_->setToolTip(frame.complete
            ? QString("Open results: %1").arg(completionOutcome(frame))
            : "Available after evacuation completes or the time limit is reached");
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
    if (!frame.complete) {
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
        returnToAuthoringHandler_,
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
