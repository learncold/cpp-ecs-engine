#include "application/ScenarioResultWidget.h"

#include <algorithm>
#include <utility>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>

#include "application/ScenarioCanvasWidget.h"
#include "application/SimulationCanvasWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"

namespace safecrowd::application {
namespace {

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
}

QFrame* createMetricCard(const QString& title, const QString& value, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setStyleSheet(ui::panelStyleSheet());
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);

    auto* titleLabel = createLabel(title, card);
    titleLabel->setStyleSheet(ui::mutedTextStyleSheet());
    auto* valueLabel = createLabel(value, card, ui::FontRole::SectionTitle);
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return card;
}

bool allAgentsEvacuated(const safecrowd::domain::SimulationFrame& frame) {
    return frame.totalAgentCount > 0 && frame.evacuatedAgentCount >= frame.totalAgentCount;
}

QString completionOutcome(const safecrowd::domain::SimulationFrame& frame) {
    if (!frame.complete) {
        return "Stopped before completion";
    }
    return allAgentsEvacuated(frame) ? "Evacuation complete" : "Time limit reached";
}

QString bottleneckSummary(const safecrowd::domain::ScenarioRiskSnapshot& risk) {
    if (risk.bottlenecks.empty()) {
        return "None";
    }
    const auto& bottleneck = risk.bottlenecks.front();
    return QString("%1\n%2 nearby, %3 stalled")
        .arg(QString::fromStdString(bottleneck.label))
        .arg(static_cast<int>(bottleneck.nearbyAgentCount))
        .arg(static_cast<int>(bottleneck.stalledAgentCount));
}

QString configuredEventSummary(const safecrowd::domain::ScenarioDraft& scenario) {
    if (scenario.control.events.empty()) {
        return "None";
    }

    QStringList names;
    for (const auto& event : scenario.control.events) {
        names << QString::fromStdString(event.name);
    }
    return QString("%1 configured\n%2")
        .arg(static_cast<int>(scenario.control.events.size()))
        .arg(names.join(", "));
}

QWidget* createResultPanel(
    const safecrowd::domain::ScenarioDraft& scenario,
    const safecrowd::domain::SimulationFrame& frame,
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    std::function<void()> backHandler,
    std::function<void()> editHandler,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(createLabel("Baseline Result", panel, ui::FontRole::Title));
    auto* scenarioLabel = createLabel(QString("Staged baseline: %1").arg(QString::fromStdString(scenario.name)), panel);
    scenarioLabel->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(scenarioLabel);
    auto* outcomeLabel = createLabel(QString("Outcome: %1").arg(completionOutcome(frame)), panel);
    outcomeLabel->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(outcomeLabel);

    auto* metricsGrid = new QGridLayout();
    metricsGrid->setContentsMargins(0, 0, 0, 0);
    metricsGrid->setSpacing(8);
    const auto total = static_cast<int>(frame.totalAgentCount);
    const auto evacuated = static_cast<int>(frame.evacuatedAgentCount);
    const auto remaining = std::max(0, total - evacuated);
    const auto active = static_cast<int>(frame.agents.size());
    metricsGrid->addWidget(createMetricCard("Total", QString::number(total), panel), 0, 0);
    metricsGrid->addWidget(createMetricCard("Evacuated", QString("%1 / %2").arg(evacuated).arg(total), panel), 0, 1);
    metricsGrid->addWidget(createMetricCard("Remaining", QString("%1 / %2").arg(remaining).arg(total), panel), 1, 0);
    metricsGrid->addWidget(createMetricCard(
        "Elapsed / Time limit",
        QString("%1 / %2 sec")
            .arg(frame.elapsedSeconds, 0, 'f', 1)
            .arg(scenario.execution.timeLimitSeconds, 0, 'f', 0),
        panel), 1, 1);
    metricsGrid->addWidget(createMetricCard("Active", QString::number(active), panel), 2, 0);
    metricsGrid->addWidget(createMetricCard("Configured Events", configuredEventSummary(scenario), panel), 2, 1);
    metricsGrid->addWidget(createMetricCard("Completion Risk", safecrowd::domain::scenarioRiskLevelLabel(risk.completionRisk), panel), 3, 0);
    metricsGrid->addWidget(createMetricCard("Stalled", QString::number(static_cast<int>(risk.stalledAgentCount)), panel), 3, 1);
    layout->addLayout(metricsGrid);

    auto* detailArea = new QScrollArea(panel);
    detailArea->setWidgetResizable(true);
    detailArea->setFrameShape(QFrame::NoFrame);
    ui::polishScrollArea(detailArea);

    auto* details = new QWidget(detailArea);
    auto* detailsLayout = new QVBoxLayout(details);
    detailsLayout->setContentsMargins(0, 0, 10, 0);
    detailsLayout->setSpacing(12);

    auto* bottleneckHeader = createLabel("Worst Bottleneck", details, ui::FontRole::SectionTitle);
    detailsLayout->addWidget(bottleneckHeader);
    auto* bottleneck = createLabel(bottleneckSummary(risk), details);
    bottleneck->setStyleSheet(ui::mutedTextStyleSheet());
    detailsLayout->addWidget(bottleneck);

    auto* hotspotHeader = createLabel("Hotspots", details, ui::FontRole::SectionTitle);
    detailsLayout->addWidget(hotspotHeader);
    if (risk.hotspots.empty()) {
        auto* empty = createLabel("None detected", details);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        detailsLayout->addWidget(empty);
    } else {
        for (const auto& hotspot : risk.hotspots) {
            auto* row = createLabel(
                QString("(%1, %2)  -  %3 agents")
                    .arg(hotspot.center.x, 0, 'f', 1)
                    .arg(hotspot.center.y, 0, 'f', 1)
                    .arg(static_cast<int>(hotspot.agentCount)),
                details);
            row->setStyleSheet(ui::mutedTextStyleSheet());
            detailsLayout->addWidget(row);
        }
    }

    detailsLayout->addStretch(1);
    detailArea->setWidget(details);
    layout->addWidget(detailArea, 1);

    auto* actions = new QWidget(panel);
    auto* actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(8);
    auto* backButton = new QPushButton("Back to Run", actions);
    backButton->setFont(ui::font(ui::FontRole::Body));
    backButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    auto* editButton = new QPushButton("Edit Scenario", actions);
    editButton->setFont(ui::font(ui::FontRole::Body));
    editButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    actionsLayout->addWidget(backButton);
    actionsLayout->addWidget(editButton);
    layout->addWidget(actions);

    QObject::connect(backButton, &QPushButton::clicked, panel, [backHandler = std::move(backHandler)]() {
        if (backHandler) {
            backHandler();
        }
    });

    QObject::connect(editButton, &QPushButton::clicked, panel, [editHandler = std::move(editHandler)]() {
        if (editHandler) {
            editHandler();
        }
    });

    return panel;
}

}  // namespace

ScenarioResultWidget::ScenarioResultWidget(
    QString projectName,
    safecrowd::domain::FacilityLayout2D layout,
    safecrowd::domain::ScenarioDraft scenario,
    safecrowd::domain::SimulationFrame frame,
    safecrowd::domain::ScenarioRiskSnapshot risk,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void(bool)> returnToAuthoringHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(std::move(projectName)),
      layout_(std::move(layout)),
      scenario_(std::move(scenario)),
      frame_(std::move(frame)),
      risk_(std::move(risk)),
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

    auto* canvas = new SimulationCanvasWidget(layout_, shell_);
    canvas->setFrame(frame_);
    canvas->setHotspotOverlay(risk_.hotspots);
    shell_->setCanvas(canvas);
    shell_->setReviewPanel(createResultPanel(
        scenario_,
        frame_,
        risk_,
        [this]() {
            navigateToAuthoring(true);
        },
        [this]() {
            navigateToAuthoring(false);
        },
        shell_));
    shell_->setReviewPanelVisible(true);

    auto* title = new QLabel(QString("%1  -  Result").arg(projectName_), shell_);
    title->setFont(ui::font(ui::FontRole::Body));
    title->setStyleSheet(ui::mutedTextStyleSheet());
    shell_->setTopBarTrailingWidget(title);

    rootLayout->addWidget(shell_);
}

void ScenarioResultWidget::navigateToAuthoring(bool showRunPanel) {
    if (!returnToAuthoringHandler_) {
        return;
    }
    returnToAuthoringHandler_(showRunPanel);
}

}  // namespace safecrowd::application
