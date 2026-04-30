#include "application/LayoutNavigationPanelWidget.h"

#include <utility>
#include <vector>

#include <QVBoxLayout>

#include "application/NavigationTreeWidget.h"

namespace safecrowd::application {
namespace {

QString zoneLabel(const safecrowd::domain::Zone2D& zone) {
    const auto id = QString::fromStdString(zone.id);
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty() ? id : QString("%1  -  %2").arg(label, id);
}

NavigationTreeNode makeZoneNode(const safecrowd::domain::Zone2D& zone) {
    return {
        .label = zoneLabel(zone),
        .id = QString::fromStdString(zone.id),
        .detail = QString("Zone: %1").arg(QString::fromStdString(zone.id)),
    };
}

NavigationTreeNode makeSection(const QString& label, std::vector<NavigationTreeNode> children) {
    return {
        .label = label,
        .children = std::move(children),
        .expanded = true,
        .selectable = false,
    };
}

template <typename Predicate>
std::vector<NavigationTreeNode> collectZones(
    const safecrowd::domain::FacilityLayout2D& layout,
    Predicate predicate) {
    std::vector<NavigationTreeNode> nodes;
    for (const auto& zone : layout.zones) {
        if (predicate(zone)) {
            nodes.push_back(makeZoneNode(zone));
        }
    }
    return nodes;
}

std::vector<NavigationTreeNode> buildLayoutTree(const safecrowd::domain::FacilityLayout2D* facilityLayout) {
    if (facilityLayout == nullptr) {
        return {};
    }

    std::vector<NavigationTreeNode> nodes;

    auto rooms = collectZones(*facilityLayout, [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Room
            || zone.kind == safecrowd::domain::ZoneKind::Intersection
            || zone.kind == safecrowd::domain::ZoneKind::Unknown;
    });
    if (!rooms.empty()) {
        nodes.push_back(makeSection("Rooms", std::move(rooms)));
    }

    auto exits = collectZones(*facilityLayout, [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Exit
            || zone.kind == safecrowd::domain::ZoneKind::Stair;
    });
    if (!exits.empty()) {
        nodes.push_back(makeSection("Exits", std::move(exits)));
    }

    if (!facilityLayout->connections.empty()) {
        std::vector<NavigationTreeNode> connections;
        for (const auto& connection : facilityLayout->connections) {
            connections.push_back({
                .label = QString("%1  ->  %2")
                             .arg(QString::fromStdString(connection.fromZoneId), QString::fromStdString(connection.toZoneId)),
                .id = QString::fromStdString(connection.id),
                .detail = QString("Connection: %1").arg(QString::fromStdString(connection.id)),
            });
        }
        nodes.push_back(makeSection("Connections", std::move(connections)));
    }

    if (!facilityLayout->barriers.empty()) {
        std::vector<NavigationTreeNode> barriers;
        for (const auto& barrier : facilityLayout->barriers) {
            barriers.push_back({
                .label = QString::fromStdString(barrier.id),
                .id = QString::fromStdString(barrier.id),
                .detail = QString("Wall: %1").arg(QString::fromStdString(barrier.id)),
            });
        }
        nodes.push_back(makeSection("Walls", std::move(barriers)));
    }

    return nodes;
}

}  // namespace

LayoutNavigationPanelWidget::LayoutNavigationPanelWidget(
    const safecrowd::domain::FacilityLayout2D* facilityLayout,
    std::function<void(const QString&)> selectElementHandler,
    QWidget* parent,
    QWidget* headerWidget)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(new NavigationTreeWidget(
        "Layout",
        buildLayoutTree(facilityLayout),
        "No recognized layout elements",
        std::move(selectElementHandler),
        this,
        headerWidget));
}

}  // namespace safecrowd::application
