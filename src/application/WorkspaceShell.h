#pragma once

#include <functional>

#include <QStringList>
#include <QWidget>

class QAction;
class QBoxLayout;
class QFrame;
class QPushButton;

namespace safecrowd::application {

class WorkspaceShell : public QWidget {
public:
    explicit WorkspaceShell(QWidget* parent = nullptr);

    void setTools(const QStringList& tools);
    void setNavigationRail(QWidget* rail);
    void setNavigationPanel(QWidget* panel);
    void setReviewPanel(QWidget* panel);
    void setReviewPanelVisible(bool visible);
    void setTopBarTrailingWidget(QWidget* widget);
    void setCanvas(QWidget* canvas);
    void setSaveProjectHandler(std::function<void()> handler);
    void setOpenProjectHandler(std::function<void()> handler);

private:
    void clearTopBar();
    QPushButton* createTopBarButton(const QString& text);

    QBoxLayout* topBarLayout_{nullptr};
    QBoxLayout* topBarTrailingLayout_{nullptr};
    QBoxLayout* navigationRailLayout_{nullptr};
    QBoxLayout* navigationLayout_{nullptr};
    QBoxLayout* canvasLayout_{nullptr};
    QBoxLayout* reviewLayout_{nullptr};
    QFrame* reviewPanel_{nullptr};
    QAction* openProjectAction_{nullptr};
    QAction* saveProjectAction_{nullptr};
    std::function<void()> openProjectHandler_{};
    std::function<void()> saveProjectHandler_{};
};

}  // namespace safecrowd::application
