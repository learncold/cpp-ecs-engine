#include "application/ScenarioAuthoringWidget.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStringList>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "application/LayoutPreviewWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"
#include "domain/ImportResult.h"

namespace safecrowd::application {
namespace {

QString zoneLabel(const safecrowd::domain::Zone2D& zone) {
    const auto id = QString::fromStdString(zone.id);
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty() ? id : QString("%1  -  %2").arg(label, id);
}

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
}

QFrame* createCard(QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setStyleSheet(
        "QFrame { background: #ffffff; border: 1px solid #d7e0ea; border-radius: 14px; }"
        "QLabel { background: transparent; border: 0; }"
        "QComboBox, QLineEdit, QSpinBox {"
        " background: #ffffff;"
        " border: 1px solid #c9d5e2;"
        " border-radius: 10px;"
        " padding: 8px 10px;"
        " min-height: 24px;"
        "}"
        "QComboBox:focus, QLineEdit:focus, QSpinBox:focus { border-color: #1f5fae; }");
    return card;
}

QWidget* createNavigationRail(
    bool showLayout,
    std::function<void(bool)> switchViewHandler,
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

    const auto makeActivityButton = [&](const QIcon& icon, const QString& tooltip, bool checked, auto&& handler) {
        auto* button = new QToolButton(activityBar);
        button->setIcon(icon);
        button->setIconSize(QSize(22, 22));
        button->setToolTip(tooltip);
        button->setCheckable(true);
        button->setChecked(checked);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(56, 56);
        QObject::connect(button, &QToolButton::clicked, activityBar, handler);
        layout->addWidget(button);
        return button;
    };

    makeActivityButton(
        activityBar->style()->standardIcon(QStyle::SP_DirIcon),
        "Layout",
        showLayout,
        [switchViewHandler]() {
            switchViewHandler(true);
        });
    makeActivityButton(
        activityBar->style()->standardIcon(QStyle::SP_FileDialogDetailedView),
        "Crowd",
        !showLayout,
        [switchViewHandler]() {
            switchViewHandler(false);
        });
    layout->addStretch(1);
    return activityBar;
}

QWidget* createLayoutPanel(
    const safecrowd::domain::FacilityLayout2D& facilityLayout,
    QWidget* parent) {
    auto* content = new QWidget(parent);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = createLabel("Layout", content, ui::FontRole::Title);
    layout->addWidget(title);

    auto* summary = createLabel(
        QString("%1 zones\n%2 connections\n%3 walls")
            .arg(static_cast<int>(facilityLayout.zones.size()))
            .arg(static_cast<int>(facilityLayout.connections.size()))
            .arg(static_cast<int>(facilityLayout.barriers.size())),
        content);
    summary->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(summary);

    auto* scrollArea = new QScrollArea(content);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui::polishScrollArea(scrollArea);

    auto* scrollContent = new QWidget(scrollArea);
    auto* sectionsLayout = new QVBoxLayout(scrollContent);
    sectionsLayout->setContentsMargins(0, 0, 4, 0);
    sectionsLayout->setSpacing(10);

    const auto addZoneSection = [&](const QString& header, auto predicate) {
        bool hasRows = false;
        for (const auto& zone : facilityLayout.zones) {
            if (predicate(zone)) {
                hasRows = true;
                break;
            }
        }
        if (!hasRows) {
            return;
        }

        auto* sectionHeader = createLabel(header, scrollContent, ui::FontRole::SectionTitle);
        sectionHeader->setStyleSheet(ui::subtleTextStyleSheet());
        sectionsLayout->addWidget(sectionHeader);

        for (const auto& zone : facilityLayout.zones) {
            if (!predicate(zone)) {
                continue;
            }
            auto* row = new QPushButton(zoneLabel(zone), scrollContent);
            row->setFont(ui::font(ui::FontRole::Body));
            row->setStyleSheet(ui::ghostRowStyleSheet());
            sectionsLayout->addWidget(row);
        }
    };

    addZoneSection("Rooms", [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Room || zone.kind == safecrowd::domain::ZoneKind::Unknown;
    });
    addZoneSection("Paths", [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Corridor
            || zone.kind == safecrowd::domain::ZoneKind::Intersection
            || zone.kind == safecrowd::domain::ZoneKind::Stair;
    });
    addZoneSection("Exits", [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Exit;
    });

    sectionsLayout->addStretch(1);
    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea, 1);
    return content;
}

QWidget* createCrowdPanel(
    const safecrowd::domain::ScenarioDraft& draft,
    const std::vector<safecrowd::domain::OperationalEventDraft>& events,
    QWidget* parent) {
    auto* content = new QWidget(parent);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(createLabel("Crowd", content, ui::FontRole::Title));

    const auto placementCount = static_cast<int>(draft.population.initialPlacements.size());
    const auto agentCount = draft.population.initialPlacements.empty()
        ? 0
        : static_cast<int>(draft.population.initialPlacements.front().targetAgentCount);

    auto* summary = createLabel(
        QString("%1 placement\n%2 people\n%3 groups")
            .arg(placementCount)
            .arg(agentCount)
            .arg(placementCount > 0 ? 1 : 0),
        content);
    summary->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(summary);

    auto* sectionHeader = createLabel("Placements", content, ui::FontRole::SectionTitle);
    sectionHeader->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(sectionHeader);

    if (draft.population.initialPlacements.empty()) {
        auto* empty = createLabel("No pedestrian placements yet", content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        layout->addWidget(empty);
    } else {
        for (const auto& placement : draft.population.initialPlacements) {
            auto* row = new QPushButton(
                QString("%1 people  -  %2")
                    .arg(static_cast<int>(placement.targetAgentCount))
                    .arg(QString::fromStdString(placement.zoneId)),
                content);
            row->setFont(ui::font(ui::FontRole::Body));
            row->setStyleSheet(ui::ghostRowStyleSheet());
            layout->addWidget(row);
        }
    }

    auto* eventHeader = createLabel("Scenario Events", content, ui::FontRole::SectionTitle);
    eventHeader->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(eventHeader);

    if (events.empty()) {
        auto* empty = createLabel("No event markers yet", content);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        layout->addWidget(empty);
    } else {
        for (const auto& event : events) {
            auto* row = new QPushButton(QString::fromStdString(event.name), content);
            row->setFont(ui::font(ui::FontRole::Body));
            row->setStyleSheet(ui::ghostRowStyleSheet());
            layout->addWidget(row);
        }
    }

    layout->addStretch(1);
    return content;
}

void addFieldRow(QGridLayout* layout, int row, const QString& label, QWidget* field, QWidget* parent) {
    auto* fieldLabel = createLabel(label, parent, ui::FontRole::Caption);
    fieldLabel->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(fieldLabel, row, 0);
    layout->addWidget(field, row, 1);
}

}  // namespace

ScenarioAuthoringWidget::ScenarioAuthoringWidget(
    const QString& projectName,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(projectName),
      layout_(layout) {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    shell_ = new WorkspaceShell(this);
    shell_->setTools({"Project", "Scenario", "Run"});
    shell_->setSaveProjectHandler(std::move(saveProjectHandler));
    shell_->setOpenProjectHandler(std::move(openProjectHandler));
    refreshNavigationPanel();

    auto* inspector = new QWidget(shell_);
    auto* inspectorLayout = new QVBoxLayout(inspector);
    inspectorLayout->setContentsMargins(0, 0, 0, 0);
    inspectorLayout->setSpacing(12);
    inspectorLayout->addWidget(createLabel("Scenario", inspector, ui::FontRole::Title));
    draftSummaryLabel_ = createLabel("", inspector);
    draftSummaryLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    inspectorLayout->addWidget(draftSummaryLabel_);
    inspectorLayout->addStretch(1);
    readinessLabel_ = createLabel("", inspector);
    readinessLabel_->setStyleSheet(ui::mutedTextStyleSheet());
    inspectorLayout->addWidget(readinessLabel_);

    auto* runButton = new QPushButton("Run Ready", inspector);
    runButton->setFont(ui::font(ui::FontRole::Body));
    runButton->setStyleSheet(ui::primaryButtonStyleSheet());
    inspectorLayout->addWidget(runButton);
    shell_->setReviewPanel(inspector);

    showInitialForm();
    refreshDraftSummary();
    refreshReadiness();
    rootLayout->addWidget(shell_);
}

void ScenarioAuthoringWidget::refreshNavigationPanel() {
    if (shell_ == nullptr) {
        return;
    }

    shell_->setNavigationRail(createNavigationRail(
        navigationView_ == NavigationView::Layout,
        [this](bool showLayout) {
            navigationView_ = showLayout ? NavigationView::Layout : NavigationView::Crowd;
            refreshNavigationPanel();
        },
        shell_));

    if (navigationView_ == NavigationView::Layout) {
        shell_->setNavigationPanel(createLayoutPanel(layout_, shell_));
        return;
    }

    shell_->setNavigationPanel(createCrowdPanel(currentDraft(), events_, shell_));
}

void ScenarioAuthoringWidget::showInitialForm() {
    if (shell_ == nullptr) {
        return;
    }

    auto* canvasScroll = new QScrollArea(shell_);
    canvasScroll->setWidgetResizable(true);
    canvasScroll->setFrameShape(QFrame::NoFrame);
    ui::polishScrollArea(canvasScroll);

    auto* canvas = new QWidget(canvasScroll);
    canvas->setStyleSheet("QWidget { background: #f4f7fb; }");
    auto* canvasLayout = new QVBoxLayout(canvas);
    canvasLayout->setContentsMargins(28, 24, 28, 28);
    canvasLayout->setSpacing(16);

    auto* header = createLabel("Scenario Authoring", canvas, ui::FontRole::Title);
    canvasLayout->addWidget(header);

    auto* subtitle = createLabel(
        QString("%1 layout is approved. Enter the required scenario inputs before placing scenario elements.")
            .arg(projectName_),
        canvas);
    subtitle->setStyleSheet(ui::mutedTextStyleSheet());
    canvasLayout->addWidget(subtitle);

    auto* basicsCard = createCard(canvas);
    auto* basicsLayout = new QGridLayout(basicsCard);
    basicsLayout->setContentsMargins(18, 18, 18, 18);
    basicsLayout->setHorizontalSpacing(16);
    basicsLayout->setVerticalSpacing(12);

    scenarioNameEdit_ = new QLineEdit("Baseline evacuation", basicsCard);
    startZoneComboBox_ = new QComboBox(basicsCard);
    destinationComboBox_ = new QComboBox(basicsCard);
    for (const auto& zone : layout_.zones) {
        const auto id = QString::fromStdString(zone.id);
        startZoneComboBox_->addItem(zoneLabel(zone), id);
        if (zone.kind == safecrowd::domain::ZoneKind::Exit) {
            destinationComboBox_->addItem(zoneLabel(zone), id);
        }
    }
    if (destinationComboBox_->count() == 0) {
        for (const auto& zone : layout_.zones) {
            destinationComboBox_->addItem(zoneLabel(zone), QString::fromStdString(zone.id));
        }
    }

    occupantCountSpinBox_ = new QSpinBox(basicsCard);
    occupantCountSpinBox_->setRange(1, 50000);
    occupantCountSpinBox_->setValue(100);
    occupantCountSpinBox_->setSuffix(" people");

    addFieldRow(basicsLayout, 0, "Scenario name", scenarioNameEdit_, basicsCard);
    addFieldRow(basicsLayout, 1, "Start zone", startZoneComboBox_, basicsCard);
    addFieldRow(basicsLayout, 2, "Destination", destinationComboBox_, basicsCard);
    addFieldRow(basicsLayout, 3, "Population", occupantCountSpinBox_, basicsCard);
    canvasLayout->addWidget(basicsCard);

    createScenarioButton_ = new QPushButton("Create Scenario", canvas);
    createScenarioButton_->setFont(ui::font(ui::FontRole::Body));
    createScenarioButton_->setStyleSheet(ui::primaryButtonStyleSheet());
    canvasLayout->addWidget(createScenarioButton_, 0, Qt::AlignRight);
    canvasLayout->addStretch(1);

    canvasScroll->setWidget(canvas);
    shell_->setCanvas(canvasScroll);

    connect(createScenarioButton_, &QPushButton::clicked, this, [this]() {
        showScenarioCanvas();
    });

    const auto refresh = [this]() {
        refreshDraftSummary();
        refreshReadiness();
        refreshNavigationPanel();
    };
    connect(scenarioNameEdit_, &QLineEdit::textChanged, this, refresh);
    connect(startZoneComboBox_, &QComboBox::currentIndexChanged, this, refresh);
    connect(destinationComboBox_, &QComboBox::currentIndexChanged, this, refresh);
    connect(occupantCountSpinBox_, &QSpinBox::valueChanged, this, refresh);
}

void ScenarioAuthoringWidget::showScenarioCanvas() {
    if (shell_ == nullptr) {
        return;
    }

    scenarioDraft_ = currentDraft();
    selectedStartText_ = startZoneComboBox_ == nullptr ? QString{} : startZoneComboBox_->currentText();
    selectedDestinationText_ = destinationComboBox_ == nullptr ? QString{} : destinationComboBox_->currentText();
    refreshDraftSummary();
    refreshReadiness();

    safecrowd::domain::ImportResult result;
    result.layout = layout_;
    result.reviewStatus = safecrowd::domain::ImportReviewStatus::Approved;

    preview_ = new LayoutPreviewWidget(result, shell_);
    scenarioNameEdit_ = nullptr;
    startZoneComboBox_ = nullptr;
    destinationComboBox_ = nullptr;
    occupantCountSpinBox_ = nullptr;
    createScenarioButton_ = nullptr;
    shell_->setCanvas(preview_);
    navigationView_ = NavigationView::Crowd;
    refreshNavigationPanel();
}

void ScenarioAuthoringWidget::addEventDraft(const QString& name, const QString& trigger, const QString& target) {
    events_.push_back({
        .id = QString("event-%1").arg(static_cast<int>(events_.size()) + 1).toStdString(),
        .name = name.toStdString(),
        .triggerSummary = trigger.toStdString(),
        .targetSummary = target.toStdString(),
    });
    refreshDraftSummary();
    refreshReadiness();
}

void ScenarioAuthoringWidget::refreshDraftSummary() {
    if (draftSummaryLabel_ == nullptr) {
        return;
    }

    const auto draft = currentDraft();
    const auto startText = startZoneComboBox_ == nullptr ? selectedStartText_ : startZoneComboBox_->currentText();
    const auto destinationText = destinationComboBox_ == nullptr ? selectedDestinationText_ : destinationComboBox_->currentText();
    QStringList lines;
    lines << QString("Name: %1").arg(QString::fromStdString(draft.name));
    lines << QString("Population: %1").arg(
        draft.population.initialPlacements.empty()
            ? 0
            : static_cast<int>(draft.population.initialPlacements.front().targetAgentCount));
    lines << QString("Start: %1").arg(startText);
    lines << QString("Destination: %1").arg(destinationText);
    lines << QString("Events: %1").arg(static_cast<int>(events_.size()));
    for (const auto& event : events_) {
        lines << QString(" - %1 / %2").arg(QString::fromStdString(event.name), QString::fromStdString(event.triggerSummary));
    }
    draftSummaryLabel_->setText(lines.join('\n'));
}

void ScenarioAuthoringWidget::refreshReadiness() {
    if (readinessLabel_ == nullptr) {
        return;
    }

    QStringList missing;
    if (scenarioNameEdit_ != nullptr && scenarioNameEdit_->text().trimmed().isEmpty()) {
        missing << "scenario name";
    }
    if (startZoneComboBox_ != nullptr && startZoneComboBox_->currentData().toString().isEmpty()) {
        missing << "start zone";
    }
    if (destinationComboBox_ != nullptr && destinationComboBox_->currentData().toString().isEmpty()) {
        missing << "destination";
    }

    if (missing.isEmpty()) {
        readinessLabel_->setText("Ready for Sprint 1 run setup");
        return;
    }

    readinessLabel_->setText(QString("Missing required input: %1").arg(missing.join(", ")));
}

safecrowd::domain::ScenarioDraft ScenarioAuthoringWidget::currentDraft() const {
    if (scenarioNameEdit_ == nullptr && !scenarioDraft_.name.empty()) {
        return scenarioDraft_;
    }

    safecrowd::domain::ScenarioDraft draft;
    draft.scenarioId = "scenario-baseline";
    draft.name = scenarioNameEdit_ == nullptr
        ? "Baseline evacuation"
        : scenarioNameEdit_->text().trimmed().toStdString();
    draft.role = safecrowd::domain::ScenarioRole::Baseline;
    draft.sourceTemplateId = "sprint1-baseline";
    draft.execution.timeLimitSeconds = 600.0;
    draft.execution.sampleIntervalSeconds = 1.0;
    draft.execution.repeatCount = 1;
    draft.execution.baseSeed = 1;

    const auto startZoneId = startZoneComboBox_ == nullptr ? QString{} : startZoneComboBox_->currentData().toString();
    if (!startZoneId.isEmpty()) {
        draft.population.initialPlacements.push_back({
            .id = "placement-1",
            .zoneId = startZoneId.toStdString(),
            .targetAgentCount = static_cast<std::size_t>(occupantCountSpinBox_ == nullptr ? 0 : occupantCountSpinBox_->value()),
        });
    }

    draft.control.events = events_;
    return draft;
}

}  // namespace safecrowd::application
