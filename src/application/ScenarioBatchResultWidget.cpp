#include "application/ScenarioBatchResultWidget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include "application/ScenarioResultNavigation.h"
#include "application/ScenarioRunWidget.h"
#include "application/SimulationCanvasWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"
#include "domain/AlternativeRecommendationService.h"
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

bool shouldShowRecommendationEvidence(const safecrowd::domain::AlternativeRecommendationEvidence& item) {
    const auto label = QString::fromStdString(item.label);
    return !label.startsWith("Risk ") && label != "Critical pressure events";
}

QString formatPressureScore(double score) {
    return QString::number(score, 'f', 1);
}

QString formatExposureScore(double score) {
    return QString::number(score, 'f', 1);
}

QString formatExposureSeconds(double seconds) {
    return QString("%1 agent-sec").arg(seconds, 0, 'f', 1);
}

double hazardExposureSecondsForKind(
    const safecrowd::domain::HazardExposureSummary& summary,
    safecrowd::domain::EnvironmentHazardKind kind) {
    double total = 0.0;
    for (const auto& metric : summary.hazards) {
        if (metric.kind == kind) {
            total += metric.exposedAgentSeconds;
        }
    }
    return total;
}

double totalHazardExposureSeconds(const safecrowd::domain::HazardExposureSummary& summary) {
    double total = 0.0;
    for (const auto& metric : summary.hazards) {
        total += metric.exposedAgentSeconds;
    }
    return total;
}

const safecrowd::domain::HazardExposureMetric* peakHazardExposureMetric(
    const safecrowd::domain::HazardExposureSummary& summary) {
    const safecrowd::domain::HazardExposureMetric* best = nullptr;
    for (const auto& metric : summary.hazards) {
        if (best == nullptr
            || metric.peakExposedAgentCount > best->peakExposedAgentCount
            || (metric.peakExposedAgentCount == best->peakExposedAgentCount
                && metric.exposedAgentSeconds > best->exposedAgentSeconds)) {
            best = &metric;
        }
    }
    return best;
}

QTableWidget* createComparisonTable(const QStringList& headers, QWidget* parent) {
    auto* table = new QTableWidget(0, headers.size(), parent);
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

QTableWidgetItem* tableItem(const QString& text, bool emphasized = false) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    if (emphasized) {
        auto font = item->font();
        font.setBold(true);
        item->setFont(font);
        item->setBackground(QColor("#eef5ff"));
    }
    return item;
}

QString scenarioRoleLabel(safecrowd::domain::ScenarioRole role) {
    switch (role) {
    case safecrowd::domain::ScenarioRole::Baseline:
        return "Baseline";
    case safecrowd::domain::ScenarioRole::Recommended:
        return "Recommended";
    case safecrowd::domain::ScenarioRole::Alternative:
    default:
        return "Alternative";
    }
}

QString wrapPanelText(const QString& text, int maxLineCharacters) {
    if (maxLineCharacters <= 0 || text.size() <= maxLineCharacters) {
        return text;
    }

    QStringList lines;
    QString currentLine;
    for (const auto& word : text.split(' ', Qt::SkipEmptyParts)) {
        if (word.size() > maxLineCharacters) {
            if (!currentLine.isEmpty()) {
                lines.push_back(currentLine);
                currentLine.clear();
            }
            for (int offset = 0; offset < word.size(); offset += maxLineCharacters) {
                lines.push_back(word.mid(offset, maxLineCharacters));
            }
            continue;
        }

        const auto candidate = currentLine.isEmpty() ? word : QString("%1 %2").arg(currentLine, word);
        if (candidate.size() > maxLineCharacters && !currentLine.isEmpty()) {
            lines.push_back(currentLine);
            currentLine = word;
        } else {
            currentLine = candidate;
        }
    }

    if (!currentLine.isEmpty()) {
        lines.push_back(currentLine);
    }
    return lines.join('\n');
}

QString compareScenarioLabel(const SavedScenarioResultState& result) {
    const auto name = wrapPanelText(QString::fromStdString(result.scenario.name), 34);
    const auto meta = wrapPanelText(
        QString("%1  -  %2  -  %3")
            .arg(scenarioRoleLabel(result.scenario.role))
            .arg(formatSeconds(finalSeconds(result)))
            .arg(safecrowd::domain::scenarioRiskLevelLabel(result.risk.completionRisk)),
        38);
    return QString("%1\n%2").arg(name, meta);
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

std::vector<int> defaultCompareIndices(
    const std::vector<SavedScenarioResultState>& results,
    int currentResultIndex,
    int baselineResultIndex) {
    constexpr std::size_t kDefaultCompareSelectionLimit = 8;

    std::vector<int> indices;
    indices.reserve(std::min(results.size(), kDefaultCompareSelectionLimit));
    auto addIndex = [&](int index) {
        if (index < 0 || index >= static_cast<int>(results.size())) {
            return;
        }
        if (std::find(indices.begin(), indices.end(), index) == indices.end()) {
            indices.push_back(index);
        }
    };

    if (results.size() <= kDefaultCompareSelectionLimit) {
        for (int index = 0; index < static_cast<int>(results.size()); ++index) {
            addIndex(index);
        }
        return indices;
    }

    addIndex(baselineResultIndex);
    addIndex(currentResultIndex);
    for (int index = 0;
         indices.size() < kDefaultCompareSelectionLimit && index < static_cast<int>(results.size());
         ++index) {
        addIndex(index);
    }
    return indices;
}

enum class ComparisonGraphMode {
    Remaining,
    Exits,
};

class ComparisonGraphWidget final : public QWidget {
public:
    explicit ComparisonGraphWidget(ComparisonGraphMode mode, QWidget* parent = nullptr)
        : QWidget(parent),
          mode_(mode) {
        setMinimumHeight(190);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setMouseTracking(true);
    }

    void setResults(
        const std::vector<SavedScenarioResultState>& results,
        std::vector<int> selectedIndices,
        int displayIndex) {
        results_ = &results;
        selectedIndices_ = std::move(selectedIndices);
        displayIndex_ = displayIndex;
        update();
    }

    void setCurrentTimeSeconds(double seconds) {
        currentTimeSeconds_ = seconds;
        update();
    }

    void setTimingMarkerActivatedHandler(std::function<void(double)> handler) {
        timingMarkerActivatedHandler_ = std::move(handler);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        (void)event;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.fillRect(rect(), QColor("#ffffff"));
        painter.setPen(QPen(QColor("#d7e0ea"), 1));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);

        const QRectF plot = rect().adjusted(40, 16, -18, -68);
        if (results_ == nullptr || selectedIndices_.empty() || plot.width() <= 1.0 || plot.height() <= 1.0) {
            painter.setPen(QColor("#6b7785"));
            painter.setFont(ui::font(ui::FontRole::Caption));
            painter.drawText(rect(), Qt::AlignCenter, "Select scenarios to compare");
            return;
        }

        painter.setPen(QPen(QColor("#e2e8f0"), 1));
        painter.drawLine(plot.bottomLeft(), plot.bottomRight());
        painter.drawLine(plot.bottomLeft(), plot.topLeft());

        if (mode_ == ComparisonGraphMode::Remaining) {
            drawRemaining(painter, plot);
        } else {
            drawExits(painter, plot);
        }
        drawLegend(painter, QRectF(rect().left() + 12, rect().bottom() - 52, rect().width() - 24, 42));
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event == nullptr) {
            return;
        }
        const bool hover = timingMarkerHitSeconds(event->position()).has_value();
        setCursor(hover ? Qt::PointingHandCursor : Qt::ArrowCursor);
        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent* event) override {
        unsetCursor();
        QWidget::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event == nullptr || event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }
        const auto seconds = timingMarkerHitSeconds(event->position());
        if (!seconds.has_value()) {
            QWidget::mousePressEvent(event);
            return;
        }
        event->accept();
        if (timingMarkerActivatedHandler_) {
            timingMarkerActivatedHandler_(*seconds);
        }
    }

private:
    QColor colorForSeries(int index) const {
        static const std::vector<QColor> colors{
            QColor("#2563eb"),
            QColor("#16a34a"),
            QColor("#dc2626"),
            QColor("#7c3aed"),
            QColor("#ea580c"),
            QColor("#0891b2"),
            QColor("#be123c"),
            QColor("#4d7c0f"),
            QColor("#9333ea"),
            QColor("#0f766e"),
            QColor("#b45309"),
            QColor("#475569"),
        };
        if (index == displayIndex_) {
            return QColor("#1d4ed8");
        }
        return colors[static_cast<std::size_t>(std::abs(index)) % colors.size()];
    }

    Qt::PenStyle lineStyleForSeries(int index) const {
        if (index == displayIndex_) {
            return Qt::SolidLine;
        }
        constexpr int kPaletteSize = 12;
        switch ((std::abs(index) / kPaletteSize) % 4) {
        case 1:
            return Qt::DashLine;
        case 2:
            return Qt::DotLine;
        case 3:
            return Qt::DashDotLine;
        default:
            return Qt::SolidLine;
        }
    }

    QPen penForSeries(int index, double width) const {
        return QPen(
            colorForSeries(index),
            index == displayIndex_ ? std::max(width, 2.8) : width,
            lineStyleForSeries(index),
            Qt::RoundCap,
            Qt::RoundJoin);
    }

    double remainingMaxTimeSeconds() const {
        double maxTime = 1.0;
        auto considerResult = [&](int index) {
            if (!validIndex(index)) {
                return;
            }
            const auto& result = (*results_)[static_cast<std::size_t>(index)];
            maxTime = std::max(maxTime, finalSeconds(result));
            for (const auto& sample : result.artifacts.evacuationProgress) {
                maxTime = std::max(maxTime, sample.timeSeconds);
            }
        };

        for (const auto index : selectedIndices_) {
            considerResult(index);
        }
        considerResult(displayIndex_);
        if (validIndex(displayIndex_)) {
            const auto& timing = (*results_)[static_cast<std::size_t>(displayIndex_)].artifacts.timingSummary;
            if (timing.t90Seconds.has_value()) {
                maxTime = std::max(maxTime, *timing.t90Seconds);
            }
            if (timing.t95Seconds.has_value()) {
                maxTime = std::max(maxTime, *timing.t95Seconds);
            }
        }
        return maxTime;
    }

    void drawTimingMarker(
        QPainter& painter,
        const QRectF& plot,
        double maxTime,
        const std::optional<double>& seconds,
        const QString& label) const {
        if (!seconds.has_value()) {
            return;
        }
        const auto x = plot.left() + std::clamp(*seconds / maxTime, 0.0, 1.0) * plot.width();
        painter.setPen(QPen(QColor("#d97706"), 1, Qt::DashLine));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.setPen(QColor("#92400e"));
        painter.drawText(QRectF(x + 4, plot.top(), 46, 18), Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    QRectF markerHitRegion(const QRectF& plot, double maxTime, double seconds) const {
        const auto x = plot.left() + std::clamp(seconds / maxTime, 0.0, 1.0) * plot.width();
        const QRectF lineHit(x - 6.0, plot.top(), 12.0, plot.height());
        const QRectF labelHit(x - 4.0, plot.top() - 2.0, 70.0, 22.0);
        return lineHit.united(labelHit);
    }

    std::optional<double> timingMarkerHitSeconds(const QPointF& position) const {
        if (mode_ != ComparisonGraphMode::Remaining || !validIndex(displayIndex_)) {
            return std::nullopt;
        }
        const QRectF plot = QRectF(rect()).adjusted(40, 16, -18, -68);
        if (!plot.contains(position)) {
            return std::nullopt;
        }

        const auto maxTime = remainingMaxTimeSeconds();
        const auto& timing = (*results_)[static_cast<std::size_t>(displayIndex_)].artifacts.timingSummary;
        struct Candidate {
            double seconds;
            double distance;
        };
        std::optional<Candidate> best;
        auto consider = [&](const std::optional<double>& markerSeconds) {
            if (!markerSeconds.has_value()) {
                return;
            }
            const auto region = markerHitRegion(plot, maxTime, *markerSeconds);
            if (!region.contains(position)) {
                return;
            }
            const auto x = plot.left() + std::clamp(*markerSeconds / maxTime, 0.0, 1.0) * plot.width();
            const auto distance = std::abs(position.x() - x);
            if (!best.has_value() || distance < best->distance) {
                best = Candidate{.seconds = *markerSeconds, .distance = distance};
            }
        };

        consider(timing.t90Seconds);
        consider(timing.t95Seconds);
        if (!best.has_value()) {
            return std::nullopt;
        }
        return best->seconds;
    }

    void drawRemaining(QPainter& painter, const QRectF& plot) {
        const auto maxTime = remainingMaxTimeSeconds();

        painter.setFont(ui::font(ui::FontRole::Caption));
        painter.setPen(QColor("#6b7785"));
        painter.drawText(QRectF(4, plot.top() - 5, 32, 18), Qt::AlignRight | Qt::AlignVCenter, "100%");
        painter.drawText(QRectF(4, plot.bottom() - 10, 32, 18), Qt::AlignRight | Qt::AlignVCenter, "0%");
        painter.drawText(QRectF(plot.left(), plot.bottom() + 4, plot.width(), 18), Qt::AlignRight, formatSeconds(maxTime));

        for (const auto index : selectedIndices_) {
            if (!validIndex(index)) {
                continue;
            }
            const auto& result = (*results_)[static_cast<std::size_t>(index)];
            const auto series = progressSeries(result);
            if (series.empty()) {
                continue;
            }

            QPainterPath path;
            bool started = false;
            for (const auto& [time, evacuatedRatio] : series) {
                const auto remainingRatio = 1.0 - std::clamp(evacuatedRatio, 0.0, 1.0);
                const auto x = plot.left() + std::clamp(time / maxTime, 0.0, 1.0) * plot.width();
                const auto y = plot.bottom() - remainingRatio * plot.height();
                if (!started) {
                    path.moveTo(x, y);
                    started = true;
                } else {
                    path.lineTo(x, y);
                }
            }
            painter.setPen(penForSeries(index, 2.0));
            painter.drawPath(path);
        }

        if (validIndex(displayIndex_)) {
            const auto& timing = (*results_)[static_cast<std::size_t>(displayIndex_)].artifacts.timingSummary;
            drawTimingMarker(painter, plot, maxTime, timing.t90Seconds, "T90");
            drawTimingMarker(painter, plot, maxTime, timing.t95Seconds, "T95");
        }

        if (currentTimeSeconds_ >= 0.0) {
            const auto x = plot.left() + std::clamp(currentTimeSeconds_ / maxTime, 0.0, 1.0) * plot.width();
            painter.setPen(QPen(QColor("#0f172a"), 1, Qt::DashLine));
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
    }

    void drawExits(QPainter& painter, const QRectF& plot) {
        std::vector<QString> exits;
        for (const auto index : selectedIndices_) {
            if (!validIndex(index)) {
                continue;
            }
            for (const auto& exit : (*results_)[static_cast<std::size_t>(index)].artifacts.exitUsage) {
                const auto label = QString::fromStdString(exit.exitLabel.empty() ? exit.exitZoneId : exit.exitLabel);
                if (std::find(exits.begin(), exits.end(), label) == exits.end()) {
                    exits.push_back(label);
                }
            }
        }
        if (exits.empty()) {
            painter.setPen(QColor("#6b7785"));
            painter.setFont(ui::font(ui::FontRole::Caption));
            painter.drawText(plot, Qt::AlignCenter, "No exit usage data");
            return;
        }

        painter.setFont(ui::font(ui::FontRole::Caption));
        painter.setPen(QColor("#6b7785"));
        painter.drawText(QRectF(4, plot.top() - 5, 32, 18), Qt::AlignRight | Qt::AlignVCenter, "100%");
        painter.drawText(QRectF(4, plot.bottom() - 10, 32, 18), Qt::AlignRight | Qt::AlignVCenter, "0%");

        for (int exitIndex = 0; exitIndex < static_cast<int>(exits.size()); ++exitIndex) {
            const auto x = exits.size() == 1
                ? plot.center().x()
                : plot.left() + (static_cast<double>(exitIndex) / static_cast<double>(exits.size() - 1)) * plot.width();
            painter.setPen(QPen(QColor("#edf2f7"), 1));
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            painter.setPen(QColor("#6b7785"));
            painter.drawText(QRectF(x - 38, plot.bottom() + 4, 76, 18), Qt::AlignCenter, exits[static_cast<std::size_t>(exitIndex)]);
        }

        for (const auto index : selectedIndices_) {
            if (!validIndex(index)) {
                continue;
            }
            const auto& result = (*results_)[static_cast<std::size_t>(index)];
            QPainterPath path;
            bool started = false;
            for (int exitIndex = 0; exitIndex < static_cast<int>(exits.size()); ++exitIndex) {
                double usageRatio = 0.0;
                const auto& exitLabel = exits[static_cast<std::size_t>(exitIndex)];
                for (const auto& exit : result.artifacts.exitUsage) {
                    const auto label = QString::fromStdString(exit.exitLabel.empty() ? exit.exitZoneId : exit.exitLabel);
                    if (label == exitLabel) {
                        usageRatio = std::clamp(exit.usageRatio, 0.0, 1.0);
                        break;
                    }
                }
                const auto x = exits.size() == 1
                    ? plot.center().x()
                    : plot.left() + (static_cast<double>(exitIndex) / static_cast<double>(exits.size() - 1)) * plot.width();
                const auto y = plot.bottom() - usageRatio * plot.height();
                if (!started) {
                    path.moveTo(x, y);
                    started = true;
                } else {
                    path.lineTo(x, y);
                }
            }
            painter.setPen(penForSeries(index, 2.0));
            painter.drawPath(path);
            for (int exitIndex = 0; exitIndex < static_cast<int>(exits.size()); ++exitIndex) {
                double usageRatio = 0.0;
                const auto& exitLabel = exits[static_cast<std::size_t>(exitIndex)];
                for (const auto& exit : result.artifacts.exitUsage) {
                    const auto label = QString::fromStdString(exit.exitLabel.empty() ? exit.exitZoneId : exit.exitLabel);
                    if (label == exitLabel) {
                        usageRatio = std::clamp(exit.usageRatio, 0.0, 1.0);
                        break;
                    }
                }
                const auto x = exits.size() == 1
                    ? plot.center().x()
                    : plot.left() + (static_cast<double>(exitIndex) / static_cast<double>(exits.size() - 1)) * plot.width();
                const auto y = plot.bottom() - usageRatio * plot.height();
                painter.setBrush(colorForSeries(index));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(QPointF(x, y), 3.5, 3.5);
            }
        }
    }

    void drawLegend(QPainter& painter, const QRectF& legendRect) {
        painter.setFont(ui::font(ui::FontRole::Caption));
        QFontMetrics metrics(painter.font());

        std::vector<int> visibleIndices;
        visibleIndices.reserve(selectedIndices_.size());
        for (const auto index : selectedIndices_) {
            if (!validIndex(index)) {
                continue;
            }
            visibleIndices.push_back(index);
        }
        if (visibleIndices.empty()) {
            return;
        }

        constexpr double kEntryWidth = 148.0;
        constexpr int kMaxRows = 2;
        const int columns = std::max(1, static_cast<int>(std::floor(legendRect.width() / kEntryWidth)));
        const int maxEntries = std::max(1, columns * kMaxRows);
        const bool hasOverflow = static_cast<int>(visibleIndices.size()) > maxEntries;
        const int visibleCount = static_cast<int>(visibleIndices.size());
        const int scenarioEntries = hasOverflow ? std::max(0, maxEntries - 1) : std::min(visibleCount, maxEntries);
        const auto rowHeight = legendRect.height() / static_cast<double>(kMaxRows);
        const auto slotWidth = legendRect.width() / static_cast<double>(columns);

        auto drawSlot = [&](int slot, int index) {
            const int row = slot / columns;
            const int column = slot % columns;
            const auto x = legendRect.left() + static_cast<double>(column) * slotWidth;
            const auto y = legendRect.top() + (static_cast<double>(row) + 0.5) * rowHeight;
            const QRectF textRect(
                x + 24.0,
                legendRect.top() + static_cast<double>(row) * rowHeight,
                std::max(0.0, slotWidth - 30.0),
                rowHeight);
            const auto name = metrics.elidedText(
                QString::fromStdString((*results_)[static_cast<std::size_t>(index)].scenario.name),
                Qt::ElideRight,
                static_cast<int>(textRect.width()));

            painter.setPen(penForSeries(index, 2.0));
            painter.drawLine(QPointF(x, y), QPointF(x + 18.0, y));
            painter.setPen(QColor("#344256"));
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, name);
        };

        for (int slot = 0; slot < scenarioEntries; ++slot) {
            drawSlot(slot, visibleIndices[static_cast<std::size_t>(slot)]);
        }

        if (hasOverflow) {
            const int slot = scenarioEntries;
            const int row = slot / columns;
            const int column = slot % columns;
            const auto x = legendRect.left() + static_cast<double>(column) * slotWidth;
            const QRectF textRect(
                x,
                legendRect.top() + static_cast<double>(row) * rowHeight,
                slotWidth,
                rowHeight);
            painter.setPen(QColor("#6b7785"));
            painter.drawText(
                textRect,
                Qt::AlignLeft | Qt::AlignVCenter,
                QString("+%1 more").arg(static_cast<int>(visibleIndices.size()) - scenarioEntries));
        }
    }

    bool validIndex(int index) const noexcept {
        return results_ != nullptr && index >= 0 && index < static_cast<int>(results_->size());
    }

    ComparisonGraphMode mode_{ComparisonGraphMode::Remaining};
    const std::vector<SavedScenarioResultState>* results_{nullptr};
    std::vector<int> selectedIndices_{};
    int displayIndex_{0};
    double currentTimeSeconds_{-1.0};
    std::function<void(double)> timingMarkerActivatedHandler_{};
};

std::vector<safecrowd::domain::SimulationFrame> replayFramesForResult(const SavedScenarioResultState& result) {
    if (!result.artifacts.replayFrames.empty()) {
        return result.artifacts.replayFrames;
    }
    return {result.frame};
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
    for (const auto& source : scenario.population.occupantSources) {
        ScenarioCrowdPlacement uiPlacement;
        uiPlacement.id = QString::fromStdString(source.id);
        uiPlacement.name = uiPlacement.id;
        uiPlacement.kind = ScenarioCrowdPlacementKind::Source;
        uiPlacement.zoneId = QString::fromStdString(source.zoneId);
        uiPlacement.floorId = QString::fromStdString(source.floorId);
        uiPlacement.area = {source.position};
        uiPlacement.occupantCount = static_cast<int>(source.targetAgentCount);
        uiPlacement.velocity = source.initialVelocity;
        uiPlacement.sourceAgentsPerSpawn = std::max(1, static_cast<int>(source.agentsPerSpawn));
        uiPlacement.sourceStartSeconds = source.startSeconds;
        uiPlacement.sourceEndSeconds = source.endSeconds;
        uiPlacement.sourceIntervalSeconds = source.spawnIntervalSeconds;
        state.crowdPlacements.push_back(std::move(uiPlacement));
    }

    return state;
}

bool scenarioIdExists(const ScenarioAuthoringWidget::InitialState& initial, const std::string& id) {
    return std::any_of(initial.scenarios.begin(), initial.scenarios.end(), [&](const auto& scenario) {
        return scenario.draft.scenarioId == id;
    });
}

bool scenarioNameExists(const ScenarioAuthoringWidget::InitialState& initial, const std::string& name) {
    return std::any_of(initial.scenarios.begin(), initial.scenarios.end(), [&](const auto& scenario) {
        return scenario.draft.name == name;
    });
}

std::string uniqueScenarioId(const ScenarioAuthoringWidget::InitialState& initial, const std::string& requestedId) {
    const auto base = requestedId.empty() ? std::string{"recommended-scenario"} : requestedId;
    if (!scenarioIdExists(initial, base)) {
        return base;
    }
    for (int suffix = 2; suffix < 1000; ++suffix) {
        auto candidate = base + "-" + std::to_string(suffix);
        if (!scenarioIdExists(initial, candidate)) {
            return candidate;
        }
    }
    return base + "-copy";
}

std::string uniqueScenarioName(const ScenarioAuthoringWidget::InitialState& initial, const std::string& requestedName) {
    const auto base = requestedName.empty() ? std::string{"Recommended scenario"} : requestedName;
    if (!scenarioNameExists(initial, base)) {
        return base;
    }
    for (int suffix = 2; suffix < 1000; ++suffix) {
        auto candidate = base + " " + std::to_string(suffix);
        if (!scenarioNameExists(initial, candidate)) {
            return candidate;
        }
    }
    return base + " copy";
}

std::optional<int> existingScenarioIndexBySourceTemplate(
    const ScenarioAuthoringWidget::InitialState& initial,
    const std::string& sourceTemplateId) {
    if (sourceTemplateId.empty()) {
        return std::nullopt;
    }
    const auto it = std::find_if(initial.scenarios.begin(), initial.scenarios.end(), [&](const auto& scenario) {
        return scenario.draft.role == safecrowd::domain::ScenarioRole::Recommended
            && scenario.draft.sourceTemplateId == sourceTemplateId;
    });
    if (it == initial.scenarios.end()) {
        return std::nullopt;
    }
    return static_cast<int>(std::distance(initial.scenarios.begin(), it));
}

ScenarioResultNavigationView resultNavigationViewFromSaved(SavedResultNavigationView view) {
    switch (view) {
    case SavedResultNavigationView::OperationalConflict:
        return ScenarioResultNavigationView::OperationalConflict;
    case SavedResultNavigationView::Hotspot:
        return ScenarioResultNavigationView::Hotspot;
    case SavedResultNavigationView::HazardExposure:
        return ScenarioResultNavigationView::HazardExposure;
    case SavedResultNavigationView::Zone:
        return ScenarioResultNavigationView::Zone;
    case SavedResultNavigationView::Groups:
        return ScenarioResultNavigationView::Groups;
    case SavedResultNavigationView::Recommendations:
        return ScenarioResultNavigationView::Recommendations;
    case SavedResultNavigationView::Bottleneck:
    default:
        return ScenarioResultNavigationView::Bottleneck;
    }
}

SavedResultNavigationView savedResultNavigationView(ScenarioResultNavigationView view) {
    switch (view) {
    case ScenarioResultNavigationView::OperationalConflict:
        return SavedResultNavigationView::OperationalConflict;
    case ScenarioResultNavigationView::Hotspot:
        return SavedResultNavigationView::Hotspot;
    case ScenarioResultNavigationView::HazardExposure:
        return SavedResultNavigationView::HazardExposure;
    case ScenarioResultNavigationView::Zone:
        return SavedResultNavigationView::Zone;
    case ScenarioResultNavigationView::Groups:
        return SavedResultNavigationView::Groups;
    case ScenarioResultNavigationView::Recommendations:
        return SavedResultNavigationView::Recommendations;
    case ScenarioResultNavigationView::Bottleneck:
    default:
        return SavedResultNavigationView::Bottleneck;
    }
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
    if (!results_.empty()) {
        resultNavigationView_ = resultNavigationViewFromSaved(results_[static_cast<std::size_t>(currentResultIndex_)].navigationView);
    }
    selectedCompareIndices_ = defaultCompareIndices(results_, currentResultIndex_, baselineResultIndex());
    for (int index = 0; index < static_cast<int>(results_.size()); ++index) {
        selectedRecommendationIndices_.push_back(index);
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

    shell_->setCanvas(createCanvasPanel());
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

SavedResultNavigationView ScenarioBatchResultWidget::currentSavedNavigationView() const noexcept {
    return savedResultNavigationView(resultNavigationView_);
}

std::optional<ScenarioAuthoringWidget::InitialState> ScenarioBatchResultWidget::returnAuthoringState() const {
    return returnAuthoringState_;
}

QWidget* ScenarioBatchResultWidget::createCanvasPanel() {
    auto* panel = new QWidget(shell_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* selectorBar = new QFrame(panel);
    selectorBar->setStyleSheet(
        "QFrame { background: #ffffff; border-bottom: 1px solid #d7e0ea; }"
        "QLabel { background: transparent; border: 0; }"
        "QComboBox { background: #ffffff; border: 1px solid #c9d5e2; border-radius: 7px; padding: 5px 24px 5px 8px; }");
    auto* selectorLayout = new QHBoxLayout(selectorBar);
    selectorLayout->setContentsMargins(16, 8, 16, 8);
    selectorLayout->setSpacing(8);

    auto* scenarioLabel = createLabel("Scenario playback", selectorBar, ui::FontRole::Caption);
    scenarioLabel->setStyleSheet(ui::mutedTextStyleSheet());
    selectorLayout->addWidget(scenarioLabel);

    displayScenarioCombo_ = new QComboBox(selectorBar);
    for (const auto& result : results_) {
        displayScenarioCombo_->addItem(QString::fromStdString(result.scenario.name));
    }
    displayScenarioCombo_->setCurrentIndex(currentResultIndex_);
    selectorLayout->addWidget(displayScenarioCombo_, 1);

    auto* overlayLabel = createLabel("Map overlay", selectorBar, ui::FontRole::Caption);
    overlayLabel->setStyleSheet(ui::mutedTextStyleSheet());
    selectorLayout->addWidget(overlayLabel);

    overlayCombo_ = new QComboBox(selectorBar);
    overlayCombo_->addItem("Density", static_cast<int>(OverlayMode::Density));
    overlayCombo_->addItem("Pressure", static_cast<int>(OverlayMode::Pressure));
    overlayCombo_->addItem("Hotspots", static_cast<int>(OverlayMode::Hotspots));
    overlayCombo_->addItem("Bottlenecks", static_cast<int>(OverlayMode::Bottlenecks));
    overlayCombo_->addItem("Operational Conflicts", static_cast<int>(OverlayMode::OperationalConflicts));
    overlayCombo_->addItem("None", static_cast<int>(OverlayMode::None));
    overlayCombo_->setCurrentIndex(0);
    selectorLayout->addWidget(overlayCombo_);
    layout->addWidget(selectorBar);

    canvas_ = new SimulationCanvasWidget(layout_, panel);
    layout->addWidget(canvas_, 1);

    auto* replayBar = new QFrame(panel);
    replayBar->setStyleSheet(
        "QFrame { background: #ffffff; border-top: 1px solid #d7e0ea; border-bottom: 1px solid #d7e0ea; }"
        "QPushButton { border: 1px solid #c9d5e2; border-radius: 6px; padding: 5px 12px; background: #ffffff; color: #344256; }"
        "QPushButton:hover { background: #eef3f8; }"
        "QSlider::groove:horizontal { height: 6px; background: #d7e0ea; border-radius: 3px; }"
        "QSlider::handle:horizontal { width: 16px; margin: -5px 0; border-radius: 8px; background: #1f5fae; }"
        "QLabel { background: transparent; border: 0; }");
    auto* replayLayout = new QHBoxLayout(replayBar);
    replayLayout->setContentsMargins(16, 8, 16, 8);
    replayLayout->setSpacing(10);
    playButton_ = new QPushButton("Play", replayBar);
    replaySlider_ = new QSlider(Qt::Horizontal, replayBar);
    replaySlider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    replayTimeLabel_ = createLabel("", replayBar, ui::FontRole::Caption);
    replayTimeLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    replayLayout->addWidget(playButton_);
    replayLayout->addWidget(replaySlider_, 1);
    replayLayout->addWidget(replayTimeLabel_);
    layout->addWidget(replayBar);

    auto* graphPanel = new QFrame(panel);
    graphPanel->setStyleSheet(
        "QFrame { background: #ffffff; border-top: 1px solid #d7e0ea; }"
        "QLabel { border: 0; }"
        "QTabWidget::pane { border: 0; }"
        "QTabBar::tab { background: #ffffff; border: 1px solid #c9d5e2; padding: 6px 12px; margin-right: 4px; }"
        "QTabBar::tab:selected { background: #e6eef8; color: #1f5fae; }");
    graphPanel->setMinimumHeight(260);
    auto* graphLayout = new QVBoxLayout(graphPanel);
    graphLayout->setContentsMargins(16, 10, 16, 16);
    graphLayout->setSpacing(8);
    graphLayout->addWidget(createLabel("Comparison Graphs", graphPanel, ui::FontRole::SectionTitle));
    auto* tabs = new QTabWidget(graphPanel);
    remainingChart_ = new ComparisonGraphWidget(ComparisonGraphMode::Remaining, tabs);
    exitsChart_ = new ComparisonGraphWidget(ComparisonGraphMode::Exits, tabs);
    exposureTable_ = createComparisonTable({"Scenario", "Total", "Fire", "Smoke", "Peak", "Peak at"}, tabs);
    pressureTable_ = createComparisonTable({"Scenario", "Peak score", "Exposed / Critical", "Hotspots", "Events", "Peak at"}, tabs);
    static_cast<ComparisonGraphWidget*>(remainingChart_)->setResults(results_, selectedCompareIndices_, currentResultIndex_);
    static_cast<ComparisonGraphWidget*>(exitsChart_)->setResults(results_, selectedCompareIndices_, currentResultIndex_);
    static_cast<ComparisonGraphWidget*>(remainingChart_)->setTimingMarkerActivatedHandler([this](double seconds) {
        seekToTimingMarkerSeconds(seconds);
    });
    tabs->addTab(remainingChart_, "Remaining");
    tabs->addTab(exitsChart_, "Exits");
    tabs->addTab(exposureTable_, "Exposure");
    tabs->addTab(pressureTable_, "Pressure");
    graphLayout->addWidget(tabs, 1);
    layout->addWidget(graphPanel, 1);

    replayTimer_ = new QTimer(this);
    replayTimer_->setInterval(500);

    connect(displayScenarioCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < static_cast<int>(results_.size())) {
            currentResultIndex_ = index;
            refreshSelectedResult();
        }
    });
    connect(overlayCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (overlayCombo_ == nullptr || index < 0) {
            return;
        }
        const auto data = overlayCombo_->itemData(index);
        if (!data.isValid()) {
            return;
        }
        setOverlayMode(static_cast<OverlayMode>(data.toInt()));
    });
    connect(playButton_, &QPushButton::clicked, this, [this]() {
        if (replayTimer_ == nullptr || replayFrames_.size() <= 1) {
            return;
        }
        if (replayTimer_->isActive()) {
            pauseReplay();
            return;
        }
        if (replayFrameIndex_ >= static_cast<int>(replayFrames_.size()) - 1) {
            replayFrameIndex_ = 0;
            applyReplayFrame(replayFrameIndex_);
        }
        playButton_->setText("Pause");
        replayTimer_->start();
    });
    connect(replaySlider_, &QSlider::valueChanged, this, [this](int value) {
        pauseReplay();
        applyReplayFrame(value);
    });
    connect(replayTimer_, &QTimer::timeout, this, [this]() {
        advanceReplay();
    });

    return panel;
}

QWidget* ScenarioBatchResultWidget::createSummaryPanel() {
    auto* panel = new QWidget(shell_);
    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(12);
    panelLayout->addWidget(shell_ != nullptr ? shell_->createPanelHeader("Compare", panel, false) : createLabel("Compare", panel, ui::FontRole::Title));

    auto* scrollArea = new QScrollArea(panel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui::polishScrollArea(scrollArea);
    auto* content = new QWidget(scrollArea);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 10, 0);
    layout->setSpacing(12);

    auto* intro = createLabel("Choose which completed scenarios appear together in the comparison graphs and risk summary tables.", content, ui::FontRole::Caption);
    intro->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    intro->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(intro);

    comparisonCountLabel_ = createLabel("", content, ui::FontRole::Caption);
    comparisonCountLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    comparisonCountLabel_->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(comparisonCountLabel_);

    auto* selectionControls = new QWidget(content);
    selectionControls->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto* selectionLayout = new QGridLayout(selectionControls);
    selectionLayout->setContentsMargins(0, 0, 0, 0);
    selectionLayout->setSpacing(8);

    auto* selectAllButton = new QPushButton("Select All", selectionControls);
    selectAllButton->setFont(ui::font(ui::FontRole::Caption));
    selectAllButton->setMinimumWidth(0);
    selectAllButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selectAllButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    auto* baselineCurrentButton = new QPushButton("Baseline + Current", selectionControls);
    baselineCurrentButton->setFont(ui::font(ui::FontRole::Caption));
    baselineCurrentButton->setMinimumWidth(0);
    baselineCurrentButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    baselineCurrentButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    auto* clearButton = new QPushButton("Clear", selectionControls);
    clearButton->setFont(ui::font(ui::FontRole::Caption));
    clearButton->setMinimumWidth(0);
    clearButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    clearButton->setStyleSheet(ui::secondaryButtonStyleSheet());

    selectionLayout->addWidget(selectAllButton, 0, 0);
    selectionLayout->addWidget(clearButton, 0, 1);
    selectionLayout->addWidget(baselineCurrentButton, 1, 0, 1, 2);
    selectionLayout->setColumnStretch(0, 1);
    selectionLayout->setColumnStretch(1, 1);
    layout->addWidget(selectionControls);

    compareCheckBoxes_.clear();
    for (int row = 0; row < static_cast<int>(results_.size()); ++row) {
        const auto& result = results_[row];
        auto* checkbox = new QCheckBox(compareScenarioLabel(result), content);
        checkbox->setFont(ui::font(ui::FontRole::Body));
        checkbox->setMinimumWidth(0);
        checkbox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        checkbox->setChecked(std::find(selectedCompareIndices_.begin(), selectedCompareIndices_.end(), row) != selectedCompareIndices_.end());
        checkbox->setStyleSheet(
            "QCheckBox { background: #ffffff; border: 1px solid #d7e0ea; border-radius: 8px; padding: 8px; color: #16202b; }"
            "QCheckBox:hover { background: #f8fafc; }"
            "QCheckBox::indicator { width: 16px; height: 16px; }");
        compareCheckBoxes_.push_back(checkbox);
        layout->addWidget(checkbox);
        connect(checkbox, &QCheckBox::toggled, this, [this]() {
            refreshComparisonSelection();
        });
    }

    connect(selectAllButton, &QPushButton::clicked, this, [this]() {
        selectAllComparisonScenarios();
    });
    connect(baselineCurrentButton, &QPushButton::clicked, this, [this]() {
        selectBaselineAndCurrentComparisonScenarios();
    });
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        clearComparisonScenarios();
    });
    refreshComparisonCountLabel();

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

    connect(rerunButton, &QPushButton::clicked, this, [this]() {
        rerunBatch();
    });
    connect(editButton, &QPushButton::clicked, this, [this]() {
        navigateToAuthoring();
    });
    return panel;
}

void ScenarioBatchResultWidget::navigateToAuthoring() {
    pauseReplay();

    auto initial = returnAuthoringState_.value_or(ScenarioAuthoringWidget::InitialState{});
    if (initial.scenarios.empty()) {
        for (const auto& result : results_) {
            initial.scenarios.push_back(scenarioStateFromDraft(result.scenario, layout_));
        }
    }
    if (currentResultIndex_ >= 0 && currentResultIndex_ < static_cast<int>(results_.size())) {
        const auto& selectedScenarioId = results_[static_cast<std::size_t>(currentResultIndex_)].scenario.scenarioId;
        const auto selectedIt = std::find_if(initial.scenarios.begin(), initial.scenarios.end(), [&](const auto& scenario) {
            return scenario.draft.scenarioId == selectedScenarioId;
        });
        if (selectedIt != initial.scenarios.end()) {
            initial.currentScenarioIndex = static_cast<int>(std::distance(initial.scenarios.begin(), selectedIt));
        } else if (currentResultIndex_ < static_cast<int>(initial.scenarios.size())) {
            initial.currentScenarioIndex = currentResultIndex_;
        }
    }
    initial.rightPanelMode = ScenarioAuthoringWidget::RightPanelMode::Scenario;
    initial.inspectorPanelVisible = true;
    initial.scenarioPanelVisible = true;
    showAuthoring(std::move(initial));
}

void ScenarioBatchResultWidget::showAuthoring(ScenarioAuthoringWidget::InitialState initialState) {
    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    auto* authoringWidget = new ScenarioAuthoringWidget(
        projectName_,
        layout_,
        std::move(initialState),
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

void ScenarioBatchResultWidget::createRecommendedScenario(
    safecrowd::domain::ScenarioDraft recommendedScenario) {
    pauseReplay();

    auto initial = returnAuthoringState_.value_or(ScenarioAuthoringWidget::InitialState{});
    if (initial.scenarios.empty()) {
        for (const auto& result : results_) {
            initial.scenarios.push_back(scenarioStateFromDraft(result.scenario, layout_));
        }
    }

    if (const auto existingIndex = existingScenarioIndexBySourceTemplate(initial, recommendedScenario.sourceTemplateId);
        existingIndex.has_value()) {
        initial.currentScenarioIndex = *existingIndex;
        initial.rightPanelMode = ScenarioAuthoringWidget::RightPanelMode::Scenario;
        initial.inspectorPanelVisible = true;
        initial.scenarioPanelVisible = true;
        showAuthoring(std::move(initial));
        return;
    }

    recommendedScenario.scenarioId = uniqueScenarioId(initial, recommendedScenario.scenarioId);
    recommendedScenario.name = uniqueScenarioName(initial, recommendedScenario.name);

    QString baseScenarioId;
    if (currentResultIndex_ >= 0 && currentResultIndex_ < static_cast<int>(results_.size())) {
        const auto& source = results_[static_cast<std::size_t>(currentResultIndex_)].scenario;
        const auto sourceIt = std::find_if(initial.scenarios.begin(), initial.scenarios.end(), [&](const auto& scenario) {
            return scenario.draft.scenarioId == source.scenarioId;
        });
        if (sourceIt != initial.scenarios.end() && !sourceIt->baseScenarioId.isEmpty()) {
            baseScenarioId = sourceIt->baseScenarioId;
        } else if (source.role == safecrowd::domain::ScenarioRole::Baseline) {
            baseScenarioId = QString::fromStdString(source.scenarioId);
        }
    }
    if (baseScenarioId.isEmpty()) {
        const auto baselineIndex = explicitBaselineResultIndex();
        if (baselineIndex >= 0 && baselineIndex < static_cast<int>(results_.size())) {
            baseScenarioId = QString::fromStdString(results_[static_cast<std::size_t>(baselineIndex)].scenario.scenarioId);
        }
    }

    if (!baseScenarioId.isEmpty()) {
        const auto baseId = baseScenarioId.toStdString();
        const auto baselineIt = std::find_if(initial.scenarios.begin(), initial.scenarios.end(), [&](const auto& scenario) {
            return scenario.draft.scenarioId == baseId;
        });
        if (baselineIt != initial.scenarios.end()) {
            recommendedScenario.variationDiffKeys =
                safecrowd::domain::computeScenarioDiffKeys(baselineIt->draft, recommendedScenario);
        }
    }

    auto state = scenarioStateFromDraft(recommendedScenario, layout_);
    state.baseScenarioId = baseScenarioId;
    state.stagedForRun = false;
    initial.scenarios.push_back(std::move(state));
    initial.currentScenarioIndex = static_cast<int>(initial.scenarios.size()) - 1;
    initial.rightPanelMode = ScenarioAuthoringWidget::RightPanelMode::Scenario;
    initial.inspectorPanelVisible = true;
    initial.scenarioPanelVisible = true;
    showAuthoring(std::move(initial));
}

void ScenarioBatchResultWidget::pauseReplay() {
    if (replayTimer_ != nullptr) {
        replayTimer_->stop();
    }
    if (playButton_ != nullptr) {
        playButton_->setText("Play");
    }
}

void ScenarioBatchResultWidget::advanceReplay() {
    if (replayFrames_.empty()) {
        pauseReplay();
        return;
    }
    if (replayFrameIndex_ >= static_cast<int>(replayFrames_.size()) - 1) {
        pauseReplay();
        return;
    }
    applyReplayFrame(replayFrameIndex_ + 1);
}

void ScenarioBatchResultWidget::applyReplayFrame(int frameIndex) {
    if (replayFrames_.empty()) {
        if (replayTimeLabel_ != nullptr) {
            replayTimeLabel_->setText("No replay");
        }
        return;
    }

    replayFrameIndex_ = std::clamp(frameIndex, 0, static_cast<int>(replayFrames_.size()) - 1);
    applyReplayFrameData(replayFrames_[static_cast<std::size_t>(replayFrameIndex_)], replayFrameIndex_);
}

void ScenarioBatchResultWidget::applyReplayFrameData(const safecrowd::domain::SimulationFrame& frame, int sliderIndex) {
    if (results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
        return;
    }
    replayFrameIndex_ = replayFrames_.empty()
        ? 0
        : std::clamp(sliderIndex, 0, static_cast<int>(replayFrames_.size()) - 1);

    if (canvas_ != nullptr) {
        canvas_->setFrame(frame);
    }
    if (replaySlider_ != nullptr && replaySlider_->value() != replayFrameIndex_) {
        const QSignalBlocker blocker(replaySlider_);
        replaySlider_->setValue(replayFrameIndex_);
    }
    if (remainingChart_ != nullptr) {
        static_cast<ComparisonGraphWidget*>(remainingChart_)->setCurrentTimeSeconds(frame.elapsedSeconds);
    }
    if (exitsChart_ != nullptr) {
        static_cast<ComparisonGraphWidget*>(exitsChart_)->setCurrentTimeSeconds(frame.elapsedSeconds);
    }
    if (replayTimeLabel_ != nullptr) {
        const auto totalSeconds = replayFrames_.empty() ? frame.elapsedSeconds : replayFrames_.back().elapsedSeconds;
        replayTimeLabel_->setText(QString("%1 / %2 sec")
            .arg(frame.elapsedSeconds, 0, 'f', 1)
            .arg(totalSeconds, 0, 'f', 1));
    }
}

void ScenarioBatchResultWidget::applySelectedResultStaticCanvasState() {
    if (canvas_ == nullptr
        || results_.empty()
        || currentResultIndex_ < 0
        || currentResultIndex_ >= static_cast<int>(results_.size())) {
        return;
    }

    const auto& result = results_[static_cast<std::size_t>(currentResultIndex_)];
    canvas_->setConnectionBlocks(result.scenario.control.connectionBlocks);
    canvas_->setEnvironmentHazards(result.scenario.environment.hazards);
    canvas_->setHotspotOverlay(result.risk.hotspots);
    canvas_->setBottleneckOverlay(result.risk.bottlenecks);
    canvas_->setOperationalConflictOverlay(
        result.risk.operationalConflictCells,
        result.risk.operationalConflictConnections);
    canvas_->setDensityOverlay(
        result.artifacts.densitySummary.peakField.cells.empty()
            ? result.artifacts.densitySummary.peakCells
            : result.artifacts.densitySummary.peakField.cells,
        result.artifacts.densitySummary.highDensityThresholdPeoplePerSquareMeter);
    canvas_->setPressureOverlay(
        result.artifacts.pressureSummary.peakField.cells.empty()
            ? result.artifacts.pressureSummary.peakCells
            : result.artifacts.pressureSummary.peakField.cells,
        std::max(
            result.artifacts.pressureSummary.hotspotScoreThreshold,
            result.artifacts.pressureSummary.peakPressureScore));
    applyOverlayModeToCanvas();
}

void ScenarioBatchResultWidget::applyOverlayModeToCanvas() {
    if (canvas_ == nullptr) {
        return;
    }
    switch (overlayMode_) {
    case OverlayMode::Pressure:
        canvas_->setResultOverlayMode(ResultOverlayMode::Pressure);
        break;
    case OverlayMode::Hotspots:
        canvas_->setResultOverlayMode(ResultOverlayMode::Hotspots);
        break;
    case OverlayMode::Bottlenecks:
        canvas_->setResultOverlayMode(ResultOverlayMode::Bottlenecks);
        break;
    case OverlayMode::OperationalConflicts:
        canvas_->setResultOverlayMode(ResultOverlayMode::OperationalConflicts);
        break;
    case OverlayMode::None:
        canvas_->setResultOverlayMode(ResultOverlayMode::None);
        break;
    case OverlayMode::Density:
    default:
        canvas_->setResultOverlayMode(ResultOverlayMode::Density);
        break;
    }
}

int ScenarioBatchResultWidget::nearestReplayFrameIndex(double seconds) const {
    if (replayFrames_.empty()) {
        return 0;
    }
    const auto closest = std::min_element(replayFrames_.begin(), replayFrames_.end(), [seconds](const auto& lhs, const auto& rhs) {
        return std::abs(lhs.elapsedSeconds - seconds) < std::abs(rhs.elapsedSeconds - seconds);
    });
    return static_cast<int>(std::distance(replayFrames_.begin(), closest));
}

void ScenarioBatchResultWidget::showReplayFrame(const safecrowd::domain::SimulationFrame& frame) {
    pauseReplay();
    const auto index = nearestReplayFrameIndex(frame.elapsedSeconds);
    applyReplayFrameData(frame, index);
}

void ScenarioBatchResultWidget::showClosestReplayFrameAtSeconds(double seconds) {
    if (replayFrames_.empty()) {
        return;
    }
    pauseReplay();
    applyReplayFrame(nearestReplayFrameIndex(seconds));
}

void ScenarioBatchResultWidget::seekToTimingMarkerSeconds(double seconds) {
    if (results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
        return;
    }

    const auto& timing = results_[static_cast<std::size_t>(currentResultIndex_)].artifacts.timingSummary;
    if (timing.t90Seconds.has_value()
        && std::abs(seconds - *timing.t90Seconds) <= 1e-6) {
        if (timing.t90Frame.has_value()) {
            showReplayFrame(*timing.t90Frame);
        } else {
            showClosestReplayFrameAtSeconds(*timing.t90Seconds);
        }
        return;
    }
    if (timing.t95Seconds.has_value()
        && std::abs(seconds - *timing.t95Seconds) <= 1e-6) {
        if (timing.t95Frame.has_value()) {
            showReplayFrame(*timing.t95Frame);
        } else {
            showClosestReplayFrameAtSeconds(*timing.t95Seconds);
        }
        return;
    }

    showClosestReplayFrameAtSeconds(seconds);
}

void ScenarioBatchResultWidget::setOverlayMode(OverlayMode mode) {
    overlayMode_ = mode;
    if (overlayCombo_ != nullptr) {
        const auto comboIndex = overlayCombo_->findData(static_cast<int>(mode));
        if (comboIndex >= 0 && overlayCombo_->currentIndex() != comboIndex) {
            const QSignalBlocker blocker(overlayCombo_);
            overlayCombo_->setCurrentIndex(comboIndex);
        }
    }
    applyOverlayModeToCanvas();
}

void ScenarioBatchResultWidget::loadReplayForSelectedResult() {
    pauseReplay();
    if (results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
        replayFrames_.clear();
        return;
    }
    applySelectedResultStaticCanvasState();
    replayFrames_ = replayFramesForResult(results_[static_cast<std::size_t>(currentResultIndex_)]);
    replayFrameIndex_ = replayFrames_.empty() ? 0 : static_cast<int>(replayFrames_.size()) - 1;
    if (replaySlider_ != nullptr) {
        replaySlider_->setEnabled(replayFrames_.size() > 1);
        replaySlider_->setRange(0, replayFrames_.empty() ? 0 : static_cast<int>(replayFrames_.size()) - 1);
        const QSignalBlocker blocker(replaySlider_);
        replaySlider_->setValue(replayFrameIndex_);
    }
    if (playButton_ != nullptr) {
        playButton_->setEnabled(replayFrames_.size() > 1);
        playButton_->setText("Play");
    }
    applyReplayFrame(replayFrameIndex_);
}

void ScenarioBatchResultWidget::refreshComparisonSelection() {
    selectedCompareIndices_.clear();
    for (int index = 0; index < static_cast<int>(compareCheckBoxes_.size()); ++index) {
        if (compareCheckBoxes_[static_cast<std::size_t>(index)] != nullptr
            && compareCheckBoxes_[static_cast<std::size_t>(index)]->isChecked()) {
            selectedCompareIndices_.push_back(index);
        }
    }
    if (remainingChart_ != nullptr) {
        static_cast<ComparisonGraphWidget*>(remainingChart_)->setResults(results_, selectedCompareIndices_, currentResultIndex_);
    }
    if (exitsChart_ != nullptr) {
        static_cast<ComparisonGraphWidget*>(exitsChart_)->setResults(results_, selectedCompareIndices_, currentResultIndex_);
    }
    refreshComparisonCountLabel();
    refreshExposureComparisonTable();
    refreshPressureComparisonTable();
}

void ScenarioBatchResultWidget::refreshComparisonCountLabel() {
    if (comparisonCountLabel_ == nullptr) {
        return;
    }
    comparisonCountLabel_->setText(QString("Comparing %1 / %2 scenarios")
        .arg(static_cast<int>(selectedCompareIndices_.size()))
        .arg(static_cast<int>(results_.size())));
}

void ScenarioBatchResultWidget::refreshExposureComparisonTable() {
    if (exposureTable_ == nullptr) {
        return;
    }

    const std::vector<int> visibleIndices = selectedCompareIndices_;
    exposureTable_->setRowCount(static_cast<int>(visibleIndices.size()));
    for (int row = 0; row < static_cast<int>(visibleIndices.size()); ++row) {
        const auto index = visibleIndices[static_cast<std::size_t>(row)];
        if (index < 0 || index >= static_cast<int>(results_.size())) {
            continue;
        }

        const auto& result = results_[static_cast<std::size_t>(index)];
        const auto& summary = result.artifacts.hazardExposureSummary;
        const bool emphasized = index == currentResultIndex_;
        const auto* peakHazard = peakHazardExposureMetric(summary);
        exposureTable_->setItem(row, 0, tableItem(QString::fromStdString(result.scenario.name), emphasized));
        exposureTable_->setItem(row, 1, tableItem(formatExposureSeconds(totalHazardExposureSeconds(summary)), emphasized));
        exposureTable_->setItem(
            row,
            2,
            tableItem(formatExposureSeconds(hazardExposureSecondsForKind(
                summary,
                safecrowd::domain::EnvironmentHazardKind::Fire)), emphasized));
        exposureTable_->setItem(
            row,
            3,
            tableItem(formatExposureSeconds(hazardExposureSecondsForKind(
                summary,
                safecrowd::domain::EnvironmentHazardKind::Smoke)), emphasized));
        exposureTable_->setItem(
            row,
            4,
            tableItem(
                peakHazard == nullptr
                    ? QString("0 people")
                    : QString("%1 people").arg(static_cast<int>(peakHazard->peakExposedAgentCount)),
                emphasized));
        exposureTable_->setItem(
            row,
            5,
            tableItem(peakHazard == nullptr ? QString("Pending") : formatSeconds(peakHazard->peakAtSeconds), emphasized));
    }
    exposureTable_->resizeRowsToContents();
}

void ScenarioBatchResultWidget::refreshPressureComparisonTable() {
    if (pressureTable_ == nullptr) {
        return;
    }

    std::vector<int> visibleIndices = selectedCompareIndices_;

    pressureTable_->setRowCount(static_cast<int>(visibleIndices.size()));
    for (int row = 0; row < static_cast<int>(visibleIndices.size()); ++row) {
        const auto index = visibleIndices[static_cast<std::size_t>(row)];
        if (index < 0 || index >= static_cast<int>(results_.size())) {
            continue;
        }

        const auto& result = results_[static_cast<std::size_t>(index)];
        const auto& summary = result.artifacts.pressureSummary;
        const bool emphasized = index == currentResultIndex_;
        pressureTable_->setItem(row, 0, tableItem(QString::fromStdString(result.scenario.name), emphasized));
        pressureTable_->setItem(row, 1, tableItem(formatPressureScore(summary.peakPressureScore), emphasized));
        pressureTable_->setItem(
            row,
            2,
            tableItem(
                QString("%1 / %2")
                    .arg(static_cast<int>(summary.peakExposedAgentCount))
                    .arg(static_cast<int>(summary.peakCriticalAgentCount)),
                emphasized));
        pressureTable_->setItem(row, 3, tableItem(QString::number(static_cast<int>(summary.peakHotspots.size())), emphasized));
        pressureTable_->setItem(row, 4, tableItem(QString::number(static_cast<int>(summary.criticalEvents.size())), emphasized));
        pressureTable_->setItem(row, 5, tableItem(formatSeconds(summary.peakAtSeconds), emphasized));
    }
    pressureTable_->resizeRowsToContents();
}

void ScenarioBatchResultWidget::selectAllComparisonScenarios() {
    std::vector<int> indices;
    indices.reserve(results_.size());
    for (int index = 0; index < static_cast<int>(results_.size()); ++index) {
        indices.push_back(index);
    }
    setComparisonSelection(std::move(indices));
}

void ScenarioBatchResultWidget::selectBaselineAndCurrentComparisonScenarios() {
    std::vector<int> indices;
    const auto baselineIndex = baselineResultIndex();
    if (baselineIndex >= 0) {
        indices.push_back(baselineIndex);
    }
    if (currentResultIndex_ >= 0
        && currentResultIndex_ < static_cast<int>(results_.size())
        && std::find(indices.begin(), indices.end(), currentResultIndex_) == indices.end()) {
        indices.push_back(currentResultIndex_);
    }
    setComparisonSelection(std::move(indices));
}

void ScenarioBatchResultWidget::clearComparisonScenarios() {
    setComparisonSelection({});
}

void ScenarioBatchResultWidget::setComparisonSelection(std::vector<int> indices) {
    selectedCompareIndices_.clear();
    selectedCompareIndices_.reserve(indices.size());
    for (const auto index : indices) {
        if (index < 0 || index >= static_cast<int>(results_.size())) {
            continue;
        }
        if (std::find(selectedCompareIndices_.begin(), selectedCompareIndices_.end(), index) == selectedCompareIndices_.end()) {
            selectedCompareIndices_.push_back(index);
        }
    }

    syncCompareCheckBoxes();
    if (remainingChart_ != nullptr) {
        static_cast<ComparisonGraphWidget*>(remainingChart_)->setResults(results_, selectedCompareIndices_, currentResultIndex_);
    }
    if (exitsChart_ != nullptr) {
        static_cast<ComparisonGraphWidget*>(exitsChart_)->setResults(results_, selectedCompareIndices_, currentResultIndex_);
    }
    refreshComparisonCountLabel();
    refreshExposureComparisonTable();
    refreshPressureComparisonTable();
}

void ScenarioBatchResultWidget::syncCompareCheckBoxes() {
    for (int index = 0; index < static_cast<int>(compareCheckBoxes_.size()); ++index) {
        auto* checkbox = compareCheckBoxes_[static_cast<std::size_t>(index)];
        if (checkbox == nullptr) {
            continue;
        }
        const QSignalBlocker blocker(checkbox);
        checkbox->setChecked(std::find(selectedCompareIndices_.begin(), selectedCompareIndices_.end(), index)
            != selectedCompareIndices_.end());
    }
}

void ScenarioBatchResultWidget::setRecommendationScenarioSelected(int index, bool selected) {
    if (index < 0 || index >= static_cast<int>(results_.size())) {
        return;
    }
    const auto it = std::find(selectedRecommendationIndices_.begin(), selectedRecommendationIndices_.end(), index);
    if (selected && it == selectedRecommendationIndices_.end()) {
        selectedRecommendationIndices_.push_back(index);
        std::sort(selectedRecommendationIndices_.begin(), selectedRecommendationIndices_.end());
    } else if (!selected && it != selectedRecommendationIndices_.end()) {
        selectedRecommendationIndices_.erase(it);
    }
    QTimer::singleShot(0, this, [this]() {
        refreshResultNavigationPanel();
    });
}

QWidget* ScenarioBatchResultWidget::createBatchRecommendationNavigationPanel() {
    auto* panel = new QWidget(shell_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* header = createLabel("Recommendations", panel, ui::FontRole::SectionTitle);
    header->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(header);
    auto* caption = createLabel("Select scenarios to inspect their operational alternatives.", panel, ui::FontRole::Caption);
    caption->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(caption);

    auto* selector = new QFrame(panel);
    selector->setStyleSheet(ui::panelStyleSheet());
    auto* selectorLayout = new QVBoxLayout(selector);
    selectorLayout->setContentsMargins(14, 12, 14, 12);
    selectorLayout->setSpacing(8);
    selectorLayout->addWidget(createLabel("Scenarios", selector, ui::FontRole::Body));
    for (int index = 0; index < static_cast<int>(results_.size()); ++index) {
        const auto& result = results_[static_cast<std::size_t>(index)];
        auto* checkbox = new QCheckBox(
            QString("%1\n%2  -  %3")
                .arg(QString::fromStdString(result.scenario.name))
                .arg(scenarioRoleLabel(result.scenario.role))
                .arg(formatSeconds(finalSeconds(result))),
            selector);
        checkbox->setFont(ui::font(ui::FontRole::Caption));
        checkbox->setChecked(std::find(
            selectedRecommendationIndices_.begin(),
            selectedRecommendationIndices_.end(),
            index) != selectedRecommendationIndices_.end());
        checkbox->setStyleSheet(
            "QCheckBox { color: #344256; spacing: 8px; }"
            "QCheckBox::indicator { width: 16px; height: 16px; }");
        selectorLayout->addWidget(checkbox);
        connect(checkbox, &QCheckBox::toggled, this, [this, index](bool checked) {
            setRecommendationScenarioSelected(index, checked);
        });
    }
    layout->addWidget(selector);

    auto* scrollArea = new QScrollArea(panel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui::polishScrollArea(scrollArea);

    auto* content = new QWidget(scrollArea);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 10, 0);
    contentLayout->setSpacing(12);

    if (selectedRecommendationIndices_.empty()) {
        auto* empty = createLabel("Select at least one scenario.", content, ui::FontRole::Caption);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        contentLayout->addWidget(empty);
    }

    const auto baselineIndex = explicitBaselineResultIndex();
    const auto* baselineScenario =
        baselineIndex >= 0 && baselineIndex < static_cast<int>(results_.size())
            ? &results_[static_cast<std::size_t>(baselineIndex)].scenario
            : nullptr;
    const safecrowd::domain::AlternativeRecommendationService service;
    for (const auto index : selectedRecommendationIndices_) {
        if (index < 0 || index >= static_cast<int>(results_.size())) {
            continue;
        }
        const auto& result = results_[static_cast<std::size_t>(index)];
        const auto recommendation = service.recommend(
            layout_,
            result.scenario,
            result.risk,
            result.artifacts,
            baselineScenario,
            &result.frame);

        auto* scenarioHeader = new QWidget(content);
        auto* scenarioHeaderLayout = new QVBoxLayout(scenarioHeader);
        scenarioHeaderLayout->setContentsMargins(0, 0, 0, 0);
        scenarioHeaderLayout->setSpacing(2);
        auto* title = createLabel(QString::fromStdString(result.scenario.name), scenarioHeader, ui::FontRole::Body);
        title->setStyleSheet("QLabel { color: #16202b; font-weight: 600; }");
        scenarioHeaderLayout->addWidget(title);
        auto* meta = createLabel(
            QString("%1  -  %2  -  %3 recommendation%4")
                .arg(scenarioRoleLabel(result.scenario.role))
                .arg(formatSeconds(finalSeconds(result)))
                .arg(static_cast<int>(recommendation.candidates.size()))
                .arg(recommendation.candidates.size() == 1 ? "" : "s"),
            scenarioHeader,
            ui::FontRole::Caption);
        meta->setStyleSheet(ui::subtleTextStyleSheet());
        scenarioHeaderLayout->addWidget(meta);
        contentLayout->addWidget(scenarioHeader);

        if (recommendation.candidates.empty()) {
            const auto message = recommendation.blockingReasons.empty()
                ? QString("No actionable recommendation for this scenario.")
                : QString::fromStdString(recommendation.blockingReasons.front());
            auto* empty = createLabel(message, content, ui::FontRole::Caption);
            empty->setStyleSheet(ui::mutedTextStyleSheet());
            contentLayout->addWidget(empty);
            continue;
        }

        for (const auto& candidate : recommendation.candidates) {
            auto* section = new QFrame(content);
            section->setStyleSheet(ui::panelStyleSheet());
            section->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
            section->setMinimumWidth(0);
            auto* sectionLayout = new QVBoxLayout(section);
            sectionLayout->setContentsMargins(14, 12, 14, 12);
            sectionLayout->setSpacing(6);

            auto* candidateTitle = createLabel(QString::fromStdString(candidate.title), section, ui::FontRole::Body);
            candidateTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            candidateTitle->setStyleSheet("QLabel { color: #16202b; font-weight: 600; }");
            sectionLayout->addWidget(candidateTitle);

            auto* summary = createLabel(QString::fromStdString(candidate.summary), section, ui::FontRole::Caption);
            summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            summary->setStyleSheet(ui::mutedTextStyleSheet());
            sectionLayout->addWidget(summary);

            for (const auto& item : candidate.evidence) {
                if (!shouldShowRecommendationEvidence(item)) {
                    continue;
                }
                auto* evidenceLabel = createLabel(
                    QString("%1: %2")
                        .arg(QString::fromStdString(item.label),
                             QString::fromStdString(item.value)),
                    section,
                    ui::FontRole::Caption);
                evidenceLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                evidenceLabel->setStyleSheet(ui::subtleTextStyleSheet());
                evidenceLabel->setToolTip(QString::fromStdString(item.source));
                sectionLayout->addWidget(evidenceLabel);
            }

            auto* button = new QPushButton("Create Scenario", section);
            button->setFont(ui::font(ui::FontRole::Body));
            button->setStyleSheet(ui::secondaryButtonStyleSheet());
            button->setCursor(Qt::PointingHandCursor);
            button->setMinimumWidth(0);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            button->setToolTip("Create recommended scenario");
            sectionLayout->addWidget(button);
            connect(button, &QPushButton::clicked, section, [this, index, scenario = candidate.recommendedScenario]() {
                currentResultIndex_ = index;
                createRecommendedScenario(scenario);
            });

            contentLayout->addWidget(section);
        }
    }

    contentLayout->addStretch(1);
    scrollArea->setWidget(content);
    layout->addWidget(scrollArea, 1);
    return panel;
}

void ScenarioBatchResultWidget::refreshResultNavigationPanel() {
    if (shell_ == nullptr || results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
        return;
    }

    shell_->setNavigationTabs(
        scenarioResultNavigationTabs(),
        scenarioResultNavigationTabId(resultNavigationView_),
        [this](const QString& tabId) {
            resultNavigationView_ = scenarioResultNavigationViewFromTabId(tabId);
            refreshResultNavigationPanel();
        });

    const auto& result = results_[static_cast<std::size_t>(currentResultIndex_)];
    if (resultNavigationView_ == ScenarioResultNavigationView::Recommendations) {
        shell_->setNavigationPanel(createBatchRecommendationNavigationPanel());
        return;
    }

    auto bottleneckFocusHandler = [this](std::size_t index) {
        if (results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
            return;
        }
        const auto& selected = results_[static_cast<std::size_t>(currentResultIndex_)];
        if (index < selected.risk.bottlenecks.size()) {
            setOverlayMode(OverlayMode::Bottlenecks);
            const auto& bottleneck = selected.risk.bottlenecks[index];
            if (bottleneck.detectionFrame.has_value()) {
                showReplayFrame(*bottleneck.detectionFrame);
            } else if (bottleneck.detectedAtSeconds.has_value()) {
                showClosestReplayFrameAtSeconds(*bottleneck.detectedAtSeconds);
            }
        }
        if (canvas_ != nullptr) {
            canvas_->focusBottleneck(index);
        }
    };
    auto operationalConflictCellFocusHandler = [this](std::size_t index) {
        if (results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
            return;
        }
        const auto& selected = results_[static_cast<std::size_t>(currentResultIndex_)];
        if (index < selected.risk.operationalConflictCells.size()) {
            setOverlayMode(OverlayMode::OperationalConflicts);
            const auto& cell = selected.risk.operationalConflictCells[index];
            if (cell.detectionFrame.has_value()) {
                showReplayFrame(*cell.detectionFrame);
            } else if (cell.detectedAtSeconds.has_value()) {
                showClosestReplayFrameAtSeconds(*cell.detectedAtSeconds);
            }
        }
        if (canvas_ != nullptr) {
            canvas_->focusOperationalConflictCell(index);
        }
    };
    auto operationalConflictConnectionFocusHandler = [this](std::size_t index) {
        if (results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
            return;
        }
        const auto& selected = results_[static_cast<std::size_t>(currentResultIndex_)];
        if (index < selected.risk.operationalConflictConnections.size()) {
            setOverlayMode(OverlayMode::OperationalConflicts);
            const auto& connection = selected.risk.operationalConflictConnections[index];
            if (connection.detectionFrame.has_value()) {
                showReplayFrame(*connection.detectionFrame);
            } else if (connection.detectedAtSeconds.has_value()) {
                showClosestReplayFrameAtSeconds(*connection.detectedAtSeconds);
            }
        }
        if (canvas_ != nullptr) {
            canvas_->focusOperationalConflictConnection(index);
        }
    };
    auto hotspotFocusHandler = [this](std::size_t index) {
        if (results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
            return;
        }
        const auto& selected = results_[static_cast<std::size_t>(currentResultIndex_)];
        if (index < selected.risk.hotspots.size()) {
            setOverlayMode(OverlayMode::Hotspots);
            const auto& hotspot = selected.risk.hotspots[index];
            if (hotspot.detectionFrame.has_value()) {
                showReplayFrame(*hotspot.detectionFrame);
            } else if (hotspot.detectedAtSeconds.has_value()) {
                showClosestReplayFrameAtSeconds(*hotspot.detectedAtSeconds);
            }
        }
        if (canvas_ != nullptr) {
            canvas_->focusHotspot(index);
        }
    };

    shell_->setNavigationPanel(createScenarioResultNavigationPanel(
        resultNavigationView_,
        result.risk,
        result.artifacts,
        std::move(bottleneckFocusHandler),
        std::move(operationalConflictCellFocusHandler),
        std::move(operationalConflictConnectionFocusHandler),
        std::move(hotspotFocusHandler),
        shell_));
}

void ScenarioBatchResultWidget::refreshSelectedResult() {
    if (results_.empty() || currentResultIndex_ < 0 || currentResultIndex_ >= static_cast<int>(results_.size())) {
        return;
    }
    const auto& result = results_[currentResultIndex_];
    const auto baselineIndex = baselineResultIndex();

    if (displayScenarioCombo_ != nullptr && displayScenarioCombo_->currentIndex() != currentResultIndex_) {
        const QSignalBlocker blocker(displayScenarioCombo_);
        displayScenarioCombo_->setCurrentIndex(currentResultIndex_);
    }

    loadReplayForSelectedResult();

    if (remainingChart_ != nullptr) {
        static_cast<ComparisonGraphWidget*>(remainingChart_)->setResults(results_, selectedCompareIndices_, currentResultIndex_);
    }
    if (exitsChart_ != nullptr) {
        static_cast<ComparisonGraphWidget*>(exitsChart_)->setResults(results_, selectedCompareIndices_, currentResultIndex_);
    }
    refreshExposureComparisonTable();
    refreshPressureComparisonTable();
    refreshResultNavigationPanel();
    if (detailLabel_ != nullptr) {
        const auto selectedFinalSeconds = finalSeconds(result);
        const auto deltaText = baselineIndex >= 0
            ? (currentResultIndex_ == baselineIndex
                ? QString("Baseline")
                : formatDeltaSeconds(selectedFinalSeconds - finalSeconds(results_[static_cast<std::size_t>(baselineIndex)])))
            : QString("No baseline");
        const auto& exposureSummary = result.artifacts.hazardExposureSummary;
        const auto* peakHazard = peakHazardExposureMetric(exposureSummary);
        detailLabel_->setText(QString("%1 (%2)\nFinal: %3\nDelta vs baseline: %4\nEvacuated: %5 / %6 (%7)\nRisk: %8\nHotspots: %9\nBottlenecks: %10\nHazard exposure: %11 (fire %12 / smoke %13)\nHazard peak: %14 at %15; score %16\nPressure hotspots: %17\nCritical pressure: %18 agents / %19 events\nPeak pressure: %20 at %21\nT90 / T95: %22 / %23")
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
            .arg(formatExposureSeconds(totalHazardExposureSeconds(exposureSummary)))
            .arg(formatExposureSeconds(hazardExposureSecondsForKind(
                exposureSummary,
                safecrowd::domain::EnvironmentHazardKind::Fire)))
            .arg(formatExposureSeconds(hazardExposureSecondsForKind(
                exposureSummary,
                safecrowd::domain::EnvironmentHazardKind::Smoke)))
            .arg(peakHazard == nullptr
                ? QString("0 people")
                : QString("%1 people").arg(static_cast<int>(peakHazard->peakExposedAgentCount)))
            .arg(peakHazard == nullptr ? QString("Pending") : formatSeconds(peakHazard->peakAtSeconds))
            .arg(formatExposureScore(exposureSummary.totalExposureScore))
            .arg(static_cast<int>(result.artifacts.pressureSummary.peakHotspots.size()))
            .arg(static_cast<int>(result.artifacts.pressureSummary.peakCriticalAgentCount))
            .arg(static_cast<int>(result.artifacts.pressureSummary.criticalEvents.size()))
            .arg(formatPressureScore(result.artifacts.pressureSummary.peakPressureScore))
            .arg(formatSeconds(result.artifacts.pressureSummary.peakAtSeconds))
            .arg(formatSeconds(result.artifacts.timingSummary.t90Seconds))
            .arg(formatSeconds(result.artifacts.timingSummary.t95Seconds))
            + QString("\nOperational conflict: %1 score / %2 connections / HHI %3")
                .arg(result.artifacts.operationalConflictSummary.peakConflictScore, 0, 'f', 2)
                .arg(static_cast<int>(result.artifacts.operationalConflictSummary.conflictConnectionCount))
                .arg(result.artifacts.operationalConflictSummary.connectionConcentrationIndex, 0, 'f', 2)
            + QString("\nConflict exposure: %1 agent-sec  |  Top connection: %2")
                .arg(result.artifacts.operationalConflictSummary.totalConflictExposureAgentSeconds, 0, 'f', 1)
                .arg((result.artifacts.operationalConflictSummary.peakConflictScore <= 0.0
                        && result.artifacts.operationalConflictSummary.totalConflictExposureAgentSeconds <= 0.0
                        && result.artifacts.operationalConflictSummary.conflictConnectionCount == 0)
                    ? QString("None")
                    : QString::fromStdString(
                        result.artifacts.operationalConflictSummary.topConflictConnectionLabel.empty()
                            ? result.artifacts.operationalConflictSummary.topConflictConnectionId
                            : result.artifacts.operationalConflictSummary.topConflictConnectionLabel)));
    }
}

int ScenarioBatchResultWidget::explicitBaselineResultIndex() const noexcept {
    for (int index = 0; index < static_cast<int>(results_.size()); ++index) {
        if (results_[static_cast<std::size_t>(index)].scenario.role == safecrowd::domain::ScenarioRole::Baseline) {
            return index;
        }
    }
    return -1;
}

int ScenarioBatchResultWidget::baselineResultIndex() const noexcept {
    const auto explicitIndex = explicitBaselineResultIndex();
    if (explicitIndex >= 0) {
        return explicitIndex;
    }
    return results_.empty() ? -1 : 0;
}

void ScenarioBatchResultWidget::rerunBatch() {
    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }
    pauseReplay();
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
