#include "application/ScenarioResultWidget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSlider>
#include <QSignalBlocker>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
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

QString formatSecondsValue(double seconds) {
    return QString("%1 sec").arg(seconds, 0, 'f', 1);
}

QString formatOptionalMargin(const std::optional<double>& seconds) {
    if (!seconds.has_value()) {
        return "No target";
    }
    return QString("%1%2 sec").arg(*seconds >= 0.0 ? "+" : "").arg(*seconds, 0, 'f', 1);
}

QString formatDensity(double density) {
    return QString("%1 / m2").arg(density, 0, 'f', 1);
}

QString formatPercent(double ratio) {
    return QString("%1%").arg(std::clamp(ratio, 0.0, 1.0) * 100.0, 0, 'f', 0);
}

QString formatFinalStatus(const safecrowd::domain::SimulationFrame& frame) {
    return frame.complete ? QString("Complete") : QString("Incomplete");
}

QString formatEvacuatedCount(const safecrowd::domain::SimulationFrame& frame) {
    return QString("%1 / %2")
        .arg(static_cast<int>(frame.evacuatedAgentCount))
        .arg(static_cast<int>(frame.totalAgentCount));
}

double resultCompletionTime(
    const safecrowd::domain::SimulationFrame& frame,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts) {
    return artifacts.timingSummary.finalEvacuationTimeSeconds.value_or(frame.elapsedSeconds);
}

class EvacuationProgressWidget final : public QWidget {
public:
    explicit EvacuationProgressWidget(
        safecrowd::domain::ScenarioResultArtifacts artifacts,
        QWidget* parent = nullptr)
        : QWidget(parent),
          artifacts_(std::move(artifacts)) {
        setMinimumHeight(150);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setToolTip("Remaining occupant curve. T90/T95 indicate when 90%/95% of occupants have evacuated.");
    }

    void setCurrentTimeSeconds(std::optional<double> seconds) {
        currentTimeSeconds_ = seconds;
        update();
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
            const auto remainingRatio = 1.0 - std::clamp(sample.evacuatedRatio, 0.0, 1.0);
            const auto x = plot.left() + (std::clamp(sample.timeSeconds / maxTime, 0.0, 1.0) * plot.width());
            const auto y = plot.bottom() - (remainingRatio * plot.height());
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
            const auto remainingRatio = 1.0 - std::clamp(sample.evacuatedRatio, 0.0, 1.0);
            const auto x = plot.left() + (std::clamp(sample.timeSeconds / maxTime, 0.0, 1.0) * plot.width());
            const auto y = plot.bottom() - (remainingRatio * plot.height());
            painter.drawEllipse(QPointF(x, y), 2.5, 2.5);
        }

        drawTimingMarker(painter, plot, maxTime, artifacts_.timingSummary.t90Seconds, "T90");
        drawTimingMarker(painter, plot, maxTime, artifacts_.timingSummary.t95Seconds, "T95");
        drawCurrentTimeMarker(painter, plot, maxTime);

        painter.setPen(QColor("#687789"));
        const auto last = artifacts_.evacuationProgress.back();
        const auto remainingCount = last.totalCount >= last.evacuatedCount
            ? last.totalCount - last.evacuatedCount
            : std::size_t{0};
        painter.drawText(
            QRectF(plot.left(), plot.bottom() + 6, plot.width(), 18),
            Qt::AlignLeft | Qt::AlignVCenter,
            QString("%1 / %2 remaining by %3 sec")
                .arg(static_cast<int>(remainingCount))
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

    void drawCurrentTimeMarker(
        QPainter& painter,
        const QRectF& plot,
        double maxTime) const {
        if (!currentTimeSeconds_.has_value()) {
            return;
        }

        const auto x = plot.left() + (std::clamp(*currentTimeSeconds_ / maxTime, 0.0, 1.0) * plot.width());
        painter.setPen(QPen(QColor("#111827"), 1.6));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.setBrush(QColor("#111827"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(x, plot.top()), 3.5, 3.5);
        painter.setPen(QColor("#111827"));
        painter.drawText(QRectF(x + 5, plot.top() + 2, 76, 18), Qt::AlignLeft | Qt::AlignVCenter, QString("%1 sec").arg(*currentTimeSeconds_, 0, 'f', 1));
    }

    safecrowd::domain::ScenarioResultArtifacts artifacts_{};
    std::optional<double> currentTimeSeconds_{};
};

class DensityLegendWidget final : public QWidget {
public:
    explicit DensityLegendWidget(
        const safecrowd::domain::DensitySummary& summary,
        QWidget* parent = nullptr)
        : QWidget(parent),
          threshold_(summary.highDensityThresholdPeoplePerSquareMeter),
          peakDensity_(summary.peakDensityPeoplePerSquareMeter) {
        setFixedSize(230, 34);
        setToolTip("Peak density heatmap scale in people per square meter.");
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        (void)event;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF ramp(0, 4, width(), 9);
        QLinearGradient gradient(ramp.left(), ramp.center().y(), ramp.right(), ramp.center().y());
        gradient.setColorAt(0.0, QColor("#1d4ed8"));
        gradient.setColorAt(0.22, QColor("#06b6d4"));
        gradient.setColorAt(0.45, QColor("#22c55e"));
        gradient.setColorAt(0.65, QColor("#facc15"));
        gradient.setColorAt(0.82, QColor("#f97316"));
        gradient.setColorAt(1.0, QColor("#dc2626"));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawRoundedRect(ramp, 4, 4);

        painter.setFont(ui::font(ui::FontRole::Caption));
        painter.setPen(QColor("#687789"));
        if (peakDensity_ > 0.0) {
            const auto thresholdX = ramp.left()
                + (std::clamp(threshold_ / peakDensity_, 0.0, 1.0) * ramp.width());
            painter.setPen(QPen(QColor("#405063"), 1));
            painter.drawLine(QPointF(thresholdX, ramp.top() - 2), QPointF(thresholdX, ramp.bottom() + 2));
            painter.setPen(QColor("#687789"));
        }
        painter.drawText(QRectF(0, 16, 40, 16), Qt::AlignLeft | Qt::AlignVCenter, "0");
        painter.drawText(
            QRectF(48, 16, 96, 16),
            Qt::AlignCenter,
            QString("%1 /m2").arg(threshold_, 0, 'f', 1));
        painter.drawText(
            QRectF(width() - 86, 16, 86, 16),
            Qt::AlignRight | Qt::AlignVCenter,
            QString("Peak %1").arg(peakDensity_, 0, 'f', 1));
    }

private:
    double threshold_{0.0};
    double peakDensity_{0.0};
};

QFrame* createMetricCard(const QString& title, const QString& value, QWidget* parent, const QString& tooltip = {}) {
    auto* card = new QFrame(parent);
    card->setStyleSheet(ui::panelStyleSheet());
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    if (!tooltip.isEmpty()) {
        card->setToolTip(tooltip);
    }
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    auto* titleLabel = createLabel(title, card, ui::FontRole::Caption);
    titleLabel->setStyleSheet(ui::mutedTextStyleSheet());
    auto* valueLabel = createLabel(value, card, ui::FontRole::SectionTitle);
    valueLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
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

QPushButton* createReportRowButton(const QStringList& lines, QWidget* parent) {
    auto* button = new QPushButton(parent);
    button->setFont(ui::font(ui::FontRole::Body));
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    button->setMinimumWidth(0);
    button->setStyleSheet(ui::ghostRowStyleSheet());

    auto* layout = new QVBoxLayout(button);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(4);
    for (const auto& line : lines) {
        auto* label = createLabel(line, button, ui::FontRole::Body);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setStyleSheet("QLabel { background: transparent; border: 0; padding: 0; }");
        layout->addWidget(label);
    }
    return button;
}

QPushButton* createBottleneckRowButton(
    const safecrowd::domain::ScenarioBottleneckMetric& bottleneck,
    std::size_t index,
    QWidget* parent) {
    const auto label = QString::fromStdString(bottleneck.label);
    const auto id = QString::fromStdString(bottleneck.connectionId);
    QStringList lines{
        QString("%1. %2").arg(static_cast<int>(index + 1)).arg(label),
    };
    if (!id.isEmpty() && id != label) {
        lines.push_back(QString("ID: %1").arg(id));
    }
    lines.push_back(QString("%1 nearby, %2 stalled")
        .arg(static_cast<int>(bottleneck.nearbyAgentCount))
        .arg(static_cast<int>(bottleneck.stalledAgentCount)));
    if (bottleneck.detectedAtSeconds.has_value()) {
        lines.push_back(QString("Detected: %1 sec").arg(*bottleneck.detectedAtSeconds, 0, 'f', 1));
    }
    if (!bottleneck.floorId.empty()) {
        lines.push_back(QString("Floor: %1").arg(QString::fromStdString(bottleneck.floorId)));
    }
    auto* button = createReportRowButton(lines, parent);
    button->setToolTip(QString("%1\nClick to focus this bottleneck on the canvas.")
        .arg(safecrowd::domain::scenarioBottleneckDefinition()));
    return button;
}

QPushButton* createHotspotRowButton(
    const safecrowd::domain::ScenarioCongestionHotspot& hotspot,
    std::size_t index,
    QWidget* parent) {
    QStringList lines{
        QString("%1. (%2, %3) - %4 agents")
            .arg(static_cast<int>(index + 1))
            .arg(hotspot.center.x, 0, 'f', 1)
            .arg(hotspot.center.y, 0, 'f', 1)
            .arg(static_cast<int>(hotspot.agentCount)),
    };
    if (hotspot.detectedAtSeconds.has_value()) {
        lines.push_back(QString("Detected: %1 sec").arg(*hotspot.detectedAtSeconds, 0, 'f', 1));
    }
    if (!hotspot.floorId.empty()) {
        lines.push_back(QString("Floor: %1").arg(QString::fromStdString(hotspot.floorId)));
    }
    auto* button = createReportRowButton(lines, parent);
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

QTableWidget* createResultTable(const QStringList& headers, int rows, QWidget* parent) {
    auto* table = new QTableWidget(rows, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setWordWrap(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(true);
    table->setStyleSheet(
        "QTableWidget { background: #ffffff; border: 1px solid #d7e0ea; gridline-color: #e4ebf3; }"
        "QHeaderView::section { background: #eef3f8; border: 0; border-bottom: 1px solid #d7e0ea; padding: 6px; color: #4f5d6b; }"
        "QTableWidget::item { padding: 6px; }");
    return table;
}

QTableWidgetItem* tableItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QWidget* createExitUsageTable(const safecrowd::domain::ScenarioResultArtifacts& artifacts, QWidget* parent) {
    auto* container = new QWidget(parent);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    if (artifacts.exitUsage.empty()) {
        auto* empty = createLabel("No exit usage data", container);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        layout->addWidget(empty);
        layout->addStretch(1);
        return container;
    }

    auto* table = createResultTable({"Exit", "People", "Share", "Last"}, static_cast<int>(artifacts.exitUsage.size()), container);
    for (int row = 0; row < static_cast<int>(artifacts.exitUsage.size()); ++row) {
        const auto& exit = artifacts.exitUsage[static_cast<std::size_t>(row)];
        table->setItem(row, 0, tableItem(QString::fromStdString(exit.exitLabel)));
        table->setItem(row, 1, tableItem(QString::number(static_cast<int>(exit.evacuatedCount))));
        table->setItem(row, 2, tableItem(formatPercent(exit.usageRatio)));
        table->setItem(row, 3, tableItem(formatOptionalSeconds(exit.lastExitTimeSeconds)));
    }
    table->resizeRowsToContents();
    layout->addWidget(table);
    return container;
}

QWidget* createDetailTabs(const safecrowd::domain::ScenarioResultArtifacts& artifacts, QWidget* parent) {
    auto* tabs = new QTabWidget(parent);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 0; }"
        "QTabBar::tab { background: #ffffff; border: 1px solid #c9d5e2; padding: 5px 8px; margin-right: 3px; }"
        "QTabBar::tab:selected { background: #e6eef8; color: #1f5fae; }");

    auto* zones = createResultTable({"Zone", "People", "Out", "Last"}, static_cast<int>(artifacts.zoneCompletion.size()), tabs);
    for (int row = 0; row < static_cast<int>(artifacts.zoneCompletion.size()); ++row) {
        const auto& zone = artifacts.zoneCompletion[static_cast<std::size_t>(row)];
        zones->setItem(row, 0, tableItem(QString::fromStdString(zone.zoneLabel)));
        zones->setItem(row, 1, tableItem(QString::number(static_cast<int>(zone.initialCount))));
        zones->setItem(row, 2, tableItem(QString::number(static_cast<int>(zone.evacuatedCount))));
        zones->setItem(row, 3, tableItem(formatOptionalSeconds(zone.lastCompletionTimeSeconds)));
    }
    tabs->addTab(zones, "Zones");

    auto* groups = createResultTable({"Group", "People", "Out", "Last"}, static_cast<int>(artifacts.placementCompletion.size()), tabs);
    for (int row = 0; row < static_cast<int>(artifacts.placementCompletion.size()); ++row) {
        const auto& group = artifacts.placementCompletion[static_cast<std::size_t>(row)];
        groups->setItem(row, 0, tableItem(QString::fromStdString(group.placementId)));
        groups->setItem(row, 1, tableItem(QString::number(static_cast<int>(group.initialCount))));
        groups->setItem(row, 2, tableItem(QString::number(static_cast<int>(group.evacuatedCount))));
        groups->setItem(row, 3, tableItem(formatOptionalSeconds(group.lastCompletionTimeSeconds)));
    }
    tabs->addTab(groups, "Groups");

    auto* criteria = new QWidget(tabs);
    auto* criteriaLayout = new QVBoxLayout(criteria);
    criteriaLayout->setContentsMargins(0, 8, 0, 0);
    criteriaLayout->setSpacing(6);
    auto* density = createLabel(
        QString("High density: %1 / m2 or higher for accumulated duration.")
            .arg(artifacts.densitySummary.highDensityThresholdPeoplePerSquareMeter, 0, 'f', 1),
        criteria);
    auto* stalled = createLabel(safecrowd::domain::scenarioStalledDefinition(), criteria);
    auto* bottleneck = createLabel(safecrowd::domain::scenarioBottleneckDefinition(), criteria);
    density->setStyleSheet(ui::mutedTextStyleSheet());
    stalled->setStyleSheet(ui::mutedTextStyleSheet());
    bottleneck->setStyleSheet(ui::mutedTextStyleSheet());
    criteriaLayout->addWidget(density);
    criteriaLayout->addWidget(stalled);
    criteriaLayout->addWidget(bottleneck);
    criteriaLayout->addStretch(1);
    tabs->addTab(criteria, "Criteria");

    return tabs;
}

QStringList resultInterpretations(
    const safecrowd::domain::SimulationFrame& frame,
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts) {
    QStringList lines;
    if (artifacts.timingSummary.marginSeconds.has_value()) {
        if (*artifacts.timingSummary.marginSeconds < 0.0) {
            lines.push_back(QString("Evacuation exceeds the target by %1 sec; review the worst bottleneck first.")
                .arg(std::abs(*artifacts.timingSummary.marginSeconds), 0, 'f', 1));
        } else {
            lines.push_back(QString("Evacuation remains within the target with %1 sec of margin.")
                .arg(*artifacts.timingSummary.marginSeconds, 0, 'f', 1));
        }
    }
    if (artifacts.densitySummary.peakDensityPeoplePerSquareMeter
        >= artifacts.densitySummary.highDensityThresholdPeoplePerSquareMeter) {
        lines.push_back(QString("Peak density reaches %1; apply staff guidance or entry control near the highlighted area.")
            .arg(formatDensity(artifacts.densitySummary.peakDensityPeoplePerSquareMeter)));
    }
    if (!risk.bottlenecks.empty()) {
        lines.push_back(QString("The strongest bottleneck is %1; opening capacity or redirecting people here is the first action.")
            .arg(QString::fromStdString(risk.bottlenecks.front().label)));
    }
    if (!artifacts.exitUsage.empty() && artifacts.exitUsage.front().usageRatio >= 0.65) {
        lines.push_back(QString("%1 handles %2 of evacuees; use signs or staff to distribute exits.")
            .arg(QString::fromStdString(artifacts.exitUsage.front().exitLabel), formatPercent(artifacts.exitUsage.front().usageRatio)));
    }
    if (lines.empty()) {
        lines.push_back(QString("No dominant risk is detected in this result; keep the current plan as the baseline."));
    }
    while (lines.size() > 3) {
        lines.removeLast();
    }
    (void)frame;
    return lines;
}

class ResultReplayControls final : public QWidget {
public:
    ResultReplayControls(
        std::vector<safecrowd::domain::SimulationFrame> frames,
        SimulationCanvasWidget* canvas,
        EvacuationProgressWidget* progressWidget,
        QWidget* parent = nullptr)
        : QWidget(parent),
          frames_(std::move(frames)),
          canvas_(canvas),
          progressWidget_(progressWidget) {
        setStyleSheet(
            "QWidget { background: #ffffff; }"
            "QPushButton { border: 1px solid #c9d5e2; border-radius: 8px; padding: 6px 12px; background: #ffffff; color: #16202b; font-weight: 600; }"
            "QPushButton:hover { background: #eef3f8; }"
            "QPushButton:disabled { color: #94a3b8; }"
            "QSlider::groove:horizontal { height: 6px; background: #d7e0ea; border-radius: 3px; }"
            "QSlider::handle:horizontal { width: 16px; margin: -5px 0; border-radius: 8px; background: #1f5fae; }"
            "QLabel { background: transparent; }");

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 8, 16, 8);
        layout->setSpacing(10);

        playButton_ = new QPushButton("Play", this);
        playButton_->setCursor(Qt::PointingHandCursor);
        layout->addWidget(playButton_);

        slider_ = new QSlider(Qt::Horizontal, this);
        slider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        layout->addWidget(slider_, 1);

        timeLabel_ = createLabel("", this, ui::FontRole::Caption);
        timeLabel_->setMinimumWidth(110);
        timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        timeLabel_->setStyleSheet(ui::mutedTextStyleSheet());
        layout->addWidget(timeLabel_);

        timer_ = new QTimer(this);
        timer_->setInterval(500);

        const bool enabled = frames_.size() > 1 && canvas_ != nullptr;
        playButton_->setEnabled(enabled);
        slider_->setEnabled(enabled);
        slider_->setRange(0, frames_.empty() ? 0 : static_cast<int>(frames_.size() - 1));
        currentIndex_ = frames_.empty() ? 0 : static_cast<int>(frames_.size() - 1);
        applyFrame(currentIndex_);

        QObject::connect(playButton_, &QPushButton::clicked, this, [this]() {
            togglePlayback();
        });
        QObject::connect(slider_, &QSlider::valueChanged, this, [this](int value) {
            currentIndex_ = value;
            applyFrame(currentIndex_);
            if (currentIndex_ >= static_cast<int>(frames_.size()) - 1) {
                pause();
            }
        });
        QObject::connect(timer_, &QTimer::timeout, this, [this]() {
            if (frames_.empty()) {
                pause();
                return;
            }
            if (currentIndex_ >= static_cast<int>(frames_.size()) - 1) {
                pause();
                return;
            }
            ++currentIndex_;
            applyFrame(currentIndex_);
        });
    }

    void showFrame(const safecrowd::domain::SimulationFrame& frame) {
        pause();
        currentIndex_ = nearestFrameIndex(frame.elapsedSeconds);
        applyFrameData(frame, currentIndex_);
    }

    void showClosestFrameAtSeconds(double seconds) {
        pause();
        currentIndex_ = nearestFrameIndex(seconds);
        applyFrame(currentIndex_);
    }

private:
    int nearestFrameIndex(double seconds) const {
        if (frames_.empty()) {
            return 0;
        }
        const auto closest = std::min_element(frames_.begin(), frames_.end(), [seconds](const auto& lhs, const auto& rhs) {
            return std::abs(lhs.elapsedSeconds - seconds) < std::abs(rhs.elapsedSeconds - seconds);
        });
        return static_cast<int>(std::distance(frames_.begin(), closest));
    }

    void togglePlayback() {
        if (timer_ == nullptr || frames_.size() <= 1) {
            return;
        }
        if (timer_->isActive()) {
            pause();
            return;
        }
        if (currentIndex_ >= static_cast<int>(frames_.size()) - 1) {
            currentIndex_ = 0;
            applyFrame(currentIndex_);
        }
        playButton_->setText("Pause");
        timer_->start();
    }

    void pause() {
        if (timer_ != nullptr) {
            timer_->stop();
        }
        if (playButton_ != nullptr) {
            playButton_->setText("Play");
        }
    }

    void applyFrame(int index) {
        if (frames_.empty()) {
            timeLabel_->setText("No replay");
            return;
        }
        index = std::clamp(index, 0, static_cast<int>(frames_.size() - 1));
        applyFrameData(frames_[static_cast<std::size_t>(index)], index);
    }

    void applyFrameData(const safecrowd::domain::SimulationFrame& frame, int sliderIndex) {
        if (canvas_ != nullptr) {
            canvas_->setFrame(frame);
        }
        if (progressWidget_ != nullptr) {
            progressWidget_->setCurrentTimeSeconds(frame.elapsedSeconds);
        }
        if (slider_ != nullptr && slider_->value() != sliderIndex) {
            const QSignalBlocker blocker(slider_);
            slider_->setValue(sliderIndex);
        }
        const auto totalSeconds = frames_.empty() ? frame.elapsedSeconds : frames_.back().elapsedSeconds;
        timeLabel_->setText(QString("%1 / %2 sec")
            .arg(frame.elapsedSeconds, 0, 'f', 1)
            .arg(totalSeconds, 0, 'f', 1));
    }

    std::vector<safecrowd::domain::SimulationFrame> frames_{};
    SimulationCanvasWidget* canvas_{nullptr};
    EvacuationProgressWidget* progressWidget_{nullptr};
    QPushButton* playButton_{nullptr};
    QSlider* slider_{nullptr};
    QLabel* timeLabel_{nullptr};
    QTimer* timer_{nullptr};
    int currentIndex_{0};
};

std::vector<safecrowd::domain::SimulationFrame> replayFramesForResult(
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    const safecrowd::domain::SimulationFrame& fallbackFrame) {
    if (!artifacts.replayFrames.empty()) {
        return artifacts.replayFrames;
    }
    return {fallbackFrame};
}

QWidget* createResultGraphPanel(
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    EvacuationProgressWidget** progressWidgetOut,
    QWidget* parent) {
    auto* panel = new QFrame(parent);
    panel->setStyleSheet(
        "QFrame { background: #ffffff; border-top: 1px solid #d7e0ea; }"
        "QLabel { border: 0; }"
        "QPushButton { border: 1px solid #c9d5e2; border-radius: 6px; padding: 4px 10px; background: #ffffff; color: #344256; }"
        "QPushButton:hover { background: #eef3f8; }");
    panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    panel->setMinimumHeight(220);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 10, 16, 16);
    layout->setSpacing(10);

    auto* header = new QWidget(panel);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    auto* title = createLabel("Evacuation Progress", header, ui::FontRole::SectionTitle);
    title->setToolTip("Cumulative evacuation curve and percentile completion times.");
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);
    auto* toggleButton = new QPushButton(header);
    toggleButton->setText("Hide graph");
    toggleButton->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(toggleButton);
    layout->addWidget(header);

    auto* tabs = new QTabWidget(panel);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 0; }"
        "QTabBar::tab { background: #ffffff; border: 1px solid #c9d5e2; padding: 6px 12px; margin-right: 4px; }"
        "QTabBar::tab:selected { background: #e6eef8; color: #1f5fae; }");

    auto* remainingTab = new QWidget(tabs);
    auto* remainingLayout = new QVBoxLayout(remainingTab);
    remainingLayout->setContentsMargins(0, 8, 0, 0);
    remainingLayout->setSpacing(8);
    auto* progressWidget = new EvacuationProgressWidget(artifacts, remainingTab);
    if (progressWidgetOut != nullptr) {
        *progressWidgetOut = progressWidget;
    }
    remainingLayout->addWidget(progressWidget, 1);
    auto* timing = createLabel(
        QString("T90: %1    T95: %2")
            .arg(formatOptionalSeconds(artifacts.timingSummary.t90Seconds))
            .arg(formatOptionalSeconds(artifacts.timingSummary.t95Seconds)),
        remainingTab,
        ui::FontRole::Caption);
    timing->setStyleSheet(ui::mutedTextStyleSheet());
    remainingLayout->addWidget(timing);
    tabs->addTab(remainingTab, "Remaining");
    tabs->addTab(createExitUsageTable(artifacts, tabs), "Exits");
    layout->addWidget(tabs, 1);

    QObject::connect(toggleButton, &QPushButton::clicked, panel, [panel, tabs, toggleButton]() {
        const bool nextVisible = !tabs->isVisible();
        tabs->setVisible(nextVisible);
        toggleButton->setText(nextVisible ? "Hide graph" : "Show graph");
        panel->setMaximumHeight(nextVisible ? QWIDGETSIZE_MAX : 48);
        panel->setMinimumHeight(nextVisible ? 220 : 48);
    });

    return panel;
}

QWidget* createResultCanvasPanel(
    SimulationCanvasWidget* canvas,
    const safecrowd::domain::SimulationFrame& frame,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    ResultReplayControls** replayControlsOut,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* overlayBar = new QFrame(panel);
    overlayBar->setStyleSheet(
        "QFrame { background: #ffffff; border-bottom: 1px solid #d7e0ea; }"
        "QLabel { background: transparent; border: 0; }"
        "QComboBox { background: #ffffff; border: 1px solid #c9d5e2; border-radius: 7px; padding: 5px 24px 5px 8px; }");
    auto* overlayLayout = new QHBoxLayout(overlayBar);
    overlayLayout->setContentsMargins(16, 8, 16, 8);
    overlayLayout->setSpacing(8);
    auto* overlayLabel = createLabel("Map overlay", overlayBar, ui::FontRole::Caption);
    overlayLabel->setStyleSheet(ui::mutedTextStyleSheet());
    auto* overlayCombo = new QComboBox(overlayBar);
    overlayCombo->addItem("Peak Density", static_cast<int>(ResultOverlayMode::Density));
    overlayCombo->addItem("Bottlenecks", static_cast<int>(ResultOverlayMode::Bottlenecks));
    overlayCombo->addItem("Hotspots", static_cast<int>(ResultOverlayMode::Hotspots));
    overlayCombo->addItem("None", static_cast<int>(ResultOverlayMode::None));
    overlayCombo->setToolTip("Switch between result map overlays.");
    overlayLayout->addWidget(overlayLabel);
    overlayLayout->addWidget(overlayCombo);
    overlayLayout->addSpacing(10);
    overlayLayout->addWidget(new DensityLegendWidget(artifacts.densitySummary, overlayBar));
    overlayLayout->addStretch(1);
    layout->addWidget(overlayBar);
    canvas->setDensityOverlay(artifacts.densitySummary.peakField.cells.empty()
            ? artifacts.densitySummary.peakCells
            : artifacts.densitySummary.peakField.cells);
    canvas->setResultOverlayMode(ResultOverlayMode::Density);
    QObject::connect(overlayCombo, &QComboBox::currentIndexChanged, panel, [canvas, overlayCombo](int index) {
        const auto mode = static_cast<ResultOverlayMode>(overlayCombo->itemData(index).toInt());
        canvas->setResultOverlayMode(mode);
    });
    layout->addWidget(canvas, 1);
    EvacuationProgressWidget* progressWidget = nullptr;
    auto* graphPanel = createResultGraphPanel(artifacts, &progressWidget, panel);
    auto* replayControls = new ResultReplayControls(replayFramesForResult(artifacts, frame), canvas, progressWidget, panel);
    if (replayControlsOut != nullptr) {
        *replayControlsOut = replayControls;
    }
    layout->addWidget(replayControls);
    layout->addWidget(graphPanel, 1);
    return panel;
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

    layout->addWidget(shell != nullptr ? shell->createPanelHeader("Overview", panel, false) : createLabel("Overview", panel, ui::FontRole::Title));
    auto* scenarioLabel = createLabel(QString("Scenario: %1").arg(QString::fromStdString(scenario.name)), panel);
    scenarioLabel->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(scenarioLabel);
    auto* finalStatusLabel = createLabel(
        QString("Final: %1\nEvacuated: %2").arg(formatFinalStatus(frame), formatEvacuatedCount(frame)),
        panel);
    finalStatusLabel->setStyleSheet(ui::mutedTextStyleSheet());
    finalStatusLabel->setToolTip("Final run state at the end of the simulation.");
    layout->addWidget(finalStatusLabel);

    auto* metricsGrid = new QGridLayout();
    metricsGrid->setContentsMargins(0, 0, 0, 0);
    metricsGrid->setSpacing(8);
    metricsGrid->setColumnStretch(0, 1);
    metricsGrid->setColumnStretch(1, 1);
    const auto completionTime = resultCompletionTime(frame, artifacts);
    const auto worstBottleneck = risk.bottlenecks.empty()
        ? QString("None")
        : QString::fromStdString(risk.bottlenecks.front().label);
    const auto slowestGroup = artifacts.placementCompletion.empty()
        ? QString("Pending")
        : QString::fromStdString(artifacts.placementCompletion.front().placementId);
    metricsGrid->addWidget(createMetricCard(
        "Completion",
        formatSecondsValue(completionTime),
        panel,
        "Final evacuation time when all occupants completed evacuation; otherwise elapsed run time."), 0, 0);
    metricsGrid->addWidget(createMetricCard(
        "Margin",
        formatOptionalMargin(artifacts.timingSummary.marginSeconds),
        panel,
        "Scenario target time minus final or elapsed evacuation time."), 0, 1);
    metricsGrid->addWidget(createMetricCard(
        "Density",
        formatDensity(artifacts.densitySummary.peakDensityPeoplePerSquareMeter),
        panel,
        "Highest density observed during the run. The map shows the peak density field as a heatmap."), 1, 0);
    metricsGrid->addWidget(createMetricCard(
        "Bottleneck",
        worstBottleneck,
        panel,
        QString("Worst bottleneck observed during the run.\n\n%1")
            .arg(safecrowd::domain::scenarioBottleneckDefinition())), 1, 1);
    metricsGrid->addWidget(createMetricCard(
        "Slowest",
        slowestGroup,
        panel,
        "The source placement with the latest completion time."), 2, 0);
    metricsGrid->addWidget(createMetricCard(
        "Peak Risk",
        safecrowd::domain::scenarioRiskLevelLabel(risk.completionRisk),
        panel,
        "Highest completion risk observed at any point during the run."), 2, 1);
    metricsGrid->addWidget(createMetricCard(
        "Max Stalled",
        QString::number(static_cast<int>(risk.stalledAgentCount)),
        panel,
        "Largest number of agents classified as stalled at the same time during the run."), 3, 0);
    metricsGrid->addWidget(createMetricCard(
        "T90",
        formatOptionalSeconds(artifacts.timingSummary.t90Seconds),
        panel,
        "Time at which 90% of occupants completed evacuation."), 3, 1);
    metricsGrid->addWidget(createMetricCard(
        "T95",
        formatOptionalSeconds(artifacts.timingSummary.t95Seconds),
        panel,
        "Time at which 95% of occupants completed evacuation."), 4, 0);
    layout->addLayout(metricsGrid);
    auto* detailsHeader = createReportSectionHeader("Details", panel);
    layout->addWidget(detailsHeader);
    layout->addWidget(createDetailTabs(artifacts, panel), 1);
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
    const safecrowd::domain::SimulationFrame& frame,
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    std::function<void(std::size_t)> bottleneckFocusHandler,
    std::function<void(std::size_t)> hotspotFocusHandler,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* caption = createLabel("Risk findings and spatial reports", panel, ui::FontRole::Caption);
    caption->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(caption);

    auto* area = new QScrollArea(panel);
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui::polishScrollArea(area);

    auto* content = new QWidget(area);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 10, 0);
    contentLayout->setSpacing(12);

    auto* interpretationHeader = createReportSectionHeader("Decision Notes", content);
    contentLayout->addWidget(interpretationHeader);
    for (const auto& line : resultInterpretations(frame, risk, artifacts)) {
        auto* row = createReportRowButton({line}, content);
        row->setEnabled(false);
        contentLayout->addWidget(row);
    }

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
        .reviewPanelWidth = 320,
    }, this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(saveProjectHandler_);
    shell_->setOpenProjectHandler(openProjectHandler_);
    shell_->setBackHandler([this]() {
        navigateToAuthoring(true);
    });
    shell_->setNavigationTabs(
        {
            {
                .id = "reports",
                .label = "Reports",
            },
        },
        "reports",
        [](const QString&) {});

    auto* canvas = new SimulationCanvasWidget(layout_, shell_);
    canvas->setFrame(frame_);
    canvas->setHotspotOverlay(risk_.hotspots);
    canvas->setBottleneckOverlay(risk_.bottlenecks);
    ResultReplayControls* replayControls = nullptr;
    shell_->setCanvas(createResultCanvasPanel(canvas, frame_, artifacts_, &replayControls, shell_));
    shell_->setNavigationPanel(createResultFindingsPanel(
        frame_,
        risk_,
        artifacts_,
        [this, canvas, replayControls](std::size_t index) {
            if (index < risk_.bottlenecks.size() && replayControls != nullptr) {
                const auto& bottleneck = risk_.bottlenecks[index];
                if (bottleneck.detectionFrame.has_value()) {
                    replayControls->showFrame(*bottleneck.detectionFrame);
                } else if (bottleneck.detectedAtSeconds.has_value()) {
                    replayControls->showClosestFrameAtSeconds(*bottleneck.detectedAtSeconds);
                }
            }
            canvas->setResultOverlayMode(ResultOverlayMode::Bottlenecks);
            canvas->focusBottleneck(index);
        },
        [this, canvas, replayControls](std::size_t index) {
            if (index < risk_.hotspots.size() && replayControls != nullptr) {
                const auto& hotspot = risk_.hotspots[index];
                if (hotspot.detectionFrame.has_value()) {
                    replayControls->showFrame(*hotspot.detectionFrame);
                } else if (hotspot.detectedAtSeconds.has_value()) {
                    replayControls->showClosestFrameAtSeconds(*hotspot.detectedAtSeconds);
                }
            }
            canvas->setResultOverlayMode(ResultOverlayMode::Hotspots);
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

    rootLayout->addWidget(shell_);
}

const safecrowd::domain::ScenarioDraft& ScenarioResultWidget::scenario() const noexcept {
    return scenario_;
}

const safecrowd::domain::SimulationFrame& ScenarioResultWidget::frame() const noexcept {
    return frame_;
}

const safecrowd::domain::ScenarioRiskSnapshot& ScenarioResultWidget::risk() const noexcept {
    return risk_;
}

const safecrowd::domain::ScenarioResultArtifacts& ScenarioResultWidget::artifacts() const noexcept {
    return artifacts_;
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
        backToLayoutReviewHandler_,
        this);

    rootLayout->replaceWidget(shell_, authoringWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
}

}  // namespace safecrowd::application
