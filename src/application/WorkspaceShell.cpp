#include "application/WorkspaceShell.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QMenu>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace safecrowd::application {
namespace {

QFont makeWorkspaceFont(int pointSize) {
    QFont font;
    font.setPointSize(pointSize);
    font.setWeight(QFont::Normal);
    return font;
}

QFrame* createPanel(QWidget* parent) {
    auto* frame = new QFrame(parent);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setLineWidth(1);
    frame->setStyleSheet("QFrame { border: 1px solid #222222; background: #ffffff; }");
    return frame;
}

void replaceSingleWidget(QBoxLayout* layout, QWidget* widget) {
    while (auto* item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    layout->addWidget(widget);
}

}  // namespace

WorkspaceShell::WorkspaceShell(QWidget* parent)
    : QWidget(parent) {
    setObjectName("WorkspaceShell");
    setStyleSheet("#WorkspaceShell { background: #ffffff; }");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* topBar = new QFrame(this);
    topBar->setFixedHeight(54);
    topBar->setFrameShape(QFrame::StyledPanel);
    topBar->setLineWidth(1);
    topBar->setStyleSheet("QFrame { border: 1px solid #222222; background: #ffffff; }");

    topBarLayout_ = new QHBoxLayout(topBar);
    topBarLayout_->setContentsMargins(0, 0, 0, 0);
    topBarLayout_->setSpacing(0);
    rootLayout->addWidget(topBar);

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto* navigationPanel = createPanel(this);
    navigationPanel->setFixedWidth(194);
    navigationLayout_ = new QVBoxLayout(navigationPanel);
    navigationLayout_->setContentsMargins(14, 14, 14, 14);
    navigationLayout_->setSpacing(10);
    bodyLayout->addWidget(navigationPanel);

    auto* centerStack = new QWidget(this);
    auto* centerLayout = new QVBoxLayout(centerStack);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    auto* canvasPanel = createPanel(centerStack);
    canvasPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    canvasLayout_ = new QVBoxLayout(canvasPanel);
    canvasLayout_->setContentsMargins(0, 0, 0, 0);
    canvasLayout_->setSpacing(0);
    centerLayout->addWidget(canvasPanel, 1);

    auto* bottomPanel = createPanel(centerStack);
    bottomPanel->setFixedHeight(154);
    bottomLayout_ = new QVBoxLayout(bottomPanel);
    bottomLayout_->setContentsMargins(14, 14, 14, 14);
    bottomLayout_->setSpacing(0);
    centerLayout->addWidget(bottomPanel, 0);

    bodyLayout->addWidget(centerStack, 1);

    auto* reviewPanel = createPanel(this);
    reviewPanel->setFixedWidth(230);
    reviewLayout_ = new QVBoxLayout(reviewPanel);
    reviewLayout_->setContentsMargins(14, 14, 14, 14);
    reviewLayout_->setSpacing(10);
    bodyLayout->addWidget(reviewPanel);

    rootLayout->addLayout(bodyLayout, 1);
}

void WorkspaceShell::setTools(const QStringList& tools) {
    clearTopBar();

    for (const auto& tool : tools) {
        auto* button = createTopBarButton(tool);
        if (tool == "Project") {
            auto* menu = new QMenu(button);
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

    topBarLayout_->addStretch(1);
}

void WorkspaceShell::clearTopBar() {
    while (auto* item = topBarLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    saveProjectAction_ = nullptr;
}

QPushButton* WorkspaceShell::createTopBarButton(const QString& text) {
    auto* button = new QPushButton(text, this);
    button->setFont(makeWorkspaceFont(20));
    button->setFixedSize(166, 54);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(
        "QPushButton {"
        " background: #ffffff;"
        " border: 0;"
        " border-right: 1px solid #222222;"
        " color: #000000;"
        "}"
        "QPushButton:hover { background: #f4f4f4; }"
        "QPushButton::menu-indicator { image: none; width: 0; }");
    return button;
}

void WorkspaceShell::setSaveProjectHandler(std::function<void()> handler) {
    saveProjectHandler_ = std::move(handler);
}

void WorkspaceShell::setNavigationPanel(QWidget* panel) {
    replaceSingleWidget(navigationLayout_, panel);
}

void WorkspaceShell::setReviewPanel(QWidget* panel) {
    replaceSingleWidget(reviewLayout_, panel);
}

void WorkspaceShell::setCanvas(QWidget* canvas) {
    replaceSingleWidget(canvasLayout_, canvas);
}

void WorkspaceShell::setBottomPanel(QWidget* panel) {
    replaceSingleWidget(bottomLayout_, panel);
}

}  // namespace safecrowd::application
