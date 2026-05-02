#pragma once

#include <functional>
#include <vector>

#include <QIcon>
#include <QString>
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
    bool showReviewPanelToggle{true};
    int navigationRailWidth{56};
    int navigationPanelWidth{260};
    int reviewPanelWidth{280};
};

struct WorkspaceNavigationTab {
    QString id{};
    QString label{};
    QIcon icon{};
};

class WorkspaceShell : public QWidget {
public:
    explicit WorkspaceShell(QWidget* parent = nullptr);
    explicit WorkspaceShell(WorkspaceShellOptions options, QWidget* parent = nullptr);

    void setTools(const QStringList& tools);
    void setBackHandler(std::function<void()> handler);
    QPushButton* createBackButton(QWidget* parent = nullptr) const;
    QWidget* createPanelHeader(const QString& title, QWidget* parent = nullptr, bool includeBackButton = true) const;
    void setNavigationTabs(
        std::vector<WorkspaceNavigationTab> tabs,
        const QString& activeTabId,
        std::function<void(const QString&)> tabChangedHandler);
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
    void updateReviewPanelToggle();
    QWidget* createDefaultNavigationRail();
    QWidget* createNavigationTabRail();
    void rebuildDefaultNavigationRail();
    void applyNavigationMode();
    void handleNavigationTabClicked(const QString& tabId);
    void clearTopBar();
    void rebuildTopBar();
    QPushButton* createTopBarButton(const QString& text);

    QFrame* topBar_{nullptr};
    QBoxLayout* topBarLayout_{nullptr};
    QBoxLayout* topBarSystemTrailingLayout_{nullptr};
    QBoxLayout* topBarTrailingLayout_{nullptr};
    QBoxLayout* navigationRailLayout_{nullptr};
    QBoxLayout* navigationLayout_{nullptr};
    QBoxLayout* canvasLayout_{nullptr};
    QBoxLayout* reviewLayout_{nullptr};
    QWidget* navigationCluster_{nullptr};
    QWidget* navigationRail_{nullptr};
    QFrame* navigationPanel_{nullptr};
    QFrame* reviewPanel_{nullptr};
    QPushButton* reviewPanelToggleButton_{nullptr};
    int navigationRailWidth_{56};
    int navigationPanelWidth_{260};
    int reviewPanelWidth_{280};
    bool reviewPanelVisible_{true};
    QAction* openProjectAction_{nullptr};
    QAction* saveProjectAction_{nullptr};
    std::function<void()> openProjectHandler_{};
    std::function<void()> saveProjectHandler_{};
    std::function<void()> backHandler_{};
    QStringList tools_{};
    std::vector<WorkspaceNavigationTab> navigationTabs_{};
    QString activeNavigationTabId_{};
    std::function<void(const QString&)> navigationTabChangedHandler_{};
    WorkspaceNavigationMode navigationMode_{WorkspaceNavigationMode::RailAndPanel};
    bool navigationPanelCollapsed_{false};
    bool customNavigationRail_{false};
};

}  // namespace safecrowd::application
