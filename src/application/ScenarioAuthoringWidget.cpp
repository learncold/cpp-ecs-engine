#include "application/ScenarioAuthoringWidget.h"

#include <algorithm>
#include <utility>

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "application/LayoutNavigationPanelWidget.h"
#include "application/NavigationTreeWidget.h"
#include "application/ScenarioCanvasWidget.h"
#include "application/ScenarioRunWidget.h"
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

QWidget* createNavigationRail(
    ScenarioAuthoringWidget::NavigationView currentView,
    std::function<void(ScenarioAuthoringWidget::NavigationView)> switchViewHandler,
    const WorkspaceShell* shell,
    QWidget* parent) {
    auto* activityBar = new QFrame(parent);
    activityBar->setFixedWidth(56);
    activityBar->setStyleSheet(
        "QFrame { background: #eef3f8; border: 0; border-right: 1px solid #d7e0ea; border-radius: 0px; }"
        "QToolButton { background: transparent; border: 0; border-left: 3px solid transparent; border-radius: 0px; }"
        "QToolButton:hover { background: #e3ebf4; }"
        "QToolButton:checked { background: #ffffff; border-left-color: #1f5fae; }");

    auto* layout = new QVBoxLayout(activityBar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const auto makeActivityButton = [&](const QIcon& icon, const QString& tooltip, ScenarioAuthoringWidget::NavigationView view) {
        auto* button = new QToolButton(activityBar);
        button->setIcon(icon);
        button->setIconSize(QSize(22, 22));
        button->setToolTip(tooltip);
        button->setCheckable(true);
        button->setChecked(currentView == view);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(56, 56);
        QObject::connect(button, &QToolButton::clicked, activityBar, [switchViewHandler, view]() {
            switchViewHandler(view);
        });
        layout->addWidget(button);
    };

    makeActivityButton(makeLayoutIcon(QColor("#1f5fae")), "Layout", ScenarioAuthoringWidget::NavigationView::Layout);
    makeActivityButton(makeCrowdIcon(QColor("#1f5fae")), "Crowd", ScenarioAuthoringWidget::NavigationView::Crowd);
    makeActivityButton(makeEventsIcon(QColor("#1f5fae")), "Events", ScenarioAuthoringWidget::NavigationView::Events);
    layout->addStretch(1);
    if (shell != nullptr) {
        layout->addWidget(shell->createBackButton(activityBar), 0, Qt::AlignHCenter);
    }
    return activityBar;
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
                .detail = QString("Zone: %1\nVelocity: (%2, %3)")
                              .arg(placement.zoneId)
                              .arg(placement.velocity.x, 0, 'f', 2)
                              .arg(placement.velocity.y, 0, 'f', 2),
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
            .children = group ? std::move(occupants) : std::vector<NavigationTreeNode>{},
            .expanded = false,
        });
    }

    return placements;
}

QWidget* createCrowdPanel(
    const ScenarioAuthoringWidget::ScenarioState* scenario,
    std::function<void(const QString&)> selectPlacementHandler,
    const WorkspaceShell* shell,
    QWidget* parent) {
    return new NavigationTreeWidget(
        "Crowd",
        buildCrowdTree(scenario),
        "No pedestrian placements yet",
        std::move(selectPlacementHandler),
        parent,
        shell != nullptr ? shell->createPanelHeader("Crowd", parent, false) : nullptr);
}

std::vector<NavigationTreeNode> buildEventsTree(const ScenarioAuthoringWidget::ScenarioState* scenario) {
    if (scenario == nullptr || scenario->events.empty()) {
        return {};
    }

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

    return {{
        .label = QString("Events (%1)").arg(static_cast<int>(scenario->events.size())),
        .children = std::move(events),
        .expanded = true,
        .selectable = false,
    }};
}

QWidget* createEventsPanel(
    const ScenarioAuthoringWidget::ScenarioState* scenario,
    const WorkspaceShell* shell,
    QWidget* parent) {
    return new NavigationTreeWidget(
        "Events",
        buildEventsTree(scenario),
        "No operational events yet",
        {},
        parent,
        shell != nullptr ? shell->createPanelHeader("Events", parent, false) : nullptr);
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
    shell_->setTopBarTrailingWidget(createTopBarTogglePanel());
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
    refreshNavigationPanel();
    refreshInspector();
    if (rightPanelMode_ == RightPanelMode::Run) {
        refreshRightPanel();
    }
}

void ScenarioAuthoringWidget::createScenarioFromCurrent() {
    showScenarioNameDialog(currentScenarioIndex_);
}

void ScenarioAuthoringWidget::createScenarioWithName(const QString& name, int sourceIndex) {
    const auto trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return;
    }

    ScenarioState scenario;
    if (sourceIndex >= 0 && sourceIndex < static_cast<int>(scenarios_.size())) {
        scenario = scenarios_[sourceIndex];
        scenario.baseScenarioId = QString::fromStdString(scenarios_[sourceIndex].draft.scenarioId);
        scenario.draft.role = safecrowd::domain::ScenarioRole::Alternative;
        scenario.draft.variationDiffKeys = {"branch.duplicated"};
        scenario.stagedForRun = false;
    } else {
        scenario.draft.role = safecrowd::domain::ScenarioRole::Baseline;
        scenario.draft.sourceTemplateId = "sprint1-baseline";
        scenario.draft.execution.timeLimitSeconds = 600.0;
        scenario.draft.execution.sampleIntervalSeconds = 1.0;
        scenario.draft.execution.repeatCount = 1;
        scenario.draft.execution.baseSeed = 1;

        const auto* destinationZone = firstDestinationZone(layout_);
        const auto* startZone = firstStartZone(layout_);
        if (startZone != nullptr) {
            scenario.startText = zoneLabel(*startZone);
        }
        if (destinationZone != nullptr) {
            scenario.destinationText = zoneLabel(*destinationZone);
        }
    }

    scenario.draft.name = trimmedName.toStdString();
    scenario.draft.scenarioId = QString("scenario-%1").arg(scenarios_.size() + 1).toStdString();
    scenarios_.push_back(std::move(scenario));
    currentScenarioIndex_ = static_cast<int>(scenarios_.size()) - 1;
    refreshScenarioSwitcher();
    refreshCanvas();
    refreshNavigationPanel();
    refreshInspector();
    if (rightPanelMode_ == RightPanelMode::Run) {
        refreshRightPanel();
    }
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
    canvas_->setConnectionBlocks(scenario->draft.control.connectionBlocks);
    canvas_->setConnectionBlocksChangedHandler([this](const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks) {
        auto* current = currentScenario();
        if (current == nullptr) {
            return;
        }
        current->draft.control.connectionBlocks = blocks;
        refreshInspector();
        if (rightPanelMode_ == RightPanelMode::Run) {
            refreshRightPanel();
        }
    });
    shell_->setCanvas(canvas_);
}

void ScenarioAuthoringWidget::refreshInspector() {
    const auto* scenario = currentScenario();
    const bool hasScenario = scenario != nullptr;

    if (scenarioSummaryLabel_ != nullptr) {
        if (!hasScenario) {
            scenarioSummaryLabel_->setText("No scenario selected");
        } else {
            int people = 0;
            for (const auto& placement : scenario->crowdPlacements) {
                people += placement.occupantCount;
            }
            scenarioSummaryLabel_->setText(QString("Name: %1\nRole: %2\nPopulation: %3\nStart: %4\nDestination: %5\nEvents: %6")
                .arg(
                    QString::fromStdString(scenario->draft.name),
                    scenario->draft.role == safecrowd::domain::ScenarioRole::Baseline ? "Baseline" : "Alternative")
                .arg(people)
                .arg(scenario->startText, scenario->destinationText)
                .arg(static_cast<int>(scenario->events.size())));
        }
    }

    if (changesLabel_ != nullptr) {
        if (!hasScenario || scenario->baseScenarioId.isEmpty()) {
            changesLabel_->setText("Changes from baseline: none");
        } else {
            QStringList changes;
            if (!scenario->events.empty()) {
                changes << QString("Events: %1 configured").arg(static_cast<int>(scenario->events.size()));
            }
            if (changes.isEmpty()) {
                changes << "No changed fields yet";
            }
            changesLabel_->setText(QString("Based on: %1\nChanged:\n- %2")
                .arg(scenario->baseScenarioId, changes.join("\n- ")));
        }
    }

    if (newScenarioButton_ != nullptr) {
        newScenarioButton_->setText(hasScenario ? "New Scenario from Current" : "New Scenario");
    }
    if (stageScenarioButton_ != nullptr) {
        stageScenarioButton_->setEnabled(hasScenario);
        stageScenarioButton_->setText(hasScenario && scenario->stagedForRun ? "Staged for Run" : "Stage Scenario");
    }
}

void ScenarioAuthoringWidget::refreshNavigationPanel() {
    if (shell_ == nullptr) {
        return;
    }

    shell_->setNavigationRail(createNavigationRail(
        navigationView_,
        [this](NavigationView view) {
            navigationView_ = view;
            refreshNavigationPanel();
        },
        shell_,
        shell_));

    if (navigationView_ == NavigationView::Layout) {
        shell_->setNavigationPanel(new LayoutNavigationPanelWidget(
            &layout_,
            [this](const QString& elementId) {
                if (canvas_ != nullptr) {
                    canvas_->activateLayoutElement(elementId);
                }
            },
            shell_,
            shell_->createPanelHeader("Layout", shell_, false)));
        return;
    }
    if (navigationView_ == NavigationView::Crowd) {
        shell_->setNavigationPanel(createCrowdPanel(
            currentScenario(),
            [this](const QString& placementId) {
                if (canvas_ != nullptr) {
                    canvas_->focusPlacement(placementId);
                }
            },
            shell_,
            shell_));
        return;
    }
    shell_->setNavigationPanel(createEventsPanel(currentScenario(), shell_, shell_));
}

void ScenarioAuthoringWidget::refreshRightPanel() {
    scenarioSwitcher_ = nullptr;
    scenarioSummaryLabel_ = nullptr;
    changesLabel_ = nullptr;
    newScenarioButton_ = nullptr;
    stageScenarioButton_ = nullptr;
    stagedScenariosLabel_ = nullptr;
    executeRunButton_ = nullptr;

    if (shell_ == nullptr) {
        return;
    }

    shell_->setReviewPanelVisible(rightPanelMode_ != RightPanelMode::None);
    if (rightPanelMode_ == RightPanelMode::Scenario) {
        shell_->setReviewPanel(createScenarioPanel());
        refreshScenarioSwitcher();
        refreshInspector();
        return;
    }
    if (rightPanelMode_ == RightPanelMode::Run) {
        shell_->setReviewPanel(createRunPanel());
    }
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

void ScenarioAuthoringWidget::setRightPanelMode(RightPanelMode mode) {
    rightPanelMode_ = mode;
    if (scenarioPanelButton_ != nullptr) {
        scenarioPanelButton_->setChecked(mode == RightPanelMode::Scenario);
    }
    if (runPanelButton_ != nullptr) {
        runPanelButton_->setChecked(mode == RightPanelMode::Run);
    }
    refreshRightPanel();
}

void ScenarioAuthoringWidget::stageCurrentScenario() {
    auto* scenario = currentScenario();
    if (scenario == nullptr) {
        return;
    }

    scenario->stagedForRun = true;
    refreshInspector();
    if (rightPanelMode_ == RightPanelMode::Run) {
        refreshRightPanel();
    }
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
        initialPlacement.area.outline = placement.area;
        initialPlacement.targetAgentCount = static_cast<std::size_t>(placement.occupantCount);
        initialPlacement.initialVelocity = placement.velocity;
        scenario->draft.population.initialPlacements.push_back(std::move(initialPlacement));
    }

    refreshNavigationPanel();
    refreshInspector();
    if (rightPanelMode_ == RightPanelMode::Run) {
        refreshRightPanel();
    }
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

QWidget* ScenarioAuthoringWidget::createRunPanel() {
    auto* panel = new QWidget(shell_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(createLabel("Run", panel, ui::FontRole::Title));

    stagedScenariosLabel_ = createLabel("", panel);
    stagedScenariosLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    QStringList lines;
    const auto stagedCount = std::count_if(scenarios_.begin(), scenarios_.end(), [](const auto& scenario) {
        return scenario.stagedForRun;
    });
    if (stagedCount == 0) {
        lines << "No staged scenarios";
    } else {
        lines << "Staged scenarios";
        for (const auto& scenario : scenarios_) {
            if (!scenario.stagedForRun) {
                continue;
            }
            const auto role = scenario.draft.role == safecrowd::domain::ScenarioRole::Baseline ? "Baseline" : "Alternative";
            lines << QString("- %1 (%2)").arg(QString::fromStdString(scenario.draft.name), role);
        }
    }
    stagedScenariosLabel_->setText(lines.join('\n'));
    layout->addWidget(stagedScenariosLabel_);
    layout->addStretch(1);

    executeRunButton_ = new QPushButton("Run Staged Scenarios", panel);
    executeRunButton_->setFont(ui::font(ui::FontRole::Body));
    executeRunButton_->setStyleSheet(ui::primaryButtonStyleSheet());
    executeRunButton_->setEnabled(stagedCount > 0);
    layout->addWidget(executeRunButton_);
    connect(executeRunButton_, &QPushButton::clicked, this, [this]() {
        runFirstStagedBaselineScenario();
    });

    return panel;
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

    scenarioSummaryLabel_ = createLabel("", inspector);
    scenarioSummaryLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    inspectorLayout->addWidget(scenarioSummaryLabel_);

    changesLabel_ = createLabel("", inspector);
    changesLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    inspectorLayout->addWidget(changesLabel_);
    inspectorLayout->addStretch(1);

    stageScenarioButton_ = new QPushButton("Stage Scenario", inspector);
    stageScenarioButton_->setFont(ui::font(ui::FontRole::Body));
    stageScenarioButton_->setStyleSheet(ui::primaryButtonStyleSheet());
    inspectorLayout->addWidget(stageScenarioButton_);

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

    return inspector;
}

QWidget* ScenarioAuthoringWidget::createTopBarTogglePanel() {
    auto* panel = new QWidget(shell_);
    auto* layout = new QHBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    const auto buttonStyle =
        "QPushButton { background: #ffffff; border: 1px solid #c9d5e2; border-radius: 10px; color: #16202b; font-weight: 600; padding: 7px 14px; }"
        "QPushButton:hover { background: #eef3f8; }"
        "QPushButton:checked { background: #e6eef8; border-color: #1f5fae; color: #1f5fae; }";

    scenarioPanelButton_ = new QPushButton("Scenario", panel);
    scenarioPanelButton_->setCheckable(true);
    scenarioPanelButton_->setChecked(rightPanelMode_ == RightPanelMode::Scenario);
    scenarioPanelButton_->setFont(ui::font(ui::FontRole::Body));
    scenarioPanelButton_->setStyleSheet(buttonStyle);
    runPanelButton_ = new QPushButton("Run", panel);
    runPanelButton_->setCheckable(true);
    runPanelButton_->setChecked(rightPanelMode_ == RightPanelMode::Run);
    runPanelButton_->setFont(ui::font(ui::FontRole::Body));
    runPanelButton_->setStyleSheet(buttonStyle);

    layout->addWidget(scenarioPanelButton_);
    layout->addWidget(runPanelButton_);

    connect(scenarioPanelButton_, &QPushButton::clicked, this, [this](bool checked) {
        setRightPanelMode(checked ? RightPanelMode::Scenario : RightPanelMode::None);
    });
    connect(runPanelButton_, &QPushButton::clicked, this, [this](bool checked) {
        setRightPanelMode(checked ? RightPanelMode::Run : RightPanelMode::None);
    });

    return panel;
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
        return scenario.stagedForRun && scenario.draft.role == safecrowd::domain::ScenarioRole::Baseline;
    });
    return it == scenarios_.end() ? nullptr : &(*it);
}

}  // namespace safecrowd::application
