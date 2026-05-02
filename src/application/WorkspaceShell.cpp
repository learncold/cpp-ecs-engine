#include "application/WorkspaceShell.h"

#include <algorithm>

#include <QAction>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "application/UiStyle.h"

namespace safecrowd::application {
namespace {

QFrame* createPanel(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setLineWidth(1);
    frame->setStyleSheet(ui::panelStyleSheet());
    return frame;
}

void replaceSingleWidget(QBoxLayout* layout, QWidget* widget) {
    while (auto* item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    layout->addWidget(widget);
}

QPushButton* createFlatTopBarButton(QWidget* parent, const QString& text) {
    auto* button = new QPushButton(text, parent);
    button->setFont(ui::font(ui::FontRole::Body));
    button->setMinimumHeight(32);
    button->setMinimumWidth(92);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(
        "QPushButton {"
        " background: transparent;"
        " border: 0;"
        " border-radius: 0;"
        " color: #16202b;"
        " padding: 4px 10px;"
        " text-align: left;"
        "}"
        "QPushButton:hover {"
        " background: #eef3f8;"
        "}"
        "QPushButton::menu-indicator {"
        " subcontrol-origin: padding;"
        " subcontrol-position: center right;"
        " }");
    return button;
}

QIcon makeBackIcon(const QColor& color) {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(19, 9), QPointF(11, 16));
    painter.drawLine(QPointF(11, 16), QPointF(19, 23));
    return QIcon(pixmap);
}

QPushButton* createPanelBackButton(QWidget* parent) {
    auto* button = new QPushButton(parent);
    button->setIcon(makeBackIcon(QColor("#16202b")));
    button->setIconSize(QSize(22, 22));
    button->setFixedSize(32, 32);
    button->setToolTip("Back");
    button->setAccessibleName("Back");
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(
        "QPushButton {"
        " background: transparent;"
        " border: 0;"
        " border-radius: 10px;"
        "}"
        "QPushButton:hover {"
        " background: #eef3f8;"
        "}");
    return button;
}

}  // namespace

WorkspaceShell::WorkspaceShell(QWidget* parent)
    : WorkspaceShell(WorkspaceShellOptions{}, parent) {
}

WorkspaceShell::WorkspaceShell(WorkspaceShellOptions options, QWidget* parent)
    : QWidget(parent) {
    initialize(options);
}

void WorkspaceShell::initialize(const WorkspaceShellOptions& options) {
    setObjectName("WorkspaceShell");
    setStyleSheet("#WorkspaceShell { background: #f4f7fb; }");
    navigationRailWidth_ = options.navigationRailWidth;
    navigationPanelWidth_ = options.navigationPanelWidth;
    reviewPanelWidth_ = options.reviewPanelWidth;

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    topBar_ = new QFrame(this);
    topBar_->setFixedHeight(48);
    topBar_->setFrameShape(QFrame::StyledPanel);
    topBar_->setLineWidth(1);
    topBar_->setStyleSheet(
        "QFrame {"
        " background: #ffffff;"
        " border: 0;"
        " border-bottom: 1px solid #d7e0ea;"
        "}"
    );
    topBar_->setVisible(options.showTopBar);

    auto* topBarRootLayout = new QHBoxLayout(topBar_);
    topBarRootLayout->setContentsMargins(16, 8, 16, 8);
    topBarRootLayout->setSpacing(4);

    auto* topBarLeft = new QWidget(topBar_);
    topBarLayout_ = new QHBoxLayout(topBarLeft);
    topBarLayout_->setContentsMargins(0, 0, 0, 0);
    topBarLayout_->setSpacing(4);
    topBarRootLayout->addWidget(topBarLeft);
    topBarRootLayout->addStretch(1);

    auto* topBarTrailing = new QWidget(topBar_);
    auto* topBarTrailingRootLayout = new QHBoxLayout(topBarTrailing);
    topBarTrailingRootLayout->setContentsMargins(0, 0, 0, 0);
    topBarTrailingRootLayout->setSpacing(8);
    auto* topBarSystemTrailing = new QWidget(topBarTrailing);
    topBarSystemTrailingLayout_ = new QHBoxLayout(topBarSystemTrailing);
    topBarSystemTrailingLayout_->setContentsMargins(0, 0, 0, 0);
    topBarSystemTrailingLayout_->setSpacing(8);
    auto* topBarCustomTrailing = new QWidget(topBarTrailing);
    topBarTrailingLayout_ = new QHBoxLayout(topBarCustomTrailing);
    topBarTrailingLayout_->setContentsMargins(0, 0, 0, 0);
    topBarTrailingLayout_->setSpacing(8);
    topBarTrailingRootLayout->addWidget(topBarSystemTrailing);
    topBarTrailingRootLayout->addWidget(topBarCustomTrailing);
    topBarRootLayout->addWidget(topBarTrailing);
    rootLayout->addWidget(topBar_);

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    navigationCluster_ = new QWidget(this);
    auto* leftClusterLayout = new QHBoxLayout(navigationCluster_);
    leftClusterLayout->setContentsMargins(0, 0, 0, 0);
    leftClusterLayout->setSpacing(0);

    navigationRail_ = new QWidget(navigationCluster_);
    navigationRail_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    navigationRailLayout_ = new QVBoxLayout(navigationRail_);
    navigationRailLayout_->setContentsMargins(0, 0, 0, 0);
    navigationRailLayout_->setSpacing(0);
    rebuildDefaultNavigationRail();
    leftClusterLayout->addWidget(navigationRail_);

    navigationPanel_ = createPanel(navigationCluster_);
    navigationPanel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    navigationLayout_ = new QVBoxLayout(navigationPanel_);
    navigationLayout_->setContentsMargins(18, 18, 18, 18);
    navigationLayout_->setSpacing(12);
    leftClusterLayout->addWidget(navigationPanel_);
    bodyLayout->addWidget(navigationCluster_);

    auto* centerStack = new QWidget(this);
    auto* centerLayout = new QVBoxLayout(centerStack);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    auto* canvasPanel = new QWidget(centerStack);
    canvasPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    canvasLayout_ = new QVBoxLayout(canvasPanel);
    canvasLayout_->setContentsMargins(0, 0, 0, 0);
    canvasLayout_->setSpacing(0);
    centerLayout->addWidget(canvasPanel, 1);

    bodyLayout->addWidget(centerStack, 1);

    reviewPanel_ = createPanel(this);
    reviewPanel_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    reviewLayout_ = new QVBoxLayout(reviewPanel_);
    reviewLayout_->setContentsMargins(18, 18, 18, 18);
    reviewLayout_->setSpacing(12);
    bodyLayout->addWidget(reviewPanel_);

    rootLayout->addLayout(bodyLayout, 1);
    if (options.showReviewPanelToggle) {
        reviewPanelToggleButton_ = new QPushButton(topBar_);
        reviewPanelToggleButton_->setFont(ui::font(ui::FontRole::Body));
        reviewPanelToggleButton_->setCursor(Qt::PointingHandCursor);
        reviewPanelToggleButton_->setMinimumHeight(32);
        reviewPanelToggleButton_->setStyleSheet(
            "QPushButton {"
            " background: #ffffff;"
            " border: 1px solid #d7e0ea;"
            " border-radius: 8px;"
            " color: #16202b;"
            " font-weight: 600;"
            " padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            " background: #eef3f8;"
            " border-color: #b8c6d6;"
            "}");
        reviewPanelToggleButton_->setToolTip("Show or hide the right panel.");
        reviewPanelToggleButton_->setAccessibleName("Toggle right panel");
        topBarSystemTrailingLayout_->addWidget(reviewPanelToggleButton_);
        connect(reviewPanelToggleButton_, &QPushButton::clicked, this, [this]() {
            setReviewPanelVisible(!reviewPanelVisible_);
        });
    }
    setNavigationMode(options.navigationMode);
    setReviewPanelVisible(options.showReviewPanel);
}

void WorkspaceShell::setFixedWidthVisible(QWidget* widget, bool visible, int width) {
    if (widget == nullptr) {
        return;
    }

    const auto clampedWidth = std::max(0, width);
    widget->setVisible(visible);
    widget->setMinimumWidth(visible ? clampedWidth : 0);
    widget->setMaximumWidth(visible ? clampedWidth : 0);
    widget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void WorkspaceShell::updateReviewPanelToggle() {
    if (reviewPanelToggleButton_ == nullptr) {
        return;
    }
    reviewPanelToggleButton_->setText(reviewPanelVisible_ ? "Hide Panel" : "Show Panel");
}

QWidget* WorkspaceShell::createDefaultNavigationRail() {
    auto* rail = new QFrame(navigationRail_);
    rail->setFixedWidth(navigationRailWidth_);
    rail->setStyleSheet(
        "QFrame {"
        " background: #eef3f8;"
        " border: 0;"
        " border-right: 1px solid #d7e0ea;"
        " border-radius: 0px;"
        "}"
    );

    auto* layout = new QVBoxLayout(rail);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setSpacing(0);
    layout->addStretch(1);
    if (backHandler_) {
        layout->addWidget(createBackButton(rail), 0, Qt::AlignHCenter);
    }
    return rail;
}

QWidget* WorkspaceShell::createNavigationTabRail() {
    auto* rail = new QFrame(navigationRail_);
    rail->setFixedWidth(navigationRailWidth_);
    rail->setStyleSheet(
        "QFrame { background: #eef3f8; border: 0; border-right: 1px solid #d7e0ea; border-radius: 0px; }"
        "QToolButton { background: transparent; border: 0; border-left: 3px solid transparent; border-radius: 0px; padding: 0px; }"
        "QToolButton:hover { background: #e3ebf4; }"
        "QToolButton:checked { background: #ffffff; border-left-color: #1f5fae; }");

    auto* layout = new QVBoxLayout(rail);
    layout->setContentsMargins(0, 0, 0, 12);
    layout->setSpacing(0);

    for (const auto& tab : navigationTabs_) {
        auto* button = new QToolButton(rail);
        button->setToolTip(tab.label);
        button->setAccessibleName(tab.label);
        button->setCheckable(true);
        button->setChecked(tab.id == activeNavigationTabId_);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(navigationRailWidth_, navigationRailWidth_);
        button->setFont(ui::font(ui::FontRole::Caption));
        if (!tab.icon.isNull()) {
            button->setIcon(tab.icon);
            button->setIconSize(QSize(22, 22));
        } else {
            button->setText(tab.label);
        }
        connect(button, &QToolButton::clicked, this, [this, tabId = tab.id]() {
            handleNavigationTabClicked(tabId);
        });
        layout->addWidget(button);
    }

    layout->addStretch(1);
    if (backHandler_) {
        layout->addWidget(createBackButton(rail), 0, Qt::AlignHCenter);
    }
    return rail;
}

void WorkspaceShell::rebuildDefaultNavigationRail() {
    if (navigationRailLayout_ == nullptr || customNavigationRail_) {
        return;
    }
    replaceSingleWidget(navigationRailLayout_, createDefaultNavigationRail());
}

void WorkspaceShell::setTools(const QStringList& tools) {
    tools_ = tools;
    rebuildTopBar();
}

void WorkspaceShell::setBackHandler(std::function<void()> handler) {
    backHandler_ = std::move(handler);
    if (!customNavigationRail_) {
        rebuildDefaultNavigationRail();
    }
}

QPushButton* WorkspaceShell::createBackButton(QWidget* parent) const {
    auto* button = createPanelBackButton(parent);
    connect(button, &QPushButton::clicked, button, [this]() {
        if (backHandler_) {
            backHandler_();
        }
    });
    return button;
}

QWidget* WorkspaceShell::createPanelHeader(const QString& title, QWidget* parent, bool includeBackButton) const {
    auto* header = new QWidget(parent);
    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    if (includeBackButton && backHandler_) {
        layout->addWidget(createBackButton(header), 0, Qt::AlignVCenter);
    }

    auto* label = new QLabel(title, header);
    label->setFont(ui::font(ui::FontRole::Title));
    label->setWordWrap(false);
    layout->addWidget(label, 1, Qt::AlignVCenter);
    return header;
}

void WorkspaceShell::clearTopBar() {
    while (auto* item = topBarLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    openProjectAction_ = nullptr;
    saveProjectAction_ = nullptr;
}

void WorkspaceShell::rebuildTopBar() {
    clearTopBar();

    for (const auto& tool : tools_) {
        auto* button = createTopBarButton(tool);
        if (tool == "Project") {
            auto* menu = new QMenu(button);
            openProjectAction_ = menu->addAction("Open Project");
            connect(openProjectAction_, &QAction::triggered, this, [this]() {
                if (openProjectHandler_) {
                    openProjectHandler_();
                }
            });
            saveProjectAction_ = menu->addAction("Save Project");
            connect(saveProjectAction_, &QAction::triggered, this, [this]() {
                if (saveProjectHandler_) {
                    saveProjectHandler_();
                }
            });
            button->setMenu(menu);
        }
        topBarLayout_->addWidget(button);
    }
}

QPushButton* WorkspaceShell::createTopBarButton(const QString& text) {
    return createFlatTopBarButton(this, text);
}

void WorkspaceShell::setSaveProjectHandler(std::function<void()> handler) {
    saveProjectHandler_ = std::move(handler);
}

void WorkspaceShell::setOpenProjectHandler(std::function<void()> handler) {
    openProjectHandler_ = std::move(handler);
}

void WorkspaceShell::setNavigationTabs(
    std::vector<WorkspaceNavigationTab> tabs,
    const QString& activeTabId,
    std::function<void(const QString&)> tabChangedHandler) {
    if (!activeNavigationTabId_.isEmpty() && activeNavigationTabId_ != activeTabId) {
        navigationPanelCollapsed_ = false;
    }
    navigationTabs_ = std::move(tabs);
    activeNavigationTabId_ = activeTabId;
    navigationTabChangedHandler_ = std::move(tabChangedHandler);
    customNavigationRail_ = true;
    replaceSingleWidget(navigationRailLayout_, createNavigationTabRail());
    applyNavigationMode();
}

void WorkspaceShell::setNavigationRail(QWidget* rail) {
    customNavigationRail_ = true;
    replaceSingleWidget(navigationRailLayout_, rail);
}

void WorkspaceShell::setNavigationPanel(QWidget* panel) {
    replaceSingleWidget(navigationLayout_, panel);
}

void WorkspaceShell::setNavigationVisible(bool visible) {
    setNavigationMode(visible ? WorkspaceNavigationMode::RailAndPanel : WorkspaceNavigationMode::None);
}

void WorkspaceShell::setNavigationMode(WorkspaceNavigationMode mode) {
    navigationMode_ = mode;
    if (mode == WorkspaceNavigationMode::None || mode == WorkspaceNavigationMode::RailOnly) {
        navigationPanelCollapsed_ = false;
    }
    applyNavigationMode();
}

void WorkspaceShell::applyNavigationMode() {
    if (navigationCluster_ == nullptr || navigationRail_ == nullptr || navigationPanel_ == nullptr) {
        return;
    }

    const auto showRail = navigationMode_ == WorkspaceNavigationMode::RailOnly
        || navigationMode_ == WorkspaceNavigationMode::PanelOnly
        || navigationMode_ == WorkspaceNavigationMode::RailAndPanel;
    const auto modeShowsPanel = navigationMode_ == WorkspaceNavigationMode::PanelOnly
        || navigationMode_ == WorkspaceNavigationMode::RailAndPanel;
    const auto showPanel = modeShowsPanel && !navigationPanelCollapsed_;
    setFixedWidthVisible(navigationRail_, showRail, navigationRailWidth_);
    setFixedWidthVisible(navigationPanel_, showPanel, navigationPanelWidth_);
    navigationCluster_->setVisible(showRail || showPanel);
}

void WorkspaceShell::handleNavigationTabClicked(const QString& tabId) {
    if (tabId == activeNavigationTabId_) {
        navigationPanelCollapsed_ = !navigationPanelCollapsed_;
        applyNavigationMode();
        return;
    }

    activeNavigationTabId_ = tabId;
    navigationPanelCollapsed_ = false;
    applyNavigationMode();
    if (navigationTabChangedHandler_) {
        navigationTabChangedHandler_(tabId);
    }
}

void WorkspaceShell::setReviewPanel(QWidget* panel) {
    replaceSingleWidget(reviewLayout_, panel);
}

void WorkspaceShell::setReviewPanelVisible(bool visible) {
    reviewPanelVisible_ = visible;
    setFixedWidthVisible(reviewPanel_, visible, reviewPanelWidth_);
    updateReviewPanelToggle();
}

void WorkspaceShell::setTopBarTrailingWidget(QWidget* widget) {
    replaceSingleWidget(topBarTrailingLayout_, widget);
}

void WorkspaceShell::setCanvas(QWidget* canvas) {
    replaceSingleWidget(canvasLayout_, canvas);
}

}  // namespace safecrowd::application
