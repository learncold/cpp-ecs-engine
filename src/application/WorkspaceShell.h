#pragma once

#include <functional>

#include <QStringList>
#include <QWidget>

class QAction;
class QBoxLayout;
class QPushButton;

namespace safecrowd::application {

class WorkspaceShell : public QWidget {
public:
    explicit WorkspaceShell(QWidget* parent = nullptr);

    void setTools(const QStringList& tools);
    void setNavigationPanel(QWidget* panel);
    void setReviewPanel(QWidget* panel);
    void setCanvas(QWidget* canvas);
    void setBottomPanel(QWidget* panel);
    void setSaveProjectHandler(std::function<void()> handler);

private:
    void clearTopBar();
    QPushButton* createTopBarButton(const QString& text);

    QBoxLayout* topBarLayout_{nullptr};
    QBoxLayout* navigationLayout_{nullptr};
    QBoxLayout* canvasLayout_{nullptr};
    QBoxLayout* reviewLayout_{nullptr};
    QBoxLayout* bottomLayout_{nullptr};
    QAction* saveProjectAction_{nullptr};
    std::function<void()> saveProjectHandler_{};
};

}  // namespace safecrowd::application
