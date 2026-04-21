#pragma once

#include <functional>

#include <QWidget>

namespace safecrowd::application {

class ProjectNavigatorActions : public QWidget {
public:
    explicit ProjectNavigatorActions(QWidget* parent = nullptr);

    void setNewProjectHandler(std::function<void()> handler);

private:
    std::function<void()> newProjectHandler_{};
};

}  // namespace safecrowd::application
