#include "application/ScenarioResultWidget.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include <QColor>
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
#include "application/ScenarioRunWidget.h"
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

QFrame* createMetricCard(const QString& title, const QString& value, QWidget* parent, const QString& tooltip = {}) {
    auto* card = new QFrame(parent);
    card->setStyleSheet(ui::panelStyleSheet());
    if (!tooltip.isEmpty()) {
        card->setToolTip(tooltip);
    }
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);

    auto* titleLabel = createLabel(title, card);
    titleLabel->setStyleSheet(ui::mutedTextStyleSheet());
    auto* valueLabel = createLabel(value, card, ui::FontRole::SectionTitle);
    if (!tooltip.isEmpty()) {
        titleLabel->setToolTip(tooltip);
        valueLabel->setToolTip(tooltip);
    }
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return card;
}

QLabel* createReportSectionHeader(const QString& text, QWidget* parent) {
    auto* label = createLabel(text, parent, ui::FontRole::SectionTitle);
    label->setStyleSheet(ui::mutedTextStyleSheet());
    return label;
}

QPushButton* createBottleneckRowButton(
    const safecrowd::domain::ScenarioBottleneckMetric& bottleneck,
    std::size_t index,
    QWidget* parent) {
    const auto label = QString::fromStdString(bottleneck.label);
    const auto id = QString::fromStdString(bottleneck.connectionId);
    const auto idLine = (!id.isEmpty() && id != label) ? QString("\nID: %1").arg(id) : QString{};
    auto* button = new QPushButton(
        QString("%1. %2%3\n%4 nearby, %5 stalled")
            .arg(static_cast<int>(index + 1))
            .arg(label, idLine)
            .arg(static_cast<int>(bottleneck.nearbyAgentCount))
            .arg(static_cast<int>(bottleneck.stalledAgentCount)),
        parent);
    button->setFont(ui::font(ui::FontRole::Body));
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(ui::ghostRowStyleSheet());
    button->setToolTip(QString("%1\nClick to focus this bottleneck on the canvas.")
        .arg(safecrowd::domain::scenarioBottleneckDefinition()));
    return button;
}

QPushButton* createHotspotRowButton(
    const safecrowd::domain::ScenarioCongestionHotspot& hotspot,
    std::size_t index,
    QWidget* parent) {
    auto* button = new QPushButton(
        QString("%1. (%2, %3)  -  %4 agents")
            .arg(static_cast<int>(index + 1))
            .arg(hotspot.center.x, 0, 'f', 1)
            .arg(hotspot.center.y, 0, 'f', 1)
            .arg(static_cast<int>(hotspot.agentCount)),
        parent);
    button->setFont(ui::font(ui::FontRole::Body));
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(ui::ghostRowStyleSheet());
    button->setToolTip(QString("%1\nClick to focus this hotspot on the canvas.")
        .arg(safecrowd::domain::scenarioHotspotDefinition()));
    return button;
}

QFrame* createLegendSwatch(const QColor& color, QWidget* parent) {
    auto* swatch = new QFrame(parent);
    swatch->setFixedSize(22, 12);
    swatch->setStyleSheet(QString(
        "QFrame {"
        " background: %1;"
        " border: 1px solid rgba(127, 29, 29, 80);"
        " border-radius: 3px;"
        "}").arg(color.name()));
    return swatch;
}

QWidget* createHotspotLegend(QWidget* parent) {
    auto* legend = new QWidget(parent);
    auto* layout = new QHBoxLayout(legend);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto* label = createLabel("Intensity", legend, ui::FontRole::Caption);
    label->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(label);
    layout->addWidget(createLegendSwatch(QColor("#f97316"), legend));
    layout->addWidget(createLegendSwatch(QColor("#dc2626"), legend));
    layout->addWidget(createLegendSwatch(QColor("#b91c1c"), legend));
    layout->addStretch(1);
    legend->setToolTip("Hotspot color intensity is relative to the largest hotspot in this result.");
    return legend;
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
    std::function<void()> runAgainHandler,
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
    metricsGrid->addWidget(createMetricCard(
        "Risk",
        safecrowd::domain::scenarioRiskLevelLabel(risk.completionRisk),
        panel,
        safecrowd::domain::scenarioRiskDefinition()), 1, 0);
    metricsGrid->addWidget(createMetricCard(
        "Stalled",
        QString::number(static_cast<int>(risk.stalledAgentCount)),
        panel,
        safecrowd::domain::scenarioStalledDefinition()), 1, 1);
    layout->addLayout(metricsGrid);
    layout->addStretch(1);

    auto* actions = new QWidget(panel);
    auto* actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(8);
    auto* backButton = new QPushButton("Run Again", actions);
    backButton->setFont(ui::font(ui::FontRole::Body));
    backButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    auto* editButton = new QPushButton("Edit Scenario", actions);
    editButton->setFont(ui::font(ui::FontRole::Body));
    editButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    actionsLayout->addWidget(backButton);
    actionsLayout->addWidget(editButton);
    layout->addWidget(actions);

    QObject::connect(backButton, &QPushButton::clicked, panel, [runAgainHandler = std::move(runAgainHandler)]() {
        if (runAgainHandler) {
            runAgainHandler();
        }
    });

    QObject::connect(editButton, &QPushButton::clicked, panel, [editHandler = std::move(editHandler)]() {
        if (editHandler) {
            editHandler();
        }
    });

    return panel;
}

QWidget* createResultFindingsPanel(
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    std::function<void(std::size_t)> bottleneckFocusHandler,
    std::function<void(std::size_t)> hotspotFocusHandler,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(createLabel("Result Reports", panel, ui::FontRole::Title));
    auto* caption = createLabel("Risk findings and spatial reports", panel, ui::FontRole::Caption);
    caption->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(caption);

    auto* area = new QScrollArea(panel);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    ui::polishScrollArea(area);

    auto* content = new QWidget(area);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 10, 0);
    contentLayout->setSpacing(12);

    auto* bottleneckHeader = createReportSectionHeader("Bottlenecks", content);
    bottleneckHeader->setToolTip(safecrowd::domain::scenarioBottleneckDefinition());
    contentLayout->addWidget(bottleneckHeader);
    if (risk.bottlenecks.empty()) {
        auto* empty = createLabel("None detected", content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        contentLayout->addWidget(empty);
    } else {
        for (std::size_t index = 0; index < risk.bottlenecks.size(); ++index) {
            auto* row = createBottleneckRowButton(risk.bottlenecks[index], index, content);
            QObject::connect(row, &QPushButton::clicked, content, [bottleneckFocusHandler, index]() {
                if (bottleneckFocusHandler) {
                    bottleneckFocusHandler(index);
                }
            });
            contentLayout->addWidget(row);
        }
    }

    auto* hotspotHeader = createReportSectionHeader("Hotspots", content);
    hotspotHeader->setToolTip(safecrowd::domain::scenarioHotspotDefinition());
    contentLayout->addWidget(hotspotHeader);
    contentLayout->addWidget(createHotspotLegend(content));
    if (risk.hotspots.empty()) {
        auto* empty = createLabel("None detected", content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        contentLayout->addWidget(empty);
    } else {
        for (std::size_t index = 0; index < risk.hotspots.size(); ++index) {
            auto* row = createHotspotRowButton(risk.hotspots[index], index, content);
            QObject::connect(row, &QPushButton::clicked, content, [hotspotFocusHandler, index]() {
                if (hotspotFocusHandler) {
                    hotspotFocusHandler(index);
                }
            });
            contentLayout->addWidget(row);
        }
    }

    auto* futureHeader = createReportSectionHeader("Additional Reports", content);
    contentLayout->addWidget(futureHeader);
    auto* futureText = createLabel("Flow, capacity, exposure, and comparison reports can be added here.", content);
    futureText->setStyleSheet(ui::subtleTextStyleSheet());
    contentLayout->addWidget(futureText);

    contentLayout->addStretch(1);
    area->setWidget(content);
    layout->addWidget(area, 1);
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

    shell_ = new WorkspaceShell(WorkspaceShellOptions{
        .showTopBar = true,
        .navigationMode = WorkspaceNavigationMode::PanelOnly,
        .showReviewPanel = true,
        .reviewPanelWidth = 280,
    }, this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(saveProjectHandler_);
    shell_->setOpenProjectHandler(openProjectHandler_);

    auto* canvas = new SimulationCanvasWidget(layout_, shell_);
    canvas->setFrame(frame_);
    canvas->setHotspotOverlay(risk_.hotspots);
    canvas->setBottleneckOverlay(risk_.bottlenecks);
    shell_->setCanvas(canvas);
    shell_->setNavigationPanel(createResultFindingsPanel(
        risk_,
        [canvas](std::size_t index) {
            canvas->focusBottleneck(index);
        },
        [canvas](std::size_t index) {
            canvas->focusHotspot(index);
        },
        shell_));
    shell_->setReviewPanel(createResultPanel(
        scenario_,
        frame_,
        risk_,
        [this]() {
            rerunScenario();
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

void ScenarioResultWidget::rerunScenario() {
    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    auto* runWidget = new ScenarioRunWidget(
        projectName_,
        layout_,
        scenario_,
        saveProjectHandler_,
        openProjectHandler_,
        this);

    rootLayout->replaceWidget(shell_, runWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
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
