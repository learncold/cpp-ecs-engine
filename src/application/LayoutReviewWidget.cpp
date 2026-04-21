#include "application/LayoutReviewWidget.h"

#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

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

QFrame* createIssueCard(const safecrowd::domain::ImportIssue& issue, QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setFrameShape(QFrame::StyledPanel);
    card->setLineWidth(1);
    card->setStyleSheet(
        "QFrame {"
        " border: 1px solid #777777;"
        " background: #ffffff;"
        "}"
        "QLabel { border: 0; }");

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 7, 8, 8);
    layout->setSpacing(5);

    auto* title = new QLabel(issueTitle(issue), card);
    title->setFont(makeFont(11));
    title->setWordWrap(true);
    layout->addWidget(title);

    auto* severity = new QLabel(QString::fromUtf8(safecrowd::domain::toString(issue.severity)), card);
    severity->setFont(makeFont(9));
    severity->setStyleSheet("QLabel { color: #a33a24; }");
    layout->addWidget(severity);

    const auto detailText = issueDetail(issue);
    if (!detailText.isEmpty()) {
        auto* detail = new QLabel(detailText, card);
        detail->setFont(makeFont(9));
        detail->setWordWrap(true);
        detail->setStyleSheet("QLabel { color: #333333; }");
        layout->addWidget(detail);
    }

    return card;
}

QWidget* createNavigationPanel(const safecrowd::domain::ImportResult& importResult, QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* blocking = new QLabel("Blocking", panel);
    blocking->setFont(makeFont(20));
    layout->addWidget(blocking);

    auto* scrollArea = new QScrollArea(panel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* scrollContent = new QWidget(scrollArea);
    auto* issueLayout = new QVBoxLayout(scrollContent);
    issueLayout->setContentsMargins(0, 0, 0, 0);
    issueLayout->setSpacing(8);

    int blockingIssueCount = 0;
    for (const auto& issue : importResult.issues) {
        if (!issue.blocksSimulation()) {
            continue;
        }

        ++blockingIssueCount;
        issueLayout->addWidget(createIssueCard(issue, scrollContent));
    }

    if (blockingIssueCount == 0) {
        auto* emptyLabel = new QLabel("No blocking issues", scrollContent);
        emptyLabel->setFont(makeFont(11));
        emptyLabel->setWordWrap(true);
        emptyLabel->setStyleSheet("QLabel { color: #555555; }");
        issueLayout->addWidget(emptyLabel);
    }

    issueLayout->addStretch(1);
    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea, 1);

    layout->addStretch(1);
    return panel;
}

QWidget* createReviewPanel(const safecrowd::domain::ImportResult& importResult, QWidget* parent) {
    auto* panel = new QWidget(parent);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* status = new QLabel(importResult.issues.empty() ? "No issues" : QString("%1 issues").arg(importResult.issues.size()), panel);
    status->setFont(makeFont(14));
    layout->addWidget(status);
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
    QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* shell = new WorkspaceShell(this);
    shell->setTools({"Project", "Tool"});
    shell->setSaveProjectHandler(std::move(saveProjectHandler));
    shell->setNavigationPanel(createNavigationPanel(importResult, shell));
    shell->setCanvas(new LayoutPreviewWidget(importResult, shell));
    shell->setReviewPanel(createReviewPanel(importResult, shell));
    shell->setBottomPanel(createBottomPanel(projectName, shell));
    layout->addWidget(shell);
}

}  // namespace safecrowd::application
