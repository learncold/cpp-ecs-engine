#include "application/ScenarioResultWidget.h"

#include <utility>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

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

QWidget* createResultPanel(
    const safecrowd::domain::ScenarioDraft& scenario,
    const safecrowd::domain::SimulationFrame& frame,
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(createLabel("Results", panel, ui::FontRole::Title));
    auto* scenarioLabel = createLabel(QString("Scenario: %1").arg(QString::fromStdString(scenario.name)), panel);
    scenarioLabel->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(scenarioLabel);

    auto* metricsGrid = new QGridLayout();
    metricsGrid->setContentsMargins(0, 0, 0, 0);
    metricsGrid->setSpacing(8);
    const auto total = static_cast<int>(frame.totalAgentCount);
    const auto evacuated = static_cast<int>(frame.evacuatedAgentCount);
    metricsGrid->addWidget(createMetricCard("Evacuated", QString("%1 / %2").arg(evacuated).arg(total), panel), 0, 0);
    metricsGrid->addWidget(createMetricCard("Time", QString("%1 sec").arg(frame.elapsedSeconds, 0, 'f', 1), panel), 0, 1);
    metricsGrid->addWidget(createMetricCard("Risk", safecrowd::domain::scenarioRiskLevelLabel(risk.completionRisk), panel), 1, 0);
    metricsGrid->addWidget(createMetricCard("Stalled", QString::number(static_cast<int>(risk.stalledAgentCount)), panel), 1, 1);
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
    backButton->setEnabled(false);
    backButton->setFont(ui::font(ui::FontRole::Body));
    backButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    auto* editButton = new QPushButton("Edit Scenario", actions);
    editButton->setEnabled(false);
    editButton->setFont(ui::font(ui::FontRole::Body));
    editButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    actionsLayout->addWidget(backButton);
    actionsLayout->addWidget(editButton);
    layout->addWidget(actions);

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
    QWidget* parent)
    : QWidget(parent) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* shell = new WorkspaceShell(this);
    shell->setTools({"Project"});
    shell->setSaveProjectHandler(std::move(saveProjectHandler));
    shell->setOpenProjectHandler(std::move(openProjectHandler));

    auto* canvas = new SimulationCanvasWidget(layout, shell);
    canvas->setFrame(frame);
    canvas->setHotspotOverlay(risk.hotspots);
    shell->setCanvas(canvas);
    shell->setReviewPanel(createResultPanel(scenario, frame, risk, shell));
    shell->setReviewPanelVisible(true);

    auto* title = new QLabel(QString("%1  -  Result").arg(projectName), shell);
    title->setFont(ui::font(ui::FontRole::Body));
    title->setStyleSheet(ui::mutedTextStyleSheet());
    shell->setTopBarTrailingWidget(title);

    rootLayout->addWidget(shell);
}

}  // namespace safecrowd::application
