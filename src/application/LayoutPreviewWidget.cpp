#include "application/LayoutPreviewWidget.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include "application/LayoutCanvasRendering.h"
#include "application/LayoutCanvasSnapping.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QToolButton>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

constexpr double kConnectionWidth = 1.2;
constexpr double kConnectionHitTolerance = 10.0;
constexpr double kDraftMinimumSize = 0.2;
constexpr double kGeometryEpsilon = 1e-4;
constexpr double kMinimumDoorWidth = 0.9;
constexpr int kTopToolbarHeight = 44;
constexpr int kPropertyPanelHeight = 42;
constexpr int kSideToolbarWidth = 44;
constexpr int kToolbarButtonSize = 44;

QRectF previewViewport(const QRect& widgetRect) {
    return layoutCanvasViewport(widgetRect, kSideToolbarWidth + 16, kTopToolbarHeight + kPropertyPanelHeight + 16, 16, 16);
}

using Bounds2D = LayoutCanvasBounds;
using LayoutTransform = LayoutCanvasTransform;

void includePoint(Bounds2D& bounds, const safecrowd::domain::Point2D& point) {
    includeLayoutCanvasPoint(bounds, point);
}

void includePolygon(Bounds2D& bounds, const safecrowd::domain::Polygon2D& polygon) {
    includeLayoutCanvasPolygon(bounds, polygon);
}

void includePolyline(Bounds2D& bounds, const safecrowd::domain::Polyline2D& polyline) {
    includeLayoutCanvasPolyline(bounds, polyline);
}

void includeLine(Bounds2D& bounds, const safecrowd::domain::LineSegment2D& line) {
    includeLayoutCanvasLine(bounds, line);
}

bool matchesFloor(const std::string& elementFloorId, const QString& floorId) {
    return floorId.isEmpty() || elementFloorId.empty() || QString::fromStdString(elementFloorId) == floorId;
}

QString defaultFloorId(const safecrowd::domain::FacilityLayout2D& layout) {
    if (!layout.floors.empty() && !layout.floors.front().id.empty()) {
        return QString::fromStdString(layout.floors.front().id);
    }
    if (!layout.levelId.empty()) {
        return QString::fromStdString(layout.levelId);
    }
    return "L1";
}

QString floorDisplayLabel(const safecrowd::domain::Floor2D& floor) {
    const auto label = QString::fromStdString(floor.label);
    const auto id = QString::fromStdString(floor.id);
    if (label.isEmpty() || label == id) {
        return id;
    }
    return QString("%1  -  %2").arg(label, id);
}

QString nextFloorId(const safecrowd::domain::FacilityLayout2D& layout) {
    int suffix = 1;
    for (const auto& floor : layout.floors) {
        const auto id = QString::fromStdString(floor.id);
        if (!id.startsWith("L")) {
            continue;
        }
        bool ok = false;
        const int value = id.mid(1).toInt(&ok);
        if (ok) {
            suffix = std::max(suffix, value + 1);
        }
    }
    return QString("L%1").arg(suffix);
}

void ensureLayoutFloors(safecrowd::domain::FacilityLayout2D& layout) {
    const auto floorId = defaultFloorId(layout);
    if (layout.floors.empty()) {
        layout.floors.push_back({
            .id = floorId.toStdString(),
            .label = floorId.toStdString(),
        });
    }
    if (layout.levelId.empty()) {
        layout.levelId = floorId.toStdString();
    }
    for (auto& zone : layout.zones) {
        if (zone.floorId.empty()) {
            zone.floorId = floorId.toStdString();
        }
    }
    for (auto& connection : layout.connections) {
        if (connection.floorId.empty()) {
            connection.floorId = floorId.toStdString();
        }
    }
    for (auto& barrier : layout.barriers) {
        if (barrier.floorId.empty()) {
            barrier.floorId = floorId.toStdString();
        }
    }
    for (auto& control : layout.controls) {
        if (control.floorId.empty()) {
            control.floorId = floorId.toStdString();
        }
    }
}

std::optional<Bounds2D> collectBounds(const safecrowd::domain::ImportResult& importResult, const QString& floorId) {
    if (importResult.layout.has_value()) {
        const auto filteredBounds = collectLayoutCanvasBounds(*importResult.layout, floorId.toStdString());
        if (filteredBounds.has_value()) {
            return filteredBounds;
        }
        const auto layoutBounds = collectLayoutCanvasBounds(*importResult.layout);
        if (layoutBounds.has_value()) {
            return layoutBounds;
        }
    }

    return collectLayoutCanvasBounds(importResult);
}

QPainterPath polygonPath(const safecrowd::domain::Polygon2D& polygon, const LayoutTransform& transform) {
    return layoutCanvasPolygonPath(polygon, transform);
}

QPolygonF polylinePath(const safecrowd::domain::Polyline2D& polyline, const LayoutTransform& transform) {
    return layoutCanvasPolylinePath(polyline, transform);
}

void drawLine(QPainter& painter, const safecrowd::domain::LineSegment2D& line, const LayoutTransform& transform) {
    drawLayoutCanvasLine(painter, line, transform);
}

void drawPolyline(QPainter& painter, const safecrowd::domain::Polyline2D& polyline, const LayoutTransform& transform) {
    drawLayoutCanvasPolyline(painter, polyline, transform);
}

bool stringListContains(const std::vector<std::string>& values, const QString& target) {
    return std::any_of(values.begin(), values.end(), [&](const std::string& value) {
        return QString::fromStdString(value) == target;
    });
}

bool traceMatches(const safecrowd::domain::ElementProvenance& provenance, const QString& targetId) {
    return stringListContains(provenance.sourceIds, targetId) || stringListContains(provenance.canonicalIds, targetId);
}

bool traceRefMatches(const safecrowd::domain::ImportResult& importResult, const QString& elementId, const QString& targetId) {
    if (elementId == targetId) {
        return true;
    }

    for (const auto& traceRef : importResult.traceRefs) {
        if (QString::fromStdString(traceRef.targetId) != elementId) {
            continue;
        }

        return QString::fromStdString(traceRef.targetId) == targetId
            || stringListContains(traceRef.sourceIds, targetId)
            || stringListContains(traceRef.canonicalIds, targetId);
    }

    return false;
}

void includeMatchingGeometryBounds(const safecrowd::domain::ImportResult& importResult, const QString& targetId, Bounds2D& bounds) {
    if (targetId.isEmpty()) {
        return;
    }

    if (importResult.layout.has_value()) {
        for (const auto& zone : importResult.layout->zones) {
            if (QString::fromStdString(zone.id) == targetId || traceMatches(zone.provenance, targetId)) {
                includePolygon(bounds, zone.area);
            }
        }
        for (const auto& connection : importResult.layout->connections) {
            if (QString::fromStdString(connection.id) == targetId || traceMatches(connection.provenance, targetId)) {
                includeLine(bounds, connection.centerSpan);
            }
        }
        for (const auto& barrier : importResult.layout->barriers) {
            if (QString::fromStdString(barrier.id) == targetId || traceMatches(barrier.provenance, targetId)) {
                includePolyline(bounds, barrier.geometry);
            }
        }
    }

    if (importResult.canonicalGeometry.has_value()) {
        for (const auto& walkable : importResult.canonicalGeometry->walkableAreas) {
            const auto id = QString::fromStdString(walkable.id);
            if (traceRefMatches(importResult, id, targetId)) {
                includePolygon(bounds, walkable.polygon);
            }
        }
        for (const auto& obstacle : importResult.canonicalGeometry->obstacles) {
            const auto id = QString::fromStdString(obstacle.id);
            if (traceRefMatches(importResult, id, targetId)) {
                includePolygon(bounds, obstacle.footprint);
            }
        }
        for (const auto& wall : importResult.canonicalGeometry->walls) {
            const auto id = QString::fromStdString(wall.id);
            if (traceRefMatches(importResult, id, targetId)) {
                includeLine(bounds, wall.segment);
            }
        }
        for (const auto& opening : importResult.canonicalGeometry->openings) {
            const auto id = QString::fromStdString(opening.id);
            if (traceRefMatches(importResult, id, targetId)) {
                includeLine(bounds, opening.span);
            }
        }
    }
}

QString zoneKindLabel(safecrowd::domain::ZoneKind kind) {
    using safecrowd::domain::ZoneKind;

    switch (kind) {
    case ZoneKind::Room:
        return "Room";
    case ZoneKind::Exit:
        return "Exit";
    case ZoneKind::Intersection:
        return "Intersection";
    case ZoneKind::Stair:
        return "Stair";
    case ZoneKind::Unknown:
    default:
        return "Unknown";
    }
}

QString connectionKindLabel(safecrowd::domain::ConnectionKind kind) {
    using safecrowd::domain::ConnectionKind;

    switch (kind) {
    case ConnectionKind::Doorway:
        return "Doorway";
    case ConnectionKind::Opening:
        return "Opening";
    case ConnectionKind::Exit:
        return "Exit";
    case ConnectionKind::Stair:
        return "Stair";
    case ConnectionKind::Ramp:
        return "Ramp";
    case ConnectionKind::Unknown:
    default:
        return "Unknown";
    }
}

Bounds2D polygonBounds(const safecrowd::domain::Polygon2D& polygon) {
    Bounds2D bounds;
    includePolygon(bounds, polygon);
    return bounds;
}

safecrowd::domain::Point2D polygonAnchor(const safecrowd::domain::Polygon2D& polygon) {
    const auto bounds = polygonBounds(polygon);
    if (!bounds.valid()) {
        return {};
    }

    return {
        .x = (bounds.minX + bounds.maxX) / 2.0,
        .y = (bounds.minY + bounds.maxY) / 2.0,
    };
}

double distanceToSegment(const QPointF& point, const QPointF& start, const QPointF& end) {
    const auto dx = end.x() - start.x();
    const auto dy = end.y() - start.y();
    const auto lengthSquared = (dx * dx) + (dy * dy);
    if (lengthSquared <= 0.0) {
        return std::hypot(point.x() - start.x(), point.y() - start.y());
    }

    const auto t = std::clamp(
        ((point.x() - start.x()) * dx + (point.y() - start.y()) * dy) / lengthSquared,
        0.0,
        1.0);
    const QPointF projection(start.x() + (t * dx), start.y() + (t * dy));
    return std::hypot(point.x() - projection.x(), point.y() - projection.y());
}

bool nearlyEqual(double a, double b, double epsilon);

bool containsZone(const safecrowd::domain::FacilityLayout2D& layout, const QString& zoneId) {
    return std::any_of(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return QString::fromStdString(zone.id) == zoneId;
    });
}

bool containsConnection(const safecrowd::domain::FacilityLayout2D& layout, const QString& connectionId) {
    return std::any_of(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        return QString::fromStdString(connection.id) == connectionId;
    });
}

bool containsBarrier(const safecrowd::domain::FacilityLayout2D& layout, const QString& barrierId) {
    return std::any_of(layout.barriers.begin(), layout.barriers.end(), [&](const auto& barrier) {
        return QString::fromStdString(barrier.id) == barrierId;
    });
}

bool hasConnectionPair(const safecrowd::domain::FacilityLayout2D& layout, const QString& fromZoneId, const QString& toZoneId) {
    return std::any_of(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        const auto from = QString::fromStdString(connection.fromZoneId);
        const auto to = QString::fromStdString(connection.toZoneId);
        return (from == fromZoneId && to == toZoneId) || (from == toZoneId && to == fromZoneId);
    });
}

bool segmentsShareSpan(
    const safecrowd::domain::LineSegment2D& first,
    const safecrowd::domain::Point2D& secondStart,
    const safecrowd::domain::Point2D& secondEnd) {
    const bool firstVertical = nearlyEqual(first.start.x, first.end.x, kGeometryEpsilon);
    const bool firstHorizontal = nearlyEqual(first.start.y, first.end.y, kGeometryEpsilon);
    const bool secondVertical = nearlyEqual(secondStart.x, secondEnd.x, kGeometryEpsilon);
    const bool secondHorizontal = nearlyEqual(secondStart.y, secondEnd.y, kGeometryEpsilon);

    if (firstVertical && secondVertical && nearlyEqual(first.start.x, secondStart.x, kGeometryEpsilon)) {
        const auto firstMin = std::min(first.start.y, first.end.y);
        const auto firstMax = std::max(first.start.y, first.end.y);
        const auto secondMin = std::min(secondStart.y, secondEnd.y);
        const auto secondMax = std::max(secondStart.y, secondEnd.y);
        return std::max(firstMin, secondMin) < std::min(firstMax, secondMax) - kGeometryEpsilon;
    }

    if (firstHorizontal && secondHorizontal && nearlyEqual(first.start.y, secondStart.y, kGeometryEpsilon)) {
        const auto firstMin = std::min(first.start.x, first.end.x);
        const auto firstMax = std::max(first.start.x, first.end.x);
        const auto secondMin = std::min(secondStart.x, secondEnd.x);
        const auto secondMax = std::max(secondStart.x, secondEnd.x);
        return std::max(firstMin, secondMin) < std::min(firstMax, secondMax) - kGeometryEpsilon;
    }

    return false;
}

bool hasConnectionPairAtSpan(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& fromZoneId,
    const QString& toZoneId,
    const safecrowd::domain::Point2D& spanStart,
    const safecrowd::domain::Point2D& spanEnd) {
    return std::any_of(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        const auto from = QString::fromStdString(connection.fromZoneId);
        const auto to = QString::fromStdString(connection.toZoneId);
        const bool samePair = (from == fromZoneId && to == toZoneId) || (from == toZoneId && to == fromZoneId);
        return samePair && segmentsShareSpan(connection.centerSpan, spanStart, spanEnd);
    });
}

QString nextConnectionId(const safecrowd::domain::FacilityLayout2D& layout) {
    int suffix = 1;
    for (const auto& connection : layout.connections) {
        const auto id = QString::fromStdString(connection.id);
        if (!id.startsWith("connection-user-")) {
            continue;
        }

        bool ok = false;
        const auto value = id.mid(QString("connection-user-").size()).toInt(&ok);
        if (ok) {
            suffix = std::max(suffix, value + 1);
        }
    }

    return QString("connection-user-%1").arg(suffix);
}

QString nextZoneId(const safecrowd::domain::FacilityLayout2D& layout) {
    int suffix = 1;
    for (const auto& zone : layout.zones) {
        const auto id = QString::fromStdString(zone.id);
        if (!id.startsWith("zone-user-")) {
            continue;
        }

        bool ok = false;
        const auto value = id.mid(QString("zone-user-").size()).toInt(&ok);
        if (ok) {
            suffix = std::max(suffix, value + 1);
        }
    }

    return QString("zone-user-%1").arg(suffix);
}

QString nextBarrierId(const safecrowd::domain::FacilityLayout2D& layout) {
    int suffix = 1;
    for (const auto& barrier : layout.barriers) {
        const auto id = QString::fromStdString(barrier.id);
        if (!id.startsWith("barrier-user-")) {
            continue;
        }

        bool ok = false;
        const auto value = id.mid(QString("barrier-user-").size()).toInt(&ok);
        if (ok) {
            suffix = std::max(suffix, value + 1);
        }
    }

    return QString("barrier-user-%1").arg(suffix);
}

QString nextVerticalConnectionId(const safecrowd::domain::FacilityLayout2D& layout) {
    int suffix = 1;
    for (const auto& connection : layout.connections) {
        const auto id = QString::fromStdString(connection.id);
        if (!id.startsWith("vertical-user-")) {
            continue;
        }

        bool ok = false;
        const auto value = id.mid(QString("vertical-user-").size()).toInt(&ok);
        if (ok) {
            suffix = std::max(suffix, value + 1);
        }
    }

    return QString("vertical-user-%1").arg(suffix);
}

QString zoneTitle(const safecrowd::domain::Zone2D& zone) {
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty()
        ? QString("Zone %1").arg(QString::fromStdString(zone.id))
        : QString("%1 (%2)").arg(label, QString::fromStdString(zone.id));
}

QString floorLabelForId(const safecrowd::domain::FacilityLayout2D& layout, const QString& floorId) {
    const auto it = std::find_if(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
        return QString::fromStdString(floor.id) == floorId;
    });
    return it == layout.floors.end() ? floorId : floorDisplayLabel(*it);
}

QString zoneDetail(const safecrowd::domain::FacilityLayout2D& layout, const safecrowd::domain::Zone2D& zone) {
    int connectionCount = 0;
    for (const auto& connection : layout.connections) {
        if (connection.fromZoneId == zone.id || connection.toZoneId == zone.id) {
            ++connectionCount;
        }
    }

    return QString("Kind: %1\nId: %2\nConnections: %3")
        .arg(zoneKindLabel(zone.kind), QString::fromStdString(zone.id))
        .arg(connectionCount);
}

QString connectionTitle(const safecrowd::domain::Connection2D& connection) {
    return QString("Connection %1").arg(QString::fromStdString(connection.id));
}

QString connectionDetail(const safecrowd::domain::Connection2D& connection) {
    const auto widthText = connection.effectiveWidth > 0.0
        ? QString::number(connection.effectiveWidth, 'f', 2)
        : QString("n/a");
    return QString("Kind: %1\nFrom: %2\nTo: %3\nWidth: %4")
        .arg(
            connectionKindLabel(connection.kind),
            QString::fromStdString(connection.fromZoneId),
            QString::fromStdString(connection.toZoneId),
            widthText);
}

bool pointInRing(const std::vector<safecrowd::domain::Point2D>& ring, const QPointF& point) {
    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const auto& a = ring[i];
        const auto& b = ring[j];
        const auto intersects = ((a.y > point.y()) != (b.y > point.y()))
            && (point.x() < ((b.x - a.x) * (point.y() - a.y) / ((b.y - a.y) == 0.0 ? 1e-9 : (b.y - a.y)) + a.x));
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool pointInPolygon(const safecrowd::domain::Polygon2D& polygon, const QPointF& point) {
    if (polygon.outline.size() < 3 || !pointInRing(polygon.outline, point)) {
        return false;
    }

    for (const auto& hole : polygon.holes) {
        if (hole.size() >= 3 && pointInRing(hole, point)) {
            return false;
        }
    }

    return true;
}

double distanceToLineSegmentWorld(const QPointF& point, const safecrowd::domain::Point2D& start, const safecrowd::domain::Point2D& end) {
    return distanceToSegment(point, QPointF(start.x, start.y), QPointF(end.x, end.y));
}

double distanceToPolygonBoundary(const safecrowd::domain::Polygon2D& polygon, const QPointF& point) {
    double best = std::numeric_limits<double>::max();
    const auto checkRing = [&](const auto& ring) {
        if (ring.size() < 2) {
            return;
        }
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const auto& a = ring[i];
            const auto& b = ring[(i + 1) % ring.size()];
            best = std::min(best, distanceToLineSegmentWorld(point, a, b));
        }
    };

    checkRing(polygon.outline);
    for (const auto& hole : polygon.holes) {
        checkRing(hole);
    }
    return best;
}

std::vector<std::size_t> zonesNearPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& point,
    const QString& floorId) {
    std::vector<std::size_t> matches;
    for (std::size_t index = 0; index < layout.zones.size(); ++index) {
        const auto& zone = layout.zones[index];
        if (!matchesFloor(zone.floorId, floorId)) {
            continue;
        }
        if (pointInPolygon(zone.area, point) || distanceToPolygonBoundary(zone.area, point) <= 0.35) {
            matches.push_back(index);
        }
    }
    return matches;
}

std::vector<std::size_t> zonesContainingPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& point,
    const QString& floorId,
    double tolerance = 0.15) {
    std::vector<std::size_t> matches;
    for (std::size_t index = 0; index < layout.zones.size(); ++index) {
        const auto& zone = layout.zones[index];
        if (!matchesFloor(zone.floorId, floorId)) {
            continue;
        }
        if (pointInPolygon(zone.area, point) || distanceToPolygonBoundary(zone.area, point) <= tolerance) {
            matches.push_back(index);
        }
    }
    return matches;
}

QString barrierTitle(const safecrowd::domain::Barrier2D& barrier) {
    return QString("Wall %1").arg(QString::fromStdString(barrier.id));
}

QString barrierDetail(const safecrowd::domain::Barrier2D& barrier) {
    return QString("Id: %1\nVertices: %2")
        .arg(QString::fromStdString(barrier.id))
        .arg(static_cast<int>(barrier.geometry.vertices.size()));
}

QRectF rectFromWorldPoints(const QPointF& startWorld, const QPointF& endWorld) {
    const auto left = std::min(startWorld.x(), endWorld.x());
    const auto right = std::max(startWorld.x(), endWorld.x());
    const auto top = std::max(startWorld.y(), endWorld.y());
    const auto bottom = std::min(startWorld.y(), endWorld.y());
    return QRectF(QPointF(left, bottom), QPointF(right, top));
}

void appendBarrierSegment(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end,
    const std::string& floorId) {
    layout.barriers.push_back({
        .id = nextBarrierId(layout).toStdString(),
        .floorId = floorId,
        .geometry = safecrowd::domain::Polyline2D{
            .vertices = {start, end},
            .closed = false,
        },
        .blocksMovement = true,
    });
}

QPainterPath worldPolygonPath(const safecrowd::domain::Polygon2D& polygon) {
    QPainterPath path;
    if (polygon.outline.empty()) {
        return path;
    }

    path.moveTo(QPointF(polygon.outline.front().x, -polygon.outline.front().y));
    for (std::size_t i = 1; i < polygon.outline.size(); ++i) {
        path.lineTo(QPointF(polygon.outline[i].x, -polygon.outline[i].y));
    }
    path.closeSubpath();

    for (const auto& hole : polygon.holes) {
        if (hole.empty()) {
            continue;
        }
        path.moveTo(QPointF(hole.front().x, -hole.front().y));
        for (std::size_t i = 1; i < hole.size(); ++i) {
            path.lineTo(QPointF(hole[i].x, -hole[i].y));
        }
        path.closeSubpath();
    }

    return path;
}

double polygonArea(const QPolygonF& polygon) {
    if (polygon.size() < 3) {
        return 0.0;
    }

    double area = 0.0;
    for (int i = 0; i < polygon.size(); ++i) {
        const QPointF& a = polygon[i];
        const QPointF& b = polygon[(i + 1) % polygon.size()];
        area += (a.x() * b.y()) - (b.x() * a.y());
    }

    return std::abs(area) * 0.5;
}

std::vector<safecrowd::domain::Polygon2D> polygonsFromFillPath(const QPainterPath& path) {
    std::vector<safecrowd::domain::Polygon2D> polygons;

    for (const auto& fillPolygon : path.toFillPolygons()) {
        QPolygonF normalized = fillPolygon;
        if (normalized.size() >= 2 && normalized.front() == normalized.back()) {
            normalized.removeLast();
        }
        if (normalized.size() < 3 || polygonArea(normalized) < kDraftMinimumSize) {
            continue;
        }

        safecrowd::domain::Polygon2D polygon;
        polygon.outline.reserve(static_cast<std::size_t>(normalized.size()));
        for (const auto& point : normalized) {
            polygon.outline.push_back({
                .x = point.x(),
                .y = -point.y(),
            });
        }
        polygons.push_back(std::move(polygon));
    }

    return polygons;
}

bool nearlyEqual(double a, double b, double epsilon = kGeometryEpsilon) {
    return std::abs(a - b) <= epsilon;
}

std::vector<std::pair<double, double>> subtractInterval(
    const std::vector<std::pair<double, double>>& source,
    double overlapStart,
    double overlapEnd) {
    std::vector<std::pair<double, double>> next;
    const auto clippedStart = std::min(overlapStart, overlapEnd);
    const auto clippedEnd = std::max(overlapStart, overlapEnd);

    for (const auto& interval : source) {
        const auto start = interval.first;
        const auto end = interval.second;
        if (clippedEnd <= start + kGeometryEpsilon || clippedStart >= end - kGeometryEpsilon) {
            next.push_back(interval);
            continue;
        }
        if (clippedStart > start + kGeometryEpsilon) {
            next.emplace_back(start, clippedStart);
        }
        if (clippedEnd < end - kGeometryEpsilon) {
            next.emplace_back(clippedEnd, end);
        }
    }

    return next;
}

std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> subtractBarrierOverlaps(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end) {
    const bool vertical = nearlyEqual(start.x, end.x);
    const bool horizontal = nearlyEqual(start.y, end.y);
    if (!vertical && !horizontal) {
        return {{start, end}};
    }

    const auto axisStart = vertical ? std::min(start.y, end.y) : std::min(start.x, end.x);
    const auto axisEnd = vertical ? std::max(start.y, end.y) : std::max(start.x, end.x);
    std::vector<std::pair<double, double>> remaining{{axisStart, axisEnd}};

    for (const auto& barrier : layout.barriers) {
        if (barrier.geometry.vertices.size() != 2) {
            continue;
        }

        const auto& a = barrier.geometry.vertices[0];
        const auto& b = barrier.geometry.vertices[1];
        const bool barrierVertical = nearlyEqual(a.x, b.x);
        const bool barrierHorizontal = nearlyEqual(a.y, b.y);
        if (vertical && barrierVertical && nearlyEqual(a.x, start.x)) {
            remaining = subtractInterval(remaining, std::min(a.y, b.y), std::max(a.y, b.y));
        } else if (horizontal && barrierHorizontal && nearlyEqual(a.y, start.y)) {
            remaining = subtractInterval(remaining, std::min(a.x, b.x), std::max(a.x, b.x));
        }

        if (remaining.empty()) {
            break;
        }
    }

    std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> segments;
    for (const auto& interval : remaining) {
        if (interval.second - interval.first <= kGeometryEpsilon) {
            continue;
        }

        if (vertical) {
            segments.push_back({
                {.x = start.x, .y = interval.first},
                {.x = start.x, .y = interval.second},
            });
        } else {
            segments.push_back({
                {.x = interval.first, .y = start.y},
                {.x = interval.second, .y = start.y},
            });
        }
    }

    return segments;
}

void appendAutoWallsForPolygon(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Polygon2D& polygon,
    const std::string& floorId) {
    if (polygon.outline.size() < 2) {
        return;
    }

    for (std::size_t i = 0; i < polygon.outline.size(); ++i) {
        const auto& start = polygon.outline[i];
        const auto& end = polygon.outline[(i + 1) % polygon.outline.size()];
        for (const auto& segment : subtractBarrierOverlaps(layout, start, end)) {
            appendBarrierSegment(layout, segment.first, segment.second, floorId);
        }
    }
}

safecrowd::domain::LineSegment2D centerSpanForRectangle(const QRectF& rectangle) {
    const double inset = std::min(rectangle.width(), rectangle.height()) * 0.25;
    if (rectangle.width() >= rectangle.height()) {
        const double y = rectangle.center().y();
        return {
            .start = {.x = rectangle.left() + inset, .y = y},
            .end = {.x = rectangle.right() - inset, .y = y},
        };
    }

    const double x = rectangle.center().x();
    const double north = std::max(rectangle.top(), rectangle.bottom());
    const double south = std::min(rectangle.top(), rectangle.bottom());
    return {
        .start = {.x = x, .y = north - inset},
        .end = {.x = x, .y = south + inset},
    };
}

QPointF entrySideMidpoint(const QRectF& rectangle, safecrowd::domain::StairEntryDirection direction) {
    const double north = std::max(rectangle.top(), rectangle.bottom());
    const double south = std::min(rectangle.top(), rectangle.bottom());
    switch (direction) {
    case safecrowd::domain::StairEntryDirection::North:
        return {rectangle.center().x(), north};
    case safecrowd::domain::StairEntryDirection::East:
        return {rectangle.right(), rectangle.center().y()};
    case safecrowd::domain::StairEntryDirection::South:
        return {rectangle.center().x(), south};
    case safecrowd::domain::StairEntryDirection::West:
        return {rectangle.left(), rectangle.center().y()};
    case safecrowd::domain::StairEntryDirection::Unspecified:
        return rectangle.center();
    }
    return rectangle.center();
}

safecrowd::domain::LineSegment2D entrySpanForRectangle(
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection direction) {
    const auto openingHalfWidth = std::max(0.35, std::min(rectangle.width(), rectangle.height()) * 0.35);
    const auto center = entrySideMidpoint(rectangle, direction);
    switch (direction) {
    case safecrowd::domain::StairEntryDirection::North:
    case safecrowd::domain::StairEntryDirection::South:
        return {
            .start = {.x = center.x() - openingHalfWidth, .y = center.y()},
            .end = {.x = center.x() + openingHalfWidth, .y = center.y()},
        };
    case safecrowd::domain::StairEntryDirection::East:
    case safecrowd::domain::StairEntryDirection::West:
        return {
            .start = {.x = center.x(), .y = center.y() - openingHalfWidth},
            .end = {.x = center.x(), .y = center.y() + openingHalfWidth},
        };
    case safecrowd::domain::StairEntryDirection::Unspecified:
        return centerSpanForRectangle(rectangle);
    }
    return centerSpanForRectangle(rectangle);
}

QPointF entryOutsideSample(const QRectF& rectangle, safecrowd::domain::StairEntryDirection direction) {
    const auto offset = std::max(rectangle.width(), rectangle.height()) * 0.25;
    auto sample = entrySideMidpoint(rectangle, direction);
    switch (direction) {
    case safecrowd::domain::StairEntryDirection::North:
        sample.ry() += offset;
        break;
    case safecrowd::domain::StairEntryDirection::East:
        sample.rx() += offset;
        break;
    case safecrowd::domain::StairEntryDirection::South:
        sample.ry() -= offset;
        break;
    case safecrowd::domain::StairEntryDirection::West:
        sample.rx() -= offset;
        break;
    case safecrowd::domain::StairEntryDirection::Unspecified:
        break;
    }
    return sample;
}

void appendStairWallsExceptEntry(
    safecrowd::domain::FacilityLayout2D& layout,
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection entryDirection,
    const std::string& floorId) {
    const double north = std::max(rectangle.top(), rectangle.bottom());
    const double south = std::min(rectangle.top(), rectangle.bottom());
    const std::pair<safecrowd::domain::StairEntryDirection, safecrowd::domain::LineSegment2D> sides[] = {
        {safecrowd::domain::StairEntryDirection::North, {{rectangle.left(), north}, {rectangle.right(), north}}},
        {safecrowd::domain::StairEntryDirection::East, {{rectangle.right(), north}, {rectangle.right(), south}}},
        {safecrowd::domain::StairEntryDirection::South, {{rectangle.right(), south}, {rectangle.left(), south}}},
        {safecrowd::domain::StairEntryDirection::West, {{rectangle.left(), south}, {rectangle.left(), north}}},
    };

    for (const auto& [side, segment] : sides) {
        if (side == entryDirection) {
            continue;
        }
        appendBarrierSegment(layout, segment.start, segment.end, floorId);
    }
}

void populateStairEntryCombo(QComboBox& comboBox, safecrowd::domain::StairEntryDirection selected) {
    comboBox.addItem("North", static_cast<int>(safecrowd::domain::StairEntryDirection::North));
    comboBox.addItem("East", static_cast<int>(safecrowd::domain::StairEntryDirection::East));
    comboBox.addItem("South", static_cast<int>(safecrowd::domain::StairEntryDirection::South));
    comboBox.addItem("West", static_cast<int>(safecrowd::domain::StairEntryDirection::West));
    const auto index = comboBox.findData(static_cast<int>(selected));
    if (index >= 0) {
        comboBox.setCurrentIndex(index);
    }
}

bool pointNearSegmentWorld(
    const QPointF& point,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end,
    double tolerance = 0.2) {
    return distanceToLineSegmentWorld(point, start, end) <= tolerance;
}

std::optional<QPointF> projectOntoSegment(
    const QPointF& point,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end) {
    const QPointF a(start.x, start.y);
    const QPointF b(end.x, end.y);
    const auto dx = b.x() - a.x();
    const auto dy = b.y() - a.y();
    const auto lengthSquared = (dx * dx) + (dy * dy);
    if (lengthSquared <= kGeometryEpsilon) {
        return std::nullopt;
    }

    const auto t = std::clamp(
        ((point.x() - a.x()) * dx + (point.y() - a.y()) * dy) / lengthSquared,
        0.0,
        1.0);
    return QPointF(a.x() + (t * dx), a.y() + (t * dy));
}

std::vector<std::size_t> uniqueZoneMerge(
    const std::vector<std::size_t>& first,
    const std::vector<std::size_t>& second) {
    std::vector<std::size_t> merged = first;
    for (const auto index : second) {
        if (std::find(merged.begin(), merged.end(), index) == merged.end()) {
            merged.push_back(index);
        }
    }
    return merged;
}

std::vector<std::size_t> zonesTouchingSegment(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end,
    const QString& floorId) {
    const QPointF midpoint((start.x + end.x) * 0.5, (start.y + end.y) * 0.5);
    std::vector<std::size_t> matches;
    for (std::size_t index = 0; index < layout.zones.size(); ++index) {
        const auto& zone = layout.zones[index];
        if (!matchesFloor(zone.floorId, floorId)) {
            continue;
        }
        if (pointNearSegmentWorld(midpoint, start, end, 0.25)
            && distanceToPolygonBoundary(zone.area, midpoint) <= 0.25) {
            matches.push_back(index);
        }
    }
    return matches;
}

struct DoorNeighbors {
    std::vector<std::size_t> firstSide{};
    std::vector<std::size_t> secondSide{};
    QPointF firstSample{};
    QPointF secondSample{};
};

DoorNeighbors doorNeighborsAcrossSegment(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end,
    const QString& floorId) {
    const QPointF midpoint((start.x + end.x) * 0.5, (start.y + end.y) * 0.5);
    const auto dx = end.x - start.x;
    const auto dy = end.y - start.y;
    const auto length = std::hypot(dx, dy);
    if (length <= kGeometryEpsilon) {
        return {};
    }

    const QPointF normal(-dy / length, dx / length);
    constexpr double kSampleOffset = 0.18;
    const QPointF sampleA = midpoint + normal * kSampleOffset;
    const QPointF sampleB = midpoint - normal * kSampleOffset;

    DoorNeighbors neighbors;
    neighbors.firstSample = sampleA;
    neighbors.secondSample = sampleB;
    neighbors.firstSide = zonesContainingPoint(layout, sampleA, floorId);
    neighbors.secondSide = zonesContainingPoint(layout, sampleB, floorId);

    if (neighbors.firstSide.empty() && neighbors.secondSide.empty()) {
        const auto fallback = zonesTouchingSegment(layout, start, end, floorId);
        if (!fallback.empty()) {
            neighbors.firstSide = fallback;
        }
    }

    return neighbors;
}

std::optional<std::size_t> choosePrimaryZone(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<std::size_t>& candidates,
    std::optional<std::size_t> preferredOther = std::nullopt) {
    for (const auto index : candidates) {
        if (preferredOther.has_value() && index == *preferredOther) {
            continue;
        }
        if (layout.zones[index].kind != safecrowd::domain::ZoneKind::Exit) {
            return index;
        }
    }
    for (const auto index : candidates) {
        if (!preferredOther.has_value() || index != *preferredOther) {
            return index;
        }
    }
    return std::nullopt;
}

QString createExitZoneAtDoor(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& gapStart,
    const safecrowd::domain::Point2D& gapEnd,
    const QPointF& outsideDirection,
    const std::string& floorId) {
    const auto zoneId = nextZoneId(layout);
    const auto zoneNumber = static_cast<int>(layout.zones.size()) + 1;
    const auto width = std::hypot(gapEnd.x - gapStart.x, gapEnd.y - gapStart.y);
    const auto depth = std::max(0.75, width * 0.6);

    const QPointF a(gapStart.x, gapStart.y);
    const QPointF b(gapEnd.x, gapEnd.y);
    const QPointF offset = outsideDirection * depth;

    layout.zones.push_back({
        .id = zoneId.toStdString(),
        .floorId = floorId,
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = QString("Exit %1").arg(zoneNumber).toStdString(),
        .area = safecrowd::domain::Polygon2D{
            .outline = {
                {.x = a.x(), .y = a.y()},
                {.x = b.x(), .y = b.y()},
                {.x = b.x() + offset.x(), .y = b.y() + offset.y()},
                {.x = a.x() + offset.x(), .y = a.y() + offset.y()},
            },
        },
        .defaultCapacity = 20u,
    });

    return zoneId;
}

void replaceBarrierWithSegments(
    safecrowd::domain::FacilityLayout2D& layout,
    std::size_t barrierIndex,
    const std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>>& segments,
    const std::string& floorId) {
    if (barrierIndex >= layout.barriers.size()) {
        return;
    }

    layout.barriers.erase(layout.barriers.begin() + static_cast<std::ptrdiff_t>(barrierIndex));
    for (const auto& segment : segments) {
        if (std::hypot(segment.second.x - segment.first.x, segment.second.y - segment.first.y) <= kGeometryEpsilon) {
            continue;
        }
        appendBarrierSegment(layout, segment.first, segment.second, floorId);
    }
}

QIcon makeToolIcon(const QString& glyph, const QColor& color, bool filled = false) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.8));
    if (filled) {
        painter.setBrush(color);
    } else {
        painter.setBrush(Qt::NoBrush);
    }

    if (glyph == "room") {
        painter.drawRect(QRectF(4, 5, 16, 14));
    } else if (glyph == "exit") {
        painter.drawRect(QRectF(5, 5, 10, 14));
        painter.drawLine(QPointF(15, 12), QPointF(20, 12));
        painter.drawLine(QPointF(17, 10), QPointF(20, 12));
        painter.drawLine(QPointF(17, 14), QPointF(20, 12));
    } else if (glyph == "wall") {
        painter.drawLine(QPointF(5, 19), QPointF(19, 5));
    } else if (glyph == "door") {
        painter.drawLine(QPointF(5, 18), QPointF(19, 6));
        painter.drawEllipse(QPointF(5, 18), 1.5, 1.5);
        painter.drawEllipse(QPointF(19, 6), 1.5, 1.5);
    } else if (glyph == "stair") {
        painter.drawRect(QRectF(5, 5, 14, 14));
        painter.drawLine(QPointF(7, 16), QPointF(17, 16));
        painter.drawLine(QPointF(7, 12), QPointF(17, 12));
        painter.drawLine(QPointF(7, 8), QPointF(17, 8));
        painter.drawLine(QPointF(9, 18), QPointF(17, 6));
    } else if (glyph == "select") {
        QPainterPath path;
        path.moveTo(6, 4);
        path.lineTo(16, 11);
        path.lineTo(11.5, 12.2);
        path.lineTo(13.5, 18.5);
        path.lineTo(10.7, 19.3);
        path.lineTo(8.9, 13.3);
        path.lineTo(5.5, 16.3);
        path.closeSubpath();
        painter.drawPath(path);
    } else if (glyph == "delete") {
        painter.drawRect(QRectF(7, 8, 10, 11));
        painter.drawLine(QPointF(5, 8), QPointF(19, 8));
        painter.drawLine(QPointF(9, 5), QPointF(15, 5));
    } else if (glyph == "reset") {
        painter.drawArc(QRectF(5, 5, 14, 14), 40 * 16, 280 * 16);
        painter.drawLine(QPointF(15, 4), QPointF(19, 5));
        painter.drawLine(QPointF(15, 4), QPointF(17, 8));
    } else if (glyph == "add") {
        painter.drawLine(QPointF(12, 5), QPointF(12, 19));
        painter.drawLine(QPointF(5, 12), QPointF(19, 12));
    }

    return QIcon(pixmap);
}

std::optional<QString> hitTestZone(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& position,
    const LayoutTransform& transform,
    const QString& floorId) {
    for (auto it = layout.zones.rbegin(); it != layout.zones.rend(); ++it) {
        if (!matchesFloor(it->floorId, floorId)) {
            continue;
        }
        if (polygonPath(it->area, transform).contains(position)) {
            return QString::fromStdString(it->id);
        }
    }

    return std::nullopt;
}

std::optional<QString> hitTestConnection(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& position,
    const LayoutTransform& transform,
    const QString& floorId) {
    double bestDistance = std::numeric_limits<double>::max();
    std::optional<QString> bestId;

    for (const auto& connection : layout.connections) {
        if (!matchesFloor(connection.floorId, floorId)) {
            continue;
        }
        const auto start = transform.map(connection.centerSpan.start);
        const auto end = transform.map(connection.centerSpan.end);
        const auto distance = distanceToSegment(position, start, end);
        if (distance <= kConnectionHitTolerance && distance < bestDistance) {
            bestDistance = distance;
            bestId = QString::fromStdString(connection.id);
        }
    }

    return bestId;
}

std::optional<QString> hitTestBarrier(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& position,
    const LayoutTransform& transform,
    const QString& floorId) {
    double bestDistance = std::numeric_limits<double>::max();
    std::optional<QString> bestId;

    for (const auto& barrier : layout.barriers) {
        if (!matchesFloor(barrier.floorId, floorId)) {
            continue;
        }
        for (std::size_t i = 1; i < barrier.geometry.vertices.size(); ++i) {
            const auto start = transform.map(barrier.geometry.vertices[i - 1]);
            const auto end = transform.map(barrier.geometry.vertices[i]);
            const auto distance = distanceToSegment(position, start, end);
            if (distance <= kConnectionHitTolerance && distance < bestDistance) {
                bestDistance = distance;
                bestId = QString::fromStdString(barrier.id);
            }
        }
    }

    return bestId;
}

}  // namespace

LayoutPreviewWidget::LayoutPreviewWidget(safecrowd::domain::ImportResult importResult, QWidget* parent)
    : QWidget(parent),
      importResult_(std::move(importResult)) {
    if (importResult_.layout.has_value()) {
        ensureLayoutFloors(*importResult_.layout);
        currentFloorId_ = defaultFloorId(*importResult_.layout);
    }
    setMinimumSize(520, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    QCoreApplication::instance()->installEventFilter(this);
    setupToolbars();
    refreshFloorSelector();
    repositionToolbars();
}

LayoutPreviewWidget::~LayoutPreviewWidget() {
    if (auto* app = QCoreApplication::instance(); app != nullptr) {
        app->removeEventFilter(this);
    }
}

void LayoutPreviewWidget::focusElement(const QString& elementId) {
    if (importResult_.layout.has_value()) {
        if (containsZone(*importResult_.layout, elementId)) {
            selectFloorForElement(elementId);
            focusIssueTarget(elementId);
            selectZone(elementId);
            return;
        }
        if (containsConnection(*importResult_.layout, elementId)) {
            selectFloorForElement(elementId);
            focusIssueTarget(elementId);
            selectConnection(elementId);
            return;
        }
        if (containsBarrier(*importResult_.layout, elementId)) {
            selectFloorForElement(elementId);
            focusIssueTarget(elementId);
            selectBarrier(elementId);
            return;
        }
    }

    focusIssueTarget(elementId);
}

void LayoutPreviewWidget::focusIssueTarget(const QString& targetId) {
    selectedZoneId_.clear();
    selectedConnectionId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_ = targetId;

    Bounds2D targetBounds;
    includeMatchingGeometryBounds(importResult_, targetId, targetBounds);
    const auto worldBounds = collectBounds(importResult_, currentFloorId());
    if (targetBounds.valid() && worldBounds.has_value()) {
        const QRectF viewport = previewViewport(rect());
        const auto targetWidth = std::max(targetBounds.maxX - targetBounds.minX, 1.0);
        const auto targetHeight = std::max(targetBounds.maxY - targetBounds.minY, 1.0);
        const auto targetZoom = 0.55 * std::min(viewport.width() / targetWidth, viewport.height() / targetHeight)
            / std::min(viewport.width() / (worldBounds->maxX - worldBounds->minX), viewport.height() / (worldBounds->maxY - worldBounds->minY));
        camera_.setZoom(std::clamp(targetZoom, 1.0, 30.0));

        const LayoutTransform transform(*worldBounds, viewport, camera_.zoom(), {});
        const safecrowd::domain::Point2D center{
            .x = (targetBounds.minX + targetBounds.maxX) / 2.0,
            .y = (targetBounds.minY + targetBounds.maxY) / 2.0,
        };
        camera_.setPanOffset(viewport.center() - transform.map(center));
    }

    update();
}

void LayoutPreviewWidget::resetView() {
    camera_.reset();
    update();
}

void LayoutPreviewWidget::setImportResult(safecrowd::domain::ImportResult importResult) {
    importResult_ = std::move(importResult);

    if (!importResult_.layout.has_value()) {
        currentFloorId_.clear();
        clearSelection();
        if (toolbarCorner_ != nullptr) {
            toolbarCorner_->hide();
        }
        if (topToolbar_ != nullptr) {
            topToolbar_->hide();
        }
        if (propertyPanel_ != nullptr) {
            propertyPanel_->hide();
        }
        if (sideToolbar_ != nullptr) {
            sideToolbar_->hide();
        }
        return;
    }

    ensureLayoutFloors(*importResult_.layout);
    const bool currentFloorExists = std::any_of(
        importResult_.layout->floors.begin(),
        importResult_.layout->floors.end(),
        [&](const auto& floor) {
            return QString::fromStdString(floor.id) == currentFloorId_;
        });
    if (!currentFloorExists) {
        currentFloorId_ = defaultFloorId(*importResult_.layout);
    }

    if (!selectedZoneId_.isEmpty() && !containsZone(*importResult_.layout, selectedZoneId_)) {
        selectedZoneId_.clear();
    }
    if (!selectedConnectionId_.isEmpty() && !containsConnection(*importResult_.layout, selectedConnectionId_)) {
        selectedConnectionId_.clear();
    }
    if (!selectedBarrierId_.isEmpty() && !containsBarrier(*importResult_.layout, selectedBarrierId_)) {
        selectedBarrierId_.clear();
    }

    if (toolbarCorner_ != nullptr) {
        toolbarCorner_->setVisible(true);
    }
    if (topToolbar_ != nullptr) {
        topToolbar_->setVisible(true);
    }
    if (propertyPanel_ != nullptr) {
        propertyPanel_->setVisible(true);
    }
    if (sideToolbar_ != nullptr) {
        sideToolbar_->setVisible(true);
    }

    refreshFloorSelector();
    refreshPropertyPanel();
    update();
}

void LayoutPreviewWidget::setSelectionChangedHandler(std::function<void(const PreviewSelection&)> handler) {
    selectionChangedHandler_ = std::move(handler);
}

void LayoutPreviewWidget::setLayoutEditedHandler(std::function<void(const safecrowd::domain::FacilityLayout2D&)> handler) {
    layoutEditedHandler_ = std::move(handler);
}

bool LayoutPreviewWidget::eventFilter(QObject* watched, QEvent* event) {
    (void)watched;

    camera_.handleGlobalKeyEvent(event);

    return QWidget::eventFilter(watched, event);
}

void LayoutPreviewWidget::keyPressEvent(QKeyEvent* event) {
    if (camera_.handleKeyPress(event)) {
        return;
    }

    QWidget::keyPressEvent(event);
}

void LayoutPreviewWidget::keyReleaseEvent(QKeyEvent* event) {
    if (camera_.handleKeyRelease(event)) {
        return;
    }

    QWidget::keyReleaseEvent(event);
}

void LayoutPreviewWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    QWidget::mouseDoubleClickEvent(event);
    resetView();
}

void LayoutPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!camera_.panning() && !drafting_) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (drafting_) {
        const auto bounds = collectBounds(importResult_, currentFloorId());
        if (bounds.has_value()) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            const auto world = transform.unmap(event->position());
            draftCurrentWorld_ = snapWorldPoint(QPointF(world.x, world.y), transform);
            update();
            event->accept();
            return;
        }
    }

    if (camera_.updatePan(event)) {
        update();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void LayoutPreviewWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);

    if (camera_.beginPan(event)) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const auto bounds = collectBounds(importResult_, currentFloorId());
        if (!bounds.has_value()) {
            QWidget::mousePressEvent(event);
            return;
        }

        if (toolMode_ == ToolMode::DrawDoor) {
            applyToolAt(event->position());
            event->accept();
            return;
        }

        if (toolMode_ != ToolMode::Select && toolMode_ != ToolMode::Delete) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            const auto world = transform.unmap(event->position());
            drafting_ = true;
            draftStartWorld_ = snapWorldPoint(QPointF(world.x, world.y), transform);
            draftCurrentWorld_ = draftStartWorld_;
            event->accept();
            return;
        }

        applyToolAt(event->position());
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void LayoutPreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (camera_.finishPan(event)) {
        return;
    }

    if (drafting_ && event->button() == Qt::LeftButton) {
        drafting_ = false;
        const auto bounds = collectBounds(importResult_, currentFloorId());
        if (bounds.has_value()) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            const auto world = transform.unmap(event->position());
            draftCurrentWorld_ = snapWorldPoint(QPointF(world.x, world.y), transform);
        }

        switch (toolMode_) {
        case ToolMode::DrawRoom:
            createZone(draftStartWorld_, draftCurrentWorld_, safecrowd::domain::ZoneKind::Room);
            break;
        case ToolMode::DrawExit:
            createZone(draftStartWorld_, draftCurrentWorld_, safecrowd::domain::ZoneKind::Exit);
            break;
        case ToolMode::DrawWall:
            createBarrier(draftStartWorld_, draftCurrentWorld_);
            break;
        case ToolMode::DrawStair:
            createVerticalLink(draftStartWorld_, draftCurrentWorld_);
            break;
        case ToolMode::DrawDoor:
            break;
        case ToolMode::Select:
        case ToolMode::Delete:
            break;
        }

        update();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void LayoutPreviewWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(250, 252, 255));

    const auto bounds = collectBounds(importResult_, currentFloorId());
    if (!bounds.has_value()) {
        painter.setPen(QPen(QColor(80, 80, 80), 1));
        painter.setFont(QFont("Segoe UI", 14, QFont::DemiBold));
        painter.drawText(rect(), Qt::AlignCenter, "No layout geometry imported");
        return;
    }

    const QRectF viewport = previewViewport(rect());
    const LayoutTransform transform(*bounds, viewport, camera_.zoom(), camera_.panOffset());

    drawLayoutCanvasGrid(painter, viewport);

    if (importResult_.layout.has_value()) {
        drawFacilityLayoutCanvas(painter, *importResult_.layout, transform, currentFloorId().toStdString());
    } else if (importResult_.canonicalGeometry.has_value()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(231, 238, 246));
        for (const auto& walkable : importResult_.canonicalGeometry->walkableAreas) {
            painter.drawPath(polygonPath(walkable.polygon, transform));
        }
    }

    if (importResult_.canonicalGeometry.has_value()) {
        painter.setBrush(QColor(222, 145, 70, 120));
        painter.setPen(QPen(QColor(177, 110, 39), 1.5));
        for (const auto& obstacle : importResult_.canonicalGeometry->obstacles) {
            painter.drawPath(polygonPath(obstacle.footprint, transform));
        }

        painter.setPen(QPen(QColor(82, 92, 105), 2.5));
        for (const auto& wall : importResult_.canonicalGeometry->walls) {
            drawLine(painter, wall.segment, transform);
        }

        painter.setPen(QPen(QColor(66, 156, 96), 2.5, Qt::DashLine));
        for (const auto& opening : importResult_.canonicalGeometry->openings) {
            drawLine(painter, opening.span, transform);
        }
    }

    QString highlightTargetId = focusedTargetId_;
    if (!selectedConnectionId_.isEmpty()) {
        highlightTargetId = selectedConnectionId_;
    } else if (!selectedBarrierId_.isEmpty()) {
        highlightTargetId = selectedBarrierId_;
    } else if (!selectedZoneId_.isEmpty()) {
        highlightTargetId = selectedZoneId_;
    }

    if (!highlightTargetId.isEmpty()) {
        painter.setBrush(QColor(255, 219, 102, 96));
        painter.setPen(QPen(QColor(194, 74, 44), 3.5));

        if (importResult_.layout.has_value()) {
            for (const auto& zone : importResult_.layout->zones) {
                if (!matchesFloor(zone.floorId, currentFloorId())) {
                    continue;
                }
                if (QString::fromStdString(zone.id) == highlightTargetId || traceMatches(zone.provenance, highlightTargetId)) {
                    painter.drawPath(polygonPath(zone.area, transform));
                }
            }
            for (const auto& connection : importResult_.layout->connections) {
                if (!matchesFloor(connection.floorId, currentFloorId())) {
                    continue;
                }
                if (QString::fromStdString(connection.id) == highlightTargetId || traceMatches(connection.provenance, highlightTargetId)) {
                    drawLine(painter, connection.centerSpan, transform);
                }
            }
            for (const auto& barrier : importResult_.layout->barriers) {
                if (!matchesFloor(barrier.floorId, currentFloorId())) {
                    continue;
                }
                if (QString::fromStdString(barrier.id) == highlightTargetId || traceMatches(barrier.provenance, highlightTargetId)) {
                    drawPolyline(painter, barrier.geometry, transform);
                }
            }
        }

        if (importResult_.canonicalGeometry.has_value()) {
            for (const auto& walkable : importResult_.canonicalGeometry->walkableAreas) {
                const auto id = QString::fromStdString(walkable.id);
                if (traceRefMatches(importResult_, id, highlightTargetId)) {
                    painter.drawPath(polygonPath(walkable.polygon, transform));
                }
            }
            for (const auto& obstacle : importResult_.canonicalGeometry->obstacles) {
                const auto id = QString::fromStdString(obstacle.id);
                if (traceRefMatches(importResult_, id, highlightTargetId)) {
                    painter.drawPath(polygonPath(obstacle.footprint, transform));
                }
            }
            for (const auto& wall : importResult_.canonicalGeometry->walls) {
                const auto id = QString::fromStdString(wall.id);
                if (traceRefMatches(importResult_, id, highlightTargetId)) {
                    drawLine(painter, wall.segment, transform);
                }
            }
            for (const auto& opening : importResult_.canonicalGeometry->openings) {
                const auto id = QString::fromStdString(opening.id);
                if (traceRefMatches(importResult_, id, highlightTargetId)) {
                    drawLine(painter, opening.span, transform);
                }
            }
        }
    }

    if (drafting_) {
        painter.setBrush(QColor(31, 95, 174, 60));
        painter.setPen(QPen(QColor(31, 95, 174), 2.0, Qt::DashLine));

        const safecrowd::domain::Point2D start{draftStartWorld_.x(), draftStartWorld_.y()};
        const safecrowd::domain::Point2D current{draftCurrentWorld_.x(), draftCurrentWorld_.y()};
        if (toolMode_ == ToolMode::DrawWall || toolMode_ == ToolMode::DrawDoor) {
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(transform.map(start), transform.map(current));
        } else {
            const safecrowd::domain::Polygon2D polygon{
                .outline = {
                    {.x = std::min(start.x, current.x), .y = std::min(start.y, current.y)},
                    {.x = std::max(start.x, current.x), .y = std::min(start.y, current.y)},
                    {.x = std::max(start.x, current.x), .y = std::max(start.y, current.y)},
                    {.x = std::min(start.x, current.x), .y = std::max(start.y, current.y)},
                },
            };
            painter.drawPath(polygonPath(polygon, transform));
        }
    }

    painter.setPen(QPen(QColor(115, 128, 140), 1));
    painter.setFont(QFont("Segoe UI", 9, QFont::Medium));
    painter.drawText(viewport.adjusted(0, -10, 0, 0), Qt::AlignTop | Qt::AlignRight, QString("Zoom %1%").arg(static_cast<int>(camera_.zoom() * 100.0)));
    painter.drawText(viewport.adjusted(0, -10, 0, 0), Qt::AlignTop | Qt::AlignLeft, "Layout Preview");

}

void LayoutPreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    repositionToolbars();
}

void LayoutPreviewWidget::wheelEvent(QWheelEvent* event) {
    const auto bounds = collectBounds(importResult_, currentFloorId());
    if (!bounds.has_value()) {
        QWidget::wheelEvent(event);
        return;
    }

    if (camera_.zoomAt(event, *bounds, previewViewport(rect()))) {
        update();
        return;
    }

    QWidget::wheelEvent(event);
}

void LayoutPreviewWidget::applyToolAt(const QPointF& position) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    const auto bounds = collectBounds(importResult_, currentFloorId());
    if (!bounds.has_value()) {
        return;
    }

    const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
    const auto floorId = currentFloorId();
    QPointF testPosition = position;
    if (toolMode_ == ToolMode::DrawDoor) {
        const auto world = transform.unmap(position);
        const auto snappedWorld = snapWorldPoint(QPointF(world.x, world.y), transform);
        testPosition = transform.map({.x = snappedWorld.x(), .y = snappedWorld.y()});
    }
    const auto zoneId = hitTestZone(*importResult_.layout, testPosition, transform, floorId);
    const auto connectionId = hitTestConnection(*importResult_.layout, testPosition, transform, floorId);
    const auto barrierId = hitTestBarrier(*importResult_.layout, testPosition, transform, floorId);

    switch (toolMode_) {
    case ToolMode::Select:
        if (zoneId.has_value()) {
            selectZone(*zoneId);
        } else if (connectionId.has_value()) {
            selectConnection(*connectionId);
        } else if (barrierId.has_value()) {
            selectBarrier(*barrierId);
        } else {
            clearSelection();
        }
        return;
    case ToolMode::Delete:
        if (connectionId.has_value()) {
            deleteConnection(*connectionId);
        } else if (barrierId.has_value()) {
            deleteBarrier(*barrierId);
        }
        return;
    case ToolMode::DrawRoom:
    case ToolMode::DrawExit:
    case ToolMode::DrawWall:
    case ToolMode::DrawStair:
        return;
    case ToolMode::DrawDoor: {
        if (!barrierId.has_value()) {
            return;
        }
        const auto world = transform.unmap(testPosition);
        createDoorAt(*barrierId, QPointF(world.x, world.y));
        return;
    }
    }
}

void LayoutPreviewWidget::clearSelection() {
    selectedZoneId_.clear();
    selectedConnectionId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_.clear();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createZone(const QPointF& startWorld, const QPointF& endWorld, safecrowd::domain::ZoneKind kind) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    auto& layout = *importResult_.layout;
    const auto rectangle = rectFromWorldPoints(startWorld, endWorld);
    if (rectangle.width() < kDraftMinimumSize || rectangle.height() < kDraftMinimumSize) {
        return;
    }

    QString zoneLabel = "Room";
    if (kind == safecrowd::domain::ZoneKind::Exit) {
        zoneLabel = "Exit";
    }

    std::vector<safecrowd::domain::Polygon2D> polygonsToCreate;
    const safecrowd::domain::Polygon2D rectanglePolygon{
        .outline = {
            {.x = rectangle.left(), .y = rectangle.top()},
            {.x = rectangle.right(), .y = rectangle.top()},
            {.x = rectangle.right(), .y = rectangle.bottom()},
            {.x = rectangle.left(), .y = rectangle.bottom()},
        },
    };

    if (kind == safecrowd::domain::ZoneKind::Room) {
        QPainterPath candidatePath = worldPolygonPath(rectanglePolygon);
        QPainterPath occupiedRooms;
        for (const auto& zone : layout.zones) {
            if (zone.kind != safecrowd::domain::ZoneKind::Room) {
                continue;
            }
            occupiedRooms = occupiedRooms.united(worldPolygonPath(zone.area));
        }
        polygonsToCreate = polygonsFromFillPath(candidatePath.subtracted(occupiedRooms).simplified());
    } else {
        polygonsToCreate.push_back(rectanglePolygon);
    }

    if (polygonsToCreate.empty()) {
        return;
    }

    QString lastZoneId;
    for (const auto& polygon : polygonsToCreate) {
        const auto zoneId = nextZoneId(layout);
        const auto zoneNumber = static_cast<int>(layout.zones.size()) + 1;
        const auto floorId = currentFloorId().toStdString();
        layout.zones.push_back({
            .id = zoneId.toStdString(),
            .floorId = floorId,
            .kind = kind,
            .label = QString("%1 %2").arg(zoneLabel).arg(zoneNumber).toStdString(),
            .area = polygon,
            .defaultCapacity = kind == safecrowd::domain::ZoneKind::Exit ? 20u : 0u,
        });

        if (kind == safecrowd::domain::ZoneKind::Room && roomAutoWallsEnabled_) {
            appendAutoWallsForPolygon(layout, polygon, floorId);
        }

        lastZoneId = zoneId;
    }

    selectedZoneId_ = lastZoneId;
    selectedConnectionId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_ = lastZoneId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createBarrier(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    const auto length = std::hypot(endWorld.x() - startWorld.x(), endWorld.y() - startWorld.y());
    if (length < kDraftMinimumSize) {
        return;
    }

    auto& layout = *importResult_.layout;
    const auto barrierId = nextBarrierId(layout);
    layout.barriers.push_back({
        .id = barrierId.toStdString(),
        .floorId = currentFloorId().toStdString(),
        .geometry = safecrowd::domain::Polyline2D{
            .vertices = {
                {.x = startWorld.x(), .y = startWorld.y()},
                {.x = endWorld.x(), .y = endWorld.y()},
            },
            .closed = false,
        },
        .blocksMovement = true,
    });

    selectedBarrierId_ = barrierId;
    selectedZoneId_.clear();
    selectedConnectionId_.clear();
    focusedTargetId_ = barrierId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createConnection(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    const auto length = std::hypot(endWorld.x() - startWorld.x(), endWorld.y() - startWorld.y());
    if (length < kDraftMinimumSize) {
        return;
    }

    auto& layout = *importResult_.layout;
    const auto floorId = currentFloorId();
    const auto startCandidates = zonesNearPoint(layout, startWorld, floorId);
    const auto endCandidates = zonesNearPoint(layout, endWorld, floorId);

    std::vector<std::size_t> candidates = startCandidates;
    for (const auto index : endCandidates) {
        if (std::find(candidates.begin(), candidates.end(), index) == candidates.end()) {
            candidates.push_back(index);
        }
    }

    if (candidates.size() < 2) {
        return;
    }

    const auto fromZoneId = QString::fromStdString(layout.zones[candidates[0]].id);
    const auto toZoneId = QString::fromStdString(layout.zones[candidates[1]].id);
    if (fromZoneId == toZoneId || hasConnectionPair(layout, fromZoneId, toZoneId)) {
        return;
    }

    const auto connectionId = nextConnectionId(layout);
    layout.connections.push_back({
        .id = connectionId.toStdString(),
        .floorId = currentFloorId().toStdString(),
        .kind = (layout.zones[candidates[0]].kind == safecrowd::domain::ZoneKind::Exit
                || layout.zones[candidates[1]].kind == safecrowd::domain::ZoneKind::Exit)
            ? safecrowd::domain::ConnectionKind::Exit
            : safecrowd::domain::ConnectionKind::Opening,
        .fromZoneId = fromZoneId.toStdString(),
        .toZoneId = toZoneId.toStdString(),
        .effectiveWidth = kConnectionWidth,
        .directionality = safecrowd::domain::TravelDirection::Bidirectional,
        .centerSpan = {
            .start = {.x = startWorld.x(), .y = startWorld.y()},
            .end = {.x = endWorld.x(), .y = endWorld.y()},
        },
    });

    selectedConnectionId_ = connectionId;
    selectedZoneId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_ = connectionId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createVerticalLink(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    auto& layout = *importResult_.layout;
    const auto sourceFloorId = currentFloorId();
    const auto targetFloorId = verticalTargetFloorId();
    if (sourceFloorId.isEmpty() || targetFloorId.isEmpty() || sourceFloorId == targetFloorId) {
        return;
    }

    const auto rectangle = rectFromWorldPoints(startWorld, endWorld);
    if (rectangle.width() < kDraftMinimumSize || rectangle.height() < kDraftMinimumSize) {
        return;
    }

    const safecrowd::domain::Polygon2D footprint{
        .outline = {
            {.x = rectangle.left(), .y = rectangle.top()},
            {.x = rectangle.right(), .y = rectangle.top()},
            {.x = rectangle.right(), .y = rectangle.bottom()},
            {.x = rectangle.left(), .y = rectangle.bottom()},
        },
    };
    const QPointF center = rectangle.center();
    auto floorElevation = [&](const QString& floorId) {
        const auto it = std::find_if(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
            return QString::fromStdString(floor.id) == floorId;
        });
        return it == layout.floors.end() ? 0.0 : it->elevationMeters;
    };
    const bool sourceIsLower = floorElevation(sourceFloorId) <= floorElevation(targetFloorId);
    const auto sourceEntryDirection = sourceIsLower ? lowerStairEntryDirection_ : upperStairEntryDirection_;
    const auto targetEntryDirection = sourceIsLower ? upperStairEntryDirection_ : lowerStairEntryDirection_;
    const auto sourceEntrySpan = entrySpanForRectangle(rectangle, sourceEntryDirection);
    const auto targetEntrySpan = entrySpanForRectangle(rectangle, targetEntryDirection);
    const auto sourceOutsideSample = entryOutsideSample(rectangle, sourceEntryDirection);
    const auto targetOutsideSample = entryOutsideSample(rectangle, targetEntryDirection);
    const auto sourceZoneCandidates = zonesContainingPoint(layout, sourceOutsideSample, sourceFloorId, 0.35);
    const auto targetZoneCandidates = zonesContainingPoint(layout, targetOutsideSample, targetFloorId, 0.35);
    const auto sourceZone = choosePrimaryZone(layout, sourceZoneCandidates);
    const auto targetZone = choosePrimaryZone(layout, targetZoneCandidates);

    const auto sourceStairZoneId = nextZoneId(layout);
    const auto sourceZoneNumber = static_cast<int>(layout.zones.size()) + 1;
    const auto linkKind = verticalLinkCreatesRamp_
        ? safecrowd::domain::ConnectionKind::Ramp
        : safecrowd::domain::ConnectionKind::Stair;
    const auto zoneLabel = verticalLinkCreatesRamp_ ? QString("Ramp") : QString("Stair");
    const auto span = centerSpanForRectangle(rectangle);
    const auto effectiveWidth = std::max(0.9, std::min(rectangle.width(), rectangle.height()));

    layout.zones.push_back({
        .id = sourceStairZoneId.toStdString(),
        .floorId = sourceFloorId.toStdString(),
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = QString("%1 %2").arg(zoneLabel).arg(sourceZoneNumber).toStdString(),
        .area = footprint,
        .defaultCapacity = 8,
        .isStair = !verticalLinkCreatesRamp_,
        .isRamp = verticalLinkCreatesRamp_,
    });
    appendStairWallsExceptEntry(layout, rectangle, sourceEntryDirection, sourceFloorId.toStdString());

    const auto targetStairZoneId = nextZoneId(layout);
    const auto targetZoneNumber = static_cast<int>(layout.zones.size()) + 1;
    layout.zones.push_back({
        .id = targetStairZoneId.toStdString(),
        .floorId = targetFloorId.toStdString(),
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = QString("%1 %2").arg(zoneLabel).arg(targetZoneNumber).toStdString(),
        .area = footprint,
        .defaultCapacity = 8,
        .isStair = !verticalLinkCreatesRamp_,
        .isRamp = verticalLinkCreatesRamp_,
    });
    appendStairWallsExceptEntry(layout, rectangle, targetEntryDirection, targetFloorId.toStdString());

    if (sourceZone.has_value()) {
        layout.connections.push_back({
            .id = nextConnectionId(layout).toStdString(),
            .floorId = sourceFloorId.toStdString(),
            .kind = safecrowd::domain::ConnectionKind::Opening,
            .fromZoneId = layout.zones[*sourceZone].id,
            .toZoneId = sourceStairZoneId.toStdString(),
            .effectiveWidth = effectiveWidth,
            .directionality = safecrowd::domain::TravelDirection::Bidirectional,
            .centerSpan = sourceEntrySpan,
        });
    }

    const auto verticalConnectionId = nextVerticalConnectionId(layout);
    layout.connections.push_back({
        .id = verticalConnectionId.toStdString(),
        .floorId = sourceFloorId.toStdString(),
        .kind = linkKind,
        .fromZoneId = sourceStairZoneId.toStdString(),
        .toZoneId = targetStairZoneId.toStdString(),
        .effectiveWidth = effectiveWidth,
        .directionality = safecrowd::domain::TravelDirection::Bidirectional,
        .isStair = !verticalLinkCreatesRamp_,
        .isRamp = verticalLinkCreatesRamp_,
        .lowerEntryDirection = lowerStairEntryDirection_,
        .upperEntryDirection = upperStairEntryDirection_,
        .centerSpan = span,
    });

    if (targetZone.has_value()) {
        layout.connections.push_back({
            .id = nextConnectionId(layout).toStdString(),
            .floorId = targetFloorId.toStdString(),
            .kind = safecrowd::domain::ConnectionKind::Opening,
            .fromZoneId = targetStairZoneId.toStdString(),
            .toZoneId = layout.zones[*targetZone].id,
            .effectiveWidth = effectiveWidth,
            .directionality = safecrowd::domain::TravelDirection::Bidirectional,
            .centerSpan = targetEntrySpan,
        });
    }

    selectedConnectionId_ = verticalConnectionId;
    selectedZoneId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_ = verticalConnectionId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createDoorAt(const QString& barrierId, const QPointF& position) {
    if (!importResult_.layout.has_value() || barrierId.isEmpty()) {
        return;
    }

    auto& layout = *importResult_.layout;
    const auto barrierIt = std::find_if(layout.barriers.begin(), layout.barriers.end(), [&](const auto& barrier) {
        return QString::fromStdString(barrier.id) == barrierId && barrier.geometry.vertices.size() == 2;
    });
    if (barrierIt == layout.barriers.end()) {
        return;
    }

    const auto barrierIndex = static_cast<std::size_t>(std::distance(layout.barriers.begin(), barrierIt));
    const auto& barrierStart = barrierIt->geometry.vertices[0];
    const auto& barrierEnd = barrierIt->geometry.vertices[1];
    const auto projected = projectOntoSegment(position, barrierStart, barrierEnd);
    if (!projected.has_value()) {
        return;
    }

    const auto segmentLength = std::hypot(barrierEnd.x - barrierStart.x, barrierEnd.y - barrierStart.y);
    const auto openingWidth = std::clamp(doorWidth_, kMinimumDoorWidth, segmentLength - kGeometryEpsilon);
    if (segmentLength <= openingWidth + kGeometryEpsilon) {
        return;
    }

    const bool vertical = nearlyEqual(barrierStart.x, barrierEnd.x);
    const bool horizontal = nearlyEqual(barrierStart.y, barrierEnd.y);
    if (!vertical && !horizontal) {
        return;
    }

    safecrowd::domain::Point2D gapStart{};
    safecrowd::domain::Point2D gapEnd{};
    if (vertical) {
        const auto minY = std::min(barrierStart.y, barrierEnd.y);
        const auto maxY = std::max(barrierStart.y, barrierEnd.y);
        const auto centerY = std::clamp(projected->y(), minY + openingWidth * 0.5, maxY - openingWidth * 0.5);
        gapStart = {.x = barrierStart.x, .y = centerY - openingWidth * 0.5};
        gapEnd = {.x = barrierStart.x, .y = centerY + openingWidth * 0.5};
    } else {
        const auto minX = std::min(barrierStart.x, barrierEnd.x);
        const auto maxX = std::max(barrierStart.x, barrierEnd.x);
        const auto centerX = std::clamp(projected->x(), minX + openingWidth * 0.5, maxX - openingWidth * 0.5);
        gapStart = {.x = centerX - openingWidth * 0.5, .y = barrierStart.y};
        gapEnd = {.x = centerX + openingWidth * 0.5, .y = barrierStart.y};
    }

    const auto neighbors = doorNeighborsAcrossSegment(layout, gapStart, gapEnd, currentFloorId());
    const auto firstZone = choosePrimaryZone(layout, neighbors.firstSide);
    const auto secondZone = choosePrimaryZone(layout, neighbors.secondSide, firstZone);

    QString fromZoneId;
    QString toZoneId;
    safecrowd::domain::ConnectionKind connectionKind =
        doorCreatesLeaf_ ? safecrowd::domain::ConnectionKind::Doorway : safecrowd::domain::ConnectionKind::Opening;

    if (firstZone.has_value() && secondZone.has_value()) {
        fromZoneId = QString::fromStdString(layout.zones[*firstZone].id);
        toZoneId = QString::fromStdString(layout.zones[*secondZone].id);
        if (layout.zones[*firstZone].kind == safecrowd::domain::ZoneKind::Exit
            || layout.zones[*secondZone].kind == safecrowd::domain::ZoneKind::Exit) {
            connectionKind = safecrowd::domain::ConnectionKind::Exit;
        }
    } else {
        const auto interiorZone = firstZone.has_value() ? firstZone : secondZone;
        if (!interiorZone.has_value()) {
            return;
        }

        const bool useFirstOutside = !firstZone.has_value();
        const QPointF outsideSample = useFirstOutside ? neighbors.firstSample : neighbors.secondSample;
        const QPointF insideSample = useFirstOutside ? neighbors.secondSample : neighbors.firstSample;
        QPointF outsideDirection = outsideSample - insideSample;
        const auto directionLength = std::hypot(outsideDirection.x(), outsideDirection.y());
        if (directionLength <= kGeometryEpsilon) {
            return;
        }
        outsideDirection /= directionLength;

        fromZoneId = QString::fromStdString(layout.zones[*interiorZone].id);
        toZoneId = createExitZoneAtDoor(layout, gapStart, gapEnd, outsideDirection, currentFloorId().toStdString());
        connectionKind = safecrowd::domain::ConnectionKind::Exit;
    }

    if (fromZoneId.isEmpty()
        || toZoneId.isEmpty()
        || fromZoneId == toZoneId
        || hasConnectionPairAtSpan(layout, fromZoneId, toZoneId, gapStart, gapEnd)) {
        return;
    }

    std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> remainingSegments;
    if (vertical) {
        const auto lowerStart = std::min(barrierStart.y, barrierEnd.y);
        const auto lowerEnd = std::max(barrierStart.y, barrierEnd.y);
        if (gapStart.y - lowerStart > kGeometryEpsilon) {
            remainingSegments.push_back({
                {.x = barrierStart.x, .y = lowerStart},
                {.x = barrierStart.x, .y = gapStart.y},
            });
        }
        if (lowerEnd - gapEnd.y > kGeometryEpsilon) {
            remainingSegments.push_back({
                {.x = barrierStart.x, .y = gapEnd.y},
                {.x = barrierStart.x, .y = lowerEnd},
            });
        }
    } else {
        const auto lowerStart = std::min(barrierStart.x, barrierEnd.x);
        const auto lowerEnd = std::max(barrierStart.x, barrierEnd.x);
        if (gapStart.x - lowerStart > kGeometryEpsilon) {
            remainingSegments.push_back({
                {.x = lowerStart, .y = barrierStart.y},
                {.x = gapStart.x, .y = barrierStart.y},
            });
        }
        if (lowerEnd - gapEnd.x > kGeometryEpsilon) {
            remainingSegments.push_back({
                {.x = gapEnd.x, .y = barrierStart.y},
                {.x = lowerEnd, .y = barrierStart.y},
            });
        }
    }

    replaceBarrierWithSegments(layout, barrierIndex, remainingSegments, currentFloorId().toStdString());

    const auto connectionId = nextConnectionId(layout);
    layout.connections.push_back({
        .id = connectionId.toStdString(),
        .floorId = currentFloorId().toStdString(),
        .kind = connectionKind,
        .fromZoneId = fromZoneId.toStdString(),
        .toZoneId = toZoneId.toStdString(),
        .effectiveWidth = openingWidth,
        .directionality = safecrowd::domain::TravelDirection::Bidirectional,
        .centerSpan = {.start = gapStart, .end = gapEnd},
    });

    selectedConnectionId_ = connectionId;
    selectedZoneId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_ = connectionId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::deleteConnection(const QString& connectionId) {
    if (!importResult_.layout.has_value() || connectionId.isEmpty()) {
        return;
    }

    auto& connections = importResult_.layout->connections;
    const auto it = std::remove_if(connections.begin(), connections.end(), [&](const auto& connection) {
        return QString::fromStdString(connection.id) == connectionId;
    });
    if (it == connections.end()) {
        return;
    }

    connections.erase(it, connections.end());
    selectedConnectionId_.clear();
    focusedTargetId_.clear();
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::deleteBarrier(const QString& barrierId) {
    if (!importResult_.layout.has_value() || barrierId.isEmpty()) {
        return;
    }

    auto& barriers = importResult_.layout->barriers;
    const auto it = std::remove_if(barriers.begin(), barriers.end(), [&](const auto& barrier) {
        return QString::fromStdString(barrier.id) == barrierId;
    });
    if (it == barriers.end()) {
        return;
    }

    barriers.erase(it, barriers.end());
    selectedBarrierId_.clear();
    focusedTargetId_.clear();
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::emitCurrentSelection() {
    if (selectionChangedHandler_) {
        selectionChangedHandler_(currentSelection());
    }
}

QPointF LayoutPreviewWidget::snapWorldPoint(const QPointF& worldPoint, const LayoutCanvasTransform& transform) const {
    if (!importResult_.layout.has_value()) {
        return worldPoint;
    }

    const auto snapped = snapLayoutPoint(
        *importResult_.layout,
        currentFloorId().toStdString(),
        {.x = worldPoint.x(), .y = worldPoint.y()},
        transform);
    return QPointF(snapped.point.x, snapped.point.y);
}

void LayoutPreviewWidget::notifyLayoutEdited() {
    if (layoutEditedHandler_ && importResult_.layout.has_value()) {
        layoutEditedHandler_(*importResult_.layout);
    }
}

void LayoutPreviewWidget::repositionToolbars() {
    if (toolbarCorner_ != nullptr) {
        toolbarCorner_->setGeometry(0, 0, kSideToolbarWidth, kTopToolbarHeight + kPropertyPanelHeight);
        toolbarCorner_->raise();
    }
    if (topToolbar_ != nullptr) {
        topToolbar_->setGeometry(kSideToolbarWidth, 0, width() - kSideToolbarWidth, kTopToolbarHeight);
        topToolbar_->raise();
    }
    if (propertyPanel_ != nullptr) {
        propertyPanel_->setGeometry(kSideToolbarWidth, kTopToolbarHeight, width() - kSideToolbarWidth, kPropertyPanelHeight);
        propertyPanel_->raise();
    }
    if (sideToolbar_ != nullptr) {
        sideToolbar_->setGeometry(
            0,
            kTopToolbarHeight + kPropertyPanelHeight,
            kSideToolbarWidth,
            std::max(0, height() - kTopToolbarHeight - kPropertyPanelHeight));
        sideToolbar_->raise();
    }
}

void LayoutPreviewWidget::selectBarrier(const QString& barrierId) {
    selectedBarrierId_ = barrierId;
    selectedZoneId_.clear();
    selectedConnectionId_.clear();
    focusedTargetId_ = barrierId;
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::selectConnection(const QString& connectionId) {
    selectedConnectionId_ = connectionId;
    selectedZoneId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_ = connectionId;
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::selectFloorForElement(const QString& elementId) {
    if (!importResult_.layout.has_value() || elementId.isEmpty()) {
        return;
    }

    const auto& layout = *importResult_.layout;
    auto selectFloor = [&](const std::string& floorId) {
        if (floorId.empty()) {
            return;
        }
        const auto floorIdText = QString::fromStdString(floorId);
        if (floorIdText == currentFloorId_) {
            return;
        }
        currentFloorId_ = floorIdText;
        refreshFloorSelector();
    };

    for (const auto& zone : layout.zones) {
        if (QString::fromStdString(zone.id) == elementId) {
            selectFloor(zone.floorId);
            return;
        }
    }
    for (const auto& connection : layout.connections) {
        if (QString::fromStdString(connection.id) == elementId) {
            selectFloor(connection.floorId);
            return;
        }
    }
    for (const auto& barrier : layout.barriers) {
        if (QString::fromStdString(barrier.id) == elementId) {
            selectFloor(barrier.floorId);
            return;
        }
    }
}

void LayoutPreviewWidget::selectZone(const QString& zoneId) {
    selectedZoneId_ = zoneId;
    selectedConnectionId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_ = zoneId;
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::addFloor() {
    if (!importResult_.layout.has_value()) {
        return;
    }

    auto& layout = *importResult_.layout;
    ensureLayoutFloors(layout);
    const auto floorId = nextFloorId(layout);
    layout.floors.push_back({
        .id = floorId.toStdString(),
        .label = QString("Floor %1").arg(layout.floors.size() + 1).toStdString(),
    });
    currentFloorId_ = floorId;
    clearSelection();
    refreshFloorSelector();
    notifyLayoutEdited();
    update();
}

QString LayoutPreviewWidget::currentFloorId() const {
    if (!currentFloorId_.isEmpty()) {
        return currentFloorId_;
    }
    if (importResult_.layout.has_value()) {
        return defaultFloorId(*importResult_.layout);
    }
    return {};
}

QString LayoutPreviewWidget::verticalTargetFloorId() const {
    if (verticalTargetFloorComboBox_ == nullptr || verticalTargetFloorComboBox_->currentIndex() < 0) {
        return {};
    }
    return verticalTargetFloorComboBox_->currentData().toString();
}

void LayoutPreviewWidget::refreshFloorSelector() {
    if (floorComboBox_ == nullptr) {
        return;
    }

    const QSignalBlocker blocker(floorComboBox_);
    const bool targetSignalsWereBlocked = verticalTargetFloorComboBox_ != nullptr
        && verticalTargetFloorComboBox_->blockSignals(true);
    floorComboBox_->clear();
    if (verticalTargetFloorComboBox_ != nullptr) {
        verticalTargetFloorComboBox_->clear();
    }

    if (!importResult_.layout.has_value()) {
        floorComboBox_->setEnabled(false);
        if (verticalTargetFloorComboBox_ != nullptr) {
            verticalTargetFloorComboBox_->setEnabled(false);
            verticalTargetFloorComboBox_->blockSignals(targetSignalsWereBlocked);
        }
        if (addFloorButton_ != nullptr) {
            addFloorButton_->setEnabled(false);
        }
        return;
    }

    auto& layout = *importResult_.layout;
    ensureLayoutFloors(layout);
    if (currentFloorId_.isEmpty()) {
        currentFloorId_ = defaultFloorId(layout);
    }

    int selectedIndex = 0;
    for (std::size_t index = 0; index < layout.floors.size(); ++index) {
        const auto& floor = layout.floors[index];
        const auto floorId = QString::fromStdString(floor.id);
        floorComboBox_->addItem(floorDisplayLabel(floor), floorId);
        if (floorId == currentFloorId_) {
            selectedIndex = static_cast<int>(index);
        } else if (verticalTargetFloorComboBox_ != nullptr) {
            verticalTargetFloorComboBox_->addItem(floorDisplayLabel(floor), floorId);
        }
    }

    floorComboBox_->setCurrentIndex(selectedIndex);
    floorComboBox_->setEnabled(layout.floors.size() > 1);
    if (verticalTargetFloorComboBox_ != nullptr) {
        verticalTargetFloorComboBox_->setEnabled(verticalTargetFloorComboBox_->count() > 0);
        verticalTargetFloorComboBox_->blockSignals(targetSignalsWereBlocked);
    }
    if (addFloorButton_ != nullptr) {
        addFloorButton_->setEnabled(true);
    }
}

void LayoutPreviewWidget::setToolMode(ToolMode mode) {
    toolMode_ = mode;
    if (selectToolButton_ != nullptr) {
        selectToolButton_->setChecked(toolMode_ == ToolMode::Select);
    }
    if (roomToolButton_ != nullptr) {
        roomToolButton_->setChecked(toolMode_ == ToolMode::DrawRoom);
    }
    if (exitToolButton_ != nullptr) {
        exitToolButton_->setChecked(toolMode_ == ToolMode::DrawExit);
    }
    if (wallToolButton_ != nullptr) {
        wallToolButton_->setChecked(toolMode_ == ToolMode::DrawWall);
    }
    if (doorToolButton_ != nullptr) {
        doorToolButton_->setChecked(toolMode_ == ToolMode::DrawDoor);
    }
    if (stairToolButton_ != nullptr) {
        stairToolButton_->setChecked(toolMode_ == ToolMode::DrawStair);
    }
    if (deleteToolButton_ != nullptr) {
        deleteToolButton_->setChecked(toolMode_ == ToolMode::Delete);
    }

    refreshPropertyPanel();
    update();
}

void LayoutPreviewWidget::setupToolbars() {
    const QString frameStyle =
        "QFrame { background: rgba(255, 255, 255, 245); border: 1px solid #d7e0ea; border-radius: 0px; }"
        "QToolButton { background: transparent; border: 0; border-radius: 0px; }"
        "QToolButton:hover { background: #eef3f8; }"
        "QToolButton:checked { background: #dce9f9; }"
        "QLabel { color: #607086; border: 0; padding-left: 10px; }"
        "QComboBox { min-height: 28px; padding: 0 8px; border: 1px solid #c9d5e2; border-radius: 0px; background: #ffffff; color: #16202b; }";

    toolbarCorner_ = new QFrame(this);
    toolbarCorner_->setStyleSheet(frameStyle);

    topToolbar_ = new QFrame(this);
    topToolbar_->setStyleSheet(frameStyle);
    auto* topLayout = new QHBoxLayout(topToolbar_);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    sideToolbar_ = new QFrame(this);
    sideToolbar_->setStyleSheet(frameStyle);
    auto* sideLayout = new QVBoxLayout(sideToolbar_);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    propertyPanel_ = new QFrame(this);
    propertyPanel_->setStyleSheet(
        "QFrame { background: rgba(255, 255, 255, 245); border: 1px solid #d7e0ea; border-radius: 0px; }"
        "QDoubleSpinBox { min-height: 24px; padding: 0 8px; border: 1px solid #c9d5e2; border-radius: 0px; background: #ffffff; color: #16202b; }"
        "QComboBox { min-height: 24px; padding: 0 8px; border: 1px solid #c9d5e2; border-radius: 0px; background: #ffffff; color: #16202b; }"
        "QCheckBox { color: #16202b; spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }");
    auto* propertyLayout = new QHBoxLayout(propertyPanel_);
    propertyLayout->setContentsMargins(12, 0, 16, 0);
    propertyLayout->setSpacing(14);

    roomAutoWallsCheckBox_ = new QCheckBox("Auto-create surrounding walls", propertyPanel_);
    roomAutoWallsCheckBox_->setChecked(roomAutoWallsEnabled_);
    propertyLayout->addWidget(roomAutoWallsCheckBox_);

    doorWidthSpinBox_ = new QDoubleSpinBox(propertyPanel_);
    doorWidthSpinBox_->setDecimals(2);
    doorWidthSpinBox_->setSingleStep(0.1);
    doorWidthSpinBox_->setRange(kMinimumDoorWidth, 20.0);
    doorWidthSpinBox_->setValue(doorWidth_);
    doorWidthSpinBox_->setSuffix(" m");
    doorWidthSpinBox_->setToolTip("Door/opening width");
    propertyLayout->addWidget(doorWidthSpinBox_);

    doorLeafCheckBox_ = new QCheckBox("Install door leaf", propertyPanel_);
    doorLeafCheckBox_->setChecked(doorCreatesLeaf_);
    propertyLayout->addWidget(doorLeafCheckBox_);

    verticalTargetFloorComboBox_ = new QComboBox(propertyPanel_);
    verticalTargetFloorComboBox_->setMinimumWidth(140);
    verticalTargetFloorComboBox_->setToolTip("Target floor");
    propertyLayout->addWidget(verticalTargetFloorComboBox_);

    lowerStairEntryLabel_ = new QLabel("Lower entry", propertyPanel_);
    propertyLayout->addWidget(lowerStairEntryLabel_);
    lowerStairEntryComboBox_ = new QComboBox(propertyPanel_);
    lowerStairEntryComboBox_->setMinimumWidth(118);
    lowerStairEntryComboBox_->setToolTip("Lower-floor stair entry side");
    populateStairEntryCombo(*lowerStairEntryComboBox_, lowerStairEntryDirection_);
    propertyLayout->addWidget(lowerStairEntryComboBox_);

    upperStairEntryLabel_ = new QLabel("Upper entry", propertyPanel_);
    propertyLayout->addWidget(upperStairEntryLabel_);
    upperStairEntryComboBox_ = new QComboBox(propertyPanel_);
    upperStairEntryComboBox_->setMinimumWidth(118);
    upperStairEntryComboBox_->setToolTip("Upper-floor stair entry side");
    populateStairEntryCombo(*upperStairEntryComboBox_, upperStairEntryDirection_);
    propertyLayout->addWidget(upperStairEntryComboBox_);

    rampLinkCheckBox_ = new QCheckBox("Ramp", propertyPanel_);
    rampLinkCheckBox_->setChecked(verticalLinkCreatesRamp_);
    propertyLayout->addWidget(rampLinkCheckBox_);
    propertyLayout->addStretch(1);

    const auto makeButton = [&](QFrame* host, QBoxLayout* layout, const QIcon& icon, const QString& tooltip) {
        auto* button = new QToolButton(host);
        button->setCheckable(true);
        button->setIcon(icon);
        button->setIconSize(QSize(22, 22));
        button->setToolTip(tooltip);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(kToolbarButtonSize, kToolbarButtonSize);
        layout->addWidget(button);
        return button;
    };

    selectToolButton_ = makeButton(topToolbar_, topLayout, makeToolIcon("select", QColor("#16202b")), "Select");
    deleteToolButton_ = makeButton(topToolbar_, topLayout, makeToolIcon("delete", QColor("#8f2d20")), "Delete");
    resetViewButton_ = makeButton(topToolbar_, topLayout, makeToolIcon("reset", QColor("#1f5fae")), "Reset View");
    resetViewButton_->setCheckable(false);
    auto* floorLabel = new QLabel("Floor", topToolbar_);
    floorLabel->setFixedHeight(kTopToolbarHeight);
    topLayout->addWidget(floorLabel);
    floorComboBox_ = new QComboBox(topToolbar_);
    floorComboBox_->setMinimumWidth(140);
    floorComboBox_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    floorComboBox_->setToolTip("Active floor");
    topLayout->addWidget(floorComboBox_);
    addFloorButton_ = makeButton(topToolbar_, topLayout, makeToolIcon("add", QColor("#1f5fae")), "Add Floor");
    addFloorButton_->setCheckable(false);
    topLayout->addStretch(1);

    roomToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("room", QColor("#2f5d8a")), "Draw Room");
    exitToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("exit", QColor("#2d8f5b")), "Draw Exit");
    wallToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("wall", QColor("#6c4f38")), "Draw Wall");
    doorToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("door", QColor("#8e6b23")), "Draw Door");
    stairToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("stair", QColor("#6a5d9f")), "Draw Stair/Ramp");
    sideLayout->addStretch(1);

    connect(selectToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::Select); });
    connect(deleteToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::Delete); });
    connect(resetViewButton_, &QToolButton::clicked, this, [this]() { resetView(); });
    connect(floorComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0 || floorComboBox_ == nullptr) {
            return;
        }
        const auto floorId = floorComboBox_->itemData(index).toString();
        if (floorId.isEmpty() || floorId == currentFloorId_) {
            return;
        }
        currentFloorId_ = floorId;
        clearSelection();
        refreshFloorSelector();
        camera_.reset();
        update();
    });
    connect(addFloorButton_, &QToolButton::clicked, this, [this]() { addFloor(); });
    connect(roomToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawRoom); });
    connect(exitToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawExit); });
    connect(wallToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawWall); });
    connect(doorToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawDoor); });
    connect(stairToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawStair); });
    connect(roomAutoWallsCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        roomAutoWallsEnabled_ = checked;
    });
    connect(doorWidthSpinBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        doorWidth_ = value;
    });
    connect(doorLeafCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        doorCreatesLeaf_ = checked;
    });
    connect(rampLinkCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        verticalLinkCreatesRamp_ = checked;
    });
    connect(lowerStairEntryComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0 && lowerStairEntryComboBox_ != nullptr) {
            lowerStairEntryDirection_ = static_cast<safecrowd::domain::StairEntryDirection>(
                lowerStairEntryComboBox_->itemData(index).toInt());
        }
    });
    connect(upperStairEntryComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0 && upperStairEntryComboBox_ != nullptr) {
            upperStairEntryDirection_ = static_cast<safecrowd::domain::StairEntryDirection>(
                upperStairEntryComboBox_->itemData(index).toInt());
        }
    });

    const auto visible = importResult_.layout.has_value();
    toolbarCorner_->setVisible(visible);
    topToolbar_->setVisible(visible);
    propertyPanel_->setVisible(visible);
    sideToolbar_->setVisible(visible);
    refreshFloorSelector();
    setToolMode(ToolMode::Select);
}

void LayoutPreviewWidget::refreshPropertyPanel() {
    if (propertyPanel_ == nullptr
        || roomAutoWallsCheckBox_ == nullptr
        || doorWidthSpinBox_ == nullptr
        || doorLeafCheckBox_ == nullptr
        || verticalTargetFloorComboBox_ == nullptr
        || lowerStairEntryLabel_ == nullptr
        || lowerStairEntryComboBox_ == nullptr
        || upperStairEntryLabel_ == nullptr
        || upperStairEntryComboBox_ == nullptr
        || rampLinkCheckBox_ == nullptr) {
        return;
    }

    const bool showRoomWallsOption = toolMode_ == ToolMode::DrawRoom;
    const bool showDoorOptions = toolMode_ == ToolMode::DrawDoor;
    const bool showVerticalOptions = toolMode_ == ToolMode::DrawStair;
    roomAutoWallsCheckBox_->setVisible(showRoomWallsOption);
    doorWidthSpinBox_->setVisible(showDoorOptions);
    doorLeafCheckBox_->setVisible(showDoorOptions);
    verticalTargetFloorComboBox_->setVisible(showVerticalOptions);
    lowerStairEntryLabel_->setVisible(showVerticalOptions);
    lowerStairEntryComboBox_->setVisible(showVerticalOptions);
    upperStairEntryLabel_->setVisible(showVerticalOptions);
    upperStairEntryComboBox_->setVisible(showVerticalOptions);
    rampLinkCheckBox_->setVisible(showVerticalOptions);
    propertyPanel_->setVisible(importResult_.layout.has_value() && (showRoomWallsOption || showDoorOptions || showVerticalOptions));
}

PreviewSelection LayoutPreviewWidget::currentSelection() const {
    PreviewSelection selection;

    if (importResult_.layout.has_value() && !selectedZoneId_.isEmpty()) {
        const auto& layout = *importResult_.layout;
        const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
            return QString::fromStdString(zone.id) == selectedZoneId_;
        });
        if (it != layout.zones.end()) {
            selection.kind = PreviewSelectionKind::Zone;
            selection.id = selectedZoneId_;
            selection.title = zoneTitle(*it);
            selection.detail = zoneDetail(layout, *it);
            return selection;
        }
    }

    if (importResult_.layout.has_value() && !selectedConnectionId_.isEmpty()) {
        const auto& layout = *importResult_.layout;
        const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
            return QString::fromStdString(connection.id) == selectedConnectionId_;
        });
        if (it != layout.connections.end()) {
            selection.kind = PreviewSelectionKind::Connection;
            selection.id = selectedConnectionId_;
            selection.title = connectionTitle(*it);
            selection.detail = connectionDetail(*it);
            return selection;
        }
    }

    if (importResult_.layout.has_value() && !selectedBarrierId_.isEmpty()) {
        const auto& layout = *importResult_.layout;
        const auto it = std::find_if(layout.barriers.begin(), layout.barriers.end(), [&](const auto& barrier) {
            return QString::fromStdString(barrier.id) == selectedBarrierId_;
        });
        if (it != layout.barriers.end()) {
            selection.kind = PreviewSelectionKind::Barrier;
            selection.id = selectedBarrierId_;
            selection.title = barrierTitle(*it);
            selection.detail = barrierDetail(*it);
            return selection;
        }
    }

    selection.title = "No selection";
    selection.detail = "Use the top and left toolbars to select, draw rooms, exits, walls, and doors.";
    return selection;
}

}  // namespace safecrowd::application
