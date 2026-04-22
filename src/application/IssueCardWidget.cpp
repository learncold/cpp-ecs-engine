#include "application/IssueCardWidget.h"

#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

#include "domain/ImportIssue.h"

namespace safecrowd::application {
namespace {

QFont makeIssueFont(int pointSize) {
    QFont font;
    font.setPointSize(pointSize);
    font.setWeight(QFont::Normal);
    return font;
}

QString issueTitle(const safecrowd::domain::ImportIssue& issue) {
    return QString::fromUtf8(safecrowd::domain::toString(issue.code));
}

QString compactTarget(const safecrowd::domain::ImportIssue& issue) {
    if (!issue.targetId.empty()) {
        return QString("Target: %1").arg(QString::fromStdString(issue.targetId));
    }
    if (!issue.sourceId.empty()) {
        return QString("Source: %1").arg(QString::fromStdString(issue.sourceId));
    }
    return {};
}

}  // namespace

IssueCardWidget::IssueCardWidget(
    safecrowd::domain::ImportIssue issue,
    std::function<void(const safecrowd::domain::ImportIssue&)> selectIssueHandler,
    QWidget* parent)
    : QFrame(parent),
      issue_(std::move(issue)),
      selectIssueHandler_(std::move(selectIssueHandler)) {
    setFrameShape(QFrame::StyledPanel);
    setLineWidth(1);
    setStyleSheet(
        "IssueCardWidget {"
        " border: 1px solid #888888;"
        " background: #ffffff;"
        "}"
        "IssueCardWidget:hover {"
        " background: #f5f5f5;"
        "}"
        "QLabel { border: 0; background: transparent; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 9);
    layout->setSpacing(6);

    auto* title = new QLabel(issueTitle(issue_), this);
    title->setFont(makeIssueFont(10));
    title->setWordWrap(true);
    layout->addWidget(title);

    auto* severity = new QLabel(QString::fromUtf8(safecrowd::domain::toString(issue_.severity)), this);
    severity->setFont(makeIssueFont(8));
    severity->setStyleSheet("QLabel { color: #a33a24; }");
    layout->addWidget(severity);

    if (!issue_.message.empty()) {
        auto* message = new QLabel(QString::fromStdString(issue_.message), this);
        message->setFont(makeIssueFont(9));
        message->setWordWrap(true);
        message->setStyleSheet("QLabel { color: #333333; }");
        layout->addWidget(message);
    }

    const auto targetText = compactTarget(issue_);
    if (!targetText.isEmpty()) {
        auto* target = new QLabel(targetText, this);
        target->setFont(makeIssueFont(8));
        target->setWordWrap(true);
        target->setStyleSheet("QLabel { color: #555555; }");
        layout->addWidget(target);
    }
}

void IssueCardWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && selectIssueHandler_) {
        selectIssueHandler_(issue_);
        event->accept();
        return;
    }

    QFrame::mousePressEvent(event);
}

}  // namespace safecrowd::application
