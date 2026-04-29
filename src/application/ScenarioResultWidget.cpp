#include "application/ScenarioResultWidget.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
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

QString formatOptionalSeconds(const std::optional<double>& seconds) {
    return seconds.has_value() ? QString("%1 sec").arg(*seconds, 0, 'f', 1) : QString("Pending");
}

class EvacuationProgressWidget final : public QWidget {
public:
    explicit EvacuationProgressWidget(
        safecrowd::domain::ScenarioResultArtifacts artifacts,
        QWidget* parent = nullptr)
        : QWidget(parent),
          artifacts_(std::move(artifacts)) {
        setMinimumHeight(150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip("Cumulative evacuation curve. T90/T95 indicate when 90%/95% of occupants have evacuated.");
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        (void)event;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor("#ffffff"));
        painter.setPen(QPen(QColor("#d8e2ee"), 1));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));

        const QRectF plot = QRectF(rect()).adjusted(34, 18, -14, -28);
        painter.setPen(QPen(QColor("#e4ebf3"), 1));
        painter.drawLine(plot.bottomLeft(), plot.bottomRight());
        painter.drawLine(plot.bottomLeft(), plot.topLeft());
        painter.setFont(ui::font(ui::FontRole::Caption));
        painter.setPen(QColor("#687789"));
        painter.drawText(QRectF(4, plot.top() - 8, 26, 18), Qt::AlignRight | Qt::AlignVCenter, "100%");
        painter.drawText(QRectF(4, plot.bottom() - 10, 26, 18), Qt::AlignRight | Qt::AlignVCenter, "0%");

        if (artifacts_.evacuationProgress.empty()) {
            painter.drawText(plot, Qt::AlignCenter, "No evacuation samples");
            return;
        }

        const auto maxTimeIt = std::max_element(
            artifacts_.evacuationProgress.begin(),
            artifacts_.evacuationProgress.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.timeSeconds < rhs.timeSeconds;
            });
        const auto maxTime = std::max(1.0, maxTimeIt->timeSeconds);

        QPainterPath path;
        for (std::size_t index = 0; index < artifacts_.evacuationProgress.size(); ++index) {
            const auto& sample = artifacts_.evacuationProgress[index];
            const auto x = plot.left() + (std::clamp(sample.timeSeconds / maxTime, 0.0, 1.0) * plot.width());
            const auto y = plot.bottom() - (std::clamp(sample.evacuatedRatio, 0.0, 1.0) * plot.height());
            if (index == 0) {
                path.moveTo(x, y);
            } else {
                path.lineTo(x, y);
            }
        }

        painter.setPen(QPen(QColor("#1f5fae"), 2.5));
        painter.drawPath(path);
        painter.setBrush(QColor("#1f5fae"));
        painter.setPen(Qt::NoPen);
        for (const auto& sample : artifacts_.evacuationProgress) {
            const auto x = plot.left() + (std::clamp(sample.timeSeconds / maxTime, 0.0, 1.0) * plot.width());
            const auto y = plot.bottom() - (std::clamp(sample.evacuatedRatio, 0.0, 1.0) * plot.height());
            painter.drawEllipse(QPointF(x, y), 2.5, 2.5);
        }

        drawTimingMarker(painter, plot, maxTime, artifacts_.timingSummary.t90Seconds, "T90");
        drawTimingMarker(painter, plot, maxTime, artifacts_.timingSummary.t95Seconds, "T95");

        painter.setPen(QColor("#687789"));
        const auto last = artifacts_.evacuationProgress.back();
        painter.drawText(
            QRectF(plot.left(), plot.bottom() + 6, plot.width(), 18),
            Qt::AlignLeft | Qt::AlignVCenter,
            QString("%1 / %2 evacuated by %3 sec")
                .arg(static_cast<int>(last.evacuatedCount))
                .arg(static_cast<int>(last.totalCount))
                .arg(last.timeSeconds, 0, 'f', 1));
    }

private:
    void drawTimingMarker(
        QPainter& painter,
        const QRectF& plot,
        double maxTime,
        const std::optional<double>& seconds,
        const QString& label) const {
        if (!seconds.has_value()) {
            return;
        }
        const auto x = plot.left() + (std::clamp(*seconds / maxTime, 0.0, 1.0) * plot.width());
        painter.setPen(QPen(QColor("#d97706"), 1, Qt::DashLine));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.setPen(QColor("#92400e"));
        painter.drawText(QRectF(x + 4, plot.top(), 46, 18), Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    safecrowd::domain::ScenarioResultArtifacts artifacts_{};
};

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
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    std::function<void()> runAgainHandler,
    std::function<void()> editHandler,
    const WorkspaceShell* shell,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(shell != nullptr ? shell->createPanelHeader("Baseline Result", panel) : createLabel("Baseline Result", panel, ui::FontRole::Title));
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
    metricsGrid->addWidget(createMetricCard(
        "T90",
        formatOptionalSeconds(artifacts.timingSummary.t90Seconds),
        panel,
        "Time at which 90% of occupants completed evacuation."), 4, 0);
    metricsGrid->addWidget(createMetricCard(
        "T95",
        formatOptionalSeconds(artifacts.timingSummary.t95Seconds),
        panel,
        "Time at which 95% of occupants completed evacuation."), 4, 1);
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
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
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

    auto* progressHeader = createReportSectionHeader("Evacuation Progress", content);
    progressHeader->setToolTip("Cumulative evacuation curve and percentile completion times.");
    contentLayout->addWidget(progressHeader);
    contentLayout->addWidget(new EvacuationProgressWidget(artifacts, content));
    auto* timing = createLabel(
        QString("T90: %1\nT95: %2")
            .arg(formatOptionalSeconds(artifacts.timingSummary.t90Seconds))
            .arg(formatOptionalSeconds(artifacts.timingSummary.t95Seconds)),
        content,
        ui::FontRole::Caption);
    timing->setStyleSheet(ui::mutedTextStyleSheet());
    contentLayout->addWidget(timing);

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
    safecrowd::domain::ScenarioResultArtifacts artifacts,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(std::move(projectName)),
      layout_(std::move(layout)),
      scenario_(std::move(scenario)),
      frame_(std::move(frame)),
      risk_(std::move(risk)),
      artifacts_(std::move(artifacts)),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)),
      backToLayoutReviewHandler_(std::move(backToLayoutReviewHandler)) {
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
    shell_->setBackHandler([this]() {
        navigateToAuthoring(true);
    });

    auto* canvas = new SimulationCanvasWidget(layout_, shell_);
    canvas->setFrame(frame_);
    canvas->setHotspotOverlay(risk_.hotspots);
    canvas->setBottleneckOverlay(risk_.bottlenecks);
    shell_->setCanvas(canvas);
    shell_->setNavigationPanel(createResultFindingsPanel(
        artifacts_,
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
        artifacts_,
        [this]() {
            rerunScenario();
        },
        [this]() {
            navigateToAuthoring(false);
        },
        shell_,
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
        backToLayoutReviewHandler_,
        this);

    rootLayout->replaceWidget(shell_, runWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
}

void ScenarioResultWidget::navigateToAuthoring(bool showRunPanel) {
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
        backToLayoutReviewHandler_,
        this);

    rootLayout->replaceWidget(shell_, authoringWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
}

}  // namespace safecrowd::application
