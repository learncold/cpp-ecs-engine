#include "application/WorkspaceShell.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

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

}  // namespace

WorkspaceShell::WorkspaceShell(QWidget* parent)
    : QWidget(parent) {
    setObjectName("WorkspaceShell");
    setStyleSheet("#WorkspaceShell { background: #f4f7fb; }");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* topBar = new QFrame(this);
    topBar->setFixedHeight(48);
    topBar->setFrameShape(QFrame::StyledPanel);
    topBar->setLineWidth(1);
    topBar->setStyleSheet(
        "QFrame {"
        " background: #ffffff;"
        " border: 0;"
        " border-bottom: 1px solid #d7e0ea;"
        "}"
    );

    auto* topBarRootLayout = new QHBoxLayout(topBar);
    topBarRootLayout->setContentsMargins(16, 8, 16, 8);
    topBarRootLayout->setSpacing(4);

    auto* topBarLeft = new QWidget(topBar);
    topBarLayout_ = new QHBoxLayout(topBarLeft);
    topBarLayout_->setContentsMargins(0, 0, 0, 0);
    topBarLayout_->setSpacing(4);
    topBarRootLayout->addWidget(topBarLeft);
    topBarRootLayout->addStretch(1);

    auto* topBarTrailing = new QWidget(topBar);
    topBarTrailingLayout_ = new QHBoxLayout(topBarTrailing);
    topBarTrailingLayout_->setContentsMargins(0, 0, 0, 0);
    topBarTrailingLayout_->setSpacing(8);
    topBarRootLayout->addWidget(topBarTrailing);
    rootLayout->addWidget(topBar);

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    navigationCluster_ = new QWidget(this);
    auto* leftClusterLayout = new QHBoxLayout(navigationCluster_);
    leftClusterLayout->setContentsMargins(0, 0, 0, 0);
    leftClusterLayout->setSpacing(0);

    auto* navigationRail = new QWidget(navigationCluster_);
    navigationRail->setFixedWidth(56);
    navigationRailLayout_ = new QVBoxLayout(navigationRail);
    navigationRailLayout_->setContentsMargins(0, 0, 0, 0);
    navigationRailLayout_->setSpacing(0);
    leftClusterLayout->addWidget(navigationRail);

    auto* navigationPanel = createPanel(navigationCluster_);
    navigationPanel->setFixedWidth(260);
    navigationLayout_ = new QVBoxLayout(navigationPanel);
    navigationLayout_->setContentsMargins(18, 18, 18, 18);
    navigationLayout_->setSpacing(12);
    leftClusterLayout->addWidget(navigationPanel);
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
    reviewPanel_->setFixedWidth(280);
    reviewLayout_ = new QVBoxLayout(reviewPanel_);
    reviewLayout_->setContentsMargins(18, 18, 18, 18);
    reviewLayout_->setSpacing(12);
    bodyLayout->addWidget(reviewPanel_);

    rootLayout->addLayout(bodyLayout, 1);
}

void WorkspaceShell::setTools(const QStringList& tools) {
    clearTopBar();

    for (const auto& tool : tools) {
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

void WorkspaceShell::clearTopBar() {
    while (auto* item = topBarLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    openProjectAction_ = nullptr;
    saveProjectAction_ = nullptr;
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

void WorkspaceShell::setNavigationRail(QWidget* rail) {
    replaceSingleWidget(navigationRailLayout_, rail);
}

void WorkspaceShell::setNavigationPanel(QWidget* panel) {
    replaceSingleWidget(navigationLayout_, panel);
}

void WorkspaceShell::setNavigationVisible(bool visible) {
    if (navigationCluster_ != nullptr) {
        navigationCluster_->setVisible(visible);
    }
}

void WorkspaceShell::setReviewPanel(QWidget* panel) {
    replaceSingleWidget(reviewLayout_, panel);
}

void WorkspaceShell::setReviewPanelVisible(bool visible) {
    if (reviewPanel_ != nullptr) {
        reviewPanel_->setVisible(visible);
    }
}

void WorkspaceShell::setTopBarTrailingWidget(QWidget* widget) {
    replaceSingleWidget(topBarTrailingLayout_, widget);
}

void WorkspaceShell::setCanvas(QWidget* canvas) {
    replaceSingleWidget(canvasLayout_, canvas);
}

}  // namespace safecrowd::application
