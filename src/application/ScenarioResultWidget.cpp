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
#include <QVBoxLayout>

#include "application/ScenarioAuthoringWidget.h"
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
    QWidget* parent)
    : QWidget(parent),
      projectName_(std::move(projectName)),
      layout_(std::move(layout)),
      scenario_(std::move(scenario)),
      frame_(std::move(frame)),
      risk_(std::move(risk)),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)) {
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
    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    ScenarioAuthoringWidget::InitialState initial;
    initial.scenarios.push_back(scenarioStateFromDraft(scenario_, layout_));
    initial.currentScenarioIndex = 0;
    initial.navigationView = ScenarioAuthoringWidget::NavigationView::Layout;
    initial.rightPanelMode = showRunPanel
        ? ScenarioAuthoringWidget::RightPanelMode::Run
        : ScenarioAuthoringWidget::RightPanelMode::Scenario;

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
}

}  // namespace safecrowd::application
