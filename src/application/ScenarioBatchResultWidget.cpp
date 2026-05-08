#include "application/ScenarioBatchResultWidget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include <QHeaderView>
#include <QLabel>
#include <QAbstractItemView>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

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

QString formatSeconds(const std::optional<double>& seconds) {
    return seconds.has_value() ? QString("%1 sec").arg(*seconds, 0, 'f', 1) : QString("Pending");
}

QString formatSeconds(double seconds) {
    return QString("%1 sec").arg(seconds, 0, 'f', 1);
}

QString formatPercent(std::size_t numerator, std::size_t denominator) {
    if (denominator == 0) {
        return "0%";
    }
    const auto ratio = std::clamp(static_cast<double>(numerator) / static_cast<double>(denominator), 0.0, 1.0);
    return QString("%1%").arg(ratio * 100.0, 0, 'f', 0);
}

QTableWidgetItem* readonlyItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

std::vector<safecrowd::domain::ScenarioDraft> scenariosFromResults(
    const std::vector<SavedScenarioResultState>& results) {
    std::vector<safecrowd::domain::ScenarioDraft> scenarios;
    scenarios.reserve(results.size());
    for (const auto& result : results) {
        scenarios.push_back(result.scenario);
    }
    return scenarios;
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
        uiPlacement.floorId = QString::fromStdString(placement.floorId);
        uiPlacement.area = placement.area.outline;
        uiPlacement.occupantCount = static_cast<int>(placement.targetAgentCount);
        uiPlacement.velocity = placement.initialVelocity;
        uiPlacement.distribution = placement.distribution;
        uiPlacement.generatedPositions = placement.explicitPositions;
        state.crowdPlacements.push_back(std::move(uiPlacement));
    }

    return state;
}

}  // namespace

ScenarioBatchResultWidget::ScenarioBatchResultWidget(
    QString projectName,
    safecrowd::domain::FacilityLayout2D layout,
    std::vector<SavedScenarioResultState> results,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState,
    int currentResultIndex,
    QWidget* parent)
    : QWidget(parent),
      projectName_(std::move(projectName)),
      layout_(std::move(layout)),
      results_(std::move(results)),
      returnAuthoringState_(std::move(returnAuthoringState)),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)),
      backToLayoutReviewHandler_(std::move(backToLayoutReviewHandler)),
      currentResultIndex_(currentResultIndex) {
    if (currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
        currentResultIndex_ = 0;
    }

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    shell_ = new WorkspaceShell(WorkspaceShellOptions{
        .showTopBar = true,
        .navigationMode = WorkspaceNavigationMode::PanelOnly,
        .showReviewPanel = true,
        .reviewPanelWidth = 360,
    }, this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(saveProjectHandler_);
    shell_->setOpenProjectHandler(openProjectHandler_);
    shell_->setBackHandler([this]() {
        navigateToAuthoring();
    });

    canvas_ = new SimulationCanvasWidget(layout_, shell_);
    shell_->setCanvas(canvas_);
    shell_->setReviewPanel(createSummaryPanel());
    shell_->setReviewPanelVisible(true);
    rootLayout->addWidget(shell_);
    refreshSelectedResult();
}

const std::vector<SavedScenarioResultState>& ScenarioBatchResultWidget::results() const noexcept {
    return results_;
}

int ScenarioBatchResultWidget::currentResultIndex() const noexcept {
    return currentResultIndex_;
}

std::optional<ScenarioAuthoringWidget::InitialState> ScenarioBatchResultWidget::returnAuthoringState() const {
    return returnAuthoringState_;
}

QWidget* ScenarioBatchResultWidget::createSummaryPanel() {
    auto* panel = new QWidget(shell_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(shell_ != nullptr ? shell_->createPanelHeader("Comparison", panel, false) : createLabel("Comparison", panel, ui::FontRole::Title));

    table_ = new QTableWidget(static_cast<int>(results_.size()), 7, panel);
    table_->setHorizontalHeaderLabels({"Scenario", "Final", "Evac.", "Risk", "Bottlenecks", "T90", "T95"});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setStyleSheet(
        "QTableWidget { background: #ffffff; border: 1px solid #d7e0ea; border-radius: 8px; gridline-color: #e4ebf3; }"
        "QHeaderView::section { background: #eef3f8; border: 0; padding: 6px; color: #4f5d6b; }"
        "QTableWidget::item { padding: 6px; }");
    for (int row = 0; row < static_cast<int>(results_.size()); ++row) {
        const auto& result = results_[row];
        table_->setItem(row, 0, readonlyItem(QString::fromStdString(result.scenario.name)));
        table_->setItem(row, 1, readonlyItem(formatSeconds(
            result.artifacts.timingSummary.finalEvacuationTimeSeconds.value_or(result.frame.elapsedSeconds))));
        table_->setItem(row, 2, readonlyItem(formatPercent(result.frame.evacuatedAgentCount, result.frame.totalAgentCount)));
        table_->setItem(row, 3, readonlyItem(safecrowd::domain::scenarioRiskLevelLabel(result.risk.completionRisk)));
        table_->setItem(row, 4, readonlyItem(QString::number(static_cast<int>(result.risk.bottlenecks.size()))));
        table_->setItem(row, 5, readonlyItem(formatSeconds(result.artifacts.timingSummary.t90Seconds)));
        table_->setItem(row, 6, readonlyItem(formatSeconds(result.artifacts.timingSummary.t95Seconds)));
    }
    table_->resizeColumnsToContents();
    layout->addWidget(table_, 1);

    detailLabel_ = createLabel("", panel);
    detailLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(detailLabel_);
    layout->addStretch(1);

    auto* rerunButton = new QPushButton("Run Again", panel);
    rerunButton->setFont(ui::font(ui::FontRole::Body));
    rerunButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    layout->addWidget(rerunButton);
    auto* editButton = new QPushButton("Edit Scenarios", panel);
    editButton->setFont(ui::font(ui::FontRole::Body));
    editButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    layout->addWidget(editButton);

    connect(table_, &QTableWidget::currentCellChanged, this, [this](int currentRow, int, int, int) {
        if (currentRow >= 0 && currentRow < static_cast<int>(results_.size())) {
            currentResultIndex_ = currentRow;
            refreshSelectedResult();
        }
    });
    connect(rerunButton, &QPushButton::clicked, this, [this]() {
        rerunBatch();
    });
    connect(editButton, &QPushButton::clicked, this, [this]() {
        navigateToAuthoring();
    });
    if (!results_.empty()) {
        table_->selectRow(currentResultIndex_);
    }
    return panel;
}

void ScenarioBatchResultWidget::navigateToAuthoring() {
    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    auto initial = returnAuthoringState_.value_or(ScenarioAuthoringWidget::InitialState{});
    if (initial.scenarios.empty()) {
        for (const auto& result : results_) {
            initial.scenarios.push_back(scenarioStateFromDraft(result.scenario, layout_));
        }
        initial.currentScenarioIndex = currentResultIndex_;
    }
    initial.rightPanelMode = ScenarioAuthoringWidget::RightPanelMode::Scenario;

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

void ScenarioBatchResultWidget::refreshSelectedResult() {
    if (results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
        return;
    }
    const auto& result = results_[currentResultIndex_];
    if (canvas_ != nullptr) {
        canvas_->setFrame(result.frame);
        canvas_->setHotspotOverlay(result.risk.hotspots);
        canvas_->setBottleneckOverlay(result.risk.bottlenecks);
        canvas_->setDensityOverlay(result.artifacts.densitySummary.peakField.cells.empty()
            ? result.artifacts.densitySummary.peakCells
            : result.artifacts.densitySummary.peakField.cells);
        canvas_->setResultOverlayMode(ResultOverlayMode::Density);
    }
    if (detailLabel_ != nullptr) {
        const auto finalSeconds = result.artifacts.timingSummary.finalEvacuationTimeSeconds.value_or(result.frame.elapsedSeconds);
        detailLabel_->setText(QString("Selected: %1\nFinal: %2\nEvacuated: %3 / %4\nPeak Risk: %5\nBottlenecks: %6")
            .arg(QString::fromStdString(result.scenario.name))
            .arg(formatSeconds(finalSeconds))
            .arg(static_cast<int>(result.frame.evacuatedAgentCount))
            .arg(static_cast<int>(result.frame.totalAgentCount))
            .arg(safecrowd::domain::scenarioRiskLevelLabel(result.risk.completionRisk))
            .arg(static_cast<int>(result.risk.bottlenecks.size())));
    }
}

void ScenarioBatchResultWidget::rerunBatch() {
    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }
    auto* runWidget = new ScenarioRunWidget(
        projectName_,
        layout_,
        scenariosFromResults(results_),
        results_,
        saveProjectHandler_,
        openProjectHandler_,
        backToLayoutReviewHandler_,
        returnAuthoringState_,
        this);
    rootLayout->replaceWidget(shell_, runWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
    canvas_ = nullptr;
}

}  // namespace safecrowd::application
