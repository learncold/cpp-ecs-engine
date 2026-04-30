#include "application/LayoutNavigationPanelWidget.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <QVBoxLayout>

#include "application/NavigationTreeWidget.h"

namespace safecrowd::application {
namespace {

constexpr double kGeometryEpsilon = 1e-4;

QString floorActionId(const std::string& floorId) {
    return QString("floor:%1").arg(QString::fromStdString(floorId));
}

bool matchesFloor(const std::string& elementFloorId, const std::string& floorId) {
    return floorId.empty() || elementFloorId.empty() || elementFloorId == floorId;
}

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

QString floorLabel(const safecrowd::domain::Floor2D& floor) {
    const auto id = QString::fromStdString(floor.id);
    const auto label = QString::fromStdString(floor.label);
    return label.isEmpty() || label == id ? id : QString("%1  -  %2").arg(label, id);
}

QString connectionLabel(const safecrowd::domain::Connection2D& connection) {
    const auto id = QString::fromStdString(connection.id);
    return id.isEmpty()
        ? QString("%1  ->  %2").arg(QString::fromStdString(connection.fromZoneId), QString::fromStdString(connection.toZoneId))
        : id;
}

QString barrierLabel(const safecrowd::domain::Barrier2D& barrier) {
    return QString::fromStdString(barrier.id);
}

NavigationTreeNode makeSection(const QString& label, std::vector<NavigationTreeNode> children, const QString& id = {}) {
    return {
        .label = label,
        .id = id,
        .children = std::move(children),
        .expanded = true,
        .selectable = !id.isEmpty(),
    };
}

double distancePointToSegment(
    const safecrowd::domain::Point2D& point,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end) {
    const auto dx = end.x - start.x;
    const auto dy = end.y - start.y;
    const auto lengthSquared = (dx * dx) + (dy * dy);
    if (lengthSquared <= kGeometryEpsilon) {
        return std::hypot(point.x - start.x, point.y - start.y);
    }

    const auto t = std::clamp(
        (((point.x - start.x) * dx) + ((point.y - start.y) * dy)) / lengthSquared,
        0.0,
        1.0);
    return std::hypot(point.x - (start.x + (dx * t)), point.y - (start.y + (dy * t)));
}

double distanceToPolygonBoundary(
    const safecrowd::domain::Polygon2D& polygon,
    const safecrowd::domain::Point2D& point) {
    double best = std::numeric_limits<double>::max();
    const auto checkRing = [&](const std::vector<safecrowd::domain::Point2D>& ring) {
        if (ring.size() < 2) {
            return;
        }
        for (std::size_t index = 0; index < ring.size(); ++index) {
            best = std::min(best, distancePointToSegment(point, ring[index], ring[(index + 1) % ring.size()]));
        }
    };

    checkRing(polygon.outline);
    for (const auto& hole : polygon.holes) {
        checkRing(hole);
    }
    return best;
}

bool barrierTouchesZone(const safecrowd::domain::Barrier2D& barrier, const safecrowd::domain::Zone2D& zone) {
    for (const auto& point : barrier.geometry.vertices) {
        if (distanceToPolygonBoundary(zone.area, point) <= 0.08) {
            return true;
        }
    }
    return false;
}

bool isRoomLikeZone(const safecrowd::domain::Zone2D& zone) {
    return zone.kind == safecrowd::domain::ZoneKind::Room
        || zone.kind == safecrowd::domain::ZoneKind::Intersection
        || zone.kind == safecrowd::domain::ZoneKind::Unknown
        || zone.kind == safecrowd::domain::ZoneKind::Exit
        || zone.kind == safecrowd::domain::ZoneKind::Stair;
}

std::vector<NavigationTreeNode> roomChildren(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Zone2D& room) {
    std::vector<NavigationTreeNode> children;

    for (const auto& barrier : layout.barriers) {
        if (!matchesFloor(barrier.floorId, room.floorId) || !barrierTouchesZone(barrier, room)) {
            continue;
        }
        children.push_back({
            .label = QString("Wall  -  %1").arg(barrierLabel(barrier)),
            .id = QString::fromStdString(barrier.id),
            .detail = QString("Wall: %1").arg(QString::fromStdString(barrier.id)),
        });
    }

    for (const auto& connection : layout.connections) {
        if (!matchesFloor(connection.floorId, room.floorId)
            || (connection.fromZoneId != room.id && connection.toZoneId != room.id)) {
            continue;
        }
        children.push_back({
            .label = QString("Door  -  %1").arg(connectionLabel(connection)),
            .id = QString::fromStdString(connection.id),
            .detail = QString("Door: %1").arg(QString::fromStdString(connection.id)),
        });
    }

    return children;
}

std::vector<safecrowd::domain::Floor2D> layoutFloors(const safecrowd::domain::FacilityLayout2D& layout) {
    if (!layout.floors.empty()) {
        return layout.floors;
    }

    std::vector<safecrowd::domain::Floor2D> floors;
    const auto fallback = layout.levelId.empty() ? std::string{"L1"} : layout.levelId;
    floors.push_back({.id = fallback, .label = fallback});
    return floors;
}

std::vector<NavigationTreeNode> buildLayoutTree(const safecrowd::domain::FacilityLayout2D* facilityLayout) {
    if (facilityLayout == nullptr) {
        return {};
    }

    std::vector<NavigationTreeNode> nodes;
    for (const auto& floor : layoutFloors(*facilityLayout)) {
        std::vector<NavigationTreeNode> rooms;
        for (const auto& zone : facilityLayout->zones) {
            if (!matchesFloor(zone.floorId, floor.id) || !isRoomLikeZone(zone)) {
                continue;
            }
            auto roomNode = makeZoneNode(zone);
            roomNode.children = roomChildren(*facilityLayout, zone);
            rooms.push_back(std::move(roomNode));
        }
        nodes.push_back(makeSection(floorLabel(floor), std::move(rooms), floorActionId(floor.id)));
    }

    if (nodes.empty()) {
        std::vector<NavigationTreeNode> rooms;
        for (const auto& zone : facilityLayout->zones) {
            if (isRoomLikeZone(zone)) {
                auto roomNode = makeZoneNode(zone);
                roomNode.children = roomChildren(*facilityLayout, zone);
                rooms.push_back(std::move(roomNode));
            }
        }
        if (!rooms.empty()) {
            nodes.push_back({
                .label = "Layout",
                .children = std::move(rooms),
                .expanded = true,
                .selectable = false,
            });
        }
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
