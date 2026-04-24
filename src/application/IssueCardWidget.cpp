#include "application/IssueCardWidget.h"

#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

#include "application/UiStyle.h"
#include "domain/ImportIssue.h"

namespace safecrowd::application {
namespace {

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
    setStyleSheet(ui::cardStyleSheet());
    setCursor(Qt::PointingHandCursor);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto* title = new QLabel(issueTitle(issue_), this);
    title->setFont(ui::font(ui::FontRole::Body));
    title->setWordWrap(true);
    layout->addWidget(title);

    auto* severity = new QLabel(QString::fromUtf8(safecrowd::domain::toString(issue_.severity)), this);
    severity->setFont(ui::font(ui::FontRole::Caption));
    const auto severityColor = issue_.blocksSimulation()
        ? QColor("#b54708")
        : (issue_.severity == safecrowd::domain::ImportIssueSeverity::Warning ? QColor("#c27100") : QColor("#1f5fae"));
    severity->setStyleSheet(ui::severityTextStyleSheet(severityColor));
    layout->addWidget(severity);

    if (!issue_.message.empty()) {
        auto* message = new QLabel(QString::fromStdString(issue_.message), this);
        message->setFont(ui::font(ui::FontRole::Body));
        message->setWordWrap(true);
        message->setStyleSheet(ui::mutedTextStyleSheet());
        layout->addWidget(message);
    }

    const auto targetText = compactTarget(issue_);
    if (!targetText.isEmpty()) {
        auto* target = new QLabel(targetText, this);
        target->setFont(ui::font(ui::FontRole::Caption));
        target->setWordWrap(true);
        target->setStyleSheet(ui::subtleTextStyleSheet());
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
