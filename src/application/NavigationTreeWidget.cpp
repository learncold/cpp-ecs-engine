#include "application/NavigationTreeWidget.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "application/UiStyle.h"

namespace safecrowd::application {
namespace {

constexpr int kIdRole = Qt::UserRole;
constexpr int kSelectableRole = Qt::UserRole + 1;

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
}

class NavigationTreeView final : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event != nullptr && event->button() == Qt::LeftButton) {
            if (auto* item = toggleAreaItem(event->position()); item != nullptr) {
                suppressToggleRelease_ = true;
                item->setExpanded(!item->isExpanded());
                event->accept();
                return;
            }
        }

        suppressToggleRelease_ = false;
        QTreeWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (suppressToggleRelease_) {
            suppressToggleRelease_ = false;
            if (event != nullptr) {
                event->accept();
            }
            return;
        }

        QTreeWidget::mouseReleaseEvent(event);
    }

    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override {
        if (!model()->hasChildren(index)) {
            return;
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#4f5d6b"));

        const QPoint center = rect.center();
        QPolygon arrow;
        if (isExpanded(index)) {
            arrow << QPoint(center.x() - 5, center.y() - 2)
                  << QPoint(center.x() + 5, center.y() - 2)
                  << QPoint(center.x(), center.y() + 4);
        } else {
            arrow << QPoint(center.x() - 2, center.y() - 5)
                  << QPoint(center.x() - 2, center.y() + 5)
                  << QPoint(center.x() + 4, center.y());
        }
        painter->drawPolygon(arrow);
        painter->restore();
    }

private:
    QTreeWidgetItem* toggleAreaItem(const QPointF& position) const {
        auto* item = itemAt(position.toPoint());
        if (item == nullptr || item->childCount() <= 0) {
            return nullptr;
        }

        int depth = 0;
        for (auto* parentItem = item->parent(); parentItem != nullptr; parentItem = parentItem->parent()) {
            ++depth;
        }

        constexpr int kExtraToggleWidth = 24;
        const int toggleWidth = ((depth + 1) * indentation()) + kExtraToggleWidth;
        return position.x() <= toggleWidth ? item : nullptr;
    }

    bool suppressToggleRelease_{false};
};

class NavigationTreeDelegate final : public QStyledItemDelegate {
public:
    explicit NavigationTreeDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);

        const bool selected = itemOption.state & QStyle::State_Selected;
        const bool hovered = itemOption.state & QStyle::State_MouseOver;
        itemOption.state &= ~QStyle::State_Selected;
        itemOption.state &= ~QStyle::State_MouseOver;
        if (selected) {
            itemOption.palette.setColor(QPalette::Text, QColor("#1f5fae"));
        }

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        if (selected || hovered) {
            const auto* widget = itemOption.widget;
            const QRect textRect = widget == nullptr
                ? option.rect
                : widget->style()->subElementRect(QStyle::SE_ItemViewItemText, &itemOption, widget);
            const QRect backgroundRect = textRect.adjusted(-8, -3, 8, 3);
            painter->setPen(selected ? QColor("#b8c6d6") : QColor("#d7e0ea"));
            painter->setBrush(selected ? QColor("#e6eef8") : QColor("#eef3f8"));
            painter->drawRoundedRect(backgroundRect, 6, 6);
        }

        QStyledItemDelegate::paint(painter, itemOption, index);
        painter->restore();
    }
};

QString navigationTreeStyleSheet(bool interactive) {
    const auto itemHover = interactive
        ? QString(
            "QTreeWidget::item:hover {"
            " background: transparent;"
            " border-color: transparent;"
            "}")
        : QString(
            "QTreeWidget::item:hover {"
            " background: transparent;"
            " border-color: transparent;"
            "}");
    const auto itemSelected = interactive
        ? QString(
            "QTreeWidget::item:selected {"
            " background: transparent;"
            " border-color: transparent;"
            " color: #1f5fae;"
            "}")
        : QString(
            "QTreeWidget::item:selected {"
            " background: transparent;"
            " border-color: transparent;"
            " color: #16202b;"
            "}");

    return QString(
        "QTreeWidget {"
        " background: transparent;"
        " border: 0;"
        " outline: 0;"
        " color: #16202b;"
        " show-decoration-selected: 0;"
        "}"
        "QTreeWidget::item {"
        " min-height: 26px;"
        " padding: 3px 6px;"
        " border: 1px solid transparent;"
        " border-radius: 6px;"
        "}"
        "%1"
        "%2"
        "QTreeWidget::branch {"
        " background: transparent;"
        " border: 0;"
        " image: none;"
        "}"
        "QTreeWidget::branch:selected, QTreeWidget::branch:hover,"
        "QTreeWidget::branch:has-children:selected, QTreeWidget::branch:has-children:hover,"
        "QTreeWidget::branch:!has-children:selected, QTreeWidget::branch:!has-children:hover,"
        "QTreeWidget::branch:closed:selected, QTreeWidget::branch:closed:hover,"
        "QTreeWidget::branch:open:selected, QTreeWidget::branch:open:hover,"
        "QTreeView::branch:selected, QTreeView::branch:hover,"
        "QTreeView::branch:has-children:selected, QTreeView::branch:has-children:hover,"
        "QTreeView::branch:!has-children:selected, QTreeView::branch:!has-children:hover,"
        "QTreeView::branch:closed:selected, QTreeView::branch:closed:hover,"
        "QTreeView::branch:open:selected, QTreeView::branch:open:hover {"
        " background: transparent;"
        " border: 0;"
        " image: none;"
        "}"
    ).arg(itemHover, itemSelected);
}

void collectExpandedIds(const QTreeWidgetItem* item, QSet<QString>& expandedIds) {
    if (item == nullptr) {
        return;
    }

    const auto id = item->data(0, kIdRole).toString();
    if (item->isExpanded() && !id.isEmpty()) {
        expandedIds.insert(id);
    }
    for (int index = 0; index < item->childCount(); ++index) {
        collectExpandedIds(item->child(index), expandedIds);
    }
}

QSet<QString> collectExpandedIds(const QTreeWidget* tree) {
    QSet<QString> expandedIds;
    if (tree == nullptr) {
        return expandedIds;
    }

    const auto* root = tree->invisibleRootItem();
    for (int index = 0; index < root->childCount(); ++index) {
        collectExpandedIds(root->child(index), expandedIds);
    }
    return expandedIds;
}

QTreeWidgetItem* addTreeNode(
    QTreeWidgetItem* parentItem,
    const NavigationTreeNode& node,
    const NavigationTreeState& state,
    QTreeWidgetItem** selectedItem) {
    auto* item = new QTreeWidgetItem(parentItem);
    item->setText(0, node.label);
    item->setToolTip(0, node.detail.isEmpty() ? node.label : node.detail);
    item->setData(0, kIdRole, node.id);
    item->setData(0, kSelectableRole, node.selectable);
    if (!node.selectable || node.id.isEmpty()) {
        item->setForeground(0, QColor("#4f5d6b"));
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    }

    for (const auto& child : node.children) {
        addTreeNode(item, child, state, selectedItem);
    }
    const bool expanded = state.restoreExpandedState && !node.id.isEmpty()
        ? state.expandedNodeIds.contains(node.id)
        : node.expanded;
    item->setExpanded(expanded);
    if (selectedItem != nullptr && *selectedItem == nullptr && !state.selectedId.isEmpty() && node.id == state.selectedId) {
        *selectedItem = item;
    }

    return item;
}

}  // namespace

NavigationTreeWidget::NavigationTreeWidget(
    const QString& title,
    std::vector<NavigationTreeNode> nodes,
    const QString& emptyText,
    std::function<void(const QString&)> activateItemHandler,
    QWidget* parent,
    QWidget* headerWidget,
    NavigationTreeState state,
    std::function<void(const QSet<QString>&)> expandedStateChangedHandler)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(headerWidget != nullptr ? headerWidget : createLabel(title, this, ui::FontRole::Title));

    if (nodes.empty()) {
        auto* empty = createLabel(emptyText, this);
        empty->setStyleSheet(ui::mutedTextStyleSheet());
        layout->addWidget(empty);
        layout->addStretch(1);
        return;
    }

    const bool interactive = static_cast<bool>(activateItemHandler);
    auto* tree = new NavigationTreeView(this);
    tree->setHeaderHidden(true);
    tree->setColumnCount(1);
    tree->setRootIsDecorated(true);
    tree->setAnimated(true);
    tree->setIndentation(16);
    tree->setFrameShape(QFrame::NoFrame);
    tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree->setSelectionMode(interactive ? QAbstractItemView::SingleSelection : QAbstractItemView::NoSelection);
    tree->setFocusPolicy(interactive ? Qt::StrongFocus : Qt::NoFocus);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->setUniformRowHeights(true);
    tree->setFont(ui::font(ui::FontRole::Body));
    tree->setItemDelegate(new NavigationTreeDelegate(tree));
    tree->setStyleSheet(navigationTreeStyleSheet(interactive));

    QTreeWidgetItem* selectedItem = nullptr;
    for (const auto& node : nodes) {
        addTreeNode(tree->invisibleRootItem(), node, state, &selectedItem);
    }
    if (selectedItem != nullptr) {
        auto* ancestor = selectedItem->parent();
        while (ancestor != nullptr) {
            ancestor->setExpanded(true);
            ancestor = ancestor->parent();
        }
        tree->setCurrentItem(selectedItem);
        selectedItem->setSelected(true);
    }

    if (activateItemHandler) {
        QObject::connect(tree, &QTreeWidget::itemClicked, tree, [activateItemHandler, tree](QTreeWidgetItem* item, int) {
            if (item == nullptr) {
                return;
            }

            const auto selectable = item->data(0, kSelectableRole).toBool();
            const auto id = item->data(0, kIdRole).toString();
            if (selectable && !id.isEmpty()) {
                QTimer::singleShot(0, tree, [activateItemHandler, id]() {
                    activateItemHandler(id);
                });
            }
        });
    } else {
        QObject::connect(tree, &QTreeWidget::itemClicked, tree, [tree](QTreeWidgetItem*, int) {
            tree->clearSelection();
            tree->setCurrentIndex(QModelIndex());
        });
    }

    if (expandedStateChangedHandler) {
        const auto notifyExpandedStateChanged = [tree, expandedStateChangedHandler]() {
            expandedStateChangedHandler(collectExpandedIds(tree));
        };
        QObject::connect(tree, &QTreeWidget::itemExpanded, tree, [notifyExpandedStateChanged](QTreeWidgetItem*) {
            notifyExpandedStateChanged();
        });
        QObject::connect(tree, &QTreeWidget::itemCollapsed, tree, [notifyExpandedStateChanged](QTreeWidgetItem*) {
            notifyExpandedStateChanged();
        });
    }

    layout->addWidget(tree, 1);
}

}  // namespace safecrowd::application
