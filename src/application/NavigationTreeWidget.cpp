#include "application/NavigationTreeWidget.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
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

QTreeWidgetItem* addTreeNode(QTreeWidgetItem* parentItem, const NavigationTreeNode& node) {
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
        addTreeNode(item, child);
    }
    item->setExpanded(node.expanded);

    return item;
}

}  // namespace

NavigationTreeWidget::NavigationTreeWidget(
    const QString& title,
    std::vector<NavigationTreeNode> nodes,
    const QString& emptyText,
    std::function<void(const QString&)> activateItemHandler,
    QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    layout->addWidget(createLabel(title, this, ui::FontRole::Title));

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

    for (const auto& node : nodes) {
        addTreeNode(tree->invisibleRootItem(), node);
    }

    if (activateItemHandler) {
        QObject::connect(tree, &QTreeWidget::itemClicked, tree, [activateItemHandler](QTreeWidgetItem* item, int) {
            if (item == nullptr) {
                return;
            }

            const auto selectable = item->data(0, kSelectableRole).toBool();
            const auto id = item->data(0, kIdRole).toString();
            if (selectable && !id.isEmpty()) {
                activateItemHandler(id);
            }
        });
    } else {
        QObject::connect(tree, &QTreeWidget::itemClicked, tree, [tree](QTreeWidgetItem*, int) {
            tree->clearSelection();
            tree->setCurrentIndex(QModelIndex());
        });
    }

    layout->addWidget(tree, 1);
}

}  // namespace safecrowd::application
