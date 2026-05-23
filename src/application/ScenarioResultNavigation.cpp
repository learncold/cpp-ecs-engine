#include "application/ScenarioResultNavigation.h"

#include <algorithm>
#include <optional>
#include <utility>

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>

#include "application/UiStyle.h"

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

QString formatPercent(double ratio) {
    return QString("%1%").arg(std::clamp(ratio, 0.0, 1.0) * 100.0, 0, 'f', 0);
}

QLabel* createReportSectionHeader(const QString& text, QWidget* parent) {
    auto* label = createLabel(text, parent, ui::FontRole::SectionTitle);
    label->setStyleSheet(ui::mutedTextStyleSheet());
    return label;
}

QFrame* createReportInfoRow(const QStringList& lines, QWidget* parent) {
    auto* row = new QFrame(parent);
    row->setStyleSheet(ui::panelStyleSheet());
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(4);
    for (const auto& line : lines) {
        auto* label = createLabel(line, row, ui::FontRole::Body);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setStyleSheet("QLabel { background: transparent; border: 0; padding: 0; }");
        layout->addWidget(label);
    }
    return row;
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

QPushButton* createOperationalConflictCellRowButton(
    const safecrowd::domain::ScenarioOperationalConflictCellMetric& cell,
    std::size_t index,
    QWidget* parent) {
    QStringList lines{
        QString("%1. Conflict score %2")
            .arg(static_cast<int>(index + 1))
            .arg(cell.conflictScore, 0, 'f', 2),
        QString("Counterflow %1  |  %2 vs %3 movers")
            .arg(formatPercent(cell.counterflowRatio))
            .arg(static_cast<int>(cell.forwardCount))
            .arg(static_cast<int>(cell.reverseCount)),
        QString("Duration %1 sec  |  Speed %2 m/s")
            .arg(cell.durationSeconds, 0, 'f', 1)
            .arg(cell.averageSpeed, 0, 'f', 2),
    };
    if (!cell.nearestConnectionLabel.empty() || !cell.nearestConnectionId.empty()) {
        lines.push_back(QString("Nearest connection: %1")
            .arg(QString::fromStdString(
                !cell.nearestConnectionLabel.empty() ? cell.nearestConnectionLabel : cell.nearestConnectionId)));
    }
    if (!cell.floorId.empty()) {
        lines.push_back(QString("Floor: %1").arg(QString::fromStdString(cell.floorId)));
    }
    auto* button = createReportRowButton(lines, parent);
    button->setToolTip(QString("%1\nClick to focus this conflict hotspot on the canvas.")
        .arg(safecrowd::domain::scenarioOperationalConflictDefinition()));
    return button;
}

QPushButton* createOperationalConflictConnectionRowButton(
    const safecrowd::domain::ScenarioOperationalConflictConnectionMetric& connection,
    std::size_t index,
    QWidget* parent) {
    QStringList lines{
        QString("%1. %2")
            .arg(static_cast<int>(index + 1))
            .arg(QString::fromStdString(
                !connection.label.empty() ? connection.label : connection.connectionId)),
        QString("Score %1  |  Counterflow %2")
            .arg(connection.conflictScore, 0, 'f', 2)
            .arg(formatPercent(connection.counterflowRatio)),
        QString("Queue %1  |  Duration %2 sec  |  Speed %3 m/s")
            .arg(static_cast<int>(connection.queueAgentCount))
            .arg(connection.durationSeconds, 0, 'f', 1)
            .arg(connection.averageSpeed, 0, 'f', 2),
    };
    if (!connection.floorId.empty()) {
        lines.push_back(QString("Floor: %1").arg(QString::fromStdString(connection.floorId)));
    }
    auto* button = createReportRowButton(lines, parent);
    button->setToolTip(QString("%1\nClick to focus this conflict connection on the canvas.")
        .arg(safecrowd::domain::scenarioOperationalConflictDefinition()));
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

QIcon makeResultNavigationIcon(const QString& tabId, const QColor& color) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (tabId == "bottleneck") {
        painter.drawLine(QPointF(12, 12), QPointF(20, 21));
        painter.drawLine(QPointF(12, 32), QPointF(20, 23));
        painter.drawLine(QPointF(32, 12), QPointF(24, 21));
        painter.drawLine(QPointF(32, 32), QPointF(24, 23));
        painter.setBrush(color);
        painter.drawEllipse(QPointF(22, 22), 2.8, 2.8);
    } else if (tabId == "operational-conflict") {
        painter.drawLine(QPointF(12, 22), QPointF(32, 22));
        painter.drawLine(QPointF(22, 12), QPointF(22, 32));
        painter.drawLine(QPointF(17, 17), QPointF(13, 13));
        painter.drawLine(QPointF(17, 17), QPointF(13, 21));
        painter.drawLine(QPointF(27, 27), QPointF(31, 23));
        painter.drawLine(QPointF(27, 27), QPointF(31, 31));
        painter.setBrush(color);
        painter.drawEllipse(QPointF(22, 22), 2.8, 2.8);
    } else if (tabId == "hotspot") {
        painter.drawEllipse(QPointF(22, 22), 12, 12);
        painter.drawEllipse(QPointF(22, 22), 7, 7);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(22, 22), 3, 3);
    } else if (tabId == "zone") {
        painter.drawRoundedRect(QRectF(11, 11, 22, 22), 4, 4);
        painter.drawLine(QPointF(22, 11), QPointF(22, 33));
        painter.drawLine(QPointF(11, 22), QPointF(33, 22));
    } else if (tabId == "groups") {
        painter.drawEllipse(QPointF(22, 14), 5, 5);
        painter.drawEllipse(QPointF(14, 20), 4, 4);
        painter.drawEllipse(QPointF(30, 20), 4, 4);
        painter.drawArc(QRectF(12, 22, 20, 14), 20 * 16, 140 * 16);
        painter.drawArc(QRectF(5, 26, 18, 10), 30 * 16, 120 * 16);
        painter.drawArc(QRectF(21, 26, 18, 10), 30 * 16, 120 * 16);
    } else {
        painter.drawRoundedRect(QRectF(12, 12, 20, 20), 4, 4);
        painter.drawLine(QPointF(16, 18), QPointF(24, 18));
        painter.drawLine(QPointF(24, 18), QPointF(21, 15));
        painter.drawLine(QPointF(24, 18), QPointF(21, 21));
        painter.drawLine(QPointF(28, 26), QPointF(20, 26));
        painter.drawLine(QPointF(20, 26), QPointF(23, 23));
        painter.drawLine(QPointF(20, 26), QPointF(23, 29));
    }

    return QIcon(pixmap);
}

struct ResultReportPanelParts {
    QWidget* panel{nullptr};
    QWidget* content{nullptr};
    QVBoxLayout* contentLayout{nullptr};
};

ResultReportPanelParts createResultReportPanel(
    const QString& title,
    const QString& caption,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* header = createReportSectionHeader(title, panel);
    layout->addWidget(header);
    if (!caption.isEmpty()) {
        auto* captionLabel = createLabel(caption, panel, ui::FontRole::Caption);
        captionLabel->setStyleSheet(ui::subtleTextStyleSheet());
        layout->addWidget(captionLabel);
    }

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
    area->setWidget(content);
    layout->addWidget(area, 1);

    return {
        .panel = panel,
        .content = content,
        .contentLayout = contentLayout,
    };
}

QWidget* createBottleneckReportPanel(
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    std::function<void(std::size_t)> bottleneckFocusHandler,
    QWidget* parent) {
    auto parts = createResultReportPanel("Bottleneck", "Detected passage constraints", parent);
    auto* bottleneckHeader = createReportSectionHeader("Bottlenecks", parts.content);
    bottleneckHeader->setToolTip(safecrowd::domain::scenarioBottleneckDefinition());
    parts.contentLayout->addWidget(bottleneckHeader);
    if (risk.bottlenecks.empty()) {
        auto* empty = createLabel("None detected", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
    } else {
        for (std::size_t index = 0; index < risk.bottlenecks.size(); ++index) {
            auto* row = createBottleneckRowButton(risk.bottlenecks[index], index, parts.content);
            QObject::connect(row, &QPushButton::clicked, parts.content, [bottleneckFocusHandler, index]() {
                if (bottleneckFocusHandler) {
                    bottleneckFocusHandler(index);
                }
            });
            parts.contentLayout->addWidget(row);
        }
    }
    parts.contentLayout->addStretch(1);
    return parts.panel;
}

QWidget* createHotspotReportPanel(
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    std::function<void(std::size_t)> hotspotFocusHandler,
    QWidget* parent) {
    auto parts = createResultReportPanel("Hotspot", "Peak congestion locations", parent);
    auto* hotspotHeader = createReportSectionHeader("Hotspots", parts.content);
    hotspotHeader->setToolTip(safecrowd::domain::scenarioHotspotDefinition());
    parts.contentLayout->addWidget(hotspotHeader);
    parts.contentLayout->addWidget(createHotspotLegend(parts.content));
    if (risk.hotspots.empty()) {
        auto* empty = createLabel("None detected", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
    } else {
        for (std::size_t index = 0; index < risk.hotspots.size(); ++index) {
            auto* row = createHotspotRowButton(risk.hotspots[index], index, parts.content);
            QObject::connect(row, &QPushButton::clicked, parts.content, [hotspotFocusHandler, index]() {
                if (hotspotFocusHandler) {
                    hotspotFocusHandler(index);
                }
            });
            parts.contentLayout->addWidget(row);
        }
    }
    parts.contentLayout->addStretch(1);
    return parts.panel;
}

QWidget* createZoneReportPanel(const safecrowd::domain::ScenarioResultArtifacts& artifacts, QWidget* parent) {
    auto parts = createResultReportPanel("Zone", "Completion by source zone", parent);
    if (artifacts.zoneCompletion.empty()) {
        auto* empty = createLabel("No zone completion data", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
    } else {
        for (const auto& zone : artifacts.zoneCompletion) {
            parts.contentLayout->addWidget(createReportInfoRow({
                QString::fromStdString(zone.zoneLabel),
                QString("People: %1    Out: %2")
                    .arg(static_cast<int>(zone.initialCount))
                    .arg(static_cast<int>(zone.evacuatedCount)),
                QString("Last: %1").arg(formatOptionalSeconds(zone.lastCompletionTimeSeconds)),
            }, parts.content));
        }
    }
    parts.contentLayout->addStretch(1);
    return parts.panel;
}

QWidget* createGroupsReportPanel(const safecrowd::domain::ScenarioResultArtifacts& artifacts, QWidget* parent) {
    auto parts = createResultReportPanel("Groups", "Completion by crowd placement", parent);
    if (artifacts.placementCompletion.empty()) {
        auto* empty = createLabel("No group completion data", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
    } else {
        for (const auto& group : artifacts.placementCompletion) {
            parts.contentLayout->addWidget(createReportInfoRow({
                QString::fromStdString(group.placementId),
                QString("People: %1    Out: %2")
                    .arg(static_cast<int>(group.initialCount))
                    .arg(static_cast<int>(group.evacuatedCount)),
                QString("Last: %1").arg(formatOptionalSeconds(group.lastCompletionTimeSeconds)),
            }, parts.content));
        }
    }
    parts.contentLayout->addStretch(1);
    return parts.panel;
}

QWidget* createOperationalConflictReportPanel(
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    std::function<void(std::size_t)> operationalConflictCellFocusHandler,
    std::function<void(std::size_t)> operationalConflictConnectionFocusHandler,
    QWidget* parent) {
    auto parts = createResultReportPanel("Operational Conflict", "Counterflow and concentrated connector load", parent);
    auto* summaryHeader = createReportSectionHeader("Summary", parts.content);
    summaryHeader->setToolTip(safecrowd::domain::scenarioOperationalConflictDefinition());
    parts.contentLayout->addWidget(summaryHeader);
    parts.contentLayout->addWidget(createReportInfoRow({
        QString("Peak conflict score: %1")
            .arg(artifacts.operationalConflictSummary.peakConflictScore, 0, 'f', 2),
        QString("Total exposure: %1 agent-sec")
            .arg(artifacts.operationalConflictSummary.totalConflictExposureAgentSeconds, 0, 'f', 1),
        QString("Longest duration: %1 sec")
            .arg(artifacts.operationalConflictSummary.longestConflictDurationSeconds, 0, 'f', 1),
        QString("Conflict connections: %1  |  Peak queued: %2")
            .arg(static_cast<int>(artifacts.operationalConflictSummary.conflictConnectionCount))
            .arg(static_cast<int>(artifacts.operationalConflictSummary.peakQueuedAgents)),
        QString("Connection concentration: %1")
            .arg(artifacts.operationalConflictSummary.connectionConcentrationIndex, 0, 'f', 2),
    }, parts.content));

    auto* cellHeader = createReportSectionHeader("Conflict Cells", parts.content);
    parts.contentLayout->addWidget(cellHeader);
    if (risk.operationalConflictCells.empty()) {
        auto* empty = createLabel("None detected", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
    } else {
        for (std::size_t index = 0; index < risk.operationalConflictCells.size(); ++index) {
            auto* row = createOperationalConflictCellRowButton(risk.operationalConflictCells[index], index, parts.content);
            QObject::connect(row, &QPushButton::clicked, parts.content, [operationalConflictCellFocusHandler, index]() {
                if (operationalConflictCellFocusHandler) {
                    operationalConflictCellFocusHandler(index);
                }
            });
            parts.contentLayout->addWidget(row);
        }
    }

    auto* connectionHeader = createReportSectionHeader("Conflict Connections", parts.content);
    parts.contentLayout->addWidget(connectionHeader);
    if (risk.operationalConflictConnections.empty()) {
        auto* empty = createLabel("None detected", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
    } else {
        for (std::size_t index = 0; index < risk.operationalConflictConnections.size(); ++index) {
            auto* row = createOperationalConflictConnectionRowButton(
                risk.operationalConflictConnections[index],
                index,
                parts.content);
            QObject::connect(row, &QPushButton::clicked, parts.content, [operationalConflictConnectionFocusHandler, index]() {
                if (operationalConflictConnectionFocusHandler) {
                    operationalConflictConnectionFocusHandler(index);
                }
            });
            parts.contentLayout->addWidget(row);
        }
    }

    if (!artifacts.connectionUsage.empty()) {
        auto* usageHeader = createReportSectionHeader("Top Connection Load", parts.content);
        parts.contentLayout->addWidget(usageHeader);
        const auto usageCount = std::min<std::size_t>(3, artifacts.connectionUsage.size());
        for (std::size_t index = 0; index < usageCount; ++index) {
            const auto& metric = artifacts.connectionUsage[index];
            parts.contentLayout->addWidget(createReportInfoRow({
                QString::fromStdString(!metric.label.empty() ? metric.label : metric.connectionId),
                QString("Traversals %1  |  Share %2")
                    .arg(static_cast<int>(metric.traversalCount))
                    .arg(formatPercent(metric.usageRatio)),
                QString("Peak window %1  |  Queue exposure %2 agent-sec")
                    .arg(static_cast<int>(metric.peakWindowCount))
                    .arg(metric.queueExposureAgentSeconds, 0, 'f', 1),
            }, parts.content));
        }
    }

    parts.contentLayout->addStretch(1);
    return parts.panel;
}

bool shouldShowRecommendationEvidence(const safecrowd::domain::AlternativeRecommendationEvidence& item) {
    const auto label = QString::fromStdString(item.label);
    return !label.startsWith("Risk ") && label != "Critical pressure events";
}

}  // namespace

std::vector<WorkspaceNavigationTab> scenarioResultNavigationTabs() {
    return {
        {
            .id = "bottleneck",
            .label = "Bottleneck",
            .icon = makeResultNavigationIcon("bottleneck", QColor("#1f5fae")),
        },
        {
            .id = "operational-conflict",
            .label = "Operational Conflict",
            .icon = makeResultNavigationIcon("operational-conflict", QColor("#1f5fae")),
        },
        {
            .id = "hotspot",
            .label = "Hotspot",
            .icon = makeResultNavigationIcon("hotspot", QColor("#1f5fae")),
        },
        {
            .id = "zone",
            .label = "Zone",
            .icon = makeResultNavigationIcon("zone", QColor("#1f5fae")),
        },
        {
            .id = "groups",
            .label = "Groups",
            .icon = makeResultNavigationIcon("groups", QColor("#1f5fae")),
        },
        {
            .id = "recommendations",
            .label = "Recommendations",
            .icon = makeResultNavigationIcon("recommendations", QColor("#1f5fae")),
        },
    };
}

QString scenarioResultNavigationTabId(ScenarioResultNavigationView view) {
    switch (view) {
    case ScenarioResultNavigationView::OperationalConflict:
        return "operational-conflict";
    case ScenarioResultNavigationView::Hotspot:
        return "hotspot";
    case ScenarioResultNavigationView::Zone:
        return "zone";
    case ScenarioResultNavigationView::Groups:
        return "groups";
    case ScenarioResultNavigationView::Recommendations:
        return "recommendations";
    case ScenarioResultNavigationView::Bottleneck:
    default:
        return "bottleneck";
    }
}

ScenarioResultNavigationView scenarioResultNavigationViewFromTabId(const QString& tabId) {
    if (tabId == "operational-conflict") {
        return ScenarioResultNavigationView::OperationalConflict;
    }
    if (tabId == "hotspot") {
        return ScenarioResultNavigationView::Hotspot;
    }
    if (tabId == "zone") {
        return ScenarioResultNavigationView::Zone;
    }
    if (tabId == "groups") {
        return ScenarioResultNavigationView::Groups;
    }
    if (tabId == "recommendations") {
        return ScenarioResultNavigationView::Recommendations;
    }
    return ScenarioResultNavigationView::Bottleneck;
}

QWidget* createScenarioResultNavigationPanel(
    ScenarioResultNavigationView view,
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    std::function<void(std::size_t)> bottleneckFocusHandler,
    std::function<void(std::size_t)> operationalConflictCellFocusHandler,
    std::function<void(std::size_t)> operationalConflictConnectionFocusHandler,
    std::function<void(std::size_t)> hotspotFocusHandler,
    QWidget* parent) {
    switch (view) {
    case ScenarioResultNavigationView::OperationalConflict:
        return createOperationalConflictReportPanel(
            risk,
            artifacts,
            std::move(operationalConflictCellFocusHandler),
            std::move(operationalConflictConnectionFocusHandler),
            parent);
    case ScenarioResultNavigationView::Hotspot:
        return createHotspotReportPanel(risk, std::move(hotspotFocusHandler), parent);
    case ScenarioResultNavigationView::Zone:
        return createZoneReportPanel(artifacts, parent);
    case ScenarioResultNavigationView::Groups:
        return createGroupsReportPanel(artifacts, parent);
    case ScenarioResultNavigationView::Recommendations:
        return createResultReportPanel("Recommendations", "Recommended operational changes", parent).panel;
    case ScenarioResultNavigationView::Bottleneck:
    default:
        return createBottleneckReportPanel(risk, std::move(bottleneckFocusHandler), parent);
    }
}

QWidget* createScenarioRecommendationNavigationPanel(
    const safecrowd::domain::AlternativeRecommendationResult& recommendation,
    std::function<void(safecrowd::domain::ScenarioDraft)> createScenarioHandler,
    QWidget* parent) {
    auto parts = createResultReportPanel("Recommendations", "Operational alternatives for this result", parent);
    if (recommendation.candidates.empty()) {
        const auto message = recommendation.blockingReasons.empty()
            ? QString("No actionable recommendation for this result.")
            : QString::fromStdString(recommendation.blockingReasons.front());
        auto* empty = createLabel(message, parts.content, ui::FontRole::Caption);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
        parts.contentLayout->addStretch(1);
        return parts.panel;
    }

    for (const auto& candidate : recommendation.candidates) {
        auto* section = new QFrame(parts.content);
        section->setStyleSheet(ui::panelStyleSheet());
        section->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        section->setMinimumWidth(0);
        auto* sectionLayout = new QVBoxLayout(section);
        sectionLayout->setContentsMargins(14, 12, 14, 12);
        sectionLayout->setSpacing(6);

        auto* title = createLabel(QString::fromStdString(candidate.title), section, ui::FontRole::Body);
        title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        title->setStyleSheet("QLabel { color: #16202b; font-weight: 600; }");
        sectionLayout->addWidget(title);

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
        QObject::connect(button, &QPushButton::clicked, section, [createScenarioHandler, scenario = candidate.recommendedScenario]() {
            if (createScenarioHandler) {
                createScenarioHandler(scenario);
            }
        });

        parts.contentLayout->addWidget(section);
    }
    parts.contentLayout->addStretch(1);
    return parts.panel;
}

}  // namespace safecrowd::application
