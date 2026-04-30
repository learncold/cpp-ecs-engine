#include "application/LayoutReviewWidget.h"

#include <algorithm>

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QScrollArea>
#include <QStringList>
#include <QKeySequence>
#include <QToolButton>
#include <QVBoxLayout>

#include "application/IssueCardWidget.h"
#include "application/LayoutNavigationPanelWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"
#include "domain/ImportIssue.h"
#include "domain/ImportValidationService.h"

namespace safecrowd::application {
namespace {

QString issueCodeText(const safecrowd::domain::ImportIssue& issue) {
    return QString::fromUtf8(safecrowd::domain::toString(issue.code));
}

QString issueDetail(const safecrowd::domain::ImportIssue& issue) {
    QStringList details;

    if (!issue.message.empty()) {
        details.push_back(QString::fromStdString(issue.message));
    }
    if (!issue.sourceId.empty()) {
        details.push_back(QString("Source: %1").arg(QString::fromStdString(issue.sourceId)));
    }
    if (!issue.targetId.empty()) {
        details.push_back(QString("Target: %1").arg(QString::fromStdString(issue.targetId)));
    }

    return details.join('\n');
}

QString issueTarget(const safecrowd::domain::ImportIssue& issue) {
    if (!issue.targetId.empty()) {
        return QString::fromStdString(issue.targetId);
    }
    return QString::fromStdString(issue.sourceId);
}

bool isLiveValidationIssue(safecrowd::domain::ImportIssueCode code) {
    using safecrowd::domain::ImportIssueCode;

    switch (code) {
    case ImportIssueCode::MissingExit:
    case ImportIssueCode::MissingRoom:
    case ImportIssueCode::DisconnectedWalkableArea:
    case ImportIssueCode::WidthBelowMinimum:
        return true;
    default:
        return false;
    }
}

QIcon makeIssuesIcon(const QColor& color) {
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    QPolygonF triangle;
    triangle << QPointF(22, 9) << QPointF(34, 32) << QPointF(10, 32);
    painter.drawPolygon(triangle);
    painter.drawLine(QPointF(22, 17), QPointF(22, 24));
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QPointF(22, 28), 1.7, 1.7);
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

QWidget* createIssueList(
    const safecrowd::domain::ImportResult& importResult,
    std::function<bool(const safecrowd::domain::ImportIssue&)> filter,
    const QString& emptyMessage,
    std::function<void(const safecrowd::domain::ImportIssue&)> selectIssueHandler,
    QWidget* parent) {
    auto* scrollArea = new QScrollArea(parent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui::polishScrollArea(scrollArea);

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("QWidget { background: transparent; }");
    auto* issueLayout = new QVBoxLayout(scrollContent);
    issueLayout->setContentsMargins(0, 0, 4, 0);
    issueLayout->setSpacing(10);

    int issueCount = 0;
    for (const auto& issue : importResult.issues) {
        if (!filter(issue)) {
            continue;
        }

        ++issueCount;
        issueLayout->addWidget(new IssueCardWidget(issue, selectIssueHandler, scrollContent));
    }

    if (issueCount == 0) {
        auto* emptyLabel = new QLabel(emptyMessage, scrollContent);
        emptyLabel->setFont(ui::font(ui::FontRole::Body));
        emptyLabel->setWordWrap(true);
        emptyLabel->setStyleSheet(ui::mutedTextStyleSheet());
        issueLayout->addWidget(emptyLabel);
    }

    issueLayout->addStretch(1);
    scrollArea->setWidget(scrollContent);
    return scrollArea;
}

QPushButton* createIssueFilterButton(const QString& label, int count, bool selected, QWidget* parent) {
    auto* button = new QPushButton(QString("%1  %2").arg(label).arg(count), parent);
    button->setCheckable(true);
    button->setChecked(selected);
    button->setMinimumHeight(40);
    button->setFont(ui::font(ui::FontRole::Caption));
    button->setStyleSheet(ui::tagStyleSheet(selected));
    return button;
}

QWidget* createNavigationRail(
    bool showIssues,
    std::function<void(bool)> switchViewHandler,
    const WorkspaceShell* shell,
    QWidget* parent) {
    auto* activityBar = new QFrame(parent);
    activityBar->setFixedWidth(56);
    activityBar->setStyleSheet(
        "QFrame {"
        " background: #eef3f8;"
        " border: 0;"
        " border-right: 1px solid #d7e0ea;"
        " border-radius: 0px;"
        "}"
    );
    auto* activityLayout = new QVBoxLayout(activityBar);
    activityLayout->setContentsMargins(0, 0, 0, 0);
    activityLayout->setSpacing(0);

    const auto makeActivityButton = [&](const QIcon& icon, const QString& tooltip, bool checked, auto&& handler) {
        auto* button = new QToolButton(activityBar);
        button->setIcon(icon);
        button->setIconSize(QSize(22, 22));
        button->setCheckable(true);
        button->setChecked(checked);
        button->setToolTip(tooltip);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(56, 56);
        button->setStyleSheet(
            "QToolButton {"
            " background: transparent;"
            " border: 0;"
            " border-left: 3px solid transparent;"
            " border-radius: 0px;"
            " padding: 0px;"
            "}"
            "QToolButton:hover {"
            " background: #e3ebf4;"
            "}"
            "QToolButton:checked {"
            " background: #ffffff;"
            " border-left-color: #1f5fae;"
            "}"
        );
        QObject::connect(button, &QToolButton::clicked, activityBar, handler);
        activityLayout->addWidget(button);
        return button;
    };

    auto* issuesButton = makeActivityButton(
        makeIssuesIcon(QColor("#1f5fae")),
        "Issues",
        showIssues,
        [switchViewHandler]() {
            switchViewHandler(true);
        });
    auto* layoutButton = makeActivityButton(
        makeLayoutIcon(QColor("#1f5fae")),
        "Layout",
        !showIssues,
        [switchViewHandler]() {
            switchViewHandler(false);
        });
    (void)issuesButton;
    (void)layoutButton;
    activityLayout->addStretch(1);
    if (shell != nullptr) {
        activityLayout->addWidget(shell->createBackButton(activityBar), 0, Qt::AlignHCenter);
    }
    return activityBar;
}

QWidget* createNavigationPanel(
    const safecrowd::domain::ImportResult& importResult,
    bool showIssues,
    std::function<void(const safecrowd::domain::ImportIssue&)> selectIssueHandler,
    std::function<void(const QString&)> selectLayoutElementHandler,
    const WorkspaceShell* shell,
    QWidget* parent) {
    auto* content = new QWidget(parent);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    if (!showIssues) {
        layout->addWidget(new LayoutNavigationPanelWidget(
            importResult.layout.has_value() ? &(*importResult.layout) : nullptr,
            std::move(selectLayoutElementHandler),
            content,
            shell != nullptr ? shell->createPanelHeader("Layout", content, false) : nullptr));
        return content;
    }

    if (shell != nullptr) {
        layout->addWidget(shell->createPanelHeader("Issues", content, false));
    } else {
        auto* title = new QLabel("Issues", content);
        title->setFont(ui::font(ui::FontRole::Title));
        layout->addWidget(title);
    }

    const auto blockingCount = std::count_if(importResult.issues.begin(), importResult.issues.end(), [](const auto& issue) {
        return issue.blocksSimulation();
    });
    const auto warningCount = std::count_if(importResult.issues.begin(), importResult.issues.end(), [](const auto& issue) {
        return !issue.blocksSimulation() && issue.severity == safecrowd::domain::ImportIssueSeverity::Warning;
    });
    const auto infoCount = std::count_if(importResult.issues.begin(), importResult.issues.end(), [](const auto& issue) {
        return issue.severity == safecrowd::domain::ImportIssueSeverity::Info;
    });

    auto* filterLayout = new QVBoxLayout();
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(8);

    auto* listHost = new QWidget(content);
    auto* listHostLayout = new QVBoxLayout(listHost);
    listHostLayout->setContentsMargins(0, 0, 0, 0);
    listHostLayout->setSpacing(0);

    auto showList = [=](QWidget* list) {
        while (auto* item = listHostLayout->takeAt(0)) {
            delete item->widget();
            delete item;
        }
        listHostLayout->addWidget(list);
    };

    auto* blockingButton = createIssueFilterButton("Blocking", blockingCount, true, content);
    auto* warningButton = createIssueFilterButton("Warnings", warningCount, false, content);
    auto* infoButton = createIssueFilterButton("Info", infoCount, false, content);
    filterLayout->addWidget(blockingButton);
    filterLayout->addWidget(warningButton);
    filterLayout->addWidget(infoButton);
    layout->addLayout(filterLayout);
    layout->addWidget(listHost, 1);

    const auto setSelected = [=](QPushButton* selected) {
        blockingButton->setChecked(selected == blockingButton);
        warningButton->setChecked(selected == warningButton);
        infoButton->setChecked(selected == infoButton);
        blockingButton->setStyleSheet(ui::tagStyleSheet(selected == blockingButton));
        warningButton->setStyleSheet(ui::tagStyleSheet(selected == warningButton));
        infoButton->setStyleSheet(ui::tagStyleSheet(selected == infoButton));
    };

    QObject::connect(blockingButton, &QPushButton::clicked, content, [=]() {
        setSelected(blockingButton);
        showList(createIssueList(
            importResult,
            [](const safecrowd::domain::ImportIssue& issue) { return issue.blocksSimulation(); },
            "No blocking issues",
            selectIssueHandler,
            listHost));
    });
    QObject::connect(warningButton, &QPushButton::clicked, content, [=]() {
        setSelected(warningButton);
        showList(createIssueList(
            importResult,
            [](const safecrowd::domain::ImportIssue& issue) { return !issue.blocksSimulation() && issue.severity == safecrowd::domain::ImportIssueSeverity::Warning; },
            "No warnings",
            selectIssueHandler,
            listHost));
    });
    QObject::connect(infoButton, &QPushButton::clicked, content, [=]() {
        setSelected(infoButton);
        showList(createIssueList(
            importResult,
            [](const safecrowd::domain::ImportIssue& issue) { return issue.severity == safecrowd::domain::ImportIssueSeverity::Info; },
            "No info issues",
            selectIssueHandler,
            listHost));
    });

    showList(createIssueList(
        importResult,
        [](const safecrowd::domain::ImportIssue& issue) { return issue.blocksSimulation(); },
        "No blocking issues",
        selectIssueHandler,
        listHost));

    return content;
}

QWidget* createReviewPanel(
    QLabel** inspectorTitle,
    QLabel** inspectorDetail,
    QLabel** approvalStatus,
    QPushButton** approveButton,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* inspectorHeader = new QLabel("Inspector", panel);
    inspectorHeader->setFont(ui::font(ui::FontRole::Title));
    layout->addWidget(inspectorHeader);

    *inspectorTitle = new QLabel("No selection", panel);
    (*inspectorTitle)->setFont(ui::font(ui::FontRole::Body));
    (*inspectorTitle)->setWordWrap(true);
    layout->addWidget(*inspectorTitle);

    *inspectorDetail = new QLabel("Use the top and left toolbars to draw rooms, exits, walls, and doors.", panel);
    (*inspectorDetail)->setFont(ui::font(ui::FontRole::Body));
    (*inspectorDetail)->setWordWrap(true);
    (*inspectorDetail)->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(*inspectorDetail);

    layout->addStretch(1);

    *approvalStatus = new QLabel("Resolve blocking issues first", panel);
    (*approvalStatus)->setFont(ui::font(ui::FontRole::Body));
    (*approvalStatus)->setWordWrap(true);
    (*approvalStatus)->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(*approvalStatus);

    *approveButton = new QPushButton("Approve Layout", panel);
    (*approveButton)->setFont(ui::font(ui::FontRole::Body));
    (*approveButton)->setStyleSheet(ui::primaryButtonStyleSheet());
    layout->addWidget(*approveButton);

    return panel;
}

}  // namespace

LayoutReviewWidget::LayoutReviewWidget(
    const QString& projectName,
    const safecrowd::domain::ImportResult& importResult,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    std::function<void(const safecrowd::domain::ImportResult&)> approvalHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(projectName),
      importResult_(importResult),
      openProjectHandler_(std::move(openProjectHandler)),
      approvalHandler_(std::move(approvalHandler)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    shell_ = new WorkspaceShell(this);
    preview_ = new LayoutPreviewWidget(importResult_, shell_);
    preview_->setSelectionChangedHandler([this](const PreviewSelection& selection) {
        handlePreviewSelectionChanged(selection);
    });
    preview_->setLayoutEditedHandler([this](const safecrowd::domain::FacilityLayout2D& layoutValue) {
        handleLayoutEdited(layoutValue);
    });

    auto* reviewPanel = createReviewPanel(
        &inspectorTitleLabel_,
        &inspectorDetailLabel_,
        &approvalStatusLabel_,
        &approveButton_,
        shell_);

    shell_->setTools({"Project", "Tool"});
    shell_->setSaveProjectHandler(std::move(saveProjectHandler));
    shell_->setOpenProjectHandler(openProjectHandler_);
    shell_->setBackHandler(openProjectHandler_);
    shell_->setCanvas(preview_);
    shell_->setReviewPanel(reviewPanel);

    connect(approveButton_, &QPushButton::clicked, this, [this]() {
        if (safecrowd::domain::hasBlockingImportIssue(importResult_.issues)) {
            return;
        }
        importResult_.reviewStatus = safecrowd::domain::ImportReviewStatus::Approved;
        refreshApprovalState();
        if (approvalHandler_) {
            approvalHandler_(importResult_);
        }
    });

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, [this]() {
        undoLastEdit();
    });

    applyImportResultState();

    layout->addWidget(shell_);
}

const safecrowd::domain::ImportResult& LayoutReviewWidget::currentImportResult() const noexcept {
    return importResult_;
}

bool LayoutReviewWidget::undoLastEdit() {
    if (undoHistory_.empty()) {
        return false;
    }

    importResult_.layout = undoHistory_.back();
    undoHistory_.pop_back();
    importResult_.reviewStatus = safecrowd::domain::ImportReviewStatus::Pending;
    applyImportResultState();
    return true;
}

void LayoutReviewWidget::handleIssueSelected(const safecrowd::domain::ImportIssue& issue) {
    selectedIssueTargetId_ = issueTarget(issue);
    selectedIssueCode_ = issueCodeText(issue);

    if (preview_ != nullptr) {
        preview_->focusIssueTarget(selectedIssueTargetId_);
    }

    showIssueInspector(issue);
}

void LayoutReviewWidget::handleLayoutElementSelected(const QString& elementId) {
    selectedIssueTargetId_.clear();
    selectedIssueCode_.clear();

    if (preview_ != nullptr) {
        preview_->focusElement(elementId);
    }
}

void LayoutReviewWidget::handleLayoutEdited(const safecrowd::domain::FacilityLayout2D& layout) {
    if (importResult_.layout.has_value()) {
        undoHistory_.push_back(*importResult_.layout);
    }
    importResult_.layout = layout;
    importResult_.reviewStatus = safecrowd::domain::ImportReviewStatus::Pending;
    applyImportResultState();
}

void LayoutReviewWidget::handlePreviewSelectionChanged(const PreviewSelection& selection) {
    lastSelection_ = selection;
    selectedIssueTargetId_.clear();
    selectedIssueCode_.clear();
    showSelectionInspector(selection);
}

void LayoutReviewWidget::refreshApprovalState() {
    const auto hasBlocking = safecrowd::domain::hasBlockingImportIssue(importResult_.issues);

    if (approveButton_ != nullptr) {
        approveButton_->setEnabled(!hasBlocking);
    }

    if (approvalStatusLabel_ == nullptr) {
        return;
    }

    if (hasBlocking) {
        approvalStatusLabel_->setText("Resolve blocking issues first");
        return;
    }

    if (importResult_.reviewStatus == safecrowd::domain::ImportReviewStatus::Approved) {
        approvalStatusLabel_->setText("Layout approved");
        return;
    }

    approvalStatusLabel_->setText("Ready for approval");
}

void LayoutReviewWidget::refreshNavigationPanel() {
    if (shell_ == nullptr) {
        return;
    }

    shell_->setNavigationRail(createNavigationRail(
        navigationView_ == NavigationView::Issues,
        [this](bool showIssues) {
            navigationView_ = showIssues ? NavigationView::Issues : NavigationView::Layout;
            refreshNavigationPanel();
        },
        shell_,
        shell_));
    shell_->setNavigationPanel(createNavigationPanel(
        importResult_,
        navigationView_ == NavigationView::Issues,
        [this](const auto& issue) {
            handleIssueSelected(issue);
        },
        [this](const QString& elementId) {
            handleLayoutElementSelected(elementId);
        },
        shell_,
        shell_));
}

void LayoutReviewWidget::restoreInspectorState() {
    if (!selectedIssueCode_.isEmpty()) {
        const auto it = std::find_if(importResult_.issues.begin(), importResult_.issues.end(), [&](const auto& issue) {
            return issueCodeText(issue) == selectedIssueCode_ && issueTarget(issue) == selectedIssueTargetId_;
        });
        if (it != importResult_.issues.end()) {
            showIssueInspector(*it);
            return;
        }
    }

    showSelectionInspector(lastSelection_);
}

void LayoutReviewWidget::showDefaultInspector() {
    if (inspectorTitleLabel_ != nullptr) {
        inspectorTitleLabel_->setText("No selection");
    }
    if (inspectorDetailLabel_ != nullptr) {
        inspectorDetailLabel_->setText("Use the top and left toolbars to draw rooms, exits, walls, and doors.");
    }
}

void LayoutReviewWidget::showIssueInspector(const safecrowd::domain::ImportIssue& issue) {
    if (inspectorTitleLabel_ != nullptr) {
        inspectorTitleLabel_->setText(issueCodeText(issue));
    }
    if (inspectorDetailLabel_ != nullptr) {
        inspectorDetailLabel_->setText(issueDetail(issue));
    }
}

void LayoutReviewWidget::showSelectionInspector(const PreviewSelection& selection) {
    if (selection.empty()) {
        showDefaultInspector();
        return;
    }

    if (inspectorTitleLabel_ != nullptr) {
        inspectorTitleLabel_->setText(selection.title);
    }
    if (inspectorDetailLabel_ != nullptr) {
        inspectorDetailLabel_->setText(selection.detail);
    }
}

void LayoutReviewWidget::updateValidatedIssues() {
    std::vector<safecrowd::domain::ImportIssue> preservedIssues;
    preservedIssues.reserve(importResult_.issues.size());
    for (const auto& issue : importResult_.issues) {
        if (!isLiveValidationIssue(issue.code)) {
            preservedIssues.push_back(issue);
        }
    }

    if (importResult_.layout.has_value()) {
        safecrowd::domain::ImportValidationService validator;
        auto issues = validator.validate(*importResult_.layout);
        preservedIssues.insert(preservedIssues.end(), issues.begin(), issues.end());
    }

    importResult_.issues = std::move(preservedIssues);
}

void LayoutReviewWidget::applyImportResultState() {
    updateValidatedIssues();

    if (preview_ != nullptr) {
        preview_->setImportResult(importResult_);
    }

    refreshNavigationPanel();
    refreshApprovalState();
    restoreInspectorState();
}

}  // namespace safecrowd::application
