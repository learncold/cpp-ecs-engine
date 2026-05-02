#pragma once

#include <functional>
#include <vector>

#include <QSet>
#include <QString>
#include <QWidget>

namespace safecrowd::application {

struct NavigationTreeNode {
    QString label{};
    QString id{};
    QString detail{};
    std::vector<NavigationTreeNode> children{};
    bool expanded{true};
    bool selectable{true};
};

struct NavigationTreeState {
    QSet<QString> expandedNodeIds{};
    QString selectedId{};
    bool restoreExpandedState{false};
};

class NavigationTreeWidget : public QWidget {
public:
    explicit NavigationTreeWidget(
        const QString& title,
        std::vector<NavigationTreeNode> nodes,
        const QString& emptyText,
        std::function<void(const QString&)> activateItemHandler = {},
        QWidget* parent = nullptr,
        QWidget* headerWidget = nullptr,
        NavigationTreeState state = {},
        std::function<void(const QSet<QString>&)> expandedStateChangedHandler = {});
};

}  // namespace safecrowd::application
