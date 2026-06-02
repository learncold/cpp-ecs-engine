#include "application/ScenarioResultNavigation.h"

#include <algorithm>
#include <optional>
#include <utility>

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>

#include "application/ToolIconResources.h"
#include "application/UiStyle.h"

namespace safecrowd::application {
namespace {

using ResultItemSelectionHandler = std::function<void(ScenarioResultNavigationView, std::size_t)>;

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
}

QString formatOptionalSeconds(const std::optional<double>& seconds) {
    return seconds.has_value() ? QString("%1 sec").arg(*seconds, 0, 'f', 1) : QString("Pending");
}

QString formatExposureSeconds(double seconds) {
    return QString("%1 agent-sec").arg(seconds, 0, 'f', 1);
}

QString hazardKindLabel(safecrowd::domain::EnvironmentHazardKind kind) {
    switch (kind) {
    case safecrowd::domain::EnvironmentHazardKind::Smoke:
        return "Smoke";
    case safecrowd::domain::EnvironmentHazardKind::Fire:
    default:
        return "Fire";
    }
}

QString severityLabel(safecrowd::domain::ScenarioElementSeverity severity) {
    switch (severity) {
    case safecrowd::domain::ScenarioElementSeverity::Low:
        return "Low";
    case safecrowd::domain::ScenarioElementSeverity::High:
        return "High";
    case safecrowd::domain::ScenarioElementSeverity::Medium:
    default:
        return "Medium";
    }
}

struct HazardKindExposureSummary {
    int hazardCount{0};
    double exposedAgentSeconds{0.0};
    double exposureScore{0.0};
    std::size_t peakExposedAgentCount{0};
    std::optional<double> peakAtSeconds{};
};

HazardKindExposureSummary summarizeHazardKind(
    const safecrowd::domain::HazardExposureSummary& summary,
    safecrowd::domain::EnvironmentHazardKind kind) {
    HazardKindExposureSummary result;
    for (const auto& metric : summary.hazards) {
        if (metric.kind != kind) {
            continue;
        }
        ++result.hazardCount;
        result.exposedAgentSeconds += metric.exposedAgentSeconds;
        result.exposureScore += metric.exposureScore;
        if (metric.peakExposedAgentCount > result.peakExposedAgentCount
            || (metric.peakExposedAgentCount == result.peakExposedAgentCount
                && !result.peakAtSeconds.has_value()
                && metric.peakAtSeconds.has_value())) {
            result.peakExposedAgentCount = metric.peakExposedAgentCount;
            result.peakAtSeconds = metric.peakAtSeconds;
        }
    }
    return result;
}

QString hazardDisplayName(const safecrowd::domain::HazardExposureMetric& metric) {
    if (!metric.hazardName.empty()) {
        return QString::fromStdString(metric.hazardName);
    }
    if (!metric.hazardId.empty()) {
        return QString::fromStdString(metric.hazardId);
    }
    return "Unnamed hazard";
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

QPushButton* createCrossFlowCellRowButton(
    const safecrowd::domain::ScenarioCrossFlowCellMetric& cell,
    std::size_t index,
    QWidget* parent) {
    QStringList lines{
        QString("%1. Cross-flow score %2")
            .arg(static_cast<int>(index + 1))
            .arg(cell.crossFlowScore, 0, 'f', 2),
        QString("Cross flow %1  |  %2 primary / %3 crossing movers")
            .arg(formatPercent(cell.crossFlowRatio))
            .arg(static_cast<int>(cell.primaryFlowCount))
            .arg(static_cast<int>(cell.crossFlowCount)),
        QString("Duration %1 sec  |  Speed %2 m/s")
            .arg(cell.durationSeconds, 0, 'f', 1)
            .arg(cell.averageSpeed, 0, 'f', 2),
    };
    if (!cell.floorId.empty()) {
        lines.push_back(QString("Floor: %1").arg(QString::fromStdString(cell.floorId)));
    }
    auto* button = createReportRowButton(lines, parent);
    button->setToolTip(QString("%1\nClick to focus this cross-flow hotspot on the canvas.")
        .arg(safecrowd::domain::scenarioCrossFlowDefinition()));
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

QString resultNavigationIconResourcePath(const QString& tabId) {
    if (tabId == "bottleneck") {
        return QStringLiteral(":/tool-icons/scenario-result/bottleneck.svg");
    }
    if (tabId == "cross-flow") {
        return QStringLiteral(":/tool-icons/scenario-result/cross-flow.svg");
    }
    if (tabId == "hotspot") {
        return QStringLiteral(":/tool-icons/scenario-result/hotspot.svg");
    }
    if (tabId == "exposure") {
        return QStringLiteral(":/tool-icons/scenario-result/exposure.svg");
    }
    if (tabId == "zone") {
        return QStringLiteral(":/tool-icons/scenario-result/zone.svg");
    }
    if (tabId == "groups") {
        return QStringLiteral(":/tool-icons/scenario-result/groups.svg");
    }
    return QStringLiteral(":/tool-icons/scenario-result/recommendations.svg");
}

QIcon makeResultNavigationIcon(const QString& tabId, const QColor& color) {
    return makeSvgToolIcon(resultNavigationIconResourcePath(tabId), color, QSize(22, 22));
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
    ResultItemSelectionHandler itemSelectionHandler,
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
            QObject::connect(row, &QPushButton::clicked, parts.content, [bottleneckFocusHandler, itemSelectionHandler, index]() {
                if (itemSelectionHandler) {
                    itemSelectionHandler(ScenarioResultNavigationView::Bottleneck, index);
                }
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
    ResultItemSelectionHandler itemSelectionHandler,
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
            QObject::connect(row, &QPushButton::clicked, parts.content, [hotspotFocusHandler, itemSelectionHandler, index]() {
                if (itemSelectionHandler) {
                    itemSelectionHandler(ScenarioResultNavigationView::Hotspot, index);
                }
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

QFrame* createHazardKindRow(
    const QString& label,
    const HazardKindExposureSummary& summary,
    QWidget* parent) {
    return createReportInfoRow({
        QString("%1 exposure").arg(label),
        QString("Exposure time: %1    Hazards: %2")
            .arg(formatExposureSeconds(summary.exposedAgentSeconds))
            .arg(summary.hazardCount),
        QString("Peak exposed: %1 people    Peak: %2")
            .arg(static_cast<int>(summary.peakExposedAgentCount))
            .arg(formatOptionalSeconds(summary.peakAtSeconds)),
        QString("Severity-weighted score: %1").arg(summary.exposureScore, 0, 'f', 1),
    }, parent);
}

QWidget* createHazardExposureReportPanel(
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    ResultItemSelectionHandler itemSelectionHandler,
    QWidget* parent) {
    auto parts = createResultReportPanel("Exposure", "Fire and smoke dwell-time impact", parent);
    const auto& summary = artifacts.hazardExposureSummary;
    if (summary.hazards.empty()) {
        auto* empty = createLabel("No hazard exposure data", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
        parts.contentLayout->addStretch(1);
        return parts.panel;
    }

    auto* byTypeHeader = createReportSectionHeader("By Type", parts.content);
    byTypeHeader->setToolTip("Exposure time is accumulated agent-seconds inside fire or smoke influence areas.");
    parts.contentLayout->addWidget(byTypeHeader);
    parts.contentLayout->addWidget(createHazardKindRow(
        "Fire",
        summarizeHazardKind(summary, safecrowd::domain::EnvironmentHazardKind::Fire),
        parts.content));
    parts.contentLayout->addWidget(createHazardKindRow(
        "Smoke",
        summarizeHazardKind(summary, safecrowd::domain::EnvironmentHazardKind::Smoke),
        parts.content));

    parts.contentLayout->addWidget(createReportSectionHeader("Hazards", parts.content));
    for (std::size_t index = 0; index < summary.hazards.size(); ++index) {
        const auto& hazard = summary.hazards[index];
        QStringList lines{
            QString("%1. %2 (%3)")
                .arg(static_cast<int>(index + 1))
                .arg(hazardDisplayName(hazard))
                .arg(hazardKindLabel(hazard.kind)),
            QString("Exposure: %1    Peak exposed: %2 people")
                .arg(formatExposureSeconds(hazard.exposedAgentSeconds))
                .arg(static_cast<int>(hazard.peakExposedAgentCount)),
            QString("First: %1    Peak: %2")
                .arg(formatOptionalSeconds(hazard.firstExposureSeconds))
                .arg(formatOptionalSeconds(hazard.peakAtSeconds)),
            QString("Severity: %1    Score: %2")
                .arg(severityLabel(hazard.severity))
                .arg(hazard.exposureScore, 0, 'f', 1),
        };

        QStringList locationParts;
        if (!hazard.affectedZoneId.empty()) {
            locationParts.push_back(QString("Zone: %1").arg(QString::fromStdString(hazard.affectedZoneId)));
        }
        if (!hazard.floorId.empty()) {
            locationParts.push_back(QString("Floor: %1").arg(QString::fromStdString(hazard.floorId)));
        }
        locationParts.push_back(QString("Position: %1, %2")
            .arg(hazard.position.x, 0, 'f', 1)
            .arg(hazard.position.y, 0, 'f', 1));
        lines.push_back(locationParts.join("    "));
        auto* row = createReportRowButton(lines, parts.content);
        QObject::connect(row, &QPushButton::clicked, parts.content, [itemSelectionHandler, index]() {
            if (itemSelectionHandler) {
                itemSelectionHandler(ScenarioResultNavigationView::HazardExposure, index);
            }
        });
        parts.contentLayout->addWidget(row);
    }

    parts.contentLayout->addStretch(1);
    return parts.panel;
}

QWidget* createZoneReportPanel(
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    ResultItemSelectionHandler itemSelectionHandler,
    QWidget* parent) {
    auto parts = createResultReportPanel("Zone", "Completion by source zone", parent);
    if (artifacts.zoneCompletion.empty()) {
        auto* empty = createLabel("No zone completion data", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
    } else {
        for (std::size_t index = 0; index < artifacts.zoneCompletion.size(); ++index) {
            const auto& zone = artifacts.zoneCompletion[index];
            auto* row = createReportRowButton({
                QString::fromStdString(zone.zoneLabel),
                QString("People: %1    Out: %2")
                    .arg(static_cast<int>(zone.initialCount))
                    .arg(static_cast<int>(zone.evacuatedCount)),
                QString("Last: %1").arg(formatOptionalSeconds(zone.lastCompletionTimeSeconds)),
            }, parts.content);
            QObject::connect(row, &QPushButton::clicked, parts.content, [itemSelectionHandler, index]() {
                if (itemSelectionHandler) {
                    itemSelectionHandler(ScenarioResultNavigationView::Zone, index);
                }
            });
            parts.contentLayout->addWidget(row);
        }
    }
    parts.contentLayout->addStretch(1);
    return parts.panel;
}

QWidget* createGroupsReportPanel(
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    ResultItemSelectionHandler itemSelectionHandler,
    QWidget* parent) {
    auto parts = createResultReportPanel("Groups", "Completion by crowd placement", parent);
    if (artifacts.placementCompletion.empty()) {
        auto* empty = createLabel("No group completion data", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
    } else {
        for (std::size_t index = 0; index < artifacts.placementCompletion.size(); ++index) {
            const auto& group = artifacts.placementCompletion[index];
            auto* row = createReportRowButton({
                QString::fromStdString(group.placementId),
                QString("People: %1    Out: %2")
                    .arg(static_cast<int>(group.initialCount))
                    .arg(static_cast<int>(group.evacuatedCount)),
                QString("Last: %1").arg(formatOptionalSeconds(group.lastCompletionTimeSeconds)),
            }, parts.content);
            QObject::connect(row, &QPushButton::clicked, parts.content, [itemSelectionHandler, index]() {
                if (itemSelectionHandler) {
                    itemSelectionHandler(ScenarioResultNavigationView::Groups, index);
                }
            });
            parts.contentLayout->addWidget(row);
        }
    }
    parts.contentLayout->addStretch(1);
    return parts.panel;
}

QWidget* createCrossFlowReportPanel(
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    std::function<void(std::size_t)> crossFlowCellFocusHandler,
    ResultItemSelectionHandler itemSelectionHandler,
    QWidget* parent) {
    auto parts = createResultReportPanel("Cross Flow", "Non-aligned movement streams", parent);
    auto* summaryHeader = createReportSectionHeader("Summary", parts.content);
    summaryHeader->setToolTip(safecrowd::domain::scenarioCrossFlowDefinition());
    parts.contentLayout->addWidget(summaryHeader);
    parts.contentLayout->addWidget(createReportInfoRow({
        QString("Peak cross-flow score: %1")
            .arg(artifacts.crossFlowSummary.peakCrossFlowScore, 0, 'f', 2),
        QString("Total exposure: %1 agent-sec")
            .arg(artifacts.crossFlowSummary.totalCrossFlowExposureAgentSeconds, 0, 'f', 1),
        QString("Longest duration: %1 sec")
            .arg(artifacts.crossFlowSummary.longestCrossFlowDurationSeconds, 0, 'f', 1),
        QString("Cross-flow hotspots: %1")
            .arg(static_cast<int>(artifacts.crossFlowSummary.crossFlowHotspotCount)),
    }, parts.content));

    auto* cellHeader = createReportSectionHeader("Cross-Flow Cells", parts.content);
    parts.contentLayout->addWidget(cellHeader);
    if (risk.crossFlowCells.empty()) {
        auto* empty = createLabel("None detected", parts.content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        parts.contentLayout->addWidget(empty);
    } else {
        for (std::size_t index = 0; index < risk.crossFlowCells.size(); ++index) {
            auto* row = createCrossFlowCellRowButton(risk.crossFlowCells[index], index, parts.content);
            QObject::connect(row, &QPushButton::clicked, parts.content, [crossFlowCellFocusHandler, itemSelectionHandler, index]() {
                if (itemSelectionHandler) {
                    itemSelectionHandler(ScenarioResultNavigationView::CrossFlow, index);
                }
                if (crossFlowCellFocusHandler) {
                    crossFlowCellFocusHandler(index);
                }
            });
            parts.contentLayout->addWidget(row);
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
            .id = "cross-flow",
            .label = "Cross Flow",
            .icon = makeResultNavigationIcon("cross-flow", QColor("#1f5fae")),
        },
        {
            .id = "hotspot",
            .label = "Hotspot",
            .icon = makeResultNavigationIcon("hotspot", QColor("#1f5fae")),
        },
        {
            .id = "exposure",
            .label = "Exposure",
            .icon = makeResultNavigationIcon("exposure", QColor("#1f5fae")),
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
    case ScenarioResultNavigationView::CrossFlow:
        return "cross-flow";
    case ScenarioResultNavigationView::Hotspot:
        return "hotspot";
    case ScenarioResultNavigationView::HazardExposure:
        return "exposure";
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
    if (tabId == "cross-flow") {
        return ScenarioResultNavigationView::CrossFlow;
    }
    if (tabId == "hotspot") {
        return ScenarioResultNavigationView::Hotspot;
    }
    if (tabId == "exposure") {
        return ScenarioResultNavigationView::HazardExposure;
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
    std::function<void(std::size_t)> crossFlowCellFocusHandler,
    std::function<void(std::size_t)> hotspotFocusHandler,
    std::function<void(ScenarioResultNavigationView, std::size_t)> itemSelectionHandler,
    QWidget* parent) {
    switch (view) {
    case ScenarioResultNavigationView::CrossFlow:
        return createCrossFlowReportPanel(
            risk,
            artifacts,
            std::move(crossFlowCellFocusHandler),
            std::move(itemSelectionHandler),
            parent);
    case ScenarioResultNavigationView::Hotspot:
        return createHotspotReportPanel(
            risk,
            std::move(hotspotFocusHandler),
            std::move(itemSelectionHandler),
            parent);
    case ScenarioResultNavigationView::HazardExposure:
        return createHazardExposureReportPanel(artifacts, std::move(itemSelectionHandler), parent);
    case ScenarioResultNavigationView::Zone:
        return createZoneReportPanel(artifacts, std::move(itemSelectionHandler), parent);
    case ScenarioResultNavigationView::Groups:
        return createGroupsReportPanel(artifacts, std::move(itemSelectionHandler), parent);
    case ScenarioResultNavigationView::Recommendations:
        return createResultReportPanel("Recommendations", "Recommended operational changes", parent).panel;
    case ScenarioResultNavigationView::Bottleneck:
    default:
        return createBottleneckReportPanel(
            risk,
            std::move(bottleneckFocusHandler),
            std::move(itemSelectionHandler),
            parent);
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

        if (!candidate.expectedImprovement.empty()) {
            auto* improvement = createLabel(
                QString("Expected: %1").arg(QString::fromStdString(candidate.expectedImprovement)),
                section,
                ui::FontRole::Caption);
            improvement->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            improvement->setStyleSheet(ui::subtleTextStyleSheet());
            sectionLayout->addWidget(improvement);
        }
        if (!candidate.recommendedScenario.variationDiffKeys.empty()) {
            QStringList changes;
            for (const auto& key : candidate.recommendedScenario.variationDiffKeys) {
                changes.push_back(QString::fromStdString(key));
            }
            auto* changesLabel = createLabel(
                QString("Changes: %1").arg(changes.join(", ")),
                section,
                ui::FontRole::Caption);
            changesLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            changesLabel->setStyleSheet(ui::subtleTextStyleSheet());
            sectionLayout->addWidget(changesLabel);
        }

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
