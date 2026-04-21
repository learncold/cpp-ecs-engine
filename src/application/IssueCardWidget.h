#pragma once

#include <functional>

#include <QFrame>

#include "domain/ImportIssue.h"

namespace safecrowd::application {

class IssueCardWidget : public QFrame {
public:
    IssueCardWidget(
        safecrowd::domain::ImportIssue issue,
        std::function<void(const safecrowd::domain::ImportIssue&)> selectIssueHandler,
        QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    safecrowd::domain::ImportIssue issue_{};
    std::function<void(const safecrowd::domain::ImportIssue&)> selectIssueHandler_{};
};

}  // namespace safecrowd::application
