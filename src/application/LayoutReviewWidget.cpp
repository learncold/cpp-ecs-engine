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
#include "application/WorkspaceShell.h"
#include "domain/ImportIssue.h"

namespace safecrowd::application {
namespace {

QFont makeFont(int pointSize) {
    QFont font;
    font.setPointSize(pointSize);
    font.setWeight(QFont::Normal);
    return font;
}

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
    scrollArea->setStyleSheet("QScrollArea { background: transparent; }");

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("QWidget { background: transparent; }");
    auto* issueLayout = new QVBoxLayout(scrollContent);
    issueLayout->setContentsMargins(0, 0, 4, 0);
    issueLayout->setSpacing(8);

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
        emptyLabel->setFont(makeFont(11));
        emptyLabel->setWordWrap(true);
        emptyLabel->setStyleSheet("QLabel { color: #555555; }");
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
    button->setMinimumHeight(30);
    button->setStyleSheet(
        "QPushButton {"
        " background: #ffffff;"
        " border: 1px solid #777777;"
        " padding: 4px 8px;"
        " text-align: left;"
        "}"
        "QPushButton:checked {"
        " background: #eeeeee;"
        " border-left: 4px solid #333333;"
        " font-weight: 600;"
        "}"
        "QPushButton:hover { background: #f5f5f5; }");
    return button;
}

QWidget* createNavigationPanel(
    const safecrowd::domain::ImportResult& importResult,
    std::function<void(const safecrowd::domain::ImportIssue&)> selectIssueHandler,
    QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* title = new QLabel("Issues", panel);
    title->setFont(makeFont(20));
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
    filterLayout->setSpacing(6);

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
    section->setStyleSheet(
        "QFrame { border: 1px solid #777777; background: #ffffff; }"
        "QLabel { border: 0; background: transparent; }");
    return section;
}

QWidget* createReviewPanel(const safecrowd::domain::ImportResult& importResult, QLabel** inspectorTitle, QLabel** inspectorDetail, QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    const auto blockingCount = std::count_if(importResult.issues.begin(), importResult.issues.end(), [](const auto& issue) {
        return issue.blocksSimulation();
    });

    auto* approvalSection = createPanelSection(panel);
    auto* approvalLayout = new QVBoxLayout(approvalSection);
    approvalLayout->setContentsMargins(8, 8, 8, 8);
    approvalLayout->setSpacing(8);

    auto* approveButton = new QPushButton("Approve Layout", approvalSection);
    approveButton->setEnabled(blockingCount == 0);
    approveButton->setStyleSheet(
        "QPushButton { background: #ffffff; border: 1px solid #555555; padding: 6px; }"
        "QPushButton:disabled { color: #888888; border-color: #aaaaaa; }");
    approvalLayout->addWidget(approveButton);

    auto* approvalStatus = new QLabel(blockingCount == 0 ? "Ready for approval" : "Resolve blocking issues first", approvalSection);
    approvalStatus->setFont(makeFont(10));
    approvalStatus->setWordWrap(true);
    approvalLayout->addWidget(approvalStatus);
    layout->addWidget(approvalSection);

    QObject::connect(approveButton, &QPushButton::clicked, panel, [approvalStatus]() {
        approvalStatus->setText("Layout approved");
    });

    auto* inspectorSection = createPanelSection(panel);
    auto* inspectorLayout = new QVBoxLayout(inspectorSection);
    inspectorLayout->setContentsMargins(8, 8, 8, 8);
    inspectorLayout->setSpacing(8);

    auto* inspectorHeader = new QLabel("Inspector", inspectorSection);
    inspectorHeader->setFont(makeFont(16));
    inspectorLayout->addWidget(inspectorHeader);

    *inspectorTitle = new QLabel("No issue selected", inspectorSection);
    (*inspectorTitle)->setFont(makeFont(12));
    (*inspectorTitle)->setWordWrap(true);
    inspectorLayout->addWidget(*inspectorTitle);

    *inspectorDetail = new QLabel("Select an issue from the left panel.", inspectorSection);
    (*inspectorDetail)->setFont(makeFont(10));
    (*inspectorDetail)->setWordWrap(true);
    inspectorLayout->addWidget(*inspectorDetail);
    layout->addWidget(inspectorSection);

    layout->addStretch(1);
    return panel;
}

QWidget* createBottomPanel(const QString& projectName, QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* label = new QLabel(projectName.isEmpty() ? "Layout Review" : QString("Layout Review - %1").arg(projectName), panel);
    label->setFont(makeFont(14));
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
