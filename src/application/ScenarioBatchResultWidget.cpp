#include "application/ScenarioBatchResultWidget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "application/ScenarioRunWidget.h"
#include "application/SimulationCanvasWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"
#include "domain/ScenarioAuthoring.h"

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

double finalSeconds(const SavedScenarioResultState& result) {
    return result.artifacts.timingSummary.finalEvacuationTimeSeconds.value_or(result.frame.elapsedSeconds);
}

QString formatDeltaSeconds(double deltaSeconds) {
    if (std::abs(deltaSeconds) < 0.05) {
        return "0.0 sec";
    }
    return QString("%1%2 sec")
        .arg(deltaSeconds > 0.0 ? "+" : "")
        .arg(deltaSeconds, 0, 'f', 1);
}

QString formatPercent(std::size_t numerator, std::size_t denominator) {
    if (denominator == 0) {
        return "0%";
    }
    const auto ratio = std::clamp(static_cast<double>(numerator) / static_cast<double>(denominator), 0.0, 1.0);
    return QString("%1%").arg(ratio * 100.0, 0, 'f', 0);
}

QString scenarioRoleLabel(safecrowd::domain::ScenarioRole role) {
    switch (role) {
    case safecrowd::domain::ScenarioRole::Baseline:
        return "Baseline";
    case safecrowd::domain::ScenarioRole::Alternative:
    default:
        return "Alternative";
    }
}

int riskRank(safecrowd::domain::ScenarioRiskLevel level) noexcept {
    switch (level) {
    case safecrowd::domain::ScenarioRiskLevel::High:
        return 2;
    case safecrowd::domain::ScenarioRiskLevel::Medium:
        return 1;
    case safecrowd::domain::ScenarioRiskLevel::Low:
    default:
        return 0;
    }
}

QColor deltaColor(double deltaSeconds) {
    if (deltaSeconds < -0.05) {
        return QColor("#166534");
    }
    if (deltaSeconds > 0.05) {
        return QColor("#b42318");
    }
    return QColor("#4f5d6b");
}

QTableWidgetItem* readonlyItem(const QString& text, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    item->setTextAlignment(alignment);
    return item;
}

QFrame* createMetricCard(const QString& title, const QString& value, const QString& caption, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setStyleSheet(ui::panelStyleSheet());
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    auto* titleLabel = createLabel(title, card, ui::FontRole::Caption);
    titleLabel->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(titleLabel);

    auto* valueLabel = createLabel(value, card, ui::FontRole::SectionTitle);
    layout->addWidget(valueLabel);

    auto* captionLabel = createLabel(caption, card, ui::FontRole::Caption);
    captionLabel->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(captionLabel);
    return card;
}

std::vector<std::pair<double, double>> progressSeries(const SavedScenarioResultState& result) {
    std::vector<std::pair<double, double>> series;
    if (!result.artifacts.evacuationProgress.empty()) {
        series.reserve(result.artifacts.evacuationProgress.size());
        for (const auto& sample : result.artifacts.evacuationProgress) {
            series.emplace_back(sample.timeSeconds, std::clamp(sample.evacuatedRatio, 0.0, 1.0));
        }
        return series;
    }

    const double ratio = result.frame.totalAgentCount == 0
        ? 0.0
        : std::clamp(
            static_cast<double>(result.frame.evacuatedAgentCount) / static_cast<double>(result.frame.totalAgentCount),
            0.0,
            1.0);
    series.emplace_back(0.0, 0.0);
    series.emplace_back(finalSeconds(result), ratio);
    return series;
}

class EvacuationProgressChartWidget final : public QWidget {
public:
    explicit EvacuationProgressChartWidget(QWidget* parent = nullptr)
        : QWidget(parent) {
        setMinimumHeight(150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setResults(
        const std::vector<SavedScenarioResultState>& results,
        int baselineIndex,
        int selectedIndex) {
        results_ = &results;
        baselineIndex_ = baselineIndex;
        selectedIndex_ = selectedIndex;
        update();
    }

    void setSelectedIndex(int selectedIndex) {
        selectedIndex_ = selectedIndex;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        (void)event;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor("#ffffff"));

        const QRectF bounds = rect().adjusted(12, 12, -12, -22);
        painter.setPen(QPen(QColor("#d7e0ea"), 1));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

        if (results_ == nullptr || results_->empty() || bounds.width() <= 1.0 || bounds.height() <= 1.0) {
            painter.setPen(QColor("#6b7785"));
            painter.setFont(ui::font(ui::FontRole::Caption));
            painter.drawText(rect(), Qt::AlignCenter, "No evacuation progress data");
            return;
        }

        double maxTime = 1.0;
        for (const auto& result : *results_) {
            maxTime = std::max(maxTime, finalSeconds(result));
            for (const auto& sample : result.artifacts.evacuationProgress) {
                maxTime = std::max(maxTime, sample.timeSeconds);
            }
        }

        painter.setPen(QPen(QColor("#e2e8f0"), 1));
        painter.drawLine(bounds.bottomLeft(), bounds.bottomRight());
        painter.drawLine(bounds.bottomLeft(), bounds.topLeft());

        painter.setFont(ui::font(ui::FontRole::Caption));
        painter.setPen(QColor("#6b7785"));
        painter.drawText(QRectF(bounds.left(), bounds.bottom() + 2, bounds.width(), 18), Qt::AlignLeft, "0%");
        painter.drawText(QRectF(bounds.left(), bounds.top() - 3, bounds.width(), 18), Qt::AlignLeft, "100%");
        painter.drawText(QRectF(bounds.left(), bounds.bottom() + 2, bounds.width(), 18), Qt::AlignRight, formatSeconds(maxTime));

        for (int index = 0; index < static_cast<int>(results_->size()); ++index) {
            const auto series = progressSeries((*results_)[static_cast<std::size_t>(index)]);
            if (series.empty()) {
                continue;
            }

            QPainterPath path;
            bool started = false;
            for (const auto& [time, ratio] : series) {
                const auto x = bounds.left() + std::clamp(time / maxTime, 0.0, 1.0) * bounds.width();
                const auto y = bounds.bottom() - std::clamp(ratio, 0.0, 1.0) * bounds.height();
                if (!started) {
                    path.moveTo(x, y);
                    started = true;
                } else {
                    path.lineTo(x, y);
                }
            }

            QColor color("#94a3b8");
            double width = 1.4;
            if (index == baselineIndex_) {
                color = QColor("#16202b");
                width = 2.0;
            }
            if (index == selectedIndex_) {
                color = QColor("#2563eb");
                width = 2.8;
            }

            painter.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }
    }

private:
    const std::vector<SavedScenarioResultState>* results_{nullptr};
    int baselineIndex_{0};
    int selectedIndex_{0};
};

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
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(12);
    panelLayout->addWidget(shell_ != nullptr ? shell_->createPanelHeader("Comparison", panel, false) : createLabel("Comparison", panel, ui::FontRole::Title));

    auto* scrollArea = new QScrollArea(panel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui::polishScrollArea(scrollArea);
    auto* content = new QWidget(scrollArea);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    const auto baselineIndex = baselineResultIndex();
    int bestIndex = results_.empty() ? -1 : 0;
    int highestRiskIndex = results_.empty() ? -1 : 0;
    for (int index = 0; index < static_cast<int>(results_.size()); ++index) {
        const auto& result = results_[static_cast<std::size_t>(index)];
        if (bestIndex < 0 || finalSeconds(result) < finalSeconds(results_[static_cast<std::size_t>(bestIndex)])) {
            bestIndex = index;
        }
        const auto& currentRisk = results_[static_cast<std::size_t>(highestRiskIndex)].risk;
        if (riskRank(result.risk.completionRisk) > riskRank(currentRisk.completionRisk)
            || (riskRank(result.risk.completionRisk) == riskRank(currentRisk.completionRisk)
                && result.risk.bottlenecks.size() > currentRisk.bottlenecks.size())) {
            highestRiskIndex = index;
        }
    }

    auto* cardsHost = new QWidget(content);
    auto* cards = new QGridLayout(cardsHost);
    cards->setContentsMargins(0, 0, 0, 0);
    cards->setHorizontalSpacing(8);
    cards->setVerticalSpacing(8);
    const auto baselineName = baselineIndex >= 0
        ? QString::fromStdString(results_[static_cast<std::size_t>(baselineIndex)].scenario.name)
        : QString("None");
    cards->addWidget(createMetricCard("Runs", QString::number(static_cast<int>(results_.size())), "Staged scenarios completed", cardsHost), 0, 0);
    cards->addWidget(createMetricCard("Baseline", baselineName, "Comparison reference", cardsHost), 0, 1);
    if (bestIndex >= 0) {
        const auto& best = results_[static_cast<std::size_t>(bestIndex)];
        const auto bestDelta = baselineIndex >= 0
            ? finalSeconds(best) - finalSeconds(results_[static_cast<std::size_t>(baselineIndex)])
            : 0.0;
        cards->addWidget(createMetricCard(
            "Fastest",
            formatSeconds(finalSeconds(best)),
            QString("%1  -  %2").arg(formatDeltaSeconds(bestDelta), QString::fromStdString(best.scenario.name)),
            cardsHost), 1, 0);
    }
    if (highestRiskIndex >= 0) {
        const auto& riskiest = results_[static_cast<std::size_t>(highestRiskIndex)];
        cards->addWidget(createMetricCard(
            "Highest Risk",
            safecrowd::domain::scenarioRiskLevelLabel(riskiest.risk.completionRisk),
            QString("%1 hotspots / %2 bottlenecks")
                .arg(static_cast<int>(riskiest.risk.hotspots.size()))
                .arg(static_cast<int>(riskiest.risk.bottlenecks.size())),
            cardsHost), 1, 1);
    }
    layout->addWidget(cardsHost);

    table_ = new QTableWidget(static_cast<int>(results_.size()), 8, content);
    table_->setHorizontalHeaderLabels({"Scenario", "Role", "Final", "Delta", "Evac.", "Risk", "Hotspots", "Bottlenecks"});
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setMinimumHeight(190);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table_->setStyleSheet(
        "QTableWidget { background: #ffffff; border: 1px solid #d7e0ea; border-radius: 8px; gridline-color: #e4ebf3; }"
        "QHeaderView::section { background: #eef3f8; border: 0; padding: 6px; color: #4f5d6b; }"
        "QTableWidget::item { padding: 6px; }");
    for (int row = 0; row < static_cast<int>(results_.size()); ++row) {
        const auto& result = results_[row];
        table_->setItem(row, 0, readonlyItem(QString::fromStdString(result.scenario.name)));
        table_->setItem(row, 1, readonlyItem(scenarioRoleLabel(result.scenario.role)));
        table_->setItem(row, 2, readonlyItem(formatSeconds(finalSeconds(result)), Qt::AlignRight | Qt::AlignVCenter));
        const auto deltaSeconds = baselineIndex >= 0
            ? finalSeconds(result) - finalSeconds(results_[static_cast<std::size_t>(baselineIndex)])
            : 0.0;
        auto* deltaItem = readonlyItem(row == baselineIndex ? "Baseline" : formatDeltaSeconds(deltaSeconds), Qt::AlignRight | Qt::AlignVCenter);
        deltaItem->setForeground(deltaColor(deltaSeconds));
        table_->setItem(row, 3, deltaItem);
        table_->setItem(row, 4, readonlyItem(formatPercent(result.frame.evacuatedAgentCount, result.frame.totalAgentCount), Qt::AlignRight | Qt::AlignVCenter));
        table_->setItem(row, 5, readonlyItem(safecrowd::domain::scenarioRiskLevelLabel(result.risk.completionRisk)));
        table_->setItem(row, 6, readonlyItem(QString::number(static_cast<int>(result.risk.hotspots.size())), Qt::AlignRight | Qt::AlignVCenter));
        table_->setItem(row, 7, readonlyItem(QString::number(static_cast<int>(result.risk.bottlenecks.size())), Qt::AlignRight | Qt::AlignVCenter));
    }
    table_->resizeColumnsToContents();
    layout->addWidget(table_);

    auto* overlayLabel = createLabel("Overlay", content, ui::FontRole::SectionTitle);
    layout->addWidget(overlayLabel);

    auto* overlayRow = new QWidget(content);
    auto* overlayLayout = new QHBoxLayout(overlayRow);
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    overlayLayout->setSpacing(8);
    overlayButtonGroup_ = new QButtonGroup(overlayRow);
    overlayButtonGroup_->setExclusive(true);
    const auto addOverlayButton = [this, overlayLayout, overlayRow](OverlayMode mode, const QString& text) {
        auto* button = new QPushButton(text, overlayRow);
        button->setCheckable(true);
        button->setFont(ui::font(ui::FontRole::Caption));
        button->setStyleSheet(ui::secondaryButtonStyleSheet()
            + "QPushButton:checked { background: #e8f1ff; border-color: #2563eb; color: #1d4ed8; }");
        overlayButtonGroup_->addButton(button, static_cast<int>(mode));
        overlayLayout->addWidget(button);
    };
    addOverlayButton(OverlayMode::Density, "Density");
    addOverlayButton(OverlayMode::Hotspots, "Hotspots");
    addOverlayButton(OverlayMode::Bottlenecks, "Bottlenecks");
    if (auto* button = overlayButtonGroup_->button(static_cast<int>(overlayMode_)); button != nullptr) {
        button->setChecked(true);
    }
    connect(overlayButtonGroup_, &QButtonGroup::idClicked, this, [this](int id) {
        overlayMode_ = static_cast<OverlayMode>(id);
        refreshSelectedResult();
    });
    layout->addWidget(overlayRow);

    layout->addWidget(createLabel("Evacuation Progress", content, ui::FontRole::SectionTitle));
    auto* chart = new EvacuationProgressChartWidget(content);
    chart->setResults(results_, baselineIndex, currentResultIndex_);
    progressChart_ = chart;
    layout->addWidget(chart);

    auto* detailCard = new QFrame(content);
    detailCard->setStyleSheet(ui::panelStyleSheet());
    auto* detailLayout = new QVBoxLayout(detailCard);
    detailLayout->setContentsMargins(12, 10, 12, 10);
    detailLayout->setSpacing(6);
    detailLayout->addWidget(createLabel("Selected Scenario", detailCard, ui::FontRole::SectionTitle));
    detailLabel_ = createLabel("", detailCard);
    detailLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    detailLayout->addWidget(detailLabel_);
    layout->addWidget(detailCard);
    layout->addStretch(1);

    scrollArea->setWidget(content);
    panelLayout->addWidget(scrollArea, 1);

    auto* rerunButton = new QPushButton("Run Again", panel);
    rerunButton->setFont(ui::font(ui::FontRole::Body));
    rerunButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    panelLayout->addWidget(rerunButton);
    auto* editButton = new QPushButton("Edit Scenarios", panel);
    editButton->setFont(ui::font(ui::FontRole::Body));
    editButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    panelLayout->addWidget(editButton);

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
    const auto baselineIndex = baselineResultIndex();
    if (canvas_ != nullptr) {
        canvas_->setConnectionBlocks(result.scenario.control.connectionBlocks);
        canvas_->setFrame(result.frame);
        canvas_->setHotspotOverlay(result.risk.hotspots);
        canvas_->setBottleneckOverlay(result.risk.bottlenecks);
        canvas_->setDensityOverlay(result.artifacts.densitySummary.peakField.cells.empty()
            ? result.artifacts.densitySummary.peakCells
            : result.artifacts.densitySummary.peakField.cells);
        switch (overlayMode_) {
        case OverlayMode::Hotspots:
            canvas_->setResultOverlayMode(ResultOverlayMode::Hotspots);
            break;
        case OverlayMode::Bottlenecks:
            canvas_->setResultOverlayMode(ResultOverlayMode::Bottlenecks);
            break;
        case OverlayMode::Density:
        default:
            canvas_->setResultOverlayMode(ResultOverlayMode::Density);
            break;
        }
    }
    if (progressChart_ != nullptr) {
        static_cast<EvacuationProgressChartWidget*>(progressChart_)->setSelectedIndex(currentResultIndex_);
    }
    if (detailLabel_ != nullptr) {
        const auto selectedFinalSeconds = finalSeconds(result);
        const auto deltaText = baselineIndex >= 0
            ? (currentResultIndex_ == baselineIndex
                ? QString("Baseline")
                : formatDeltaSeconds(selectedFinalSeconds - finalSeconds(results_[static_cast<std::size_t>(baselineIndex)])))
            : QString("No baseline");
        detailLabel_->setText(QString("%1 (%2)\nFinal: %3\nDelta vs baseline: %4\nEvacuated: %5 / %6 (%7)\nRisk: %8\nHotspots: %9\nBottlenecks: %10\nT90 / T95: %11 / %12")
            .arg(QString::fromStdString(result.scenario.name))
            .arg(scenarioRoleLabel(result.scenario.role))
            .arg(formatSeconds(selectedFinalSeconds))
            .arg(deltaText)
            .arg(static_cast<int>(result.frame.evacuatedAgentCount))
            .arg(static_cast<int>(result.frame.totalAgentCount))
            .arg(formatPercent(result.frame.evacuatedAgentCount, result.frame.totalAgentCount))
            .arg(safecrowd::domain::scenarioRiskLevelLabel(result.risk.completionRisk))
            .arg(static_cast<int>(result.risk.hotspots.size()))
            .arg(static_cast<int>(result.risk.bottlenecks.size()))
            .arg(formatSeconds(result.artifacts.timingSummary.t90Seconds))
            .arg(formatSeconds(result.artifacts.timingSummary.t95Seconds)));
    }
}

int ScenarioBatchResultWidget::baselineResultIndex() const noexcept {
    for (int index = 0; index < static_cast<int>(results_.size()); ++index) {
        if (results_[static_cast<std::size_t>(index)].scenario.role == safecrowd::domain::ScenarioRole::Baseline) {
            return index;
        }
    }
    return results_.empty() ? -1 : 0;
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
