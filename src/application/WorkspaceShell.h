#pragma once

#include <functional>

#include <QStringList>
#include <QWidget>

class QAction;
class QBoxLayout;
class QFrame;
class QPushButton;
class QWidget;

namespace safecrowd::application {

enum class WorkspaceNavigationMode {
    None,
    RailOnly,
    PanelOnly,
    RailAndPanel,
};

struct WorkspaceShellOptions {
    bool showTopBar{true};
    WorkspaceNavigationMode navigationMode{WorkspaceNavigationMode::RailAndPanel};
    bool showReviewPanel{true};
    int navigationRailWidth{56};
    int navigationPanelWidth{260};
    int reviewPanelWidth{280};
};

class WorkspaceShell : public QWidget {
public:
    explicit WorkspaceShell(QWidget* parent = nullptr);
    explicit WorkspaceShell(WorkspaceShellOptions options, QWidget* parent = nullptr);

    void setTools(const QStringList& tools);
    void setBackHandler(std::function<void()> handler);
    QPushButton* createBackButton(QWidget* parent = nullptr) const;
    QWidget* createPanelHeader(const QString& title, QWidget* parent = nullptr, bool includeBackButton = true) const;
    void setNavigationRail(QWidget* rail);
    void setNavigationPanel(QWidget* panel);
    void setNavigationVisible(bool visible);
    void setNavigationMode(WorkspaceNavigationMode mode);
    void setReviewPanel(QWidget* panel);
    void setReviewPanelVisible(bool visible);
    void setTopBarTrailingWidget(QWidget* widget);
    void setCanvas(QWidget* canvas);
    void setSaveProjectHandler(std::function<void()> handler);
    void setOpenProjectHandler(std::function<void()> handler);

private:
    void initialize(const WorkspaceShellOptions& options);
    void setFixedWidthVisible(QWidget* widget, bool visible, int width);
    QWidget* createDefaultNavigationRail();
    void rebuildDefaultNavigationRail();
    void clearTopBar();
    void rebuildTopBar();
    QPushButton* createTopBarButton(const QString& text);

    QFrame* topBar_{nullptr};
    QBoxLayout* topBarLayout_{nullptr};
    QBoxLayout* topBarTrailingLayout_{nullptr};
    QBoxLayout* navigationRailLayout_{nullptr};
    QBoxLayout* navigationLayout_{nullptr};
    QBoxLayout* canvasLayout_{nullptr};
    QBoxLayout* reviewLayout_{nullptr};
    QWidget* navigationCluster_{nullptr};
    QWidget* navigationRail_{nullptr};
    QFrame* navigationPanel_{nullptr};
    QFrame* reviewPanel_{nullptr};
    int navigationRailWidth_{56};
    int navigationPanelWidth_{260};
    int reviewPanelWidth_{280};
    QAction* openProjectAction_{nullptr};
    QAction* saveProjectAction_{nullptr};
    std::function<void()> openProjectHandler_{};
    std::function<void()> saveProjectHandler_{};
    std::function<void()> backHandler_{};
    QStringList tools_{};
    bool customNavigationRail_{false};
};

}  // namespace safecrowd::application
