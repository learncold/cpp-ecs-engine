#include "application/ScenarioAuthoringWidget.h"

#include <algorithm>
#include <utility>

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLayoutItem>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include "application/LayoutNavigationPanelWidget.h"
#include "application/NavigationTreeWidget.h"
#include "application/ScenarioCanvasWidget.h"
#include "application/ScenarioRunWidget.h"
#include "application/ToolIconResources.h"
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

QString zoneLabel(const safecrowd::domain::Zone2D& zone) {
    const auto id = QString::fromStdString(zone.id);
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty() ? id : QString("%1  -  %2").arg(label, id);
}

QString zoneName(const safecrowd::domain::FacilityLayout2D& layout, const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    if (it == layout.zones.end()) {
        return QString::fromStdString(zoneId);
    }
    const auto label = QString::fromStdString(it->label);
    return label.isEmpty() ? QString::fromStdString(it->id) : label;
}

QString connectionLabel(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection) {
    const auto from = zoneName(layout, connection.fromZoneId);
    const auto to = zoneName(layout, connection.toZoneId);
    if (!from.isEmpty() && !to.isEmpty()) {
        return QString("%1 -> %2").arg(from, to);
    }
    return QString::fromStdString(connection.id);
}

QString connectionLabelForId(const safecrowd::domain::FacilityLayout2D& layout, const std::string& connectionId) {
    const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        return connection.id == connectionId;
    });
    if (it == layout.connections.end()) {
        return QString::fromStdString(connectionId);
    }
    return connectionLabel(layout, *it);
}

QString blockScheduleSummary(const safecrowd::domain::ConnectionBlockDraft& block) {
    if (block.intervals.empty()) {
        return "Always blocked";
    }

    QStringList intervals;
    for (const auto& interval : block.intervals) {
        intervals << QString("%1s - %2s").arg(interval.startSeconds, 0, 'f', 1).arg(interval.endSeconds, 0, 'f', 1);
    }
    return intervals.join(", ");
}

int draftOccupantCount(const safecrowd::domain::ScenarioDraft& scenario) {
    int total = 0;
    for (const auto& placement : scenario.population.initialPlacements) {
        total += static_cast<int>(placement.targetAgentCount);
    }
    return total;
}

QString signedDelta(int delta) {
    return delta > 0 ? QString("+%1").arg(delta) : QString::number(delta);
}

QString countChangeSummary(const QString& label, int baseline, int variant) {
    const int delta = variant - baseline;
    if (delta == 0) {
        return QString("%1 details changed").arg(label);
    }
    return QString("%1 %2 (%3 -> %4)").arg(label, signedDelta(delta)).arg(baseline).arg(variant);
}

QString boolValue(bool value) {
    return value ? "on" : "off";
}

QString buildChangeSummaryLine(
    const safecrowd::domain::ScenarioDraft& baseline,
    const safecrowd::domain::ScenarioDraft& variant,
    const std::string& key) {
    if (key == "population.placements") {
        const auto baselinePlacements = static_cast<int>(baseline.population.initialPlacements.size());
        const auto variantPlacements = static_cast<int>(variant.population.initialPlacements.size());
        QStringList parts;
        const int occupantDelta = draftOccupantCount(variant) - draftOccupantCount(baseline);
        if (occupantDelta != 0) {
            parts << QString("%1 occupants").arg(signedDelta(occupantDelta));
        }
        if (baselinePlacements != variantPlacements) {
            parts << countChangeSummary("placements", baselinePlacements, variantPlacements);
        }
        if (parts.isEmpty()) {
            parts << "placement details changed";
        }
        return QString("population.placements (%1)").arg(parts.join(", "));
    }
    if (key == "environment.reducedVisibility") {
        return QString("environment.reducedVisibility (%1 -> %2)")
            .arg(boolValue(baseline.environment.reducedVisibility), boolValue(variant.environment.reducedVisibility));
    }
    if (key == "environment.familiarityProfile") {
        return QString("environment.familiarityProfile (%1 -> %2)")
            .arg(QString::fromStdString(baseline.environment.familiarityProfile),
                 QString::fromStdString(variant.environment.familiarityProfile));
    }
    if (key == "environment.guidanceProfile") {
        return QString("environment.guidanceProfile (%1 -> %2)")
            .arg(QString::fromStdString(baseline.environment.guidanceProfile),
                 QString::fromStdString(variant.environment.guidanceProfile));
    }
    if (key == "control.events") {
        return QString("control.events (%1)")
            .arg(countChangeSummary("events", static_cast<int>(baseline.control.events.size()),
                                    static_cast<int>(variant.control.events.size())));
    }
    if (key == "control.connectionBlocks") {
        return QString("control.connectionBlocks (%1)")
            .arg(countChangeSummary("blocks", static_cast<int>(baseline.control.connectionBlocks.size()),
                                    static_cast<int>(variant.control.connectionBlocks.size())));
    }
    if (key == "execution.timeLimit") {
        return QString("execution.timeLimit (%1s -> %2s)")
            .arg(baseline.execution.timeLimitSeconds, 0, 'f', 1)
            .arg(variant.execution.timeLimitSeconds, 0, 'f', 1);
    }
    if (key == "execution.sampleInterval") {
        return QString("execution.sampleInterval (%1s -> %2s)")
            .arg(baseline.execution.sampleIntervalSeconds, 0, 'f', 1)
            .arg(variant.execution.sampleIntervalSeconds, 0, 'f', 1);
    }
    if (key == "execution.repeatCount") {
        return QString("execution.repeatCount (%1 -> %2)")
            .arg(baseline.execution.repeatCount)
            .arg(variant.execution.repeatCount);
    }
    if (key == "execution.baseSeed") {
        return QString("execution.baseSeed (%1 -> %2)")
            .arg(baseline.execution.baseSeed)
            .arg(variant.execution.baseSeed);
    }
    if (key == "execution.recordOccupantHistory") {
        return QString("execution.recordOccupantHistory (%1 -> %2)")
            .arg(boolValue(baseline.execution.recordOccupantHistory),
                 boolValue(variant.execution.recordOccupantHistory));
    }
    return QString::fromStdString(key);
}

QString changeCategoryLabel(const std::string& key) {
    if (key.rfind("population.", 0) == 0) {
        return "Crowd";
    }
    if (key.rfind("environment.", 0) == 0) {
        return "Layout";
    }
    if (key.rfind("control.", 0) == 0) {
        return "Events";
    }
    if (key.rfind("execution.", 0) == 0) {
        return "Run";
    }
    return "Change";
}

QString compactChangeSummary(const QString& summary) {
    auto compact = summary;
    compact.replace("population.placements", "crowd placements");
    compact.replace("environment.reducedVisibility", "layout visibility");
    compact.replace("environment.familiarityProfile", "layout familiarity");
    compact.replace("environment.guidanceProfile", "layout guidance");
    compact.replace("control.events", "events");
    compact.replace("control.connectionBlocks", "blocked events");
    compact.replace("execution.timeLimit", "run time limit");
    compact.replace("execution.sampleInterval", "run sample interval");
    compact.replace("execution.repeatCount", "run repeat count");
    compact.replace("execution.baseSeed", "run base seed");
    compact.replace("execution.recordOccupantHistory", "run occupant history");
    return compact;
}

QStringList buildChangeSummaryLines(
    const safecrowd::domain::ScenarioDraft& baseline,
    const safecrowd::domain::ScenarioDraft& variant) {
    QStringList changes;
    for (const auto& key : variant.variationDiffKeys) {
        changes << buildChangeSummaryLine(baseline, variant, key);
    }
    return changes;
}

void clearLayout(QLayout* layout) {
    if (layout == nullptr) {
        return;
    }
    while (auto* item = layout->takeAt(0)) {
        if (auto* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

QFrame* createInspectorCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setStyleSheet(
        "QFrame { background: #ffffff; border: 1px solid #d7e0ea; border-radius: 12px; }"
        "QLabel { background: transparent; border: 0; }");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    return card;
}

QLabel* createRoleBadge(const QString& text, bool alternative, QWidget* parent) {
    auto* badge = createLabel(text, parent, ui::FontRole::Caption);
    badge->setAlignment(Qt::AlignCenter);
    badge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    badge->setStyleSheet(alternative
        ? "QLabel { background: #fff7ed; border: 1px solid #fed7aa; border-radius: 9px; color: #9a3412; padding: 3px 8px; }"
        : "QLabel { background: #e6eef8; border: 1px solid #b8c6d6; border-radius: 9px; color: #1f5fae; padding: 3px 8px; }");
    return badge;
}

void addMetaRow(QVBoxLayout* layout, const QString& label, const QString& value, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    auto* labelWidget = createLabel(label, row, ui::FontRole::Caption);
    labelWidget->setStyleSheet(ui::subtleTextStyleSheet());
    labelWidget->setMinimumWidth(62);
    labelWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    auto* valueWidget = createLabel(value.isEmpty() ? "-" : value, row, ui::FontRole::Body);
    valueWidget->setStyleSheet(ui::mutedTextStyleSheet());
    valueWidget->setMinimumWidth(0);
    valueWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    rowLayout->addWidget(labelWidget);
    rowLayout->addWidget(valueWidget, 1);
    layout->addWidget(row);
}

void addStatusMessage(QVBoxLayout* layout, const QString& text, QWidget* parent) {
    auto* message = createLabel(text, parent, ui::FontRole::Body);
    message->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(message);
}

void addDiffRow(QVBoxLayout* layout, const QString& category, const QString& summary, QWidget* parent) {
    auto* row = new QFrame(parent);
    row->setStyleSheet(
        "QFrame { background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 10px; }"
        "QLabel { background: transparent; border: 0; }");
    auto* rowLayout = new QVBoxLayout(row);
    rowLayout->setContentsMargins(7, 6, 7, 7);
    rowLayout->setSpacing(5);

    auto* categoryBadge = createLabel(category, row, ui::FontRole::Caption);
    categoryBadge->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    categoryBadge->setStyleSheet(
        "QLabel { background: #e6eef8; border: 1px solid #c9d5e2; border-radius: 8px; color: #1f5fae; padding: 3px 7px; }");
    categoryBadge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    auto* summaryLabel = createLabel(summary, row, ui::FontRole::Caption);
    summaryLabel->setStyleSheet(ui::mutedTextStyleSheet());
    summaryLabel->setMinimumWidth(0);
    summaryLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    rowLayout->addWidget(categoryBadge);
    rowLayout->addWidget(summaryLabel);
    layout->addWidget(row);
}

int totalOccupantCount(const ScenarioAuthoringWidget::ScenarioState& scenario) {
    int total = 0;
    for (const auto& placement : scenario.crowdPlacements) {
        total += std::max(0, placement.occupantCount);
    }
    if (total > 0 || !scenario.crowdPlacements.empty()) {
        return total;
    }

    for (const auto& placement : scenario.draft.population.initialPlacements) {
        total += static_cast<int>(placement.targetAgentCount);
    }
    return total;
}

bool scenarioHasOccupants(const ScenarioAuthoringWidget::ScenarioState& scenario) {
    return totalOccupantCount(scenario) > 0;
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

QIcon makeCrowdIcon(const QColor& color) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QPointF(22, 12), 5.5, 5.5);
    painter.drawEllipse(QPointF(14, 18), 4.6, 4.6);
    painter.drawEllipse(QPointF(30, 18), 4.6, 4.6);
    painter.drawRoundedRect(QRectF(12, 24, 20, 8), 4, 4);
    painter.drawRoundedRect(QRectF(6, 28, 16, 7), 3.5, 3.5);
    painter.drawRoundedRect(QRectF(22, 28, 16, 7), 3.5, 3.5);
    return QIcon(pixmap);
}

QIcon makeEventsIcon(const QColor& color) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(13, 31), QPointF(22, 12));
    painter.drawLine(QPointF(22, 12), QPointF(31, 31));
    painter.drawLine(QPointF(16, 25), QPointF(28, 25));
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QPointF(22, 12), 4, 4);
    painter.drawEllipse(QPointF(13, 31), 4, 4);
    painter.drawEllipse(QPointF(31, 31), 4, 4);
    return QIcon(pixmap);
}

QIcon makeLayoutIcon(const QColor& color) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(11, 10, 9, 10));
    painter.drawRect(QRectF(24, 10, 9, 10));
    painter.drawRect(QRectF(11, 24, 22, 10));
    painter.drawLine(QPointF(20, 15), QPointF(24, 15));
    painter.drawLine(QPointF(22, 20), QPointF(22, 24));
    return QIcon(pixmap);
}

QIcon crowdTreeIcon(const QString& resourcePath, const QColor& color) {
    return makeSvgToolIcon(resourcePath, color, QSize(18, 18));
}

QIcon individualCrowdTreeIcon() {
    return crowdTreeIcon(
        QStringLiteral(":/tool-icons/scenario-authoring/individual-occupant.svg"),
        QColor("#1f5fae"));
}

QIcon groupCrowdTreeIcon() {
    return crowdTreeIcon(
        QStringLiteral(":/tool-icons/scenario-authoring/add-occupant-group.svg"),
        QColor("#1f5fae"));
}

std::vector<NavigationTreeNode> buildCrowdTree(const ScenarioAuthoringWidget::ScenarioState* scenario) {
    if (scenario == nullptr || scenario->crowdPlacements.empty()) {
        return {};
    }

    std::vector<NavigationTreeNode> placements;
    for (const auto& placement : scenario->crowdPlacements) {
        const bool group = placement.kind == ScenarioCrowdPlacementKind::Group;
        std::vector<NavigationTreeNode> occupants;
        for (int index = 1; index <= placement.occupantCount; ++index) {
            occupants.push_back({
                .label = QString("Occupant %1").arg(index),
                .id = QString("%1/occupant-%2").arg(placement.id).arg(index),
                .detail = QString("Floor: %1\nZone: %2\nVelocity: (%3, %4)")
                              .arg(placement.floorId)
                              .arg(placement.zoneId)
                              .arg(placement.velocity.x, 0, 'f', 2)
                              .arg(placement.velocity.y, 0, 'f', 2),
                .icon = individualCrowdTreeIcon(),
            });
        }

        placements.push_back({
            .label = QString("%1  -  %2  -  %3 %4")
                         .arg(
                             placement.name.isEmpty() ? placement.id : placement.name,
                             placement.zoneId)
                         .arg(placement.occupantCount)
                         .arg(placement.occupantCount == 1 ? "occupant" : "occupants"),
            .id = placement.id,
            .detail = QString("Velocity: (%1, %2)")
                          .arg(placement.velocity.x, 0, 'f', 2)
                          .arg(placement.velocity.y, 0, 'f', 2),
            .icon = group ? groupCrowdTreeIcon() : individualCrowdTreeIcon(),
            .children = group ? std::move(occupants) : std::vector<NavigationTreeNode>{},
            .expanded = false,
        });
    }

    return placements;
}

QWidget* createCrowdPanel(
    const ScenarioAuthoringWidget::ScenarioState* scenario,
    std::function<void(const QString&)> selectPlacementHandler,
    NavigationTreeState navigationState,
    std::function<void(const QSet<QString>&)> expandedStateChangedHandler,
    const WorkspaceShell* shell,
    QWidget* parent) {
    return new NavigationTreeWidget(
        "Crowd",
        buildCrowdTree(scenario),
        "No pedestrian placements yet",
        std::move(selectPlacementHandler),
        parent,
        shell != nullptr ? shell->createPanelHeader("Crowd", parent, false) : nullptr,
        std::move(navigationState),
        std::move(expandedStateChangedHandler));
}

std::vector<NavigationTreeNode> buildEventsTree(
    const safecrowd::domain::FacilityLayout2D& layout,
    const ScenarioAuthoringWidget::ScenarioState* scenario) {
    if (scenario == nullptr) {
        return {};
    }

    std::vector<NavigationTreeNode> sections;
    if (!scenario->events.empty()) {
        std::vector<NavigationTreeNode> events;
        for (const auto& event : scenario->events) {
            const auto eventId = QString::fromStdString(event.id);
            events.push_back({
                .label = QString::fromStdString(event.name),
                .id = eventId,
                .detail = QString::fromStdString(event.targetSummary),
                .children = {
                    {
                        .label = QString("Trigger  -  %1").arg(QString::fromStdString(event.triggerSummary)),
                        .id = QString("%1/trigger").arg(eventId),
                    },
                    {
                        .label = QString("Target  -  %1").arg(QString::fromStdString(event.targetSummary)),
                        .id = QString("%1/target").arg(eventId),
                    },
                },
                .expanded = true,
            });
        }

        sections.push_back({
            .label = QString("Operational Events (%1)").arg(static_cast<int>(scenario->events.size())),
            .children = std::move(events),
            .expanded = true,
            .selectable = false,
        });
    }

    const auto& connectionBlocks = scenario->draft.control.connectionBlocks;
    if (!connectionBlocks.empty()) {
        std::vector<NavigationTreeNode> blocks;
        for (const auto& block : connectionBlocks) {
            const auto blockId = QString::fromStdString(block.id);
            const auto targetLabel = connectionLabelForId(layout, block.connectionId);
            const auto schedule = blockScheduleSummary(block);
            blocks.push_back({
                .label = QString("Blocked  -  %1").arg(targetLabel),
                .id = blockId,
                .detail = schedule,
                .children = {
                    {
                        .label = QString("Target  -  %1").arg(targetLabel),
                        .id = QString("%1/target").arg(blockId),
                    },
                    {
                        .label = QString("Schedule  -  %1").arg(schedule),
                        .id = QString("%1/schedule").arg(blockId),
                    },
                },
                .expanded = true,
            });
        }

        sections.push_back({
            .label = QString("Blocked Doors / Exits (%1)").arg(static_cast<int>(connectionBlocks.size())),
            .children = std::move(blocks),
            .expanded = true,
            .selectable = false,
        });
    }

    return sections;
}

QWidget* createEventsPanel(
    const safecrowd::domain::FacilityLayout2D& layout,
    const ScenarioAuthoringWidget::ScenarioState* scenario,
    const WorkspaceShell* shell,
    QWidget* parent) {
    return new NavigationTreeWidget(
        "Events",
        buildEventsTree(layout, scenario),
        "No operational events or blocked exits yet",
        {},
        parent,
        shell != nullptr ? shell->createPanelHeader("Events", parent, false) : nullptr);
}

SavedNavigationView savedNavigationView(ScenarioAuthoringWidget::NavigationView view) {
    switch (view) {
    case ScenarioAuthoringWidget::NavigationView::Crowd:
        return SavedNavigationView::Crowd;
    case ScenarioAuthoringWidget::NavigationView::Events:
        return SavedNavigationView::Events;
    case ScenarioAuthoringWidget::NavigationView::Layout:
    default:
        return SavedNavigationView::Layout;
    }
}

}  // namespace

ScenarioAuthoringWidget::ScenarioAuthoringWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(projectName),
      layout_(layout),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)),
      backToLayoutReviewHandler_(std::move(backToLayoutReviewHandler)) {
    initializeUi(true);
}

ScenarioAuthoringWidget::ScenarioAuthoringWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    InitialState initialState,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void()> backToLayoutReviewHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(projectName),
      layout_(layout),
      saveProjectHandler_(std::move(saveProjectHandler)),
      openProjectHandler_(std::move(openProjectHandler)),
      backToLayoutReviewHandler_(std::move(backToLayoutReviewHandler)),
      scenarios_(std::move(initialState.scenarios)),
      currentScenarioIndex_(initialState.currentScenarioIndex),
      navigationView_(initialState.navigationView),
      rightPanelMode_(initialState.rightPanelMode) {
    for (auto& scenario : scenarios_) {
        if (!scenarioHasOccupants(scenario)) {
            scenario.stagedForRun = false;
        }
    }
    rightPanelMode_ = RightPanelMode::Scenario;
    initializeUi(false);
}

void ScenarioAuthoringWidget::initializeUi(bool promptForScenario) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    shell_ = new WorkspaceShell(this);
    shell_->setTools({"Project"});
    shell_->setSaveProjectHandler(saveProjectHandler_);
    shell_->setOpenProjectHandler(openProjectHandler_);
    shell_->setBackHandler(backToLayoutReviewHandler_);
    refreshRightPanel();
    rootLayout->addWidget(shell_);

    refreshNavigationPanel();
    refreshCanvas();
    refreshInspector();
    if (promptForScenario) {
        QTimer::singleShot(0, this, [this]() {
            ensureInitialScenarioPrompt();
        });
    }
}

SavedScenarioAuthoringState ScenarioAuthoringWidget::currentSavedState() const {
    SavedScenarioAuthoringState state;
    state.currentScenarioIndex = currentScenarioIndex_;
    state.navigationView = savedNavigationView(navigationView_);
    state.rightPanelMode = SavedRightPanelMode::Scenario;
    state.scenarios.reserve(scenarios_.size());
    for (const auto& scenario : scenarios_) {
        auto draft = scenario.draft;
        draft.control.events = scenario.events;
        state.scenarios.push_back({
            .draft = std::move(draft),
            .baseScenarioId = scenario.baseScenarioId.toStdString(),
            .stagedForRun = scenario.stagedForRun && scenarioHasOccupants(scenario),
        });
    }
    return state;
}

void ScenarioAuthoringWidget::addEventDraft(const QString& name, const QString& trigger, const QString& target) {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return;
    }

    const auto exists = std::any_of(scenario->events.begin(), scenario->events.end(), [&](const auto& event) {
        return QString::fromStdString(event.name) == name;
    });
    if (exists) {
        return;
    }

    scenario->events.push_back({
        .id = QString("event-%1").arg(static_cast<int>(scenario->events.size()) + 1).toStdString(),
        .name = name.toStdString(),
        .triggerSummary = trigger.toStdString(),
        .targetSummary = target.toStdString(),
    });
    scenario->draft.control.events = scenario->events;
    recomputeDiffKeysAfterScenarioChanged(*scenario);
    refreshNavigationPanel();
    refreshInspector();
}

void ScenarioAuthoringWidget::createScenarioFromCurrent() {
    showScenarioNameDialog(currentScenarioIndex_);
}

void ScenarioAuthoringWidget::createScenarioWithName(const QString& name, int sourceIndex) {
    const auto trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return;
    }

    const auto newScenarioId = QString("scenario-%1").arg(scenarios_.size() + 1).toStdString();
    ScenarioState scenario;
    if (sourceIndex >= 0 && sourceIndex < static_cast<int>(scenarios_.size())) {
        const auto& source = scenarios_[sourceIndex];
        scenario.draft = safecrowd::domain::duplicateScenarioDraft(
            source.draft, newScenarioId, trimmedName.toStdString());
        scenario.events = source.events;
        scenario.crowdPlacements = source.crowdPlacements;
        scenario.startText = source.startText;
        scenario.destinationText = source.destinationText;
        scenario.baseScenarioId = source.draft.role == safecrowd::domain::ScenarioRole::Alternative
            ? source.baseScenarioId
            : QString::fromStdString(source.draft.scenarioId);
        scenario.stagedForRun = false;
    } else {
        scenario.draft.role = safecrowd::domain::ScenarioRole::Baseline;
        scenario.draft.sourceTemplateId = "sprint1-baseline";
        scenario.draft.execution.timeLimitSeconds = 600.0;
        scenario.draft.execution.sampleIntervalSeconds = 1.0;
        scenario.draft.execution.repeatCount = 1;
        scenario.draft.execution.baseSeed = 1;
        scenario.draft.name = trimmedName.toStdString();
        scenario.draft.scenarioId = newScenarioId;

        const auto* destinationZone = firstDestinationZone(layout_);
        const auto* startZone = firstStartZone(layout_);
        if (startZone != nullptr) {
            scenario.startText = zoneLabel(*startZone);
        }
        if (destinationZone != nullptr) {
            scenario.destinationText = zoneLabel(*destinationZone);
        }
    }

    scenarios_.push_back(std::move(scenario));
    currentScenarioIndex_ = static_cast<int>(scenarios_.size()) - 1;
    recomputeVariationDiffKeysIfAlternative(scenarios_.back());
    refreshScenarioSwitcher();
    refreshCanvas();
    refreshNavigationPanel();
    refreshInspector();
}

void ScenarioAuthoringWidget::ensureInitialScenarioPrompt() {
    if (!scenarios_.empty()) {
        return;
    }

    showScenarioNameDialog(-1);
}

void ScenarioAuthoringWidget::refreshCanvas() {
    if (shell_ == nullptr || currentScenario() == nullptr) {
        showEmptyCanvas();
        return;
    }

    auto* scenario = currentScenario();
    canvas_ = new ScenarioCanvasWidget(layout_, shell_);
    canvas_->setPlacements(scenario->crowdPlacements);
    canvas_->setPlacementsChangedHandler([this](const std::vector<ScenarioCrowdPlacement>& placements) {
        updateCurrentScenarioPlacements(placements);
    });
    canvas_->setLayoutElementActivatedHandler([this](const QString& elementId) {
        selectedLayoutElementId_ = elementId;
        if (!elementId.isEmpty()) {
            selectedCrowdElementId_.clear();
            if (navigationView_ != NavigationView::Layout) {
                navigationView_ = NavigationView::Layout;
                refreshNavigationPanel();
                return;
            }
        }
        if (navigationView_ == NavigationView::Layout) {
            refreshNavigationPanel();
        }
    });
    canvas_->setCrowdSelectionChangedHandler([this](const QString& elementId) {
        selectedCrowdElementId_ = elementId;
        if (!elementId.isEmpty()) {
            selectedLayoutElementId_.clear();
            if (navigationView_ != NavigationView::Crowd) {
                navigationView_ = NavigationView::Crowd;
                refreshNavigationPanel();
                return;
            }
        }
        if (navigationView_ == NavigationView::Crowd) {
            refreshNavigationPanel();
        }
    });
    canvas_->setConnectionBlocks(scenario->draft.control.connectionBlocks);
    canvas_->setConnectionBlocksChangedHandler([this](const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks) {
        auto* current = currentScenario();
        if (current == nullptr) {
            return;
        }
        current->draft.control.connectionBlocks = blocks;
        recomputeDiffKeysAfterScenarioChanged(*current);
        refreshNavigationPanel();
        refreshInspector();
    });
    if (!selectedLayoutElementId_.isEmpty()) {
        canvas_->focusLayoutElement(selectedLayoutElementId_);
    } else if (!selectedCrowdElementId_.isEmpty()) {
        canvas_->focusPlacement(selectedCrowdElementId_);
    }
    shell_->setCanvas(canvas_);
}

void ScenarioAuthoringWidget::refreshInspector() {
    const auto* scenario = currentScenario();
    const bool hasScenario = scenario != nullptr;

    if (scenarioOverviewPanel_ != nullptr) {
        auto* panelLayout = qobject_cast<QVBoxLayout*>(scenarioOverviewPanel_->layout());
        clearLayout(panelLayout);
        if (panelLayout != nullptr) {
            auto* card = createInspectorCard(scenarioOverviewPanel_);
            auto* cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(12, 11, 12, 11);
            cardLayout->setSpacing(8);

            if (!hasScenario) {
                addStatusMessage(cardLayout, "No scenario selected", card);
            } else {
                const bool alternative = scenario->draft.role == safecrowd::domain::ScenarioRole::Alternative;
                cardLayout->addWidget(createRoleBadge(alternative ? "Alternative" : "Baseline", alternative, card));

                auto* nameLabel = createLabel(QString::fromStdString(scenario->draft.name), card, ui::FontRole::SectionTitle);
                nameLabel->setMinimumWidth(0);
                nameLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                cardLayout->addWidget(nameLabel);

                addMetaRow(cardLayout, "Population", QString::number(totalOccupantCount(*scenario)), card);
                addMetaRow(cardLayout, "Events", QString::number(static_cast<int>(scenario->events.size())), card);
                addMetaRow(cardLayout, "Blocked", QString::number(static_cast<int>(scenario->draft.control.connectionBlocks.size())), card);
                addMetaRow(cardLayout, "Start", scenario->startText, card);
                addMetaRow(cardLayout, "Destination", scenario->destinationText, card);
                if (alternative && !scenario->baseScenarioId.isEmpty()) {
                    addMetaRow(cardLayout, "Based on", scenario->baseScenarioId, card);
                }
            }
            panelLayout->addWidget(card);
        }
    }

    if (scenarioDiffPanel_ != nullptr) {
        auto* panelLayout = qobject_cast<QVBoxLayout*>(scenarioDiffPanel_->layout());
        clearLayout(panelLayout);
        if (panelLayout != nullptr) {
            auto* card = createInspectorCard(scenarioDiffPanel_);
            auto* cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(10, 10, 10, 10);
            cardLayout->setSpacing(7);

            auto* title = createLabel("Changes", card, ui::FontRole::SectionTitle);
            cardLayout->addWidget(title);

            if (!hasScenario) {
                addStatusMessage(cardLayout, "No scenario selected", card);
            } else if (scenario->draft.role == safecrowd::domain::ScenarioRole::Baseline) {
                addStatusMessage(cardLayout, "Baseline scenario", card);
            } else if (scenario->baseScenarioId.isEmpty()) {
                addStatusMessage(cardLayout, "Alternative scenario / no baseline link", card);
            } else {
                const auto baseId = scenario->baseScenarioId.toStdString();
                const auto baselineIt = std::find_if(scenarios_.begin(), scenarios_.end(), [&](const auto& candidate) {
                    return candidate.draft.scenarioId == baseId;
                });
                if (scenario->draft.variationDiffKeys.empty()) {
                    addStatusMessage(cardLayout, "No changed fields yet", card);
                } else {
                    for (const auto& key : scenario->draft.variationDiffKeys) {
                        const auto summary = baselineIt != scenarios_.end()
                            ? buildChangeSummaryLine(baselineIt->draft, scenario->draft, key)
                            : QString::fromStdString(key);
                        addDiffRow(cardLayout, changeCategoryLabel(key), compactChangeSummary(summary), card);
                    }
                }
            }
            panelLayout->addWidget(card);
        }
    }

    if (newScenarioButton_ != nullptr) {
        newScenarioButton_->setText(hasScenario ? "New Scenario from Current" : "New Scenario");
    }
    if (stageScenarioButton_ != nullptr) {
        const bool staged = hasScenario && scenario->stagedForRun;
        const bool hasOccupants = hasScenario && scenarioHasOccupants(*scenario);
        stageScenarioButton_->setEnabled(hasScenario && (staged || hasOccupants));
        stageScenarioButton_->setText(staged ? "Unstage" : "Stage Scenario");
        stageScenarioButton_->setToolTip(hasScenario && !staged && !hasOccupants
            ? "Add at least one occupant before staging this scenario."
            : QString{});
        stageScenarioButton_->setStyleSheet(staged
            ? QString(
                "QPushButton { background: #b42318; border: 1px solid #b42318; border-radius: 12px; color: white; font-weight: 600; padding: 10px 18px; }"
                "QPushButton:hover { background: #9f1f16; }"
                "QPushButton:disabled { background: #d7e0ea; border-color: #d7e0ea; color: #8a98a8; }")
            : QString(
                "QPushButton { background: #15803d; border: 1px solid #15803d; border-radius: 12px; color: white; font-weight: 600; padding: 10px 18px; }"
                "QPushButton:hover { background: #166534; }"
                "QPushButton:disabled { background: #d7e0ea; border-color: #d7e0ea; color: #8a98a8; }"));
    }
    const auto stagedCount = std::count_if(scenarios_.begin(), scenarios_.end(), [](const auto& scenario) {
        return scenario.stagedForRun && scenarioHasOccupants(scenario);
    });
    if (stagedScenariosLabel_ != nullptr) {
        QStringList lines;
        if (stagedCount == 0) {
            lines << "No staged scenarios";
        } else {
            lines << "Staged scenarios";
            for (const auto& stagedScenario : scenarios_) {
                if (!stagedScenario.stagedForRun || !scenarioHasOccupants(stagedScenario)) {
                    continue;
                }
                const auto role = stagedScenario.draft.role == safecrowd::domain::ScenarioRole::Baseline ? "Baseline" : "Alternative";
                lines << QString("- %1 (%2)").arg(QString::fromStdString(stagedScenario.draft.name), role);
            }
        }
        stagedScenariosLabel_->setText(lines.join('\n'));
    }
    if (executeRunButton_ != nullptr) {
        executeRunButton_->setEnabled(stagedCount > 0);
    }
}

void ScenarioAuthoringWidget::refreshNavigationPanel() {
    if (shell_ == nullptr) {
        return;
    }

    const auto activeTabId = [this]() {
        switch (navigationView_) {
        case NavigationView::Crowd:
            return QString("crowd");
        case NavigationView::Events:
            return QString("events");
        case NavigationView::Layout:
        default:
            return QString("layout");
        }
    }();
    shell_->setNavigationTabs(
        {
            {
                .id = "layout",
                .label = "Layout",
                .icon = makeLayoutIcon(QColor("#1f5fae")),
            },
            {
                .id = "crowd",
                .label = "Crowd",
                .icon = makeCrowdIcon(QColor("#1f5fae")),
            },
            {
                .id = "events",
                .label = "Events",
                .icon = makeEventsIcon(QColor("#1f5fae")),
            },
        },
        activeTabId,
        [this](const QString& tabId) {
            if (tabId == "crowd") {
                navigationView_ = NavigationView::Crowd;
            } else if (tabId == "events") {
                navigationView_ = NavigationView::Events;
            } else {
                navigationView_ = NavigationView::Layout;
            }
            refreshNavigationPanel();
        });

    if (navigationView_ == NavigationView::Layout) {
        shell_->setNavigationPanel(new LayoutNavigationPanelWidget(
            &layout_,
            [this](const QString& elementId) {
                selectedLayoutElementId_ = elementId;
                selectedCrowdElementId_.clear();
                if (canvas_ != nullptr) {
                    canvas_->activateLayoutElement(elementId);
                }
            },
            shell_,
            shell_->createPanelHeader("Layout", shell_, false),
            NavigationTreeState{
                .expandedNodeIds = layoutExpandedNodeIds_,
                .selectedId = selectedLayoutElementId_,
                .restoreExpandedState = true,
            },
            [this](const QSet<QString>& expandedNodeIds) {
                layoutExpandedNodeIds_ = expandedNodeIds;
            }));
        return;
    }
    if (navigationView_ == NavigationView::Crowd) {
        shell_->setNavigationPanel(createCrowdPanel(
            currentScenario(),
            [this](const QString& placementId) {
                selectedCrowdElementId_ = placementId;
                selectedLayoutElementId_.clear();
                if (canvas_ != nullptr) {
                    canvas_->focusPlacement(placementId);
                }
            },
            NavigationTreeState{
                .expandedNodeIds = crowdExpandedNodeIds_,
                .selectedId = selectedCrowdElementId_,
                .restoreExpandedState = true,
            },
            [this](const QSet<QString>& expandedNodeIds) {
                crowdExpandedNodeIds_ = expandedNodeIds;
            },
            shell_,
            shell_));
        return;
    }
    shell_->setNavigationPanel(createEventsPanel(layout_, currentScenario(), shell_, shell_));
}

void ScenarioAuthoringWidget::refreshRightPanel() {
    scenarioSwitcher_ = nullptr;
    scenarioOverviewPanel_ = nullptr;
    scenarioDiffPanel_ = nullptr;
    newScenarioButton_ = nullptr;
    stageScenarioButton_ = nullptr;
    stagedScenariosLabel_ = nullptr;
    executeRunButton_ = nullptr;

    if (shell_ == nullptr) {
        return;
    }

    rightPanelMode_ = RightPanelMode::Scenario;
    shell_->setReviewPanelVisible(true);
    shell_->setReviewPanel(createScenarioPanel());
    refreshScenarioSwitcher();
    refreshInspector();
}

void ScenarioAuthoringWidget::refreshScenarioSwitcher() {
    if (scenarioSwitcher_ == nullptr) {
        return;
    }

    scenarioSwitcher_->blockSignals(true);
    scenarioSwitcher_->clear();
    for (const auto& scenario : scenarios_) {
        const auto role = scenario.draft.role == safecrowd::domain::ScenarioRole::Baseline ? "Baseline" : "Alternative";
        scenarioSwitcher_->addItem(QString("%1  (%2)").arg(QString::fromStdString(scenario.draft.name), role));
    }
    scenarioSwitcher_->setCurrentIndex(currentScenarioIndex_);
    scenarioSwitcher_->blockSignals(false);
}

void ScenarioAuthoringWidget::runFirstStagedBaselineScenario() {
    const auto* scenario = firstStagedBaselineScenario();
    if (scenario == nullptr) {
        if (stagedScenariosLabel_ != nullptr) {
            stagedScenariosLabel_->setText(stagedScenariosLabel_->text()
                + "\n\nNo staged baseline scenario is ready to run.");
        }
        return;
    }

    auto* rootLayout = qobject_cast<QVBoxLayout*>(layout());
    if (rootLayout == nullptr || shell_ == nullptr) {
        return;
    }

    auto* runWidget = new ScenarioRunWidget(
        projectName_,
        layout_,
        scenario->draft,
        saveProjectHandler_,
        openProjectHandler_,
        backToLayoutReviewHandler_,
        this);
    rootLayout->replaceWidget(shell_, runWidget);
    shell_->hide();
    shell_->deleteLater();
    shell_ = nullptr;
}

void ScenarioAuthoringWidget::stageCurrentScenario() {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return;
    }

    if (!scenario->stagedForRun && !scenarioHasOccupants(*scenario)) {
        QMessageBox::warning(
            this,
            "Cannot stage scenario",
            "Add at least one occupant before staging this scenario.");
        refreshInspector();
        return;
    }

    scenario->stagedForRun = !scenario->stagedForRun;
    refreshInspector();
}

void ScenarioAuthoringWidget::updateCurrentScenarioPlacements(const std::vector<ScenarioCrowdPlacement>& placements) {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return;
    }

    scenario->crowdPlacements = placements;
    scenario->draft.population.initialPlacements.clear();
    for (const auto& placement : scenario->crowdPlacements) {
        safecrowd::domain::InitialPlacement2D initialPlacement;
        initialPlacement.id = placement.id.toStdString();
        initialPlacement.zoneId = placement.zoneId.toStdString();
        initialPlacement.floorId = placement.floorId.toStdString();
        initialPlacement.area.outline = placement.area;
        initialPlacement.targetAgentCount = static_cast<std::size_t>(placement.occupantCount);
        initialPlacement.initialVelocity = placement.velocity;
        initialPlacement.distribution = placement.distribution;
        initialPlacement.explicitPositions = placement.generatedPositions;
        scenario->draft.population.initialPlacements.push_back(std::move(initialPlacement));
    }
    if (!scenarioHasOccupants(*scenario)) {
        scenario->stagedForRun = false;
    }

    recomputeDiffKeysAfterScenarioChanged(*scenario);
    refreshNavigationPanel();
    refreshInspector();
}

void ScenarioAuthoringWidget::recomputeDiffKeysAfterScenarioChanged(ScenarioState& scenario) {
    recomputeVariationDiffKeysIfAlternative(scenario);
    if (scenario.draft.role == safecrowd::domain::ScenarioRole::Baseline) {
        recomputeDependentVariationDiffKeys(QString::fromStdString(scenario.draft.scenarioId));
    }
}

void ScenarioAuthoringWidget::recomputeDependentVariationDiffKeys(const QString& baselineId) {
    if (baselineId.isEmpty()) {
        return;
    }
    for (auto& scenario : scenarios_) {
        if (scenario.baseScenarioId == baselineId) {
            recomputeVariationDiffKeysIfAlternative(scenario);
        }
    }
}

void ScenarioAuthoringWidget::recomputeVariationDiffKeysIfAlternative(ScenarioState& scenario) const {
    if (scenario.draft.role != safecrowd::domain::ScenarioRole::Alternative
        || scenario.baseScenarioId.isEmpty()) {
        scenario.draft.variationDiffKeys.clear();
        return;
    }
    const auto baseId = scenario.baseScenarioId.toStdString();
    for (const auto& candidate : scenarios_) {
        if (candidate.draft.scenarioId == baseId) {
            scenario.draft.variationDiffKeys =
                safecrowd::domain::computeScenarioDiffKeys(candidate.draft, scenario.draft);
            return;
        }
    }
    scenario.draft.variationDiffKeys.clear();
}

void ScenarioAuthoringWidget::showEmptyCanvas() {
    auto* canvas = new QWidget(shell_);
    canvas->setStyleSheet("QWidget { background: #f4f7fb; }");
    auto* layout = new QVBoxLayout(canvas);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);
    layout->addStretch(1);

    auto* title = createLabel("Create a scenario", canvas, ui::FontRole::Title);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto* detail = createLabel("Name the first scenario to start authoring Layout, Crowd, and Events settings.", canvas);
    detail->setAlignment(Qt::AlignCenter);
    detail->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(detail);

    auto* button = new QPushButton("New Scenario", canvas);
    button->setFont(ui::font(ui::FontRole::Body));
    button->setStyleSheet(ui::primaryButtonStyleSheet());
    layout->addWidget(button, 0, Qt::AlignCenter);
    layout->addStretch(1);
    connect(button, &QPushButton::clicked, this, [this]() {
        showScenarioNameDialog(currentScenarioIndex_);
    });
    shell_->setCanvas(canvas);
}

void ScenarioAuthoringWidget::showScenarioNameDialog(int sourceIndex) {
    bool ok = false;
    const auto defaultName = sourceIndex >= 0
        ? QString("%1 alternative").arg(QString::fromStdString(scenarios_[sourceIndex].draft.name))
        : QString("Baseline evacuation");
    const auto name = QInputDialog::getText(
        this,
        "New Scenario",
        "Scenario name",
        QLineEdit::Normal,
        defaultName,
        &ok);
    if (!ok || name.trimmed().isEmpty()) {
        if (scenarios_.empty()) {
            refreshInspector();
            showEmptyCanvas();
        }
        return;
    }

    createScenarioWithName(name, sourceIndex);
}

QWidget* ScenarioAuthoringWidget::createScenarioPanel() {
    auto* inspector = new QWidget(shell_);
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    inspectorLayout->setSpacing(12);
    inspectorLayout->addWidget(createLabel("Scenario", inspector, ui::FontRole::Title));

    scenarioSwitcher_ = new QComboBox(inspector);
    scenarioSwitcher_->setFont(ui::font(ui::FontRole::Body));
    scenarioSwitcher_->setStyleSheet(
        "QComboBox { background: #ffffff; border: 1px solid #c9d5e2; border-radius: 10px; padding: 8px 10px; min-height: 24px; }");
    inspectorLayout->addWidget(scenarioSwitcher_);

    newScenarioButton_ = new QPushButton("New Scenario from Current", inspector);
    newScenarioButton_->setFont(ui::font(ui::FontRole::Body));
    newScenarioButton_->setStyleSheet(ui::secondaryButtonStyleSheet());
    inspectorLayout->addWidget(newScenarioButton_);

    scenarioOverviewPanel_ = new QWidget(inspector);
    auto* overviewLayout = new QVBoxLayout(scenarioOverviewPanel_);
    overviewLayout->setContentsMargins(0, 0, 0, 0);
    overviewLayout->setSpacing(0);
    inspectorLayout->addWidget(scenarioOverviewPanel_);

    scenarioDiffPanel_ = new QWidget(inspector);
    auto* diffLayout = new QVBoxLayout(scenarioDiffPanel_);
    diffLayout->setContentsMargins(0, 0, 0, 0);
    diffLayout->setSpacing(0);
    inspectorLayout->addWidget(scenarioDiffPanel_);

    stageScenarioButton_ = new QPushButton("Stage Scenario", inspector);
    stageScenarioButton_->setFont(ui::font(ui::FontRole::Body));
    inspectorLayout->addWidget(stageScenarioButton_);

    auto* separator = new QFrame(inspector);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet("QFrame { color: #d7e0ea; background: #d7e0ea; max-height: 1px; }");
    inspectorLayout->addWidget(separator);

    inspectorLayout->addWidget(createLabel("Run", inspector, ui::FontRole::Title));

    stagedScenariosLabel_ = createLabel("", inspector);
    stagedScenariosLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    QStringList lines;
    const auto stagedCount = std::count_if(scenarios_.begin(), scenarios_.end(), [](const auto& scenario) {
        return scenario.stagedForRun && scenarioHasOccupants(scenario);
    });
    if (stagedCount == 0) {
        lines << "No staged scenarios";
    } else {
        lines << "Staged scenarios";
        for (const auto& scenario : scenarios_) {
            if (!scenario.stagedForRun || !scenarioHasOccupants(scenario)) {
                continue;
            }
            const auto role = scenario.draft.role == safecrowd::domain::ScenarioRole::Baseline ? "Baseline" : "Alternative";
            lines << QString("- %1 (%2)").arg(QString::fromStdString(scenario.draft.name), role);
        }
    }
    stagedScenariosLabel_->setText(lines.join('\n'));
    inspectorLayout->addWidget(stagedScenariosLabel_);
    inspectorLayout->addStretch(1);

    executeRunButton_ = new QPushButton("Run Staged Scenarios", inspector);
    executeRunButton_->setFont(ui::font(ui::FontRole::Body));
    executeRunButton_->setStyleSheet(ui::primaryButtonStyleSheet());
    executeRunButton_->setEnabled(stagedCount > 0);
    inspectorLayout->addWidget(executeRunButton_);

    connect(scenarioSwitcher_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0 && index < static_cast<int>(scenarios_.size()) && index != currentScenarioIndex_) {
            currentScenarioIndex_ = index;
            refreshCanvas();
            refreshNavigationPanel();
            refreshInspector();
        }
    });
    connect(newScenarioButton_, &QPushButton::clicked, this, [this]() {
        createScenarioFromCurrent();
    });
    connect(stageScenarioButton_, &QPushButton::clicked, this, [this]() {
        stageCurrentScenario();
    });
    connect(executeRunButton_, &QPushButton::clicked, this, [this]() {
        runFirstStagedBaselineScenario();
    });

    return inspector;
}

ScenarioAuthoringWidget::ScenarioState* ScenarioAuthoringWidget::currentScenario() {
    if (currentScenarioIndex_ < 0 || currentScenarioIndex_ >= static_cast<int>(scenarios_.size())) {
        return nullptr;
    }
    return &scenarios_[currentScenarioIndex_];
}

const ScenarioAuthoringWidget::ScenarioState* ScenarioAuthoringWidget::currentScenario() const {
    if (currentScenarioIndex_ < 0 || currentScenarioIndex_ >= static_cast<int>(scenarios_.size())) {
        return nullptr;
    }
    return &scenarios_[currentScenarioIndex_];
}

const ScenarioAuthoringWidget::ScenarioState* ScenarioAuthoringWidget::firstStagedBaselineScenario() const {
    const auto it = std::find_if(scenarios_.begin(), scenarios_.end(), [](const auto& scenario) {
        return scenario.stagedForRun
            && scenarioHasOccupants(scenario)
            && scenario.draft.role == safecrowd::domain::ScenarioRole::Baseline;
    });
    return it == scenarios_.end() ? nullptr : &(*it);
}

}  // namespace safecrowd::application
