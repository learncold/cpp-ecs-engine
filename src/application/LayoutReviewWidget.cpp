#include "application/LayoutReviewWidget.h"

#include <algorithm>

#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QScrollArea>
#include <QStringList>
#include <QKeySequence>
#include <QVBoxLayout>

#include "application/IssueCardWidget.h"
#include "application/LayoutNavigationPanelWidget.h"
#include "application/ToolIconResources.h"
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
    if (issue.confidence < 0.999) {
        details.push_back(QString("Confidence: %1%").arg(QString::number(issue.confidence * 100.0, 'f', 0)));
    }
    if (!issue.suggestion.empty()) {
        details.push_back(QString("Suggestion: %1").arg(QString::fromStdString(issue.suggestion)));
    }

    return details.join('\n');
}

QString importStatusText(const safecrowd::domain::ImportResult& importResult) {
    QStringList lines;
    if (!importResult.statusMessage.empty()) {
        lines.push_back(QString::fromStdString(importResult.statusMessage));
    }

    const auto& summary = importResult.artifacts.summary;
    if (summary.rawEntityCount > 0 || summary.layoutElementCount > 0 || summary.issueCount > 0) {
        lines.push_back(QString("Raw %1  -  Canonical %2  -  Layout %3  -  Issues %4")
            .arg(static_cast<qulonglong>(summary.rawEntityCount))
            .arg(static_cast<qulonglong>(summary.canonicalElementCount))
            .arg(static_cast<qulonglong>(summary.layoutElementCount))
            .arg(static_cast<qulonglong>(summary.issueCount)));
    }

    const auto& reimport = importResult.artifacts.reimport;
    if (reimport.hasComparison) {
        lines.push_back(QString("Reimport diff: +%1 / -%2 / changed %3")
            .arg(static_cast<qulonglong>(reimport.addedElements))
            .arg(static_cast<qulonglong>(reimport.removedElements))
            .arg(static_cast<qulonglong>(reimport.changedElements)));
    }

    return lines.isEmpty() ? QString("Review the imported layout before approval.") : lines.join('\n');
}

QString issueTarget(const safecrowd::domain::ImportIssue& issue) {
    if (!issue.targetId.empty()) {
        return QString::fromStdString(issue.targetId);
    }
    return QString::fromStdString(issue.sourceId);
}

bool isFloorElementId(const QString& elementId) {
    return elementId.startsWith("floor:");
}

QString floorIdFromElementId(const QString& elementId) {
    return isFloorElementId(elementId) ? elementId.mid(QString("floor:").size()) : QString{};
}

bool confirmFloorDeletion(QWidget* parent, const safecrowd::domain::ImportResult& importResult, const QString& elementId) {
    const auto floorId = floorIdFromElementId(elementId);
    if (floorId.isEmpty()) {
        return false;
    }

    QString message = QString(
        "Delete floor \"%1\" and all layout elements on it?\n\n"
        "This removes rooms, exits, stairs, doors, walls, obstructions, and controls assigned to the floor.")
        .arg(floorId);
    if (importResult.layout.has_value() && importResult.layout->floors.size() <= 1) {
        message += "\n\nThis is the only floor, so an empty replacement floor will remain to keep the layout editable.";
    }

    return QMessageBox::question(
        parent,
        "Delete Floor",
        message,
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel) == QMessageBox::Yes;
}

bool isLiveValidationIssue(safecrowd::domain::ImportIssueCode code) {
    using safecrowd::domain::ImportIssueCode;

    switch (code) {
    case ImportIssueCode::MissingExit:
    case ImportIssueCode::MissingRoom:
    case ImportIssueCode::DisconnectedWalkableArea:
    case ImportIssueCode::WidthBelowMinimum:
    case ImportIssueCode::ConnectionSpanMisaligned:
        return true;
    default:
        return false;
    }
}

QIcon makeIssuesIcon(const QColor& color) {
    return makeSvgToolIcon(QStringLiteral(":/tool-icons/etc/issues.svg"), color, QSize(22, 22));
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

QWidget* createNavigationPanel(
    const safecrowd::domain::ImportResult& importResult,
    bool showIssues,
    std::function<void(const safecrowd::domain::ImportIssue&)> selectIssueHandler,
    std::function<void(const QString&)> selectLayoutElementHandler,
    std::function<void(const QString&)> deleteLayoutElementHandler,
    NavigationTreeState layoutNavigationState,
    std::function<void(const QSet<QString>&)> layoutExpandedStateChangedHandler,
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
            shell != nullptr ? shell->createPanelHeader("Layout", content, false) : nullptr,
            std::move(layoutNavigationState),
            std::move(layoutExpandedStateChangedHandler),
            std::move(deleteLayoutElementHandler)));
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
    QWidget** inspectorEditorHost,
    QVBoxLayout** inspectorEditorLayout,
    QLabel** importStatus,
    QLabel** approvalStatus,
    QPushButton** reimportButton,
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

    *inspectorDetail = new QLabel("Use the top and left toolbars to draw rooms, exits, walls, obstructions, and doors.", panel);
    (*inspectorDetail)->setFont(ui::font(ui::FontRole::Body));
    (*inspectorDetail)->setWordWrap(true);
    (*inspectorDetail)->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(*inspectorDetail);

    *inspectorEditorHost = new QWidget(panel);
    *inspectorEditorLayout = new QVBoxLayout(*inspectorEditorHost);
    (*inspectorEditorLayout)->setContentsMargins(0, 0, 0, 0);
    (*inspectorEditorLayout)->setSpacing(8);
    layout->addWidget(*inspectorEditorHost);

    layout->addStretch(1);

    *importStatus = new QLabel("Review the imported layout before approval.", panel);
    (*importStatus)->setFont(ui::font(ui::FontRole::Caption));
    (*importStatus)->setWordWrap(true);
    (*importStatus)->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(*importStatus);

    *approvalStatus = new QLabel("Resolve blocking issues first", panel);
    (*approvalStatus)->setFont(ui::font(ui::FontRole::Body));
    (*approvalStatus)->setWordWrap(true);
    (*approvalStatus)->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(*approvalStatus);

    *reimportButton = new QPushButton("Reimport Layout", panel);
    (*reimportButton)->setFont(ui::font(ui::FontRole::Body));
    (*reimportButton)->setStyleSheet(ui::secondaryButtonStyleSheet());
    layout->addWidget(*reimportButton);

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
    std::function<void(const safecrowd::domain::ImportResult&)> reimportHandler,
    QWidget* parent)
    : QWidget(parent),
      projectName_(projectName),
      importResult_(importResult),
      openProjectHandler_(std::move(openProjectHandler)),
      approvalHandler_(std::move(approvalHandler)),
      reimportHandler_(std::move(reimportHandler)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    shell_ = new WorkspaceShell(WorkspaceShellOptions{
        .reviewPanelToggleVisibleIconPath = QStringLiteral(":/tool-icons/etc/inspector-panel.svg"),
        .reviewPanelToggleHiddenIconPath = QStringLiteral(":/tool-icons/etc/inspector-panel.svg"),
        .reviewPanelToggleName = QStringLiteral("Inspector"),
    }, this);
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
        &inspectorEditorHost_,
        &inspectorEditorLayout_,
        &importStatusLabel_,
        &approvalStatusLabel_,
        &reimportButton_,
        &approveButton_,
        shell_);

    shell_->setTools({"Project"});
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

    connect(reimportButton_, &QPushButton::clicked, this, [this]() {
        if (reimportHandler_) {
            reimportHandler_(importResult_);
        }
    });

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, [this]() {
        undoLastEdit();
    });
    auto* redoShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Z")), this);
    connect(redoShortcut, &QShortcut::activated, this, [this]() {
        redoLastUndo();
    });

    applyImportResultState();

    layout->addWidget(shell_);
}

const safecrowd::domain::ImportResult& LayoutReviewWidget::currentImportResult() const noexcept {
    return importResult_;
}

bool LayoutReviewWidget::undoLastEdit() {
    if (undoHistory_.empty() || !importResult_.layout.has_value()) {
        return false;
    }

    redoHistory_.push_back(*importResult_.layout);
    importResult_.layout = undoHistory_.back();
    undoHistory_.pop_back();
    importResult_.reviewStatus = safecrowd::domain::ImportReviewStatus::Pending;
    applyImportResultState();
    return true;
}

bool LayoutReviewWidget::redoLastUndo() {
    if (redoHistory_.empty() || !importResult_.layout.has_value()) {
        return false;
    }

    undoHistory_.push_back(*importResult_.layout);
    importResult_.layout = redoHistory_.back();
    redoHistory_.pop_back();
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
    selectedLayoutElementId_ = elementId;
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
    redoHistory_.clear();
    importResult_.layout = layout;
    importResult_.reviewStatus = safecrowd::domain::ImportReviewStatus::Pending;
    applyImportResultState();
}

void LayoutReviewWidget::handlePreviewSelectionChanged(const PreviewSelection& selection) {
    lastSelection_ = selection;
    const auto previousLayoutElementId = selectedLayoutElementId_;
    const auto nextLayoutElementId = selection.empty() || selection.kind == PreviewSelectionKind::Multiple ? QString{} : selection.id;
    selectedLayoutElementId_ = nextLayoutElementId;
    selectedIssueTargetId_.clear();
    selectedIssueCode_.clear();
    showSelectionInspector(selection);
    if (navigationView_ == NavigationView::Layout && previousLayoutElementId != nextLayoutElementId) {
        refreshNavigationPanel();
    }
}

void LayoutReviewWidget::refreshApprovalState() {
    const auto blockingCount = std::count_if(importResult_.issues.begin(), importResult_.issues.end(), [](const auto& issue) {
        return issue.blocksSimulation();
    });
    const auto hasBlocking = blockingCount > 0;

    if (importStatusLabel_ != nullptr) {
        importStatusLabel_->setText(importStatusText(importResult_));
    }

    if (approveButton_ != nullptr) {
        approveButton_->setEnabled(!hasBlocking);
        approveButton_->setToolTip(hasBlocking
            ? QString("Resolve %1 blocking issue(s) before approval").arg(static_cast<int>(blockingCount))
            : QString("Approve layout and continue to Scenario Authoring"));
    }

    if (approvalStatusLabel_ == nullptr) {
        return;
    }

    if (hasBlocking) {
        approvalStatusLabel_->setText(QString("Resolve %1 blocking issue(s) first").arg(static_cast<int>(blockingCount)));
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

    shell_->setNavigationTabs(
        {
            {
                .id = "issues",
                .label = "Issues",
                .icon = makeIssuesIcon(QColor("#1f5fae")),
            },
            {
                .id = "layout",
                .label = "Layout",
                .icon = QIcon{},
            },
        },
        navigationView_ == NavigationView::Issues ? "issues" : "layout",
        [this](const QString& tabId) {
            navigationView_ = tabId == "issues" ? NavigationView::Issues : NavigationView::Layout;
            refreshNavigationPanel();
        });
    shell_->setNavigationPanel(createNavigationPanel(
        importResult_,
        navigationView_ == NavigationView::Issues,
        [this](const auto& issue) {
            handleIssueSelected(issue);
        },
        [this](const QString& elementId) {
            handleLayoutElementSelected(elementId);
        },
        [this](const QString& elementId) {
            if (isFloorElementId(elementId) && !confirmFloorDeletion(this, importResult_, elementId)) {
                return;
            }
            if (preview_ != nullptr) {
                preview_->deleteElement(elementId);
            }
        },
        NavigationTreeState{
            .expandedNodeIds = layoutExpandedNodeIds_,
            .selectedId = selectedLayoutElementId_,
            .restoreExpandedState = true,
        },
        [this](const QSet<QString>& expandedNodeIds) {
            layoutExpandedNodeIds_ = expandedNodeIds;
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
    clearInspectorEditor();
    if (inspectorTitleLabel_ != nullptr) {
        inspectorTitleLabel_->setText("No selection");
    }
    if (inspectorDetailLabel_ != nullptr) {
        inspectorDetailLabel_->setText("Use the top and left toolbars to draw rooms, exits, walls, obstructions, and doors.");
    }
}

void LayoutReviewWidget::showIssueInspector(const safecrowd::domain::ImportIssue& issue) {
    clearInspectorEditor();
    if (inspectorTitleLabel_ != nullptr) {
        inspectorTitleLabel_->setText(issueCodeText(issue));
    }
    if (inspectorDetailLabel_ != nullptr) {
        inspectorDetailLabel_->setText(issueDetail(issue));
    }
}

void LayoutReviewWidget::clearInspectorEditor() {
    if (inspectorEditorLayout_ == nullptr || inspectorEditorHost_ == nullptr) {
        return;
    }

    while (auto* item = inspectorEditorLayout_->takeAt(0)) {
        if (auto* widget = item->widget(); widget != nullptr) {
            widget->deleteLater();
        }
        delete item;
    }
    inspectorEditorHost_->hide();
}

std::optional<std::vector<safecrowd::domain::Point2D>> LayoutReviewWidget::selectionVertices(const PreviewSelection& selection) const {
    if (!importResult_.layout.has_value() || selection.empty() || selection.kind == PreviewSelectionKind::Multiple) {
        return std::nullopt;
    }

    const auto& layout = *importResult_.layout;
    switch (selection.kind) {
    case PreviewSelectionKind::Floor:
        break;
    case PreviewSelectionKind::Zone:
        for (const auto& zone : layout.zones) {
            if (QString::fromStdString(zone.id) == selection.id) {
                return zone.area.outline;
            }
        }
        break;
    case PreviewSelectionKind::Connection:
        for (const auto& connection : layout.connections) {
            if (QString::fromStdString(connection.id) == selection.id) {
                return std::vector<safecrowd::domain::Point2D>{connection.centerSpan.start, connection.centerSpan.end};
            }
        }
        break;
    case PreviewSelectionKind::Barrier:
        for (const auto& barrier : layout.barriers) {
            if (QString::fromStdString(barrier.id) == selection.id) {
                return barrier.geometry.vertices;
            }
        }
        break;
    case PreviewSelectionKind::None:
    case PreviewSelectionKind::Multiple:
        break;
    }

    return std::nullopt;
}

void LayoutReviewWidget::showVertexEditor(const PreviewSelection& selection) {
    clearInspectorEditor();
    if (inspectorEditorLayout_ == nullptr || inspectorEditorHost_ == nullptr) {
        return;
    }

    const auto vertices = selectionVertices(selection);
    if (!vertices.has_value() || vertices->empty()) {
        return;
    }

    auto* editor = new QWidget(inspectorEditorHost_);
    auto* editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(8);

    auto* title = new QLabel("Vertices", editor);
    title->setFont(ui::font(ui::FontRole::Body));
    editorLayout->addWidget(title);

    auto* gridHost = new QWidget(editor);
    auto* grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    grid->addWidget(new QLabel("#", gridHost), 0, 0);
    grid->addWidget(new QLabel("X", gridHost), 0, 1);
    grid->addWidget(new QLabel("Y", gridHost), 0, 2);

    std::vector<QDoubleSpinBox*> xEditors;
    std::vector<QDoubleSpinBox*> yEditors;
    xEditors.reserve(vertices->size());
    yEditors.reserve(vertices->size());

    const auto makeCoordinateEditor = [&](double value) {
        auto* editor = new QDoubleSpinBox(gridHost);
        editor->setRange(-100000.0, 100000.0);
        editor->setDecimals(3);
        editor->setSingleStep(0.1);
        editor->setValue(value);
        editor->setMinimumWidth(86);
        return editor;
    };

    for (std::size_t index = 0; index < vertices->size(); ++index) {
        auto* indexLabel = new QLabel(QString::number(static_cast<int>(index + 1)), gridHost);
        indexLabel->setStyleSheet(ui::mutedTextStyleSheet());
        auto* xEditor = makeCoordinateEditor((*vertices)[index].x);
        auto* yEditor = makeCoordinateEditor((*vertices)[index].y);
        grid->addWidget(indexLabel, static_cast<int>(index + 1), 0);
        grid->addWidget(xEditor, static_cast<int>(index + 1), 1);
        grid->addWidget(yEditor, static_cast<int>(index + 1), 2);
        xEditors.push_back(xEditor);
        yEditors.push_back(yEditor);
    }
    editorLayout->addWidget(gridHost);

    auto* applyButton = new QPushButton("Apply Vertices", editor);
    applyButton->setFont(ui::font(ui::FontRole::Body));
    editorLayout->addWidget(applyButton);
    connect(applyButton, &QPushButton::clicked, this, [this, selection, xEditors, yEditors]() {
        if (preview_ == nullptr || xEditors.size() != yEditors.size()) {
            return;
        }

        std::vector<safecrowd::domain::Point2D> updatedVertices;
        updatedVertices.reserve(xEditors.size());
        for (std::size_t index = 0; index < xEditors.size(); ++index) {
            if (xEditors[index] == nullptr || yEditors[index] == nullptr) {
                return;
            }
            updatedVertices.push_back({
                .x = xEditors[index]->value(),
                .y = yEditors[index]->value(),
            });
        }
        preview_->updateElementVertices(selection.kind, selection.id, updatedVertices);
    });

    inspectorEditorLayout_->addWidget(editor);
    inspectorEditorHost_->show();
}

void LayoutReviewWidget::showSelectionInspector(const PreviewSelection& selection) {
    if (selection.empty()) {
        showDefaultInspector();
        return;
    }

    clearInspectorEditor();
    if (inspectorTitleLabel_ != nullptr) {
        inspectorTitleLabel_->setText(selection.title);
    }
    if (inspectorDetailLabel_ != nullptr) {
        inspectorDetailLabel_->setText(selection.detail);
    }
    showVertexEditor(selection);
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
