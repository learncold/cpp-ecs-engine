#include "application/ScenarioResultWidget.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <utility>

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointer>
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
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "application/ScenarioAuthoringWidget.h"
#include "application/ScenarioCanvasWidget.h"
#include "application/ScenarioResultNavigation.h"
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

QString formatPressureScore(double score) {
    return QString::number(score, 'f', 1);
}

QString simplifyLocationLabel(QString text) {
    text = text.simplified();
    if (text.isEmpty()) {
        return text;
    }

    QStringList words = text.split(' ', Qt::SkipEmptyParts);
    static const QStringList genericSuffixes{
        "Room",
        "Area",
        "Zone",
        "Passage",
        "Corridor",
        "Hallway",
        "Hall",
        "Lobby",
        "Section",
    };

    while (words.size() > 1 && genericSuffixes.contains(words.back(), Qt::CaseInsensitive)) {
        words.removeLast();
    }
    return words.join(' ');
}

QString compactWords(QString text, int maxCharactersPerSide) {
    text = text.simplified();
    if (text.size() <= maxCharactersPerSide) {
        return text;
    }

    const auto words = text.split(' ', Qt::SkipEmptyParts);
    if (words.size() >= 2) {
        QString compact = words.front();
        for (int index = 1; index < words.size(); ++index) {
            const auto candidate = compact + ' ' + words[index];
            if (candidate.size() > maxCharactersPerSide) {
                break;
            }
            compact = candidate;
        }
        if (compact.size() <= maxCharactersPerSide) {
            return compact;
        }
    }

    if (maxCharactersPerSide <= 3) {
        return text.left(std::max(1, maxCharactersPerSide));
    }
    return text.left(maxCharactersPerSide - 3).trimmed() + QStringLiteral("...");
}

QString compactBottleneckLabel(QString label) {
    label = label.simplified();
    if (label.isEmpty()) {
        return label;
    }

    constexpr int kCompactLabelLimit = 18;
    if (label.size() <= kCompactLabelLimit) {
        return label.replace(QStringLiteral("->"), QStringLiteral(">"));
    }

    const auto segments = label.split(QStringLiteral("->"), Qt::SkipEmptyParts);
    if (segments.size() != 2) {
        const auto simplified = simplifyLocationLabel(label);
        return compactWords(simplified, kCompactLabelLimit);
    }

    auto from = simplifyLocationLabel(segments[0]);
    auto to = simplifyLocationLabel(segments[1]);
    QString compact = QStringLiteral("%1 > %2").arg(from, to);
    if (compact.size() <= kCompactLabelLimit) {
        return compact;
    }

    const int sideBudget = std::max(5, (kCompactLabelLimit - 3) / 2);
    from = compactWords(from, sideBudget);
    to = compactWords(to, sideBudget);
    compact = QStringLiteral("%1 > %2").arg(from, to);
    if (compact.size() <= kCompactLabelLimit) {
        return compact;
    }

    return QStringLiteral("%1 > %2")
        .arg(compactWords(from, sideBudget - 1))
        .arg(compactWords(to, sideBudget - 1));
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
        setMouseTracking(true);
    }

    void setCurrentTimeSeconds(std::optional<double> seconds) {
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
    QRectF plotRect() const {
        return QRectF(rect()).adjusted(34, 18, -14, -28);
    }

    double maxTimeSeconds() const {
        if (artifacts_.evacuationProgress.empty()) {
            return 1.0;
        }
        const auto maxTimeIt = std::max_element(
            artifacts_.evacuationProgress.begin(),
            artifacts_.evacuationProgress.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.timeSeconds < rhs.timeSeconds;
            });
        return std::max(1.0, maxTimeIt->timeSeconds);
    }

    QRectF markerHitRegion(const QRectF& plot, double maxTime, double seconds) const {
        const auto x = plot.left() + (std::clamp(seconds / maxTime, 0.0, 1.0) * plot.width());
        const QRectF lineHit(x - 6.0, plot.top(), 12.0, plot.height());
        const QRectF labelHit(x - 4.0, plot.top() - 2.0, 70.0, 22.0);
        return lineHit.united(labelHit);
    }

    std::optional<double> timingMarkerHitSeconds(const QPointF& position) const {
        const QRectF plot = plotRect();
        if (!plot.contains(position)) {
            return std::nullopt;
        }

        const auto maxTime = maxTimeSeconds();
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
            const auto x = plot.left() + (std::clamp(*markerSeconds / maxTime, 0.0, 1.0) * plot.width());
            const auto distance = std::abs(position.x() - x);
            if (!best.has_value() || distance < best->distance) {
                best = Candidate{.seconds = *markerSeconds, .distance = distance};
            }
        };

        consider(artifacts_.timingSummary.t90Seconds);
        consider(artifacts_.timingSummary.t95Seconds);
        if (!best.has_value()) {
            return std::nullopt;
        }
        return best->seconds;
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
    std::function<void(double)> timingMarkerActivatedHandler_{};
};

class DensityLegendWidget final : public QWidget {
public:
    explicit DensityLegendWidget(
        const safecrowd::domain::DensitySummary& summary,
        QWidget* parent = nullptr)
        : QWidget(parent),
          threshold_(summary.highDensityThresholdPeoplePerSquareMeter),
          peakDensity_(summary.peakDensityPeoplePerSquareMeter) {
        setFixedSize(300, 34);
        setToolTip("Density heatmap scale uses the high-density threshold in people per square meter.");
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
        if (peakDensity_ > 0.0 && threshold_ > 0.0) {
            const auto peakX = ramp.left()
                + (std::clamp(peakDensity_ / threshold_, 0.0, 1.0) * ramp.width());
            painter.setPen(QPen(QColor("#405063"), 1));
            painter.drawLine(QPointF(peakX, ramp.top() - 2), QPointF(peakX, ramp.bottom() + 2));
            painter.setPen(QColor("#687789"));
        }
        painter.drawText(QRectF(0, 16, 40, 16), Qt::AlignLeft | Qt::AlignVCenter, "0");
        painter.drawText(
            QRectF(54, 16, 140, 16),
            Qt::AlignCenter,
            QString("High %1+ /m2").arg(threshold_, 0, 'f', 1));
        painter.drawText(
            QRectF(width() - 98, 16, 98, 16),
            Qt::AlignRight | Qt::AlignVCenter,
            QString("Peak %1").arg(peakDensity_, 0, 'f', 1));
    }

private:
    double threshold_{0.0};
    double peakDensity_{0.0};
};

class PressureLegendWidget final : public QWidget {
public:
    explicit PressureLegendWidget(
        const safecrowd::domain::PressureSummary& summary,
        QWidget* parent = nullptr)
        : QWidget(parent),
          threshold_(summary.hotspotScoreThreshold),
          peakScore_(summary.peakPressureScore) {
        setFixedSize(300, 34);
        setToolTip("Pressure heatmap scale uses the pressure hotspot score threshold and the observed peak score.");
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        (void)event;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF ramp(0, 4, width(), 9);
        QLinearGradient gradient(ramp.left(), ramp.center().y(), ramp.right(), ramp.center().y());
        gradient.setColorAt(0.0, QColor("#facc15"));
        gradient.setColorAt(0.25, QColor("#f97316"));
        gradient.setColorAt(0.55, QColor("#ef4444"));
        gradient.setColorAt(1.0, QColor("#991b1b"));
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawRoundedRect(ramp, 4, 4);

        painter.setFont(ui::font(ui::FontRole::Caption));
        painter.setPen(QColor("#687789"));
        if (peakScore_ > 0.0 && threshold_ > 0.0) {
            const auto peakX = ramp.left()
                + (std::clamp(peakScore_ / threshold_, 0.0, 1.0) * ramp.width());
            painter.setPen(QPen(QColor("#405063"), 1));
            painter.drawLine(QPointF(peakX, ramp.top() - 2), QPointF(peakX, ramp.bottom() + 2));
            painter.setPen(QColor("#687789"));
        }
        painter.drawText(QRectF(0, 16, 40, 16), Qt::AlignLeft | Qt::AlignVCenter, "0");
        painter.drawText(
            QRectF(42, 16, 178, 16),
            Qt::AlignCenter,
            QString("Hotspot %1+").arg(threshold_, 0, 'f', 1));
        painter.drawText(
            QRectF(width() - 98, 16, 98, 16),
            Qt::AlignRight | Qt::AlignVCenter,
            QString("Peak %1").arg(peakScore_, 0, 'f', 1));
    }

private:
    double threshold_{0.0};
    double peakScore_{0.0};
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
    const bool longValue = value.size() >= 12 || value.contains('>') || value.contains(' ');
    auto valueFont = longValue
        ? ui::font(ui::FontRole::Caption)
        : valueLabel->font();
    if (longValue) {
        valueFont.setWeight(QFont::DemiBold);
    } else {
        valueFont.setPointSize(std::max(11, valueFont.pointSize() - 1));
    }
    valueLabel->setFont(valueFont);
    valueLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
    valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    const auto minimumLines = longValue ? 2 : 1;
    valueLabel->setMinimumHeight((QFontMetrics(valueFont).lineSpacing() * minimumLines) + 4);
    if (!tooltip.isEmpty()) {
        titleLabel->setToolTip(tooltip);
        valueLabel->setToolTip(tooltip);
    }
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    return card;
}

QString resultCriteriaTooltip(const safecrowd::domain::ScenarioResultArtifacts& artifacts) {
    return QStringList{
        QString("High density: %1 / m2 or higher for accumulated duration.")
            .arg(artifacts.densitySummary.highDensityThresholdPeoplePerSquareMeter, 0, 'f', 1),
        QString("Pressure hotspot: score %1 or higher in a crowded cell.")
            .arg(artifacts.pressureSummary.hotspotScoreThreshold, 0, 'f', 1),
        safecrowd::domain::scenarioStalledDefinition(),
        safecrowd::domain::scenarioBottleneckDefinition(),
    }.join("\n\n");
}

QWidget* createOverviewHeader(const safecrowd::domain::ScenarioResultArtifacts& artifacts, QWidget* parent) {
    auto* header = new QWidget(parent);
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* title = createLabel("Overview", header, ui::FontRole::Title);
    title->setWordWrap(false);
    layout->addWidget(title, 0, Qt::AlignVCenter);

    auto* help = new QToolButton(header);
    help->setText("?");
    help->setCursor(Qt::PointingHandCursor);
    help->setFixedSize(22, 22);
    help->setToolTip(resultCriteriaTooltip(artifacts));
    help->setAccessibleName("Overview criteria");
    help->setStyleSheet(
        "QToolButton {"
        " background: #ffffff;"
        " border: 1px solid #c9d5e2;"
        " border-radius: 11px;"
        " color: #4f5d6b;"
        " font-weight: 700;"
        " padding: 0;"
        "}"
        "QToolButton:hover { background: #eef3f8; border-color: #9fb0c2; }");
    layout->addWidget(help, 0, Qt::AlignVCenter);
    layout->addStretch(1);
    return header;
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
    std::function<void(ResultOverlayMode)>* overlayModeHandlerOut,
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
    overlayCombo->addItem("Pressure", static_cast<int>(ResultOverlayMode::Pressure));
    overlayCombo->addItem("Bottlenecks", static_cast<int>(ResultOverlayMode::Bottlenecks));
    overlayCombo->addItem("Hotspots", static_cast<int>(ResultOverlayMode::Hotspots));
    overlayCombo->addItem("None", static_cast<int>(ResultOverlayMode::None));
    overlayCombo->setToolTip("Switch between result map overlays.");
    overlayLayout->addWidget(overlayLabel);
    overlayLayout->addWidget(overlayCombo);
    overlayLayout->addSpacing(10);
    auto* densityLegend = new DensityLegendWidget(artifacts.densitySummary, overlayBar);
    auto* pressureLegend = new PressureLegendWidget(artifacts.pressureSummary, overlayBar);
    pressureLegend->setVisible(false);
    overlayLayout->addWidget(densityLegend);
    overlayLayout->addWidget(pressureLegend);
    overlayLayout->addStretch(1);
    layout->addWidget(overlayBar);
    canvas->setDensityOverlay(artifacts.densitySummary.peakField.cells.empty()
            ? artifacts.densitySummary.peakCells
            : artifacts.densitySummary.peakField.cells,
        artifacts.densitySummary.highDensityThresholdPeoplePerSquareMeter);
    canvas->setPressureOverlay(artifacts.pressureSummary.peakField.cells.empty()
            ? artifacts.pressureSummary.peakCells
            : artifacts.pressureSummary.peakField.cells,
        std::max(
            artifacts.pressureSummary.hotspotScoreThreshold,
            artifacts.pressureSummary.peakPressureScore));
    const QPointer<SimulationCanvasWidget> canvasGuard(canvas);
    const QPointer<QComboBox> overlayComboGuard(overlayCombo);
    const QPointer<QWidget> densityLegendGuard(densityLegend);
    const QPointer<QWidget> pressureLegendGuard(pressureLegend);
    const std::function<void(ResultOverlayMode)> applyOverlayMode =
        [canvasGuard, overlayComboGuard, densityLegendGuard, pressureLegendGuard](ResultOverlayMode mode) {
            if (overlayComboGuard != nullptr) {
                const auto comboIndex = overlayComboGuard->findData(static_cast<int>(mode));
                if (comboIndex >= 0 && overlayComboGuard->currentIndex() != comboIndex) {
                    const QSignalBlocker blocker(overlayComboGuard);
                    overlayComboGuard->setCurrentIndex(comboIndex);
                }
            }
            if (densityLegendGuard != nullptr) {
                densityLegendGuard->setVisible(mode == ResultOverlayMode::Density);
            }
            if (pressureLegendGuard != nullptr) {
                pressureLegendGuard->setVisible(mode == ResultOverlayMode::Pressure);
            }
            if (canvasGuard != nullptr) {
                canvasGuard->setResultOverlayMode(mode);
            }
        };
    if (overlayModeHandlerOut != nullptr) {
        *overlayModeHandlerOut = applyOverlayMode;
    }
    applyOverlayMode(ResultOverlayMode::Density);
    QObject::connect(overlayCombo, &QComboBox::currentIndexChanged, panel, [overlayComboGuard, applyOverlayMode](int index) {
        if (overlayComboGuard == nullptr || index < 0) {
            return;
        }
        const auto data = overlayComboGuard->itemData(index);
        if (!data.isValid()) {
            return;
        }
        applyOverlayMode(static_cast<ResultOverlayMode>(data.toInt()));
    });
    layout->addWidget(canvas, 1);
    EvacuationProgressWidget* progressWidget = nullptr;
    auto* graphPanel = createResultGraphPanel(artifacts, &progressWidget, panel);
    auto* replayControls = new ResultReplayControls(replayFramesForResult(artifacts, frame), canvas, progressWidget, panel);
    if (replayControlsOut != nullptr) {
        *replayControlsOut = replayControls;
    }
    if (progressWidget != nullptr) {
        const QPointer<ResultReplayControls> replayControlsGuard(replayControls);
        const auto t90Seconds = artifacts.timingSummary.t90Seconds;
        const auto t95Seconds = artifacts.timingSummary.t95Seconds;
        const auto t90Frame = artifacts.timingSummary.t90Frame;
        const auto t95Frame = artifacts.timingSummary.t95Frame;
        progressWidget->setTimingMarkerActivatedHandler([replayControlsGuard, t90Seconds, t95Seconds, t90Frame, t95Frame](double seconds) {
            if (replayControlsGuard != nullptr) {
                if (t90Seconds.has_value()
                    && t90Frame.has_value()
                    && std::abs(seconds - *t90Seconds) <= 1e-6) {
                    replayControlsGuard->showFrame(*t90Frame);
                    return;
                }
                if (t95Seconds.has_value()
                    && t95Frame.has_value()
                    && std::abs(seconds - *t95Seconds) <= 1e-6) {
                    replayControlsGuard->showFrame(*t95Frame);
                    return;
                }
                replayControlsGuard->showClosestFrameAtSeconds(seconds);
            }
        });
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
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(createOverviewHeader(artifacts, panel));
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
    const auto worstBottleneckFull = risk.bottlenecks.empty()
        ? QString("None")
        : QString::fromStdString(risk.bottlenecks.front().label);
    const auto worstBottleneck = compactBottleneckLabel(worstBottleneckFull);
    const auto slowestGroup = artifacts.placementCompletion.empty()
        ? QString("Pending")
        : QString::fromStdString(artifacts.placementCompletion.front().placementId);
    const auto peakPressureTooltip = QString(
        "Highest pressure hotspot score observed during the run.%1%2")
        .arg(artifacts.pressureSummary.peakAtSeconds.has_value()
            ? QString("\n\nPeak at %1 sec.").arg(*artifacts.pressureSummary.peakAtSeconds, 0, 'f', 1)
            : QString())
        .arg(artifacts.pressureSummary.peakCell.has_value()
            ? QString("\nCell floor: %1").arg(QString::fromStdString(artifacts.pressureSummary.peakCell->floorId))
            : QString());
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
        QString("Worst bottleneck observed during the run.\n\nFull label: %1\n\n%2")
            .arg(worstBottleneckFull)
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
    metricsGrid->addWidget(createMetricCard(
        "Peak Pressure",
        formatPressureScore(artifacts.pressureSummary.peakPressureScore),
        panel,
        peakPressureTooltip), 4, 1);
    metricsGrid->addWidget(createMetricCard(
        "Pressure Hotspots",
        QString::number(static_cast<int>(artifacts.pressureSummary.peakHotspots.size())),
        panel,
        "Peak number of stored pressure hotspot locations from the run."), 5, 0);
    metricsGrid->addWidget(createMetricCard(
        "Pressure Events",
        QString::number(static_cast<int>(artifacts.pressureSummary.criticalEvents.size())),
        panel,
        "Stored sustained critical pressure events that met the duration and agent-count thresholds."), 5, 1);
    metricsGrid->addWidget(createMetricCard(
        "Critical Pressure",
        QString("%1 agents").arg(static_cast<int>(artifacts.pressureSummary.peakCriticalAgentCount)),
        panel,
        QString("Peak simultaneously critical agents during the run.\nExposed peak: %1 agents.")
            .arg(static_cast<int>(artifacts.pressureSummary.peakExposedAgentCount))), 6, 0);
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

ScenarioResultNavigationView resultNavigationViewFromSaved(SavedResultNavigationView view) {
    switch (view) {
    case SavedResultNavigationView::Hotspot:
        return ScenarioResultNavigationView::Hotspot;
    case SavedResultNavigationView::Zone:
        return ScenarioResultNavigationView::Zone;
    case SavedResultNavigationView::Groups:
        return ScenarioResultNavigationView::Groups;
    case SavedResultNavigationView::Bottleneck:
    default:
        return ScenarioResultNavigationView::Bottleneck;
    }
}

SavedResultNavigationView savedResultNavigationView(ScenarioResultNavigationView view) {
    switch (view) {
    case ScenarioResultNavigationView::Hotspot:
        return SavedResultNavigationView::Hotspot;
    case ScenarioResultNavigationView::Zone:
        return SavedResultNavigationView::Zone;
    case ScenarioResultNavigationView::Groups:
        return SavedResultNavigationView::Groups;
    case ScenarioResultNavigationView::Bottleneck:
    default:
        return SavedResultNavigationView::Bottleneck;
    }
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
    SavedResultNavigationView savedNavigationView,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState,
    QWidget* parent)
    : QWidget(parent),
      projectName_(std::move(projectName)),
      layout_(std::move(layout)),
      scenario_(std::move(scenario)),
      frame_(std::move(frame)),
      risk_(std::move(risk)),
      artifacts_(std::move(artifacts)),
      returnAuthoringState_(std::move(returnAuthoringState)),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)),
      backToLayoutReviewHandler_(std::move(backToLayoutReviewHandler)) {
    resultNavigationView_ = resultNavigationViewFromSaved(savedNavigationView);

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

    auto* canvas = new SimulationCanvasWidget(layout_, shell_);
    canvas->setFrame(frame_);
    canvas->setHotspotOverlay(risk_.hotspots);
    canvas->setBottleneckOverlay(risk_.bottlenecks);
    ResultReplayControls* replayControls = nullptr;
    std::function<void(ResultOverlayMode)> applyResultOverlayMode;
    shell_->setCanvas(createResultCanvasPanel(
        canvas,
        frame_,
        artifacts_,
        &replayControls,
        &applyResultOverlayMode,
        shell_));
    bottleneckFocusHandler_ = [this, canvas, replayControls, applyResultOverlayMode](std::size_t index) {
        if (index < risk_.bottlenecks.size() && replayControls != nullptr) {
            const auto& bottleneck = risk_.bottlenecks[index];
            if (bottleneck.detectionFrame.has_value()) {
                replayControls->showFrame(*bottleneck.detectionFrame);
            } else if (bottleneck.detectedAtSeconds.has_value()) {
                replayControls->showClosestFrameAtSeconds(*bottleneck.detectedAtSeconds);
            }
        }
        if (applyResultOverlayMode) {
            applyResultOverlayMode(ResultOverlayMode::Bottlenecks);
        } else {
            canvas->setResultOverlayMode(ResultOverlayMode::Bottlenecks);
        }
        canvas->focusBottleneck(index);
    };
    hotspotFocusHandler_ = [this, canvas, replayControls, applyResultOverlayMode](std::size_t index) {
        if (index < risk_.hotspots.size() && replayControls != nullptr) {
            const auto& hotspot = risk_.hotspots[index];
            if (hotspot.detectionFrame.has_value()) {
                replayControls->showFrame(*hotspot.detectionFrame);
            } else if (hotspot.detectedAtSeconds.has_value()) {
                replayControls->showClosestFrameAtSeconds(*hotspot.detectedAtSeconds);
            }
        }
        if (applyResultOverlayMode) {
            applyResultOverlayMode(ResultOverlayMode::Hotspots);
        } else {
            canvas->setResultOverlayMode(ResultOverlayMode::Hotspots);
        }
        canvas->focusHotspot(index);
    };
    refreshResultNavigationPanel();
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
        shell_));
    shell_->setReviewPanelVisible(true);

    rootLayout->addWidget(shell_);
}

void ScenarioResultWidget::refreshResultNavigationPanel() {
    if (shell_ == nullptr) {
        return;
    }

    shell_->setNavigationTabs(
        scenarioResultNavigationTabs(),
        scenarioResultNavigationTabId(resultNavigationView_),
        [this](const QString& tabId) {
            resultNavigationView_ = scenarioResultNavigationViewFromTabId(tabId);
            refreshResultNavigationPanel();
        });

    shell_->setNavigationPanel(createScenarioResultNavigationPanel(
        resultNavigationView_,
        risk_,
        artifacts_,
        bottleneckFocusHandler_,
        hotspotFocusHandler_,
        shell_));
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

SavedResultNavigationView ScenarioResultWidget::currentSavedNavigationView() const noexcept {
    return savedResultNavigationView(resultNavigationView_);
}

const std::optional<ScenarioAuthoringWidget::InitialState>& ScenarioResultWidget::returnAuthoringState() const noexcept {
    return returnAuthoringState_;
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
        frame_,
        risk_,
        artifacts_,
        this,
        returnAuthoringState_);

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

    auto initial = returnAuthoringState_.value_or(ScenarioAuthoringWidget::InitialState{});
    if (initial.scenarios.empty()) {
        initial.scenarios.push_back(scenarioStateFromDraft(scenario_, layout_));
        initial.currentScenarioIndex = 0;
        initial.navigationView = ScenarioAuthoringWidget::NavigationView::Layout;
    }
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
