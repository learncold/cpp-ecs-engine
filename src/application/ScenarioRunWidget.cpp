#include "application/ScenarioRunWidget.h"

#include <utility>

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "application/SimulationCanvasWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"

namespace safecrowd::application {
namespace {

constexpr double kSimulationDeltaSeconds = 1.0 / 30.0;

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
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
      runner_(layout_, scenario_) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    shell_ = new WorkspaceShell(this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(std::move(saveProjectHandler));
    shell_->setOpenProjectHandler(std::move(openProjectHandler));
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

    layout->addWidget(createLabel("Run", panel, ui::FontRole::Title));
    scenarioLabel_ = createLabel("", panel);
    scenarioLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    statusLabel_ = createLabel("", panel);
    statusLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    elapsedLabel_ = createLabel("", panel);
    elapsedLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    agentCountLabel_ = createLabel("", panel);
    agentCountLabel_->setStyleSheet(ui::mutedTextStyleSheet());

    layout->addWidget(scenarioLabel_);
    layout->addWidget(statusLabel_);
    layout->addWidget(elapsedLabel_);
    layout->addWidget(agentCountLabel_);
    layout->addStretch(1);

    pauseButton_ = new QPushButton("Pause", panel);
    pauseButton_->setFont(ui::font(ui::FontRole::Body));
    pauseButton_->setStyleSheet(ui::secondaryButtonStyleSheet());
    resetButton_ = new QPushButton("Reset Run", panel);
    resetButton_->setFont(ui::font(ui::FontRole::Body));
    resetButton_->setStyleSheet(ui::primaryButtonStyleSheet());
    layout->addWidget(pauseButton_);
    layout->addWidget(resetButton_);

    connect(pauseButton_, &QPushButton::clicked, this, [this]() {
        togglePaused();
    });
    connect(resetButton_, &QPushButton::clicked, this, [this]() {
        resetRun();
    });

    return panel;
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
        agentCountLabel_->setText(QString("Evacuated: %1 / %2\nActive agents: %3")
            .arg(static_cast<int>(frame.evacuatedAgentCount))
            .arg(static_cast<int>(frame.totalAgentCount))
            .arg(static_cast<int>(frame.agents.size())));
    }
    if (pauseButton_ != nullptr) {
        pauseButton_->setText(paused_ ? "Resume" : "Pause");
        pauseButton_->setEnabled(!frame.complete);
    }
}

void ScenarioRunWidget::resetRun() {
    paused_ = false;
    runner_.reset(layout_, scenario_);
    canvas_->setFrame(runner_.frame());
    refreshStatus();
    timer_->start();
}

void ScenarioRunWidget::togglePaused() {
    if (runner_.complete()) {
        return;
    }
    paused_ = !paused_;
    refreshStatus();
}

}  // namespace safecrowd::application
