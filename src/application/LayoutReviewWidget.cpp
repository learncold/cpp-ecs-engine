#include "application/LayoutReviewWidget.h"

#include <algorithm>

#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "application/IssueCardWidget.h"
#include "application/LayoutPreviewWidget.h"
#include "application/UiStyle.h"
#include "application/WorkspaceShell.h"
#include "domain/ImportIssue.h"

namespace safecrowd::application {
namespace {

QString issueTitle(const safecrowd::domain::ImportIssue& issue) {
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
    std::function<void(const safecrowd::domain::ImportIssue&)> selectIssueHandler,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = new QLabel("Issues", panel);
    title->setFont(ui::font(ui::FontRole::Title));
    layout->addWidget(title);

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

    auto* listHost = new QWidget(panel);
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

    auto* blockingButton = createIssueFilterButton("Blocking", blockingCount, true, panel);
    auto* warningButton = createIssueFilterButton("Warnings", warningCount, false, panel);
    auto* infoButton = createIssueFilterButton("Info", infoCount, false, panel);
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

    QObject::connect(blockingButton, &QPushButton::clicked, panel, [=]() {
        setSelected(blockingButton);
        showList(createIssueList(
            importResult,
            [](const safecrowd::domain::ImportIssue& issue) { return issue.blocksSimulation(); },
            "No blocking issues",
            selectIssueHandler,
            listHost));
    });
    QObject::connect(warningButton, &QPushButton::clicked, panel, [=]() {
        setSelected(warningButton);
        showList(createIssueList(
            importResult,
            [](const safecrowd::domain::ImportIssue& issue) { return !issue.blocksSimulation() && issue.severity == safecrowd::domain::ImportIssueSeverity::Warning; },
            "No warnings",
            selectIssueHandler,
            listHost));
    });
    QObject::connect(infoButton, &QPushButton::clicked, panel, [=]() {
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

    layout->addStretch(1);
    return panel;
}

QFrame* createPanelSection(QWidget* parent) {
    auto* section = new QFrame(parent);
    section->setFrameShape(QFrame::StyledPanel);
    section->setLineWidth(1);
    section->setStyleSheet(ui::panelStyleSheet());
    return section;
}

QWidget* createReviewPanel(const safecrowd::domain::ImportResult& importResult, QLabel** inspectorTitle, QLabel** inspectorDetail, QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    const auto blockingCount = std::count_if(importResult.issues.begin(), importResult.issues.end(), [](const auto& issue) {
        return issue.blocksSimulation();
    });

    auto* approvalSection = createPanelSection(panel);
    auto* approvalLayout = new QVBoxLayout(approvalSection);
    approvalLayout->setContentsMargins(16, 16, 16, 16);
    approvalLayout->setSpacing(10);

    auto* approvalHeader = new QLabel("Approval", approvalSection);
    approvalHeader->setFont(ui::font(ui::FontRole::SectionTitle));
    approvalLayout->addWidget(approvalHeader);

    auto* approveButton = new QPushButton("Approve Layout", approvalSection);
    approveButton->setEnabled(blockingCount == 0);
    approveButton->setFont(ui::font(ui::FontRole::Body));
    approveButton->setStyleSheet(ui::primaryButtonStyleSheet());
    approvalLayout->addWidget(approveButton);

    auto* approvalStatus = new QLabel(blockingCount == 0 ? "Ready for approval" : "Resolve blocking issues first", approvalSection);
    approvalStatus->setFont(ui::font(ui::FontRole::Body));
    approvalStatus->setWordWrap(true);
    approvalStatus->setStyleSheet(ui::mutedTextStyleSheet());
    approvalLayout->addWidget(approvalStatus);
    layout->addWidget(approvalSection);

    QObject::connect(approveButton, &QPushButton::clicked, panel, [approvalStatus]() {
        approvalStatus->setText("Layout approved");
    });

    auto* inspectorSection = createPanelSection(panel);
    auto* inspectorLayout = new QVBoxLayout(inspectorSection);
    inspectorLayout->setContentsMargins(16, 16, 16, 16);
    inspectorLayout->setSpacing(10);

    auto* inspectorHeader = new QLabel("Inspector", inspectorSection);
    inspectorHeader->setFont(ui::font(ui::FontRole::SectionTitle));
    inspectorLayout->addWidget(inspectorHeader);

    *inspectorTitle = new QLabel("No issue selected", inspectorSection);
    (*inspectorTitle)->setFont(ui::font(ui::FontRole::Body));
    (*inspectorTitle)->setWordWrap(true);
    inspectorLayout->addWidget(*inspectorTitle);

    *inspectorDetail = new QLabel("Select an issue from the left panel.", inspectorSection);
    (*inspectorDetail)->setFont(ui::font(ui::FontRole::Body));
    (*inspectorDetail)->setWordWrap(true);
    (*inspectorDetail)->setStyleSheet(ui::mutedTextStyleSheet());
    inspectorLayout->addWidget(*inspectorDetail);
    layout->addWidget(inspectorSection);

    layout->addStretch(1);
    return panel;
}

QWidget* createBottomPanel(const QString& projectName, QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* label = new QLabel(projectName.isEmpty() ? "Layout Review" : QString("Layout Review - %1").arg(projectName), panel);
    label->setFont(ui::font(ui::FontRole::Title));
    layout->addWidget(label, 0, Qt::AlignLeft | Qt::AlignTop);

    layout->addStretch(1);
    return panel;
}

}  // namespace

LayoutReviewWidget::LayoutReviewWidget(
    const QString& projectName,
    const safecrowd::domain::ImportResult& importResult,
    std::function<void()> saveProjectHandler,
    std::function<void()> openProjectHandler,
    QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* shell = new WorkspaceShell(this);
    auto* preview = new LayoutPreviewWidget(importResult, shell);
    QLabel* inspectorTitle = nullptr;
    QLabel* inspectorDetail = nullptr;
    auto* reviewPanel = createReviewPanel(importResult, &inspectorTitle, &inspectorDetail, shell);

    shell->setTools({"Project", "Tool"});
    shell->setSaveProjectHandler(std::move(saveProjectHandler));
    shell->setOpenProjectHandler(std::move(openProjectHandler));
    shell->setNavigationPanel(createNavigationPanel(importResult, [preview, inspectorTitle, inspectorDetail](const safecrowd::domain::ImportIssue& issue) {
        const auto target = issueTarget(issue);
        preview->focusIssueTarget(target);
        if (inspectorTitle != nullptr) {
            inspectorTitle->setText(issueTitle(issue));
        }
        if (inspectorDetail != nullptr) {
            inspectorDetail->setText(issueDetail(issue));
        }
    }, shell));
    shell->setCanvas(preview);
    shell->setReviewPanel(reviewPanel);
    shell->setBottomPanel(createBottomPanel(projectName, shell));
    layout->addWidget(shell);
}

}  // namespace safecrowd::application
