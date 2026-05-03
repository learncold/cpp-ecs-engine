#pragma once

#include <functional>

#include <QWidget>

#include "application/NewProjectRequest.h"

class QLineEdit;

namespace safecrowd::application {

class NewProjectWidget : public QWidget {
public:
    explicit NewProjectWidget(QWidget* parent = nullptr);

    void setDoneHandler(std::function<void(const NewProjectRequest&)> handler);
    void setCancelHandler(std::function<void()> handler);

private:
    NewProjectRequest request() const;

    QLineEdit* projectNameEdit_{nullptr};
    QLineEdit* layoutPathEdit_{nullptr};
    QLineEdit* folderPathEdit_{nullptr};
    bool folderEditedByUser_{false};
    std::function<void(const NewProjectRequest&)> doneHandler_{};
    std::function<void()> cancelHandler_{};
};

}  // namespace safecrowd::application
