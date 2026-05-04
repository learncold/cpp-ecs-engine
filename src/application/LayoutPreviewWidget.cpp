#include "application/LayoutPreviewWidget.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include "application/LayoutCanvasRendering.h"
#include "application/LayoutCanvasSnapping.h"
#include "application/ToolIconResources.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
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
constexpr double kPolygonCloseTolerancePixels = 12.0;
constexpr double kSelectionDragThresholdPixels = 4.0;
constexpr double kSelectionStrokeWidthPixels = 8.0;
constexpr double kSelectedContextHitTolerancePixels = 24.0;
constexpr double kMinimumDoorWidth = 0.9;
constexpr int kTopToolbarHeight = 44;
constexpr int kPropertyPanelHeight = 42;
constexpr int kSideToolbarWidth = 44;
constexpr int kToolbarButtonSize = 44;
const QColor kSelectionHighlightColor("#0b3d78");

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

Bounds2D blankCanvasBounds() {
    return {
        .minX = 0.0,
        .minY = 0.0,
        .maxX = 120.0,
        .maxY = 80.0,
    };
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
        return blankCanvasBounds();
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

QPainterPath linePath(const safecrowd::domain::LineSegment2D& line, const LayoutTransform& transform) {
    QPainterPath path;
    path.moveTo(transform.map(line.start));
    path.lineTo(transform.map(line.end));
    return path;
}

QPainterPath polylinePainterPath(const safecrowd::domain::Polyline2D& polyline, const LayoutTransform& transform) {
    QPainterPath path;
    if (polyline.vertices.empty()) {
        return path;
    }

    path.moveTo(transform.map(polyline.vertices.front()));
    for (std::size_t index = 1; index < polyline.vertices.size(); ++index) {
        path.lineTo(transform.map(polyline.vertices[index]));
    }
    if (polyline.closed && polyline.vertices.size() > 2) {
        path.closeSubpath();
    }
    return path;
}

bool strokedPathIntersectsRect(const QPainterPath& path, const QRectF& rect, double strokeWidth) {
    if (path.isEmpty() || rect.isEmpty()) {
        return false;
    }

    QPainterPathStroker stroker;
    stroker.setWidth(strokeWidth);
    return stroker.createStroke(path).intersects(rect) || rect.contains(path.boundingRect());
}

bool strokedPathContainsPoint(const QPainterPath& path, const QPointF& point, double radius) {
    if (path.isEmpty()) {
        return false;
    }
    if (path.contains(point)) {
        return true;
    }

    QPainterPathStroker stroker;
    stroker.setWidth(radius * 2.0);
    return stroker.createStroke(path).contains(point);
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

safecrowd::domain::LineSegment2D entrySpanForRectangle(
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection direction);
QPointF entryOutsideSample(const QRectF& rectangle, safecrowd::domain::StairEntryDirection direction);
std::optional<std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>>> barrierSegmentsAfterGap(
    const safecrowd::domain::Barrier2D& barrier,
    const safecrowd::domain::LineSegment2D& gap);
bool spanOverlapsPolygonBoundary(
    const safecrowd::domain::Polygon2D& polygon,
    const safecrowd::domain::LineSegment2D& span);
std::optional<QPointF> outsideSampleForBoundarySpan(
    const safecrowd::domain::Polygon2D& polygon,
    const safecrowd::domain::LineSegment2D& span);

bool isVerticalLink(const safecrowd::domain::Connection2D& connection) {
    return connection.kind == safecrowd::domain::ConnectionKind::Stair
        || connection.kind == safecrowd::domain::ConnectionKind::Ramp
        || connection.isStair
        || connection.isRamp;
}

bool isVerticalZone(const safecrowd::domain::Zone2D& zone) {
    return zone.kind == safecrowd::domain::ZoneKind::Stair || zone.isStair || zone.isRamp;
}

const safecrowd::domain::Zone2D* findZoneById(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

bool connectionVisibleOnFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const QString& floorId) {
    if (matchesFloor(connection.floorId, floorId)) {
        return true;
    }
    if (!isVerticalLink(connection)) {
        return false;
    }

    const auto* fromZone = findZoneById(layout, connection.fromZoneId);
    const auto* toZone = findZoneById(layout, connection.toZoneId);
    return (fromZone != nullptr && matchesFloor(fromZone->floorId, floorId))
        || (toZone != nullptr && matchesFloor(toZone->floorId, floorId));
}

std::optional<std::size_t> findZoneIndexById(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    if (it == layout.zones.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(layout.zones.begin(), it));
}

double floorElevation(const safecrowd::domain::FacilityLayout2D& layout, const std::string& floorId) {
    const auto it = std::find_if(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
        return floor.id == floorId;
    });
    return it == layout.floors.end() ? 0.0 : it->elevationMeters;
}

std::optional<safecrowd::domain::StairEntryDirection> stairEntryDirectionForFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId) {
    if (!isVerticalLink(connection)) {
        return std::nullopt;
    }

    const auto* fromZone = findZoneById(layout, connection.fromZoneId);
    const auto* toZone = findZoneById(layout, connection.toZoneId);
    if (fromZone == nullptr || toZone == nullptr || fromZone->floorId == toZone->floorId) {
        return std::nullopt;
    }

    const bool fromIsLower = floorElevation(layout, fromZone->floorId) <= floorElevation(layout, toZone->floorId);
    if (floorId == fromZone->floorId) {
        return fromIsLower ? connection.lowerEntryDirection : connection.upperEntryDirection;
    }
    if (floorId == toZone->floorId) {
        return fromIsLower ? connection.upperEntryDirection : connection.lowerEntryDirection;
    }
    return std::nullopt;
}

std::optional<safecrowd::domain::LineSegment2D> stairEntrySpanForFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId) {
    if (!isVerticalLink(connection)) {
        return std::nullopt;
    }

    const auto* fromZone = findZoneById(layout, connection.fromZoneId);
    const auto* toZone = findZoneById(layout, connection.toZoneId);
    const auto* stairZone = fromZone != nullptr && fromZone->floorId == floorId ? fromZone : toZone;
    if (stairZone == nullptr || !isVerticalZone(*stairZone)) {
        return std::nullopt;
    }

    const auto direction = stairEntryDirectionForFloor(layout, connection, floorId);
    const auto bounds = polygonBounds(stairZone->area);
    if (!direction.has_value() || !bounds.valid()) {
        return std::nullopt;
    }

    const auto directedEntrySpan = entrySpanForRectangle(
        QRectF(QPointF(bounds.minX, bounds.minY), QPointF(bounds.maxX, bounds.maxY)).normalized(),
        *direction);

    if (spanOverlapsPolygonBoundary(stairZone->area, connection.centerSpan)
        && (*direction == safecrowd::domain::StairEntryDirection::Unspecified
            || segmentsShareSpan(connection.centerSpan, directedEntrySpan.start, directedEntrySpan.end))) {
        return connection.centerSpan;
    }

    return directedEntrySpan;
}

std::optional<QPointF> stairEntryOutsideSampleForFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId) {
    if (!isVerticalLink(connection)) {
        return std::nullopt;
    }

    const auto* fromZone = findZoneById(layout, connection.fromZoneId);
    const auto* toZone = findZoneById(layout, connection.toZoneId);
    const auto* stairZone = fromZone != nullptr && fromZone->floorId == floorId ? fromZone : toZone;
    if (stairZone == nullptr || !isVerticalZone(*stairZone)) {
        return std::nullopt;
    }

    const auto direction = stairEntryDirectionForFloor(layout, connection, floorId);
    const auto bounds = polygonBounds(stairZone->area);
    if (!direction.has_value() || !bounds.valid()) {
        return std::nullopt;
    }

    const auto directedEntrySpan = entrySpanForRectangle(
        QRectF(QPointF(bounds.minX, bounds.minY), QPointF(bounds.maxX, bounds.maxY)).normalized(),
        *direction);

    if (spanOverlapsPolygonBoundary(stairZone->area, connection.centerSpan)
        && (*direction == safecrowd::domain::StairEntryDirection::Unspecified
            || segmentsShareSpan(connection.centerSpan, directedEntrySpan.start, directedEntrySpan.end))) {
        return outsideSampleForBoundarySpan(stairZone->area, connection.centerSpan);
    }

    return entryOutsideSample(
        QRectF(QPointF(bounds.minX, bounds.minY), QPointF(bounds.maxX, bounds.maxY)).normalized(),
        *direction);
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

QString nextBarrierId(const safecrowd::domain::FacilityLayout2D& layout, const QString& prefix) {
    int suffix = 1;
    for (const auto& barrier : layout.barriers) {
        const auto id = QString::fromStdString(barrier.id);
        if (!id.startsWith(prefix)) {
            continue;
        }

        bool ok = false;
        const auto value = id.mid(prefix.size()).toInt(&ok);
        if (ok) {
            suffix = std::max(suffix, value + 1);
        }
    }

    return QString("%1%2").arg(prefix).arg(suffix);
}

QString nextWallId(const safecrowd::domain::FacilityLayout2D& layout) {
    return nextBarrierId(layout, "wall-user-");
}

QString nextObstructionId(const safecrowd::domain::FacilityLayout2D& layout) {
    return nextBarrierId(layout, "obstruction-user-");
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

bool spanOverlapsPolygonBoundary(
    const safecrowd::domain::Polygon2D& polygon,
    const safecrowd::domain::LineSegment2D& span) {
    const auto spanLength = std::hypot(span.end.x - span.start.x, span.end.y - span.start.y);
    if (spanLength <= kGeometryEpsilon) {
        return false;
    }

    const auto checkRing = [&](const auto& ring) {
        if (ring.size() < 2) {
            return false;
        }
        for (std::size_t index = 0; index < ring.size(); ++index) {
            if (segmentsShareSpan(span, ring[index], ring[(index + 1) % ring.size()])) {
                return true;
            }
        }
        return false;
    };

    if (checkRing(polygon.outline)) {
        return true;
    }
    return std::any_of(polygon.holes.begin(), polygon.holes.end(), checkRing);
}

std::optional<QPointF> outsideSampleForBoundarySpan(
    const safecrowd::domain::Polygon2D& polygon,
    const safecrowd::domain::LineSegment2D& span) {
    const auto dx = span.end.x - span.start.x;
    const auto dy = span.end.y - span.start.y;
    const auto length = std::hypot(dx, dy);
    if (length <= kGeometryEpsilon) {
        return std::nullopt;
    }

    const QPointF center(
        (span.start.x + span.end.x) * 0.5,
        (span.start.y + span.end.y) * 0.5);
    constexpr double sampleOffset = 0.45;
    const QPointF normalA(-dy / length * sampleOffset, dx / length * sampleOffset);
    const QPointF normalB(dy / length * sampleOffset, -dx / length * sampleOffset);
    const QPointF sampleA = center + normalA;
    const QPointF sampleB = center + normalB;
    const bool sampleAInside = pointInPolygon(polygon, sampleA);
    const bool sampleBInside = pointInPolygon(polygon, sampleB);

    if (!sampleAInside && sampleBInside) {
        return sampleA;
    }
    if (!sampleBInside && sampleAInside) {
        return sampleB;
    }
    return std::nullopt;
}

double distanceToLineSegmentWorld(const QPointF& point, const safecrowd::domain::Point2D& start, const safecrowd::domain::Point2D& end) {
    return distanceToSegment(point, QPointF(start.x, start.y), QPointF(end.x, end.y));
}

double distanceBetweenScreenPoints(const QPointF& lhs, const QPointF& rhs) {
    return std::hypot(lhs.x() - rhs.x(), lhs.y() - rhs.y());
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
    return QString("%1 %2")
        .arg(barrier.geometry.closed ? QString("Obstruction") : QString("Wall"))
        .arg(QString::fromStdString(barrier.id));
}

QString barrierDetail(const safecrowd::domain::Barrier2D& barrier) {
    return QString("Kind: %1\nId: %2\nVertices: %3\nShape: %4")
        .arg(barrier.geometry.closed ? QString("Obstruction") : QString("Wall"))
        .arg(QString::fromStdString(barrier.id))
        .arg(static_cast<int>(barrier.geometry.vertices.size()))
        .arg(barrier.geometry.closed ? QString("Closed") : QString("Line"));
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
    const auto id = nextWallId(layout);
    layout.barriers.push_back({
        .id = id.toStdString(),
        .floorId = floorId,
        .geometry = safecrowd::domain::Polyline2D{
            .vertices = {start, end},
            .closed = false,
        },
        .blocksMovement = true,
    });
}

QString appendBarrierSegmentWithId(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end,
    const std::string& floorId) {
    const auto id = nextWallId(layout);
    layout.barriers.push_back({
        .id = id.toStdString(),
        .floorId = floorId,
        .geometry = safecrowd::domain::Polyline2D{
            .vertices = {start, end},
            .closed = false,
        },
        .blocksMovement = true,
    });
    return id;
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

void translatePoint(safecrowd::domain::Point2D& point, double dx, double dy) {
    point.x += dx;
    point.y += dy;
}

void translatePolygon(safecrowd::domain::Polygon2D& polygon, double dx, double dy) {
    for (auto& point : polygon.outline) {
        translatePoint(point, dx, dy);
    }
    for (auto& hole : polygon.holes) {
        for (auto& point : hole) {
            translatePoint(point, dx, dy);
        }
    }
}

void translatePolyline(safecrowd::domain::Polyline2D& polyline, double dx, double dy) {
    for (auto& point : polyline.vertices) {
        translatePoint(point, dx, dy);
    }
}

void translateLine(safecrowd::domain::LineSegment2D& line, double dx, double dy) {
    translatePoint(line.start, dx, dy);
    translatePoint(line.end, dx, dy);
}

bool segmentMatchesRingEdge(
    const safecrowd::domain::Point2D& segmentStart,
    const safecrowd::domain::Point2D& segmentEnd,
    const std::vector<safecrowd::domain::Point2D>& ring) {
    if (ring.size() < 2) {
        return false;
    }

    const bool segmentVertical = nearlyEqual(segmentStart.x, segmentEnd.x);
    const bool segmentHorizontal = nearlyEqual(segmentStart.y, segmentEnd.y);
    if (!segmentVertical && !segmentHorizontal) {
        return false;
    }

    for (std::size_t index = 0; index < ring.size(); ++index) {
        const auto& edgeStart = ring[index];
        const auto& edgeEnd = ring[(index + 1) % ring.size()];
        const bool edgeVertical = nearlyEqual(edgeStart.x, edgeEnd.x);
        const bool edgeHorizontal = nearlyEqual(edgeStart.y, edgeEnd.y);
        if (segmentVertical && edgeVertical && nearlyEqual(segmentStart.x, edgeStart.x)) {
            const auto segmentMin = std::min(segmentStart.y, segmentEnd.y);
            const auto segmentMax = std::max(segmentStart.y, segmentEnd.y);
            const auto edgeMin = std::min(edgeStart.y, edgeEnd.y);
            const auto edgeMax = std::max(edgeStart.y, edgeEnd.y);
            if (nearlyEqual(segmentMin, edgeMin) && nearlyEqual(segmentMax, edgeMax)) {
                return true;
            }
        } else if (segmentHorizontal && edgeHorizontal && nearlyEqual(segmentStart.y, edgeStart.y)) {
            const auto segmentMin = std::min(segmentStart.x, segmentEnd.x);
            const auto segmentMax = std::max(segmentStart.x, segmentEnd.x);
            const auto edgeMin = std::min(edgeStart.x, edgeEnd.x);
            const auto edgeMax = std::max(edgeStart.x, edgeEnd.x);
            if (nearlyEqual(segmentMin, edgeMin) && nearlyEqual(segmentMax, edgeMax)) {
                return true;
            }
        }
    }

    return false;
}

bool barrierMatchesZoneBoundary(
    const safecrowd::domain::Barrier2D& barrier,
    const safecrowd::domain::Zone2D& zone) {
    if (barrier.geometry.closed || barrier.geometry.vertices.size() != 2 || !matchesFloor(barrier.floorId, QString::fromStdString(zone.floorId))) {
        return false;
    }

    const auto& start = barrier.geometry.vertices[0];
    const auto& end = barrier.geometry.vertices[1];
    if (segmentMatchesRingEdge(start, end, zone.area.outline)) {
        return true;
    }
    return std::any_of(zone.area.holes.begin(), zone.area.holes.end(), [&](const auto& hole) {
        return segmentMatchesRingEdge(start, end, hole);
    });
}

struct WallInterval {
    safecrowd::domain::Barrier2D barrier{};
    double start{0.0};
    double end{0.0};
    bool vertical{false};
    double fixedCoordinate{0.0};
};

std::vector<WallInterval> normalizeWallIntervals(
    std::vector<WallInterval> intervals,
    const QStringList& preferredIds) {
    std::sort(intervals.begin(), intervals.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.barrier.floorId != rhs.barrier.floorId) {
            return lhs.barrier.floorId < rhs.barrier.floorId;
        }
        if (!nearlyEqual(lhs.fixedCoordinate, rhs.fixedCoordinate)) {
            return lhs.fixedCoordinate < rhs.fixedCoordinate;
        }
        if (lhs.vertical != rhs.vertical) {
            return lhs.vertical;
        }
        if (!nearlyEqual(lhs.start, rhs.start)) {
            return lhs.start < rhs.start;
        }
        return lhs.end < rhs.end;
    });

    std::vector<WallInterval> merged;
    for (auto interval : intervals) {
        if (interval.end - interval.start <= kGeometryEpsilon) {
            continue;
        }

        if (merged.empty()
            || merged.back().barrier.floorId != interval.barrier.floorId
            || merged.back().vertical != interval.vertical
            || !nearlyEqual(merged.back().fixedCoordinate, interval.fixedCoordinate)
            || interval.start > merged.back().end + kGeometryEpsilon) {
            merged.push_back(std::move(interval));
            continue;
        }

        auto& current = merged.back();
        const auto currentId = QString::fromStdString(current.barrier.id);
        const auto intervalId = QString::fromStdString(interval.barrier.id);
        if (!preferredIds.contains(currentId) && preferredIds.contains(intervalId)) {
            current.barrier.id = interval.barrier.id;
            current.barrier.provenance = interval.barrier.provenance;
        }
        current.end = std::max(current.end, interval.end);
        current.barrier.blocksMovement = current.barrier.blocksMovement || interval.barrier.blocksMovement;
    }

    return merged;
}

void normalizeOpenWallBarriers(safecrowd::domain::FacilityLayout2D& layout, const QStringList& preferredIds = {}) {
    std::vector<safecrowd::domain::Barrier2D> unchanged;
    std::vector<WallInterval> intervals;
    unchanged.reserve(layout.barriers.size());
    intervals.reserve(layout.barriers.size());

    for (auto barrier : layout.barriers) {
        if (barrier.geometry.closed || barrier.geometry.vertices.size() != 2) {
            unchanged.push_back(std::move(barrier));
            continue;
        }

        const auto& a = barrier.geometry.vertices[0];
        const auto& b = barrier.geometry.vertices[1];
        const bool vertical = nearlyEqual(a.x, b.x);
        const bool horizontal = nearlyEqual(a.y, b.y);
        if (!vertical && !horizontal) {
            unchanged.push_back(std::move(barrier));
            continue;
        }

        intervals.push_back({
            .barrier = std::move(barrier),
            .start = vertical ? std::min(a.y, b.y) : std::min(a.x, b.x),
            .end = vertical ? std::max(a.y, b.y) : std::max(a.x, b.x),
            .vertical = vertical,
            .fixedCoordinate = vertical ? a.x : a.y,
        });
    }

    std::vector<safecrowd::domain::Barrier2D> normalized = std::move(unchanged);
    for (auto& interval : normalizeWallIntervals(std::move(intervals), preferredIds)) {
        if (interval.vertical) {
            interval.barrier.geometry.vertices = {
                {.x = interval.fixedCoordinate, .y = interval.start},
                {.x = interval.fixedCoordinate, .y = interval.end},
            };
        } else {
            interval.barrier.geometry.vertices = {
                {.x = interval.start, .y = interval.fixedCoordinate},
                {.x = interval.end, .y = interval.fixedCoordinate},
            };
        }
        interval.barrier.geometry.closed = false;
        normalized.push_back(std::move(interval.barrier));
    }

    layout.barriers = std::move(normalized);
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
    const safecrowd::domain::Point2D& end,
    const std::string& floorId) {
    const bool vertical = nearlyEqual(start.x, end.x);
    const bool horizontal = nearlyEqual(start.y, end.y);
    if (!vertical && !horizontal) {
        return {{start, end}};
    }

    const auto axisStart = vertical ? std::min(start.y, end.y) : std::min(start.x, end.x);
    const auto axisEnd = vertical ? std::max(start.y, end.y) : std::max(start.x, end.x);
    std::vector<std::pair<double, double>> remaining{{axisStart, axisEnd}};

    for (const auto& barrier : layout.barriers) {
        if (!matchesFloor(barrier.floorId, QString::fromStdString(floorId))) {
            continue;
        }
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

std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> subtractStairEntryOverlaps(
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> segments,
    const std::string& floorId) {
    for (const auto& connection : layout.connections) {
        const auto entrySpan = stairEntrySpanForFloor(layout, connection, floorId);
        if (!entrySpan.has_value()) {
            continue;
        }

        std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> next;
        for (const auto& segment : segments) {
            const safecrowd::domain::Barrier2D virtualBarrier{
                .floorId = floorId,
                .geometry = {.vertices = {segment.first, segment.second}},
                .blocksMovement = true,
            };
            const auto remaining = barrierSegmentsAfterGap(virtualBarrier, *entrySpan);
            if (remaining.has_value()) {
                next.insert(next.end(), remaining->begin(), remaining->end());
            } else {
                next.push_back(segment);
            }
        }
        segments = std::move(next);
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
        auto segments = subtractBarrierOverlaps(layout, start, end, floorId);
        segments = subtractStairEntryOverlaps(layout, std::move(segments), floorId);
        for (const auto& segment : segments) {
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

safecrowd::domain::LineSegment2D verticalConnectionSpanForRectangle(
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection entryDirection,
    double passageWidth) {
    const double halfWidth = std::max(0.0, passageWidth) * 0.5;
    const auto center = rectangle.center();
    switch (entryDirection) {
    case safecrowd::domain::StairEntryDirection::North:
    case safecrowd::domain::StairEntryDirection::South:
        return {
            .start = {.x = center.x() - halfWidth, .y = center.y()},
            .end = {.x = center.x() + halfWidth, .y = center.y()},
        };
    case safecrowd::domain::StairEntryDirection::East:
    case safecrowd::domain::StairEntryDirection::West:
        return {
            .start = {.x = center.x(), .y = center.y() + halfWidth},
            .end = {.x = center.x(), .y = center.y() - halfWidth},
        };
    case safecrowd::domain::StairEntryDirection::Unspecified:
        return centerSpanForRectangle(rectangle);
    }
    return centerSpanForRectangle(rectangle);
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
    const double north = std::max(rectangle.top(), rectangle.bottom());
    const double south = std::min(rectangle.top(), rectangle.bottom());
    switch (direction) {
    case safecrowd::domain::StairEntryDirection::North:
        return {
            .start = {.x = rectangle.left(), .y = north},
            .end = {.x = rectangle.right(), .y = north},
        };
    case safecrowd::domain::StairEntryDirection::South:
        return {
            .start = {.x = rectangle.right(), .y = south},
            .end = {.x = rectangle.left(), .y = south},
        };
    case safecrowd::domain::StairEntryDirection::East:
        return {
            .start = {.x = rectangle.right(), .y = north},
            .end = {.x = rectangle.right(), .y = south},
        };
    case safecrowd::domain::StairEntryDirection::West:
        return {
            .start = {.x = rectangle.left(), .y = south},
            .end = {.x = rectangle.left(), .y = north},
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

struct UShapedStairGeometry {
    safecrowd::domain::Polygon2D sourceFootprint{};
    safecrowd::domain::Polygon2D targetFootprint{};
    safecrowd::domain::LineSegment2D sourceEntrySpan{};
    safecrowd::domain::LineSegment2D targetEntrySpan{};
    safecrowd::domain::LineSegment2D verticalSpan{};
    safecrowd::domain::Point2D sourceOutsideSample{};
    safecrowd::domain::Point2D targetOutsideSample{};
    double laneWidth{0.0};
};

struct UStairBasis {
    safecrowd::domain::Point2D entryLeft{};
    safecrowd::domain::Point2D right{};
    safecrowd::domain::Point2D inward{};
    double width{0.0};
    double depth{0.0};
};

safecrowd::domain::Point2D pointAt(
    const UStairBasis& basis,
    double across,
    double depth) {
    return {
        .x = basis.entryLeft.x + (basis.right.x * across) + (basis.inward.x * depth),
        .y = basis.entryLeft.y + (basis.right.y * across) + (basis.inward.y * depth),
    };
}

safecrowd::domain::Point2D midpoint(const safecrowd::domain::LineSegment2D& span) {
    return {
        .x = (span.start.x + span.end.x) * 0.5,
        .y = (span.start.y + span.end.y) * 0.5,
    };
}

UStairBasis uStairBasisForRectangle(
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection entryDirection) {
    const double north = std::max(rectangle.top(), rectangle.bottom());
    const double south = std::min(rectangle.top(), rectangle.bottom());
    switch (entryDirection) {
    case safecrowd::domain::StairEntryDirection::North:
        return {
            .entryLeft = {.x = rectangle.right(), .y = north},
            .right = {.x = -1.0, .y = 0.0},
            .inward = {.x = 0.0, .y = -1.0},
            .width = rectangle.width(),
            .depth = rectangle.height(),
        };
    case safecrowd::domain::StairEntryDirection::East:
        return {
            .entryLeft = {.x = rectangle.right(), .y = south},
            .right = {.x = 0.0, .y = 1.0},
            .inward = {.x = -1.0, .y = 0.0},
            .width = rectangle.height(),
            .depth = rectangle.width(),
        };
    case safecrowd::domain::StairEntryDirection::South:
        return {
            .entryLeft = {.x = rectangle.left(), .y = south},
            .right = {.x = 1.0, .y = 0.0},
            .inward = {.x = 0.0, .y = 1.0},
            .width = rectangle.width(),
            .depth = rectangle.height(),
        };
    case safecrowd::domain::StairEntryDirection::West:
        return {
            .entryLeft = {.x = rectangle.left(), .y = north},
            .right = {.x = 0.0, .y = -1.0},
            .inward = {.x = 1.0, .y = 0.0},
            .width = rectangle.height(),
            .depth = rectangle.width(),
        };
    case safecrowd::domain::StairEntryDirection::Unspecified:
        break;
    }
    return uStairBasisForRectangle(rectangle, safecrowd::domain::StairEntryDirection::West);
}

UShapedStairGeometry uShapedStairGeometryForRectangle(
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection entryDirection) {
    const auto basis = uStairBasisForRectangle(rectangle, entryDirection);
    const double split = basis.width * 0.5;
    const double platformStart = basis.depth * 0.70;
    const double outsideOffset = std::max(basis.width, basis.depth) * 0.25;
    const auto outsideSample = [&](const safecrowd::domain::LineSegment2D& span) {
        const auto center = midpoint(span);
        return safecrowd::domain::Point2D{
            .x = center.x - (basis.inward.x * outsideOffset),
            .y = center.y - (basis.inward.y * outsideOffset),
        };
    };

    UShapedStairGeometry geometry;
    geometry.sourceFootprint.outline = {
        pointAt(basis, split, 0.0),
        pointAt(basis, basis.width, 0.0),
        pointAt(basis, basis.width, basis.depth),
        pointAt(basis, split, basis.depth),
    };
    geometry.targetFootprint.outline = {
        pointAt(basis, 0.0, 0.0),
        pointAt(basis, split, 0.0),
        pointAt(basis, split, basis.depth),
        pointAt(basis, 0.0, basis.depth),
    };
    geometry.sourceEntrySpan = {
        .start = pointAt(basis, split, 0.0),
        .end = pointAt(basis, basis.width, 0.0),
    };
    geometry.targetEntrySpan = {
        .start = pointAt(basis, 0.0, 0.0),
        .end = pointAt(basis, split, 0.0),
    };
    geometry.verticalSpan = {
        .start = pointAt(basis, split, platformStart),
        .end = pointAt(basis, split, basis.depth),
    };
    geometry.sourceOutsideSample = outsideSample(geometry.sourceEntrySpan);
    geometry.targetOutsideSample = outsideSample(geometry.targetEntrySpan);
    geometry.laneWidth = split;
    return geometry;
}

QStringList appendWallsForPolygonExceptGaps(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Polygon2D& polygon,
    const std::vector<safecrowd::domain::LineSegment2D>& gaps,
    const std::string& floorId) {
    QStringList ids;
    if (polygon.outline.size() < 2) {
        return ids;
    }

    for (std::size_t i = 0; i < polygon.outline.size(); ++i) {
        std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> remaining{{
            polygon.outline[i],
            polygon.outline[(i + 1) % polygon.outline.size()],
        }};
        for (const auto& gap : gaps) {
            std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> next;
            for (const auto& segment : remaining) {
                const safecrowd::domain::Barrier2D virtualBarrier{
                    .floorId = floorId,
                    .geometry = {.vertices = {segment.first, segment.second}},
                    .blocksMovement = true,
                };
                const auto clipped = barrierSegmentsAfterGap(virtualBarrier, gap);
                if (clipped.has_value()) {
                    next.insert(next.end(), clipped->begin(), clipped->end());
                } else {
                    next.push_back(segment);
                }
            }
            remaining = std::move(next);
        }

        for (const auto& segment : remaining) {
            if (std::hypot(segment.second.x - segment.first.x, segment.second.y - segment.first.y) <= kGeometryEpsilon) {
                continue;
            }
            ids.append(appendBarrierSegmentWithId(layout, segment.first, segment.second, floorId));
        }
    }
    return ids;
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

safecrowd::domain::StairEntryDirection oppositeStairEntryDirection(
    safecrowd::domain::StairEntryDirection direction) {
    switch (direction) {
    case safecrowd::domain::StairEntryDirection::North:
        return safecrowd::domain::StairEntryDirection::South;
    case safecrowd::domain::StairEntryDirection::East:
        return safecrowd::domain::StairEntryDirection::West;
    case safecrowd::domain::StairEntryDirection::South:
        return safecrowd::domain::StairEntryDirection::North;
    case safecrowd::domain::StairEntryDirection::West:
        return safecrowd::domain::StairEntryDirection::East;
    case safecrowd::domain::StairEntryDirection::Unspecified:
        return safecrowd::domain::StairEntryDirection::Unspecified;
    }
    return safecrowd::domain::StairEntryDirection::Unspecified;
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

bool intervalContainsSpan(double sourceStart, double sourceEnd, double spanStart, double spanEnd) {
    const auto sourceMin = std::min(sourceStart, sourceEnd);
    const auto sourceMax = std::max(sourceStart, sourceEnd);
    const auto spanMin = std::min(spanStart, spanEnd);
    const auto spanMax = std::max(spanStart, spanEnd);
    return sourceMin <= spanMin + kGeometryEpsilon
        && sourceMax >= spanMax - kGeometryEpsilon
        && spanMax - spanMin > kGeometryEpsilon;
}

std::optional<std::size_t> barrierIndexCoveringSpan(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::LineSegment2D& span,
    const QString& floorId) {
    const bool spanVertical = nearlyEqual(span.start.x, span.end.x);
    const bool spanHorizontal = nearlyEqual(span.start.y, span.end.y);
    if (!spanVertical && !spanHorizontal) {
        return std::nullopt;
    }

    for (std::size_t index = 0; index < layout.barriers.size(); ++index) {
        const auto& barrier = layout.barriers[index];
        if (!matchesFloor(barrier.floorId, floorId)
            || barrier.geometry.closed
            || barrier.geometry.vertices.size() != 2) {
            continue;
        }

        const auto& barrierStart = barrier.geometry.vertices[0];
        const auto& barrierEnd = barrier.geometry.vertices[1];
        const bool barrierVertical = nearlyEqual(barrierStart.x, barrierEnd.x);
        const bool barrierHorizontal = nearlyEqual(barrierStart.y, barrierEnd.y);
        if (spanVertical && barrierVertical
            && nearlyEqual(span.start.x, barrierStart.x)
            && intervalContainsSpan(barrierStart.y, barrierEnd.y, span.start.y, span.end.y)) {
            return index;
        }
        if (spanHorizontal && barrierHorizontal
            && nearlyEqual(span.start.y, barrierStart.y)
            && intervalContainsSpan(barrierStart.x, barrierEnd.x, span.start.x, span.end.x)) {
            return index;
        }
    }

    return std::nullopt;
}

std::optional<std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>>> barrierSegmentsAfterGap(
    const safecrowd::domain::Barrier2D& barrier,
    const safecrowd::domain::LineSegment2D& gap) {
    if (barrier.geometry.vertices.size() != 2) {
        return std::nullopt;
    }

    const auto& start = barrier.geometry.vertices[0];
    const auto& end = barrier.geometry.vertices[1];
    const bool barrierVertical = nearlyEqual(start.x, end.x);
    const bool barrierHorizontal = nearlyEqual(start.y, end.y);
    const bool gapVertical = nearlyEqual(gap.start.x, gap.end.x);
    const bool gapHorizontal = nearlyEqual(gap.start.y, gap.end.y);
    if ((!barrierVertical && !barrierHorizontal)
        || (barrierVertical != gapVertical)
        || (barrierHorizontal != gapHorizontal)) {
        return std::nullopt;
    }

    std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> remaining;
    if (barrierVertical) {
        if (!nearlyEqual(start.x, gap.start.x)) {
            return std::nullopt;
        }
        const auto sourceStart = std::min(start.y, end.y);
        const auto sourceEnd = std::max(start.y, end.y);
        const auto gapStart = std::max(sourceStart, std::min(gap.start.y, gap.end.y));
        const auto gapEnd = std::min(sourceEnd, std::max(gap.start.y, gap.end.y));
        if (gapEnd <= gapStart + kGeometryEpsilon) {
            return std::nullopt;
        }
        if (gapStart - sourceStart > kGeometryEpsilon) {
            remaining.push_back({{.x = start.x, .y = sourceStart}, {.x = start.x, .y = gapStart}});
        }
        if (sourceEnd - gapEnd > kGeometryEpsilon) {
            remaining.push_back({{.x = start.x, .y = gapEnd}, {.x = start.x, .y = sourceEnd}});
        }
    } else {
        if (!nearlyEqual(start.y, gap.start.y)) {
            return std::nullopt;
        }
        const auto sourceStart = std::min(start.x, end.x);
        const auto sourceEnd = std::max(start.x, end.x);
        const auto gapStart = std::max(sourceStart, std::min(gap.start.x, gap.end.x));
        const auto gapEnd = std::min(sourceEnd, std::max(gap.start.x, gap.end.x));
        if (gapEnd <= gapStart + kGeometryEpsilon) {
            return std::nullopt;
        }
        if (gapStart - sourceStart > kGeometryEpsilon) {
            remaining.push_back({{.x = sourceStart, .y = start.y}, {.x = gapStart, .y = start.y}});
        }
        if (sourceEnd - gapEnd > kGeometryEpsilon) {
            remaining.push_back({{.x = gapEnd, .y = start.y}, {.x = sourceEnd, .y = start.y}});
        }
    }

    return remaining;
}

void cutBarriersAtSpan(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::LineSegment2D& gap,
    const std::string& floorId) {
    for (std::size_t index = layout.barriers.size(); index > 0; --index) {
        const auto barrierIndex = index - 1;
        const auto& barrier = layout.barriers[barrierIndex];
        if (!matchesFloor(barrier.floorId, QString::fromStdString(floorId))) {
            continue;
        }
        const auto remaining = barrierSegmentsAfterGap(barrier, gap);
        if (remaining.has_value()) {
            replaceBarrierWithSegments(layout, barrierIndex, *remaining, floorId);
        }
    }
}

void autoConnectRoomToStairEntries(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& roomZoneId,
    const QString& floorId) {
    if (roomZoneId.isEmpty() || floorId.isEmpty()) {
        return;
    }

    const auto roomIndex = findZoneIndexById(layout, roomZoneId.toStdString());
    if (!roomIndex.has_value()) {
        return;
    }

    const auto connectionCount = layout.connections.size();
    for (std::size_t index = 0; index < connectionCount; ++index) {
        const auto connection = layout.connections[index];
        const auto entrySpan = stairEntrySpanForFloor(layout, connection, floorId.toStdString());
        const auto outsideSample = stairEntryOutsideSampleForFloor(layout, connection, floorId.toStdString());
        if (!entrySpan.has_value() || !outsideSample.has_value()) {
            continue;
        }

        const auto candidates = zonesContainingPoint(layout, *outsideSample, floorId, 0.35);
        const bool roomContainsOutsideSample =
            std::find(candidates.begin(), candidates.end(), *roomIndex) != candidates.end();
        const bool roomTouchesEntrySpan = spanOverlapsPolygonBoundary(layout.zones[*roomIndex].area, *entrySpan);
        if (!roomContainsOutsideSample && !roomTouchesEntrySpan) {
            continue;
        }

        const auto* fromZone = findZoneById(layout, connection.fromZoneId);
        const auto* toZone = findZoneById(layout, connection.toZoneId);
        const auto* stairZone = fromZone != nullptr && fromZone->floorId == floorId.toStdString() ? fromZone : toZone;
        if (stairZone == nullptr || !isVerticalZone(*stairZone)) {
            continue;
        }

        const auto stairZoneId = QString::fromStdString(stairZone->id);
        if (stairZoneId == roomZoneId
            || hasConnectionPairAtSpan(layout, roomZoneId, stairZoneId, entrySpan->start, entrySpan->end)) {
            continue;
        }

        cutBarriersAtSpan(layout, *entrySpan, floorId.toStdString());
        const auto connectionId = nextConnectionId(layout);
        layout.connections.push_back({
            .id = connectionId.toStdString(),
            .floorId = floorId.toStdString(),
            .kind = safecrowd::domain::ConnectionKind::Opening,
            .fromZoneId = roomZoneId.toStdString(),
            .toZoneId = stairZoneId.toStdString(),
            .effectiveWidth = std::max(0.9, std::hypot(
                entrySpan->end.x - entrySpan->start.x,
                entrySpan->end.y - entrySpan->start.y)),
            .directionality = safecrowd::domain::TravelDirection::Bidirectional,
            .centerSpan = *entrySpan,
        });
    }
}

QString layoutToolIconResourcePath(const QString& glyph) {
    if (glyph == "select") {
        return QStringLiteral(":/tool-icons/layout-authoring/select.svg");
    }
    if (glyph == "reset") {
        return QStringLiteral(":/tool-icons/layout-authoring/reset-view.svg");
    }
    if (glyph == "add") {
        return QStringLiteral(":/tool-icons/layout-authoring/add-floor.svg");
    }
    if (glyph == "grid") {
        return QStringLiteral(":/tool-icons/layout-authoring/grid-snap.svg");
    }
    if (glyph == "room") {
        return QStringLiteral(":/tool-icons/layout-authoring/draw-room.svg");
    }
    if (glyph == "exit") {
        return QStringLiteral(":/tool-icons/layout-authoring/draw-exit.svg");
    }
    if (glyph == "wall") {
        return QStringLiteral(":/tool-icons/layout-authoring/draw-wall.svg");
    }
    if (glyph == "obstruction") {
        return QStringLiteral(":/tool-icons/layout-authoring/draw-obstruction.svg");
    }
    if (glyph == "door") {
        return QStringLiteral(":/tool-icons/layout-authoring/draw-door.svg");
    }
    if (glyph == "stair") {
        return QStringLiteral(":/tool-icons/layout-authoring/draw-stair-ramp.svg");
    }
    if (glyph == "u-stair") {
        return QStringLiteral(":/tool-icons/layout-authoring/draw-u-stair.svg");
    }
    return {};
}

QIcon makeToolIcon(const QString& glyph, const QColor& color, bool filled = false) {
    const auto resourcePath = layoutToolIconResourcePath(glyph);
    if (!resourcePath.isEmpty()) {
        const auto icon = makeSvgToolIcon(resourcePath, color, QSize(24, 24));
        if (!icon.isNull()) {
            return icon;
        }
    }

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
    } else if (glyph == "obstruction") {
        painter.setBrush(QColor(color.red(), color.green(), color.blue(), 42));
        painter.drawRect(QRectF(5, 6, 14, 12));
        painter.drawLine(QPointF(8, 9), QPointF(16, 15));
        painter.drawLine(QPointF(16, 9), QPointF(8, 15));
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
    } else if (glyph == "u-stair") {
        painter.drawRect(QRectF(5, 5, 14, 14));
        painter.drawLine(QPointF(12, 5), QPointF(12, 19));
        painter.drawLine(QPointF(7, 16), QPointF(11, 16));
        painter.drawLine(QPointF(7, 12), QPointF(11, 12));
        painter.drawLine(QPointF(13, 8), QPointF(17, 8));
        painter.drawLine(QPointF(13, 12), QPointF(17, 12));
        painter.drawLine(QPointF(9, 18), QPointF(15, 6));
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
    } else if (glyph == "reset") {
        painter.drawArc(QRectF(5, 5, 14, 14), 40 * 16, 280 * 16);
        painter.drawLine(QPointF(15, 4), QPointF(19, 5));
        painter.drawLine(QPointF(15, 4), QPointF(17, 8));
    } else if (glyph == "add") {
        painter.drawLine(QPointF(12, 5), QPointF(12, 19));
        painter.drawLine(QPointF(5, 12), QPointF(19, 12));
    } else if (glyph == "grid") {
        for (int coordinate = 6; coordinate <= 18; coordinate += 6) {
            painter.drawLine(QPointF(coordinate, 5), QPointF(coordinate, 19));
            painter.drawLine(QPointF(5, coordinate), QPointF(19, coordinate));
        }
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
        if (!connectionVisibleOnFloor(layout, connection, floorId)) {
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
        if (barrier.geometry.closed && polylinePainterPath(barrier.geometry, transform).contains(position)) {
            return QString::fromStdString(barrier.id);
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
        if (barrier.geometry.closed && barrier.geometry.vertices.size() > 2) {
            const auto start = transform.map(barrier.geometry.vertices.back());
            const auto end = transform.map(barrier.geometry.vertices.front());
            const auto distance = distanceToSegment(position, start, end);
            if (distance <= kConnectionHitTolerance && distance < bestDistance) {
                bestDistance = distance;
                bestId = QString::fromStdString(barrier.id);
            }
        }
    }

    return bestId;
}

bool selectedZoneContainsContextPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QStringList& selectedZoneIds,
    const QPointF& position,
    const LayoutTransform& transform,
    const QString& floorId) {
    if (selectedZoneIds.isEmpty()) {
        return false;
    }

    for (const auto& zone : layout.zones) {
        const auto zoneId = QString::fromStdString(zone.id);
        if (selectedZoneIds.contains(zoneId)
            && matchesFloor(zone.floorId, floorId)
            && strokedPathContainsPoint(polygonPath(zone.area, transform), position, kSelectedContextHitTolerancePixels)) {
            return true;
        }
    }

    return false;
}

bool selectedConnectionContainsContextPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QStringList& selectedConnectionIds,
    const QPointF& position,
    const LayoutTransform& transform,
    const QString& floorId) {
    if (selectedConnectionIds.isEmpty()) {
        return false;
    }

    for (const auto& connection : layout.connections) {
        const auto connectionId = QString::fromStdString(connection.id);
        if (!selectedConnectionIds.contains(connectionId) || !connectionVisibleOnFloor(layout, connection, floorId)) {
            continue;
        }

        const auto start = transform.map(connection.centerSpan.start);
        const auto end = transform.map(connection.centerSpan.end);
        if (distanceToSegment(position, start, end) <= kSelectedContextHitTolerancePixels) {
            return true;
        }
    }

    return false;
}

bool selectedBarrierContainsContextPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QStringList& selectedBarrierIds,
    const QPointF& position,
    const LayoutTransform& transform,
    const QString& floorId) {
    if (selectedBarrierIds.isEmpty()) {
        return false;
    }

    for (const auto& barrier : layout.barriers) {
        const auto barrierId = QString::fromStdString(barrier.id);
        if (!selectedBarrierIds.contains(barrierId) || !matchesFloor(barrier.floorId, floorId)) {
            continue;
        }

        if (strokedPathContainsPoint(polylinePainterPath(barrier.geometry, transform), position, kSelectedContextHitTolerancePixels)) {
            return true;
        }
    }

    return false;
}

bool selectedElementContainsContextPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& position,
    const LayoutTransform& transform,
    const QString& floorId,
    const QStringList& selectedZoneIds,
    const QStringList& selectedConnectionIds,
    const QStringList& selectedBarrierIds) {
    return selectedBarrierContainsContextPoint(layout, selectedBarrierIds, position, transform, floorId)
        || selectedConnectionContainsContextPoint(layout, selectedConnectionIds, position, transform, floorId)
        || selectedZoneContainsContextPoint(layout, selectedZoneIds, position, transform, floorId);
}

std::optional<QString> barrierIdForTraceTarget(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& targetId,
    const QString& floorId) {
    for (const auto& barrier : layout.barriers) {
        if (!matchesFloor(barrier.floorId, floorId)) {
            continue;
        }
        const auto barrierId = QString::fromStdString(barrier.id);
        if (barrierId == targetId || traceMatches(barrier.provenance, targetId)) {
            return barrierId;
        }
    }

    return std::nullopt;
}

std::optional<QString> hitTestCanonicalWallBarrier(
    const safecrowd::domain::ImportResult& importResult,
    const QPointF& position,
    const LayoutTransform& transform,
    const QString& floorId) {
    if (!importResult.layout.has_value() || !importResult.canonicalGeometry.has_value()) {
        return std::nullopt;
    }

    double bestDistance = std::numeric_limits<double>::max();
    std::optional<QString> bestBarrierId;
    for (const auto& wall : importResult.canonicalGeometry->walls) {
        const auto wallId = QString::fromStdString(wall.id);
        const auto barrierId = barrierIdForTraceTarget(*importResult.layout, wallId, floorId);
        if (!barrierId.has_value()) {
            continue;
        }

        const auto start = transform.map(wall.segment.start);
        const auto end = transform.map(wall.segment.end);
        const auto distance = distanceToSegment(position, start, end);
        if (distance <= kConnectionHitTolerance && distance < bestDistance) {
            bestDistance = distance;
            bestBarrierId = *barrierId;
        }
    }

    return bestBarrierId;
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
    if (elementId.startsWith("floor:")) {
        const auto floorId = elementId.mid(QString("floor:").size());
        if (!floorId.isEmpty()) {
            currentFloorId_ = floorId;
            clearSelection();
            refreshFloorSelector();
            camera_.reset();
            update();
        }
        return;
    }

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
    selectedZoneIds_.clear();
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
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

    pruneSelection();

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

bool LayoutPreviewWidget::updateElementVertices(
    PreviewSelectionKind kind,
    const QString& elementId,
    const std::vector<safecrowd::domain::Point2D>& vertices) {
    if (!importResult_.layout.has_value() || elementId.isEmpty()) {
        return false;
    }

    bool changed = false;
    auto& layout = *importResult_.layout;

    switch (kind) {
    case PreviewSelectionKind::Zone:
        if (vertices.size() < 3) {
            return false;
        }
        for (auto& zone : layout.zones) {
            if (QString::fromStdString(zone.id) == elementId) {
                zone.area.outline = vertices;
                changed = true;
                break;
            }
        }
        break;
    case PreviewSelectionKind::Connection:
        if (vertices.size() != 2) {
            return false;
        }
        for (auto& connection : layout.connections) {
            if (QString::fromStdString(connection.id) == elementId) {
                connection.centerSpan = {.start = vertices[0], .end = vertices[1]};
                changed = true;
                break;
            }
        }
        break;
    case PreviewSelectionKind::Barrier:
        if (vertices.size() < 2) {
            return false;
        }
        for (auto& barrier : layout.barriers) {
            if (QString::fromStdString(barrier.id) == elementId) {
                barrier.geometry.vertices = vertices;
                changed = true;
                break;
            }
        }
        if (changed) {
            normalizeOpenWallBarriers(layout, QStringList{elementId});
            pruneSelection();
        }
        break;
    case PreviewSelectionKind::None:
    case PreviewSelectionKind::Multiple:
        return false;
    }

    if (!changed) {
        return false;
    }

    emitCurrentSelection();
    notifyLayoutEdited();
    update();
    return true;
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

    if ((toolMode_ == ToolMode::DrawRoom || toolMode_ == ToolMode::DrawObstruction)
        && shapeDrawMode_ == ShapeDrawMode::Polygon
        && drafting_) {
        if (event->key() == Qt::Key_Escape) {
            drafting_ = false;
            polygonDraftPoints_.clear();
            update();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            finishPolygonDraft();
            event->accept();
            return;
        }
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
    if ((toolMode_ == ToolMode::DrawRoom || toolMode_ == ToolMode::DrawObstruction)
        && shapeDrawMode_ == ShapeDrawMode::Polygon
        && drafting_) {
        finishPolygonDraft();
        event->accept();
        return;
    }

    QWidget::mouseDoubleClickEvent(event);
    resetView();
}

void LayoutPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (selectionMoveDragging_) {
        updateSelectionMove(event->position());
        event->accept();
        return;
    }

    if (!camera_.panning() && !drafting_ && !selectionDragging_) {
        updateHoverCursor(event->position());
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (selectionDragging_) {
        selectionDragCurrent_ = event->position();
        update();
        event->accept();
        return;
    }

    if (drafting_) {
        const auto bounds = collectBounds(importResult_, currentFloorId());
        if (bounds.has_value()) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            const auto world = transform.unmap(event->position());
            const QPointF worldPoint(world.x, world.y);
            draftCurrentWorld_ = (toolMode_ == ToolMode::DrawRoom || toolMode_ == ToolMode::DrawObstruction)
                    && shapeDrawMode_ == ShapeDrawMode::Polygon
                ? snapWorldPoint(worldPoint, transform)
                : snapDragWorldPoint(draftStartWorld_, worldPoint, transform);
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

    if (event->button() == Qt::RightButton && toolMode_ == ToolMode::Select) {
        const auto bounds = collectBounds(importResult_, currentFloorId());
        if (bounds.has_value() && importResult_.layout.has_value()) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            const auto floorId = currentFloorId();
            const bool clickedCurrentSelection = selectedElementContainsContextPoint(
                *importResult_.layout,
                event->position(),
                transform,
                floorId,
                selectedZoneIds_,
                selectedConnectionIds_,
                selectedBarrierIds_);
            if (!clickedCurrentSelection) {
                const auto zoneId = hitTestZone(*importResult_.layout, event->position(), transform, floorId);
                const auto connectionId = hitTestConnection(*importResult_.layout, event->position(), transform, floorId);
                auto barrierId = hitTestBarrier(*importResult_.layout, event->position(), transform, floorId);
                if (!barrierId.has_value()) {
                    barrierId = hitTestCanonicalWallBarrier(importResult_, event->position(), transform, floorId);
                }
                if (connectionId.has_value() && !isSelected(PreviewSelectionKind::Connection, *connectionId)) {
                    selectConnection(*connectionId);
                } else if (barrierId.has_value() && !isSelected(PreviewSelectionKind::Barrier, *barrierId)) {
                    selectBarrier(*barrierId);
                } else if (zoneId.has_value() && !isSelected(PreviewSelectionKind::Zone, *zoneId)) {
                    selectZone(*zoneId);
                }
            }
        }
        showSelectionContextMenu(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    if (camera_.beginPan(event)) {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const auto bounds = collectBounds(importResult_, currentFloorId());
        if (!bounds.has_value()) {
            QWidget::mousePressEvent(event);
            return;
        }

        if ((toolMode_ == ToolMode::DrawRoom || toolMode_ == ToolMode::DrawObstruction)
            && shapeDrawMode_ == ShapeDrawMode::Polygon) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            const auto world = transform.unmap(event->position());
            const auto snappedWorld = snapWorldPoint(QPointF(world.x, world.y), transform);
            if (drafting_ && polygonDraftPoints_.size() >= 3) {
                const auto firstScreen = transform.map({
                    .x = polygonDraftPoints_.front().x(),
                    .y = polygonDraftPoints_.front().y(),
                });
                const auto currentScreen = transform.map({.x = snappedWorld.x(), .y = snappedWorld.y()});
                if (distanceBetweenScreenPoints(firstScreen, currentScreen) <= kPolygonCloseTolerancePixels) {
                    finishPolygonDraft();
                    event->accept();
                    return;
                }
            }

            drafting_ = true;
            draftCurrentWorld_ = snappedWorld;
            if (polygonDraftPoints_.empty()
                || distanceBetweenScreenPoints(
                    transform.map({.x = polygonDraftPoints_.back().x(), .y = polygonDraftPoints_.back().y()}),
                    transform.map({.x = snappedWorld.x(), .y = snappedWorld.y()})) > 1.0) {
                polygonDraftPoints_.push_back(snappedWorld);
            }
            update();
            event->accept();
            return;
        }

        if (toolMode_ != ToolMode::Select) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            const auto world = transform.unmap(event->position());
            drafting_ = true;
            draftStartWorld_ = snapWorldPoint(QPointF(world.x, world.y), transform);
            draftCurrentWorld_ = draftStartWorld_;
            event->accept();
            return;
        }

        if (importResult_.layout.has_value() && hasSelection()) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            const bool clickedCurrentSelection = selectedElementContainsContextPoint(
                *importResult_.layout,
                event->position(),
                transform,
                currentFloorId(),
                selectedZoneIds_,
                selectedConnectionIds_,
                selectedBarrierIds_);
            if (clickedCurrentSelection) {
                beginSelectionMove(event->position(), transform, *bounds);
                event->accept();
                return;
            }
        }

        selectionDragging_ = true;
        selectionDragStart_ = event->position();
        selectionDragCurrent_ = selectionDragStart_;
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void LayoutPreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (camera_.finishPan(event)) {
        return;
    }

    if (selectionMoveDragging_ && event->button() == Qt::LeftButton) {
        finishSelectionMove(event->position());
        event->accept();
        return;
    }

    if (selectionDragging_ && event->button() == Qt::LeftButton) {
        selectionDragging_ = false;
        selectionDragCurrent_ = event->position();
        const auto dragDistance = distanceBetweenScreenPoints(selectionDragStart_, selectionDragCurrent_);
        const auto bounds = collectBounds(importResult_, currentFloorId());
        if (bounds.has_value()) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            if (dragDistance <= kSelectionDragThresholdPixels) {
                selectSingleAt(event->position(), transform);
            } else {
                selectElementsInRect(QRectF(selectionDragStart_, selectionDragCurrent_).normalized(), transform);
            }
        }
        update();
        event->accept();
        return;
    }

    if (drafting_ && event->button() == Qt::LeftButton
        && !((toolMode_ == ToolMode::DrawRoom || toolMode_ == ToolMode::DrawObstruction)
            && shapeDrawMode_ == ShapeDrawMode::Polygon)) {
        drafting_ = false;
        const auto bounds = collectBounds(importResult_, currentFloorId());
        if (bounds.has_value()) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
            const auto world = transform.unmap(event->position());
            draftCurrentWorld_ = snapDragWorldPoint(draftStartWorld_, QPointF(world.x, world.y), transform);
        }

        switch (toolMode_) {
        case ToolMode::DrawRoom:
            createZone(draftStartWorld_, draftCurrentWorld_, safecrowd::domain::ZoneKind::Room);
            break;
        case ToolMode::DrawExit:
            createZone(draftStartWorld_, draftCurrentWorld_, safecrowd::domain::ZoneKind::Exit);
            break;
        case ToolMode::DrawWall:
            createWallSegment(draftStartWorld_, draftCurrentWorld_);
            break;
        case ToolMode::DrawDoor:
            if (std::hypot(draftCurrentWorld_.x() - draftStartWorld_.x(), draftCurrentWorld_.y() - draftStartWorld_.y()) < kDraftMinimumSize) {
                applyToolAt(event->position());
            } else {
                createDoorSegment(draftStartWorld_, draftCurrentWorld_);
            }
            break;
        case ToolMode::DrawObstruction:
            createObstructionRectangle(draftStartWorld_, draftCurrentWorld_);
            break;
        case ToolMode::DrawStair:
            createVerticalLink(draftStartWorld_, draftCurrentWorld_);
            break;
        case ToolMode::DrawUStair:
            createUShapedStairLink(draftStartWorld_, draftCurrentWorld_);
            break;
        case ToolMode::Select:
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

    if (gridSnapEnabled_) {
        drawLayoutCanvasGrid(painter, viewport, transform, gridSpacingMeters_);
    }

    if (importResult_.layout.has_value()) {
        drawFacilityLayoutCanvas(painter, *importResult_.layout, transform, currentFloorId().toStdString());
    } else if (importResult_.canonicalGeometry.has_value()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(231, 238, 246));
        for (const auto& walkable : importResult_.canonicalGeometry->walkableAreas) {
            painter.drawPath(polygonPath(walkable.polygon, transform));
        }
    }

    if (!importResult_.layout.has_value() && importResult_.canonicalGeometry.has_value()) {
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

    const bool hasExplicitSelection = hasSelection();
    if (hasExplicitSelection || !focusedTargetId_.isEmpty()) {
        painter.setBrush(QColor(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 44));
        painter.setPen(QPen(kSelectionHighlightColor, 2.25, Qt::DashLine));

        if (importResult_.layout.has_value()) {
            for (const auto& zone : importResult_.layout->zones) {
                if (!matchesFloor(zone.floorId, currentFloorId())) {
                    continue;
                }
                const auto id = QString::fromStdString(zone.id);
                const bool selected = selectedZoneIds_.contains(id);
                const bool focused = !hasExplicitSelection
                    && !focusedTargetId_.isEmpty()
                    && (id == focusedTargetId_ || traceMatches(zone.provenance, focusedTargetId_));
                if (selected || focused) {
                    painter.drawPath(polygonPath(zone.area, transform));
                }
            }
            for (const auto& connection : importResult_.layout->connections) {
                if (!connectionVisibleOnFloor(*importResult_.layout, connection, currentFloorId())) {
                    continue;
                }
                const auto id = QString::fromStdString(connection.id);
                const bool selected = selectedConnectionIds_.contains(id);
                const bool focused = !hasExplicitSelection
                    && !focusedTargetId_.isEmpty()
                    && (id == focusedTargetId_ || traceMatches(connection.provenance, focusedTargetId_));
                if (selected || focused) {
                    drawLine(painter, connection.centerSpan, transform);
                }
            }
            for (const auto& barrier : importResult_.layout->barriers) {
                if (!matchesFloor(barrier.floorId, currentFloorId())) {
                    continue;
                }
                const auto id = QString::fromStdString(barrier.id);
                const bool selected = selectedBarrierIds_.contains(id);
                const bool focused = !hasExplicitSelection
                    && !focusedTargetId_.isEmpty()
                    && (id == focusedTargetId_ || traceMatches(barrier.provenance, focusedTargetId_));
                if (selected || focused) {
                    drawPolyline(painter, barrier.geometry, transform);
                }
            }
        }

        if (!hasExplicitSelection && importResult_.canonicalGeometry.has_value()) {
            for (const auto& walkable : importResult_.canonicalGeometry->walkableAreas) {
                const auto id = QString::fromStdString(walkable.id);
                if (traceRefMatches(importResult_, id, focusedTargetId_)) {
                    painter.drawPath(polygonPath(walkable.polygon, transform));
                }
            }
            for (const auto& obstacle : importResult_.canonicalGeometry->obstacles) {
                const auto id = QString::fromStdString(obstacle.id);
                if (traceRefMatches(importResult_, id, focusedTargetId_)) {
                    painter.drawPath(polygonPath(obstacle.footprint, transform));
                }
            }
            for (const auto& wall : importResult_.canonicalGeometry->walls) {
                const auto id = QString::fromStdString(wall.id);
                if (traceRefMatches(importResult_, id, focusedTargetId_)) {
                    drawLine(painter, wall.segment, transform);
                }
            }
            for (const auto& opening : importResult_.canonicalGeometry->openings) {
                const auto id = QString::fromStdString(opening.id);
                if (traceRefMatches(importResult_, id, focusedTargetId_)) {
                    drawLine(painter, opening.span, transform);
                }
            }
        }
    }

    if (selectionDragging_) {
        painter.setBrush(QColor(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 28));
        painter.setPen(QPen(kSelectionHighlightColor, 1.6, Qt::DashLine));
        painter.drawRect(QRectF(selectionDragStart_, selectionDragCurrent_).normalized());
    }

    if (drafting_) {
        painter.setBrush(QColor(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 60));
        painter.setPen(QPen(kSelectionHighlightColor, 2.0, Qt::DashLine));

        if ((toolMode_ == ToolMode::DrawRoom || toolMode_ == ToolMode::DrawObstruction)
            && shapeDrawMode_ == ShapeDrawMode::Polygon) {
            QPolygonF draftPolygon;
            for (const auto& point : polygonDraftPoints_) {
                draftPolygon.append(transform.map({.x = point.x(), .y = point.y()}));
            }
            if (!polygonDraftPoints_.empty()) {
                draftPolygon.append(transform.map({.x = draftCurrentWorld_.x(), .y = draftCurrentWorld_.y()}));
            }

            painter.setBrush(polygonDraftPoints_.size() >= 3
                    ? QColor(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 42)
                    : Qt::NoBrush);
            if (draftPolygon.size() >= 2) {
                painter.drawPolyline(draftPolygon);
            }
            if (polygonDraftPoints_.size() >= 3) {
                painter.drawLine(draftPolygon.last(), draftPolygon.first());
            }
            painter.setBrush(kSelectionHighlightColor);
            painter.setPen(Qt::NoPen);
            for (const auto& point : polygonDraftPoints_) {
                painter.drawEllipse(transform.map({.x = point.x(), .y = point.y()}), 3.5, 3.5);
            }
            painter.setPen(QPen(kSelectionHighlightColor, 2.0, Qt::DashLine));
        } else {
            const safecrowd::domain::Point2D start{draftStartWorld_.x(), draftStartWorld_.y()};
            const safecrowd::domain::Point2D current{draftCurrentWorld_.x(), draftCurrentWorld_.y()};
            if (toolMode_ == ToolMode::DrawDoor || toolMode_ == ToolMode::DrawWall) {
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
    if (switchFloorByWheel(event)) {
        return;
    }

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
        selectSingleAt(testPosition, transform);
        return;
    case ToolMode::DrawRoom:
    case ToolMode::DrawExit:
    case ToolMode::DrawWall:
    case ToolMode::DrawObstruction:
    case ToolMode::DrawStair:
    case ToolMode::DrawUStair:
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
    selectedZoneIds_.clear();
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
    focusedTargetId_.clear();
    selectionDragging_ = false;
    selectionMoveDragging_ = false;
    selectionMoveAttachedBarrierIds_.clear();
    selectionMoveAnchors_.clear();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createRoomPolygon(const std::vector<QPointF>& points) {
    if (!importResult_.layout.has_value() || points.size() < 3) {
        return;
    }

    safecrowd::domain::Polygon2D roomPolygon;
    roomPolygon.outline.reserve(points.size());
    for (const auto& point : points) {
        roomPolygon.outline.push_back({.x = point.x(), .y = point.y()});
    }

    QPolygonF polygonForArea;
    for (const auto& point : points) {
        polygonForArea.append(point);
    }
    if (polygonArea(polygonForArea) < kDraftMinimumSize) {
        return;
    }

    auto& layout = *importResult_.layout;
    const auto currentFloor = currentFloorId();
    QPainterPath candidatePath = worldPolygonPath(roomPolygon);
    QPainterPath occupiedRooms;
    for (const auto& zone : layout.zones) {
        if (!matchesFloor(zone.floorId, currentFloor)) {
            continue;
        }
        if (zone.kind != safecrowd::domain::ZoneKind::Room) {
            continue;
        }
        occupiedRooms = occupiedRooms.united(worldPolygonPath(zone.area));
    }

    auto polygonsToCreate = polygonsFromFillPath(candidatePath.subtracted(occupiedRooms).simplified());
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
            .kind = safecrowd::domain::ZoneKind::Room,
            .label = QString("Room %1").arg(zoneNumber).toStdString(),
            .area = polygon,
            .defaultCapacity = 0u,
        });

        if (roomAutoWallsEnabled_) {
            appendAutoWallsForPolygon(layout, polygon, floorId);
        }
        autoConnectRoomToStairEntries(layout, zoneId, QString::fromStdString(floorId));

        lastZoneId = zoneId;
    }

    selectedZoneId_ = lastZoneId;
    selectedZoneIds_ = QStringList{lastZoneId};
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
    focusedTargetId_ = lastZoneId;
    notifyLayoutEdited();
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
        const auto currentFloor = currentFloorId();
        QPainterPath candidatePath = worldPolygonPath(rectanglePolygon);
        QPainterPath occupiedRooms;
        for (const auto& zone : layout.zones) {
            if (!matchesFloor(zone.floorId, currentFloor)) {
                continue;
            }
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
        if (kind == safecrowd::domain::ZoneKind::Room) {
            autoConnectRoomToStairEntries(layout, zoneId, QString::fromStdString(floorId));
        }

        lastZoneId = zoneId;
    }

    selectedZoneId_ = lastZoneId;
    selectedZoneIds_ = QStringList{lastZoneId};
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
    focusedTargetId_ = lastZoneId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createWallSegment(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    if (std::hypot(endWorld.x() - startWorld.x(), endWorld.y() - startWorld.y()) < kDraftMinimumSize) {
        return;
    }

    auto& layout = *importResult_.layout;
    const auto barrierId = nextWallId(layout);
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
    normalizeOpenWallBarriers(layout, QStringList{barrierId});

    selectedBarrierId_ = barrierId;
    selectedBarrierIds_ = QStringList{barrierId};
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
    focusedTargetId_ = barrierId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createObstructionPolygon(const std::vector<QPointF>& points) {
    if (!importResult_.layout.has_value() || points.size() < 3) {
        return;
    }

    QPolygonF polygonForArea;
    for (const auto& point : points) {
        polygonForArea.append(point);
    }
    if (polygonArea(polygonForArea) < kDraftMinimumSize) {
        return;
    }

    auto& layout = *importResult_.layout;
    const auto barrierId = nextObstructionId(layout);
    std::vector<safecrowd::domain::Point2D> vertices;
    vertices.reserve(points.size());
    for (const auto& point : points) {
        vertices.push_back({.x = point.x(), .y = point.y()});
    }

    layout.barriers.push_back({
        .id = barrierId.toStdString(),
        .floorId = currentFloorId().toStdString(),
        .geometry = safecrowd::domain::Polyline2D{
            .vertices = std::move(vertices),
            .closed = true,
        },
        .blocksMovement = true,
    });

    selectedBarrierId_ = barrierId;
    selectedBarrierIds_ = QStringList{barrierId};
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
    focusedTargetId_ = barrierId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createObstructionRectangle(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    const auto rectangle = rectFromWorldPoints(startWorld, endWorld);
    if (rectangle.width() < kDraftMinimumSize || rectangle.height() < kDraftMinimumSize) {
        return;
    }

    auto& layout = *importResult_.layout;
    const auto barrierId = nextObstructionId(layout);
    layout.barriers.push_back({
        .id = barrierId.toStdString(),
        .floorId = currentFloorId().toStdString(),
        .geometry = safecrowd::domain::Polyline2D{
            .vertices = {
                {.x = rectangle.left(), .y = rectangle.top()},
                {.x = rectangle.right(), .y = rectangle.top()},
                {.x = rectangle.right(), .y = rectangle.bottom()},
                {.x = rectangle.left(), .y = rectangle.bottom()},
            },
            .closed = true,
        },
        .blocksMovement = true,
    });

    selectedBarrierId_ = barrierId;
    selectedBarrierIds_ = QStringList{barrierId};
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
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
    selectedConnectionIds_ = QStringList{connectionId};
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
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
    const auto sourceEntryDirection = stairEntryDirection_;
    const auto targetEntryDirection = oppositeStairEntryDirection(sourceEntryDirection);
    const auto lowerEntryDirection = sourceIsLower ? sourceEntryDirection : targetEntryDirection;
    const auto upperEntryDirection = sourceIsLower ? targetEntryDirection : sourceEntryDirection;
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
    const auto effectiveWidth = std::max(0.9, std::min(rectangle.width(), rectangle.height()));
    const auto span = verticalConnectionSpanForRectangle(rectangle, sourceEntryDirection, effectiveWidth);

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
        cutBarriersAtSpan(layout, sourceEntrySpan, sourceFloorId.toStdString());
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
        .lowerEntryDirection = lowerEntryDirection,
        .upperEntryDirection = upperEntryDirection,
        .centerSpan = span,
    });

    if (targetZone.has_value()) {
        cutBarriersAtSpan(layout, targetEntrySpan, targetFloorId.toStdString());
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
    selectedConnectionIds_ = QStringList{verticalConnectionId};
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
    focusedTargetId_ = verticalConnectionId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::createUShapedStairLink(const QPointF& startWorld, const QPointF& endWorld) {
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

    const auto geometry = uShapedStairGeometryForRectangle(rectangle, stairEntryDirection_);
    if (geometry.laneWidth <= kGeometryEpsilon) {
        return;
    }

    const bool sourceIsLower = floorElevation(layout, sourceFloorId.toStdString())
        <= floorElevation(layout, targetFloorId.toStdString());
    const auto sourceEntryDirection = stairEntryDirection_;
    const auto targetEntryDirection = stairEntryDirection_;
    const auto lowerEntryDirection = sourceIsLower ? sourceEntryDirection : targetEntryDirection;
    const auto upperEntryDirection = sourceIsLower ? targetEntryDirection : sourceEntryDirection;

    const auto sourceZoneCandidates = zonesContainingPoint(
        layout,
        QPointF(geometry.sourceOutsideSample.x, geometry.sourceOutsideSample.y),
        sourceFloorId,
        0.35);
    const auto targetZoneCandidates = zonesContainingPoint(
        layout,
        QPointF(geometry.targetOutsideSample.x, geometry.targetOutsideSample.y),
        targetFloorId,
        0.35);
    const auto sourceZone = choosePrimaryZone(layout, sourceZoneCandidates);
    const auto targetZone = choosePrimaryZone(layout, targetZoneCandidates);

    const auto sourceStairZoneId = nextZoneId(layout);
    const auto sourceZoneNumber = static_cast<int>(layout.zones.size()) + 1;
    layout.zones.push_back({
        .id = sourceStairZoneId.toStdString(),
        .floorId = sourceFloorId.toStdString(),
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = QString("U Stair %1").arg(sourceZoneNumber).toStdString(),
        .area = geometry.sourceFootprint,
        .defaultCapacity = 8,
        .isStair = true,
    });

    const auto targetStairZoneId = nextZoneId(layout);
    const auto targetZoneNumber = static_cast<int>(layout.zones.size()) + 1;
    layout.zones.push_back({
        .id = targetStairZoneId.toStdString(),
        .floorId = targetFloorId.toStdString(),
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = QString("U Stair %1").arg(targetZoneNumber).toStdString(),
        .area = geometry.targetFootprint,
        .defaultCapacity = 8,
        .isStair = true,
    });

    QStringList preferredWallIds;
    preferredWallIds.append(appendWallsForPolygonExceptGaps(
        layout,
        geometry.sourceFootprint,
        {geometry.sourceEntrySpan, geometry.verticalSpan},
        sourceFloorId.toStdString()));
    preferredWallIds.append(appendWallsForPolygonExceptGaps(
        layout,
        geometry.targetFootprint,
        {geometry.targetEntrySpan, geometry.verticalSpan},
        targetFloorId.toStdString()));

    if (sourceZone.has_value()) {
        cutBarriersAtSpan(layout, geometry.sourceEntrySpan, sourceFloorId.toStdString());
        layout.connections.push_back({
            .id = nextConnectionId(layout).toStdString(),
            .floorId = sourceFloorId.toStdString(),
            .kind = safecrowd::domain::ConnectionKind::Opening,
            .fromZoneId = layout.zones[*sourceZone].id,
            .toZoneId = sourceStairZoneId.toStdString(),
            .effectiveWidth = geometry.laneWidth,
            .directionality = safecrowd::domain::TravelDirection::Bidirectional,
            .centerSpan = geometry.sourceEntrySpan,
        });
    }

    const auto verticalConnectionId = nextVerticalConnectionId(layout);
    layout.connections.push_back({
        .id = verticalConnectionId.toStdString(),
        .floorId = sourceFloorId.toStdString(),
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = sourceStairZoneId.toStdString(),
        .toZoneId = targetStairZoneId.toStdString(),
        .effectiveWidth = geometry.laneWidth,
        .directionality = safecrowd::domain::TravelDirection::Bidirectional,
        .isStair = true,
        .lowerEntryDirection = lowerEntryDirection,
        .upperEntryDirection = upperEntryDirection,
        .centerSpan = geometry.verticalSpan,
    });

    if (targetZone.has_value()) {
        cutBarriersAtSpan(layout, geometry.targetEntrySpan, targetFloorId.toStdString());
        layout.connections.push_back({
            .id = nextConnectionId(layout).toStdString(),
            .floorId = targetFloorId.toStdString(),
            .kind = safecrowd::domain::ConnectionKind::Opening,
            .fromZoneId = targetStairZoneId.toStdString(),
            .toZoneId = layout.zones[*targetZone].id,
            .effectiveWidth = geometry.laneWidth,
            .directionality = safecrowd::domain::TravelDirection::Bidirectional,
            .centerSpan = geometry.targetEntrySpan,
        });
    }

    normalizeOpenWallBarriers(layout, preferredWallIds);

    selectedConnectionId_ = verticalConnectionId;
    selectedConnectionIds_ = QStringList{verticalConnectionId};
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
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

    createDoorSpan(barrierId, gapStart, gapEnd);
}

bool LayoutPreviewWidget::createDoorSegment(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return false;
    }

    safecrowd::domain::LineSegment2D span{
        .start = {.x = startWorld.x(), .y = startWorld.y()},
        .end = {.x = endWorld.x(), .y = endWorld.y()},
    };
    if (std::hypot(span.end.x - span.start.x, span.end.y - span.start.y) < kMinimumDoorWidth) {
        return false;
    }

    const bool vertical = nearlyEqual(span.start.x, span.end.x);
    const bool horizontal = nearlyEqual(span.start.y, span.end.y);
    if (!vertical && !horizontal) {
        return false;
    }

    if (vertical && span.end.y < span.start.y) {
        std::swap(span.start, span.end);
    } else if (horizontal && span.end.x < span.start.x) {
        std::swap(span.start, span.end);
    }

    auto& layout = *importResult_.layout;
    const auto barrierIndex = barrierIndexCoveringSpan(layout, span, currentFloorId());
    if (barrierIndex.has_value()) {
        const auto barrierId = QString::fromStdString(layout.barriers[*barrierIndex].id);
        return createDoorSpan(barrierId, span.start, span.end);
    }

    return createDoorSpan({}, span.start, span.end);
}

bool LayoutPreviewWidget::createDoorSpan(
    const QString& barrierId,
    const safecrowd::domain::Point2D& gapStart,
    const safecrowd::domain::Point2D& gapEnd) {
    if (!importResult_.layout.has_value()) {
        return false;
    }

    auto& layout = *importResult_.layout;
    const auto openingWidth = std::hypot(gapEnd.x - gapStart.x, gapEnd.y - gapStart.y);
    if (openingWidth < kMinimumDoorWidth) {
        return false;
    }

    const bool vertical = nearlyEqual(gapStart.x, gapEnd.x);
    const bool horizontal = nearlyEqual(gapStart.y, gapEnd.y);
    if (!vertical && !horizontal) {
        return false;
    }

    std::optional<std::size_t> barrierIndex;
    std::optional<safecrowd::domain::Point2D> barrierStart;
    std::optional<safecrowd::domain::Point2D> barrierEnd;
    if (!barrierId.isEmpty()) {
        const auto barrierIt = std::find_if(layout.barriers.begin(), layout.barriers.end(), [&](const auto& barrier) {
            return QString::fromStdString(barrier.id) == barrierId && barrier.geometry.vertices.size() == 2;
        });
        if (barrierIt == layout.barriers.end()) {
            return false;
        }

        barrierIndex = static_cast<std::size_t>(std::distance(layout.barriers.begin(), barrierIt));
        barrierStart = barrierIt->geometry.vertices[0];
        barrierEnd = barrierIt->geometry.vertices[1];
        const bool barrierVertical = nearlyEqual(barrierStart->x, barrierEnd->x);
        const bool barrierHorizontal = nearlyEqual(barrierStart->y, barrierEnd->y);
        if (barrierVertical != vertical || barrierHorizontal != horizontal) {
            return false;
        }

        if (vertical) {
            if (!nearlyEqual(barrierStart->x, gapStart.x)
                || !intervalContainsSpan(barrierStart->y, barrierEnd->y, gapStart.y, gapEnd.y)) {
                return false;
            }
        } else if (!nearlyEqual(barrierStart->y, gapStart.y)
            || !intervalContainsSpan(barrierStart->x, barrierEnd->x, gapStart.x, gapEnd.x)) {
            return false;
        }

        const auto segmentLength = std::hypot(barrierEnd->x - barrierStart->x, barrierEnd->y - barrierStart->y);
        if (segmentLength <= openingWidth + kGeometryEpsilon) {
            return false;
        }
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
            return false;
        }

        const bool useFirstOutside = !firstZone.has_value();
        const QPointF outsideSample = useFirstOutside ? neighbors.firstSample : neighbors.secondSample;
        const QPointF insideSample = useFirstOutside ? neighbors.secondSample : neighbors.firstSample;
        QPointF outsideDirection = outsideSample - insideSample;
        const auto directionLength = std::hypot(outsideDirection.x(), outsideDirection.y());
        if (directionLength <= kGeometryEpsilon) {
            return false;
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
        return false;
    }

    if (barrierIndex.has_value() && barrierStart.has_value() && barrierEnd.has_value()) {
        std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> remainingSegments;
        if (vertical) {
            const auto lowerStart = std::min(barrierStart->y, barrierEnd->y);
            const auto lowerEnd = std::max(barrierStart->y, barrierEnd->y);
            if (gapStart.y - lowerStart > kGeometryEpsilon) {
                remainingSegments.push_back({
                    {.x = barrierStart->x, .y = lowerStart},
                    {.x = barrierStart->x, .y = gapStart.y},
                });
            }
            if (lowerEnd - gapEnd.y > kGeometryEpsilon) {
                remainingSegments.push_back({
                    {.x = barrierStart->x, .y = gapEnd.y},
                    {.x = barrierStart->x, .y = lowerEnd},
                });
            }
        } else {
            const auto lowerStart = std::min(barrierStart->x, barrierEnd->x);
            const auto lowerEnd = std::max(barrierStart->x, barrierEnd->x);
            if (gapStart.x - lowerStart > kGeometryEpsilon) {
                remainingSegments.push_back({
                    {.x = lowerStart, .y = barrierStart->y},
                    {.x = gapStart.x, .y = barrierStart->y},
                });
            }
            if (lowerEnd - gapEnd.x > kGeometryEpsilon) {
                remainingSegments.push_back({
                    {.x = gapEnd.x, .y = barrierStart->y},
                    {.x = lowerEnd, .y = barrierStart->y},
                });
            }
        }

        replaceBarrierWithSegments(layout, *barrierIndex, remainingSegments, currentFloorId().toStdString());
    }

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
    selectedConnectionIds_ = QStringList{connectionId};
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
    focusedTargetId_ = connectionId;
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
    return true;
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
    selectedConnectionIds_.removeAll(connectionId);
    selectPrimaryFromLists();
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
    selectedBarrierIds_.removeAll(barrierId);
    selectPrimaryFromLists();
    focusedTargetId_.clear();
    notifyLayoutEdited();
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::deleteSelectedElements() {
    if (!importResult_.layout.has_value() || !hasSelection()) {
        return;
    }

    auto& layout = *importResult_.layout;
    bool changed = false;

    const auto selectedZoneId = [&](const std::string& id) {
        return selectedZoneIds_.contains(QString::fromStdString(id));
    };

    auto& connections = layout.connections;
    const auto connectionIt = std::remove_if(connections.begin(), connections.end(), [&](const auto& connection) {
        return selectedConnectionIds_.contains(QString::fromStdString(connection.id))
            || selectedZoneId(connection.fromZoneId)
            || selectedZoneId(connection.toZoneId);
    });
    if (connectionIt != connections.end()) {
        connections.erase(connectionIt, connections.end());
        changed = true;
    }

    auto& barriers = layout.barriers;
    const auto barrierIt = std::remove_if(barriers.begin(), barriers.end(), [&](const auto& barrier) {
        return selectedBarrierIds_.contains(QString::fromStdString(barrier.id));
    });
    if (barrierIt != barriers.end()) {
        barriers.erase(barrierIt, barriers.end());
        changed = true;
    }

    auto& zones = layout.zones;
    const auto zoneIt = std::remove_if(zones.begin(), zones.end(), [&](const auto& zone) {
        return selectedZoneIds_.contains(QString::fromStdString(zone.id));
    });
    if (zoneIt != zones.end()) {
        zones.erase(zoneIt, zones.end());
        changed = true;
    }

    if (!changed) {
        clearSelection();
        return;
    }

    clearSelection();
    notifyLayoutEdited();
}

void LayoutPreviewWidget::emitCurrentSelection() {
    if (selectionChangedHandler_) {
        selectionChangedHandler_(currentSelection());
    }
}

void LayoutPreviewWidget::finishPolygonDraft() {
    if (!drafting_ || polygonDraftPoints_.size() < 3) {
        drafting_ = false;
        polygonDraftPoints_.clear();
        update();
        return;
    }

    auto points = polygonDraftPoints_;
    drafting_ = false;
    polygonDraftPoints_.clear();
    if (toolMode_ == ToolMode::DrawObstruction) {
        createObstructionPolygon(points);
    } else {
        createRoomPolygon(points);
    }
}

void LayoutPreviewWidget::beginSelectionMove(
    const QPointF& position,
    const LayoutCanvasTransform& transform,
    const LayoutCanvasBounds& bounds) {
    if (!importResult_.layout.has_value() || !hasSelection()) {
        return;
    }

    selectionMoveDragging_ = true;
    selectionMoveStart_ = position;
    selectionMoveCurrent_ = position;
    const auto world = transform.unmap(position);
    selectionMoveStartWorld_ = QPointF(world.x, world.y);
    selectionMoveBounds_ = bounds;
    selectionMoveViewport_ = previewViewport(rect());
    selectionMoveZoom_ = camera_.zoom();
    selectionMovePanOffset_ = camera_.panOffset();
    selectionMoveOriginalLayout_ = *importResult_.layout;
    selectionMoveBaseLayout_ = selectionMoveOriginalLayout_;
    selectionMoveAttachedBarrierIds_ = splitZoneBoundaryBarriersForSelection(selectionMoveBaseLayout_);
    const auto exactBoundaryBarrierIds = zoneBoundaryBarrierIdsForSelection(selectionMoveBaseLayout_);
    for (const auto& barrierId : exactBoundaryBarrierIds) {
        if (!selectionMoveAttachedBarrierIds_.contains(barrierId)) {
            selectionMoveAttachedBarrierIds_.append(barrierId);
        }
    }
    selectionMoveAnchors_ = selectionMoveAnchors(selectionMoveBaseLayout_);
    selectionMoveSnapTargetLayout_ = selectionMoveSnapTargetLayout(selectionMoveBaseLayout_);
    *importResult_.layout = selectionMoveBaseLayout_;
    setCursor(Qt::SizeAllCursor);
}

void LayoutPreviewWidget::updateSelectionMove(const QPointF& position) {
    if (!selectionMoveDragging_ || !importResult_.layout.has_value()) {
        return;
    }

    selectionMoveCurrent_ = position;
    const auto delta = selectionMoveDeltaForPosition(position);
    *importResult_.layout = selectionMoveBaseLayout_;
    applySelectedTranslation(delta.x, delta.y);
    update();
}

void LayoutPreviewWidget::finishSelectionMove(const QPointF& position) {
    if (!selectionMoveDragging_ || !importResult_.layout.has_value()) {
        return;
    }

    updateSelectionMove(position);
    selectionMoveDragging_ = false;
    selectionMoveAttachedBarrierIds_.clear();
    selectionMoveAnchors_.clear();

    const auto dragDistance = distanceBetweenScreenPoints(selectionMoveStart_, selectionMoveCurrent_);
    if (dragDistance <= kSelectionDragThresholdPixels) {
        *importResult_.layout = selectionMoveOriginalLayout_;
        emitCurrentSelection();
        updateHoverCursor(position);
        update();
        return;
    }

    normalizeOpenWallBarriers(*importResult_.layout, selectedBarrierIds_);
    pruneSelection();
    emitCurrentSelection();
    notifyLayoutEdited();
    updateHoverCursor(position);
    update();
}

void LayoutPreviewWidget::applySelectedTranslation(double dx, double dy) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    auto& layout = *importResult_.layout;
    for (auto& zone : layout.zones) {
        if (selectedZoneIds_.contains(QString::fromStdString(zone.id))) {
            translatePolygon(zone.area, dx, dy);
        }
    }
    for (auto& connection : layout.connections) {
        if (selectedConnectionIds_.contains(QString::fromStdString(connection.id))) {
            translateLine(connection.centerSpan, dx, dy);
        }
    }
    for (auto& barrier : layout.barriers) {
        const auto barrierId = QString::fromStdString(barrier.id);
        if (selectedBarrierIds_.contains(barrierId) || selectionMoveAttachedBarrierIds_.contains(barrierId)) {
            translatePolyline(barrier.geometry, dx, dy);
        }
    }
}

safecrowd::domain::Point2D LayoutPreviewWidget::selectionMoveDeltaForPosition(const QPointF& position) const {
    const LayoutTransform transform(selectionMoveBounds_, selectionMoveViewport_, selectionMoveZoom_, selectionMovePanOffset_);
    const auto world = transform.unmap(position);
    const safecrowd::domain::Point2D rawDelta{
        .x = world.x - selectionMoveStartWorld_.x(),
        .y = world.y - selectionMoveStartWorld_.y(),
    };

    if ((QGuiApplication::keyboardModifiers() & Qt::AltModifier) || selectionMoveAnchors_.empty()) {
        return rawDelta;
    }

    LayoutSnapOptions snapOptions;
    snapOptions.snapGrid = gridSnapEnabled_;
    snapOptions.gridSpacingMeters = gridSpacingMeters_;
    const auto snapped = snapLayoutSelectionDrag(
        selectionMoveSnapTargetLayout_,
        currentFloorId().toStdString(),
        selectionMoveAnchors_,
        rawDelta,
        transform,
        snapOptions);
    return snapped.delta;
}

safecrowd::domain::FacilityLayout2D LayoutPreviewWidget::selectionMoveSnapTargetLayout(
    const safecrowd::domain::FacilityLayout2D& layout) const {
    auto snapTarget = layout;

    auto& zones = snapTarget.zones;
    zones.erase(std::remove_if(zones.begin(), zones.end(), [&](const auto& zone) {
        return selectedZoneIds_.contains(QString::fromStdString(zone.id));
    }), zones.end());

    auto& connections = snapTarget.connections;
    connections.erase(std::remove_if(connections.begin(), connections.end(), [&](const auto& connection) {
        return selectedConnectionIds_.contains(QString::fromStdString(connection.id));
    }), connections.end());

    auto& barriers = snapTarget.barriers;
    barriers.erase(std::remove_if(barriers.begin(), barriers.end(), [&](const auto& barrier) {
        const auto barrierId = QString::fromStdString(barrier.id);
        return selectedBarrierIds_.contains(barrierId) || selectionMoveAttachedBarrierIds_.contains(barrierId);
    }), barriers.end());

    return snapTarget;
}

std::vector<safecrowd::domain::Point2D> LayoutPreviewWidget::selectionMoveAnchors(
    const safecrowd::domain::FacilityLayout2D& layout) const {
    std::vector<safecrowd::domain::Point2D> anchors;

    for (const auto& zone : layout.zones) {
        if (!selectedZoneIds_.contains(QString::fromStdString(zone.id))) {
            continue;
        }
        anchors.insert(anchors.end(), zone.area.outline.begin(), zone.area.outline.end());
        for (const auto& hole : zone.area.holes) {
            anchors.insert(anchors.end(), hole.begin(), hole.end());
        }
    }

    for (const auto& connection : layout.connections) {
        if (!selectedConnectionIds_.contains(QString::fromStdString(connection.id))) {
            continue;
        }
        anchors.push_back(connection.centerSpan.start);
        anchors.push_back(connection.centerSpan.end);
    }

    for (const auto& barrier : layout.barriers) {
        const auto barrierId = QString::fromStdString(barrier.id);
        if (!selectedBarrierIds_.contains(barrierId) && !selectionMoveAttachedBarrierIds_.contains(barrierId)) {
            continue;
        }
        anchors.insert(anchors.end(), barrier.geometry.vertices.begin(), barrier.geometry.vertices.end());
    }

    return anchors;
}

QStringList LayoutPreviewWidget::splitZoneBoundaryBarriersForSelection(safecrowd::domain::FacilityLayout2D& layout) const {
    struct BoundaryEdge {
        std::string floorId{};
        bool vertical{false};
        double fixedCoordinate{0.0};
        double start{0.0};
        double end{0.0};
    };

    std::vector<BoundaryEdge> edges;
    const auto appendRingEdges = [&](const std::vector<safecrowd::domain::Point2D>& ring, const std::string& floorId) {
        if (ring.size() < 2) {
            return;
        }
        for (std::size_t index = 0; index < ring.size(); ++index) {
            const auto& a = ring[index];
            const auto& b = ring[(index + 1) % ring.size()];
            const bool vertical = nearlyEqual(a.x, b.x);
            const bool horizontal = nearlyEqual(a.y, b.y);
            if (!vertical && !horizontal) {
                continue;
            }
            edges.push_back({
                .floorId = floorId,
                .vertical = vertical,
                .fixedCoordinate = vertical ? a.x : a.y,
                .start = vertical ? std::min(a.y, b.y) : std::min(a.x, b.x),
                .end = vertical ? std::max(a.y, b.y) : std::max(a.x, b.x),
            });
        }
    };

    for (const auto& zone : layout.zones) {
        if (!selectedZoneIds_.contains(QString::fromStdString(zone.id))) {
            continue;
        }
        appendRingEdges(zone.area.outline, zone.floorId);
        for (const auto& hole : zone.area.holes) {
            appendRingEdges(hole, zone.floorId);
        }
    }

    QStringList attachedBarrierIds;
    if (edges.empty()) {
        return attachedBarrierIds;
    }

    for (std::size_t reverseIndex = layout.barriers.size(); reverseIndex > 0; --reverseIndex) {
        const auto barrierIndex = reverseIndex - 1;
        const auto barrier = layout.barriers[barrierIndex];
        const auto barrierId = QString::fromStdString(barrier.id);
        if (selectedBarrierIds_.contains(barrierId)
            || barrier.geometry.closed
            || barrier.geometry.vertices.size() != 2) {
            continue;
        }

        const auto& a = barrier.geometry.vertices[0];
        const auto& b = barrier.geometry.vertices[1];
        const bool vertical = nearlyEqual(a.x, b.x);
        const bool horizontal = nearlyEqual(a.y, b.y);
        if (!vertical && !horizontal) {
            continue;
        }

        const auto fixedCoordinate = vertical ? a.x : a.y;
        const auto barrierStart = vertical ? std::min(a.y, b.y) : std::min(a.x, b.x);
        const auto barrierEnd = vertical ? std::max(a.y, b.y) : std::max(a.x, b.x);
        std::vector<double> cuts{barrierStart, barrierEnd};
        std::vector<std::pair<double, double>> attachedIntervals;

        for (const auto& edge : edges) {
            if (edge.vertical != vertical
                || !nearlyEqual(edge.fixedCoordinate, fixedCoordinate)
                || !matchesFloor(barrier.floorId, QString::fromStdString(edge.floorId))) {
                continue;
            }

            const auto overlapStart = std::max(barrierStart, edge.start);
            const auto overlapEnd = std::min(barrierEnd, edge.end);
            if (overlapEnd <= overlapStart + kGeometryEpsilon) {
                continue;
            }
            cuts.push_back(overlapStart);
            cuts.push_back(overlapEnd);
            attachedIntervals.emplace_back(overlapStart, overlapEnd);
        }

        if (attachedIntervals.empty()) {
            continue;
        }

        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end(), [](double lhs, double rhs) {
            return nearlyEqual(lhs, rhs);
        }), cuts.end());

        layout.barriers.erase(layout.barriers.begin() + static_cast<std::ptrdiff_t>(barrierIndex));
        auto insertAt = layout.barriers.begin() + static_cast<std::ptrdiff_t>(barrierIndex);
        bool originalIdUsed = false;
        for (std::size_t cutIndex = 1; cutIndex < cuts.size(); ++cutIndex) {
            const auto start = cuts[cutIndex - 1];
            const auto end = cuts[cutIndex];
            if (end <= start + kGeometryEpsilon) {
                continue;
            }

            const auto middle = (start + end) * 0.5;
            const bool attached = std::any_of(attachedIntervals.begin(), attachedIntervals.end(), [&](const auto& interval) {
                return middle >= interval.first - kGeometryEpsilon && middle <= interval.second + kGeometryEpsilon;
            });

            auto segment = barrier;
            segment.id = (!originalIdUsed && attached) ? barrier.id : nextWallId(layout).toStdString();
            originalIdUsed = originalIdUsed || attached;
            segment.geometry.vertices = vertical
                ? std::vector<safecrowd::domain::Point2D>{{.x = fixedCoordinate, .y = start}, {.x = fixedCoordinate, .y = end}}
                : std::vector<safecrowd::domain::Point2D>{{.x = start, .y = fixedCoordinate}, {.x = end, .y = fixedCoordinate}};
            segment.geometry.closed = false;
            if (attached) {
                attachedBarrierIds.append(QString::fromStdString(segment.id));
            }
            insertAt = layout.barriers.insert(insertAt, std::move(segment));
            ++insertAt;
        }
    }

    attachedBarrierIds.removeDuplicates();
    return attachedBarrierIds;
}

QStringList LayoutPreviewWidget::zoneBoundaryBarrierIdsForSelection(const safecrowd::domain::FacilityLayout2D& layout) const {
    QStringList barrierIds;
    if (selectedZoneIds_.isEmpty()) {
        return barrierIds;
    }

    for (const auto& barrier : layout.barriers) {
        const auto barrierId = QString::fromStdString(barrier.id);
        if (selectedBarrierIds_.contains(barrierId)) {
            continue;
        }

        const bool matchesSelectedZone = std::any_of(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
            return selectedZoneIds_.contains(QString::fromStdString(zone.id))
                && barrierMatchesZoneBoundary(barrier, zone);
        });
        if (matchesSelectedZone) {
            barrierIds.append(barrierId);
        }
    }

    return barrierIds;
}

void LayoutPreviewWidget::updateHoverCursor(const QPointF& position) {
    if (toolMode_ != ToolMode::Select || selectionMoveDragging_ || selectionDragging_ || drafting_ || !hasSelection()) {
        unsetCursor();
        return;
    }

    const auto bounds = collectBounds(importResult_, currentFloorId());
    if (!bounds.has_value() || !importResult_.layout.has_value()) {
        unsetCursor();
        return;
    }

    const LayoutTransform transform(*bounds, previewViewport(rect()), camera_.zoom(), camera_.panOffset());
    const bool overSelection = selectedElementContainsContextPoint(
        *importResult_.layout,
        position,
        transform,
        currentFloorId(),
        selectedZoneIds_,
        selectedConnectionIds_,
        selectedBarrierIds_);
    if (overSelection) {
        setCursor(Qt::SizeAllCursor);
    } else {
        unsetCursor();
    }
}

QPointF LayoutPreviewWidget::snapDragWorldPoint(
    const QPointF& anchorWorldPoint,
    const QPointF& worldPoint,
    const LayoutCanvasTransform& transform) const {
    if (!importResult_.layout.has_value()) {
        return worldPoint;
    }

    const auto snapped = snapLayoutDragPoint(
        *importResult_.layout,
        currentFloorId().toStdString(),
        {.x = anchorWorldPoint.x(), .y = anchorWorldPoint.y()},
        {.x = worldPoint.x(), .y = worldPoint.y()},
        transform,
        LayoutSnapOptions{
            .snapGrid = gridSnapEnabled_,
            .gridSpacingMeters = gridSpacingMeters_,
        });
    return QPointF(snapped.point.x, snapped.point.y);
}

QPointF LayoutPreviewWidget::snapWorldPoint(const QPointF& worldPoint, const LayoutCanvasTransform& transform) const {
    if (!importResult_.layout.has_value()) {
        return worldPoint;
    }

    const auto snapped = snapLayoutPoint(
        *importResult_.layout,
        currentFloorId().toStdString(),
        {.x = worldPoint.x(), .y = worldPoint.y()},
        transform,
        LayoutSnapOptions{
            .snapGrid = gridSnapEnabled_,
            .gridSpacingMeters = gridSpacingMeters_,
        });
    return QPointF(snapped.point.x, snapped.point.y);
}

bool LayoutPreviewWidget::hasSelection() const {
    return !selectedZoneIds_.isEmpty() || !selectedConnectionIds_.isEmpty() || !selectedBarrierIds_.isEmpty();
}

bool LayoutPreviewWidget::isSelected(PreviewSelectionKind kind, const QString& id) const {
    switch (kind) {
    case PreviewSelectionKind::Zone:
        return selectedZoneIds_.contains(id);
    case PreviewSelectionKind::Connection:
        return selectedConnectionIds_.contains(id);
    case PreviewSelectionKind::Barrier:
        return selectedBarrierIds_.contains(id);
    case PreviewSelectionKind::None:
    case PreviewSelectionKind::Multiple:
        return false;
    }
    return false;
}

void LayoutPreviewWidget::pruneSelection() {
    if (!importResult_.layout.has_value()) {
        clearSelection();
        return;
    }

    const auto& layout = *importResult_.layout;
    selectedZoneIds_.erase(std::remove_if(selectedZoneIds_.begin(), selectedZoneIds_.end(), [&](const auto& id) {
        return !containsZone(layout, id);
    }), selectedZoneIds_.end());
    selectedConnectionIds_.erase(std::remove_if(selectedConnectionIds_.begin(), selectedConnectionIds_.end(), [&](const auto& id) {
        return !containsConnection(layout, id);
    }), selectedConnectionIds_.end());
    selectedBarrierIds_.erase(std::remove_if(selectedBarrierIds_.begin(), selectedBarrierIds_.end(), [&](const auto& id) {
        return !containsBarrier(layout, id);
    }), selectedBarrierIds_.end());
    selectPrimaryFromLists();
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
    selectedBarrierIds_ = QStringList{barrierId};
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
    focusedTargetId_ = barrierId;
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::selectConnection(const QString& connectionId) {
    selectedConnectionId_ = connectionId;
    selectedConnectionIds_ = QStringList{connectionId};
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
    focusedTargetId_ = connectionId;
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::selectElementsInRect(const QRectF& screenRect, const LayoutCanvasTransform& transform) {
    if (!importResult_.layout.has_value() || screenRect.isEmpty()) {
        clearSelection();
        return;
    }

    selectedZoneIds_.clear();
    selectedConnectionIds_.clear();
    selectedBarrierIds_.clear();

    const auto& layout = *importResult_.layout;
    const auto floorId = currentFloorId();
    for (const auto& zone : layout.zones) {
        if (!matchesFloor(zone.floorId, floorId)) {
            continue;
        }
        const auto path = polygonPath(zone.area, transform);
        if (path.intersects(screenRect) || screenRect.contains(path.boundingRect())) {
            selectedZoneIds_.append(QString::fromStdString(zone.id));
        }
    }
    for (const auto& connection : layout.connections) {
        if (!connectionVisibleOnFloor(layout, connection, floorId)) {
            continue;
        }
        if (strokedPathIntersectsRect(linePath(connection.centerSpan, transform), screenRect, kSelectionStrokeWidthPixels)) {
            selectedConnectionIds_.append(QString::fromStdString(connection.id));
        }
    }
    for (const auto& barrier : layout.barriers) {
        if (!matchesFloor(barrier.floorId, floorId)) {
            continue;
        }
        if (strokedPathIntersectsRect(polylinePainterPath(barrier.geometry, transform), screenRect, kSelectionStrokeWidthPixels)) {
            selectedBarrierIds_.append(QString::fromStdString(barrier.id));
        }
    }

    focusedTargetId_.clear();
    selectPrimaryFromLists();
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

void LayoutPreviewWidget::selectPrimaryFromLists() {
    selectedZoneId_ = selectedZoneIds_.isEmpty() ? QString{} : selectedZoneIds_.front();
    selectedConnectionId_ = selectedConnectionIds_.isEmpty() ? QString{} : selectedConnectionIds_.front();
    selectedBarrierId_ = selectedBarrierIds_.isEmpty() ? QString{} : selectedBarrierIds_.front();

    if (!selectedZoneId_.isEmpty()) {
        focusedTargetId_ = selectedZoneId_;
    } else if (!selectedConnectionId_.isEmpty()) {
        focusedTargetId_ = selectedConnectionId_;
    } else if (!selectedBarrierId_.isEmpty()) {
        focusedTargetId_ = selectedBarrierId_;
    } else {
        focusedTargetId_.clear();
    }
}

void LayoutPreviewWidget::selectSingleAt(const QPointF& position, const LayoutCanvasTransform& transform) {
    if (!importResult_.layout.has_value()) {
        clearSelection();
        return;
    }

    const auto floorId = currentFloorId();
    const auto zoneId = hitTestZone(*importResult_.layout, position, transform, floorId);
    const auto connectionId = hitTestConnection(*importResult_.layout, position, transform, floorId);
    auto barrierId = hitTestBarrier(*importResult_.layout, position, transform, floorId);
    if (!barrierId.has_value()) {
        barrierId = hitTestCanonicalWallBarrier(importResult_, position, transform, floorId);
    }
    if (connectionId.has_value()) {
        selectConnection(*connectionId);
    } else if (barrierId.has_value()) {
        selectBarrier(*barrierId);
    } else if (zoneId.has_value()) {
        selectZone(*zoneId);
    } else {
        clearSelection();
    }
}

void LayoutPreviewWidget::selectZone(const QString& zoneId) {
    selectedZoneId_ = zoneId;
    selectedZoneIds_ = QStringList{zoneId};
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
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

bool LayoutPreviewWidget::switchFloorByWheel(QWheelEvent* event) {
    if (event == nullptr
        || !(event->modifiers() & Qt::ControlModifier)
        || !importResult_.layout.has_value()
        || importResult_.layout->floors.size() <= 1) {
        return false;
    }

    const auto delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->pixelDelta().y();
    if (delta == 0) {
        return false;
    }

    auto& layout = *importResult_.layout;
    int currentIndex = 0;
    const auto activeFloorId = currentFloorId();
    for (std::size_t index = 0; index < layout.floors.size(); ++index) {
        if (QString::fromStdString(layout.floors[index].id) == activeFloorId) {
            currentIndex = static_cast<int>(index);
            break;
        }
    }

    const auto nextIndex = std::clamp(
        currentIndex + (delta > 0 ? 1 : -1),
        0,
        static_cast<int>(layout.floors.size() - 1));
    const auto nextFloorId = QString::fromStdString(layout.floors[static_cast<std::size_t>(nextIndex)].id);
    if (!nextFloorId.isEmpty() && nextFloorId != currentFloorId_) {
        currentFloorId_ = nextFloorId;
        clearSelection();
        refreshFloorSelector();
        camera_.reset();
        update();
    }

    event->accept();
    return true;
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
    if (toolMode_ != mode) {
        drafting_ = false;
        polygonDraftPoints_.clear();
    }
    toolMode_ = mode;
    if (toolMode_ != ToolMode::Select) {
        unsetCursor();
    }
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
    if (obstructionToolButton_ != nullptr) {
        obstructionToolButton_->setChecked(toolMode_ == ToolMode::DrawObstruction);
    }
    if (doorToolButton_ != nullptr) {
        doorToolButton_->setChecked(toolMode_ == ToolMode::DrawDoor);
    }
    if (stairToolButton_ != nullptr) {
        stairToolButton_->setChecked(toolMode_ == ToolMode::DrawStair);
    }
    if (uStairToolButton_ != nullptr) {
        uStairToolButton_->setChecked(toolMode_ == ToolMode::DrawUStair);
    }

    refreshPropertyPanel();
    update();
}

void LayoutPreviewWidget::showSelectionContextMenu(const QPoint& globalPosition) {
    QMenu menu(this);
    auto* deleteAction = menu.addAction("Delete");
    deleteAction->setEnabled(hasSelection());
    const auto* selectedAction = menu.exec(globalPosition);
    if (selectedAction == deleteAction) {
        deleteSelectedElements();
    }
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

    roomAutoWallsCheckBox_ = new QCheckBox("Auto-create boundary walls", propertyPanel_);
    roomAutoWallsCheckBox_->setChecked(roomAutoWallsEnabled_);
    propertyLayout->addWidget(roomAutoWallsCheckBox_);

    shapeDrawModeComboBox_ = new QComboBox(propertyPanel_);
    shapeDrawModeComboBox_->setMinimumWidth(120);
    shapeDrawModeComboBox_->setToolTip("Drawing mode");
    shapeDrawModeComboBox_->addItem("Rectangle", static_cast<int>(ShapeDrawMode::Rectangle));
    shapeDrawModeComboBox_->addItem("Polygon", static_cast<int>(ShapeDrawMode::Polygon));
    propertyLayout->addWidget(shapeDrawModeComboBox_);

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

    stairEntryLabel_ = new QLabel("Entry", propertyPanel_);
    propertyLayout->addWidget(stairEntryLabel_);
    stairEntryComboBox_ = new QComboBox(propertyPanel_);
    stairEntryComboBox_->setMinimumWidth(118);
    stairEntryComboBox_->setToolTip("Entry side on the current floor");
    populateStairEntryCombo(*stairEntryComboBox_, stairEntryDirection_);
    propertyLayout->addWidget(stairEntryComboBox_);

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
    gridToolButton_ = makeButton(topToolbar_, topLayout, makeToolIcon("grid", QColor("#1f5fae")), "Grid snap");
    gridToolButton_->setChecked(gridSnapEnabled_);
    gridSpacingComboBox_ = new QComboBox(topToolbar_);
    gridSpacingComboBox_->setMinimumWidth(86);
    gridSpacingComboBox_->setToolTip("Grid spacing");
    gridSpacingComboBox_->addItem("0.1 m", 0.1);
    gridSpacingComboBox_->addItem("0.5 m", 0.5);
    gridSpacingComboBox_->addItem("1.0 m", 1.0);
    gridSpacingComboBox_->setCurrentIndex(1);
    gridSpacingComboBox_->setEnabled(gridSnapEnabled_);
    topLayout->addWidget(gridSpacingComboBox_);
    topLayout->addStretch(1);

    roomToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("room", QColor("#2f5d8a")), "Draw Room");
    exitToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("exit", QColor("#2d8f5b")), "Draw Exit");
    wallToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("wall", QColor("#4f5d6b")), "Draw Wall");
    obstructionToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("obstruction", QColor("#6c4f38")), "Draw Obstruction");
    doorToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("door", QColor("#8e6b23")), "Draw Door");
    stairToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("stair", QColor("#6a5d9f")), "Draw Stair/Ramp");
    uStairToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("u-stair", QColor("#4f46a5")), "Draw U-shaped Stair");
    sideLayout->addStretch(1);

    connect(selectToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::Select); });
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
    connect(gridToolButton_, &QToolButton::toggled, this, [this](bool checked) {
        gridSnapEnabled_ = checked;
        if (gridSpacingComboBox_ != nullptr) {
            gridSpacingComboBox_->setEnabled(checked);
        }
        update();
    });
    connect(gridSpacingComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0 || gridSpacingComboBox_ == nullptr) {
            return;
        }
        gridSpacingMeters_ = gridSpacingComboBox_->itemData(index).toDouble();
        update();
    });
    connect(roomToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawRoom); });
    connect(exitToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawExit); });
    connect(wallToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawWall); });
    connect(obstructionToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawObstruction); });
    connect(doorToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawDoor); });
    connect(stairToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawStair); });
    connect(uStairToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawUStair); });
    connect(roomAutoWallsCheckBox_, &QCheckBox::toggled, this, [this](bool checked) {
        roomAutoWallsEnabled_ = checked;
    });
    connect(shapeDrawModeComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0 || shapeDrawModeComboBox_ == nullptr) {
            return;
        }
        shapeDrawMode_ = static_cast<ShapeDrawMode>(shapeDrawModeComboBox_->itemData(index).toInt());
        drafting_ = false;
        polygonDraftPoints_.clear();
        update();
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
    connect(stairEntryComboBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0 && stairEntryComboBox_ != nullptr) {
            stairEntryDirection_ = static_cast<safecrowd::domain::StairEntryDirection>(
                stairEntryComboBox_->itemData(index).toInt());
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
        || shapeDrawModeComboBox_ == nullptr
        || doorWidthSpinBox_ == nullptr
        || doorLeafCheckBox_ == nullptr
        || verticalTargetFloorComboBox_ == nullptr
        || stairEntryLabel_ == nullptr
        || stairEntryComboBox_ == nullptr
        || rampLinkCheckBox_ == nullptr) {
        return;
    }

    const bool showRoomWallsOption = toolMode_ == ToolMode::DrawRoom;
    const bool showShapeOptions = toolMode_ == ToolMode::DrawRoom || toolMode_ == ToolMode::DrawObstruction;
    const bool showDoorOptions = toolMode_ == ToolMode::DrawDoor;
    const bool showVerticalOptions = toolMode_ == ToolMode::DrawStair || toolMode_ == ToolMode::DrawUStair;
    const bool showRampOption = toolMode_ == ToolMode::DrawStair;
    roomAutoWallsCheckBox_->setVisible(showRoomWallsOption);
    shapeDrawModeComboBox_->setVisible(showShapeOptions);
    doorWidthSpinBox_->setVisible(showDoorOptions);
    doorLeafCheckBox_->setVisible(showDoorOptions);
    verticalTargetFloorComboBox_->setVisible(showVerticalOptions);
    stairEntryLabel_->setVisible(showVerticalOptions);
    stairEntryComboBox_->setVisible(showVerticalOptions);
    rampLinkCheckBox_->setVisible(showRampOption);
    propertyPanel_->setVisible(importResult_.layout.has_value() && (showShapeOptions || showDoorOptions || showVerticalOptions));
}

PreviewSelection LayoutPreviewWidget::currentSelection() const {
    PreviewSelection selection;

    const int selectedCount = selectedZoneIds_.size() + selectedConnectionIds_.size() + selectedBarrierIds_.size();
    if (selectedCount > 1) {
        selection.kind = PreviewSelectionKind::Multiple;
        selection.id = "multiple";
        selection.title = QString("%1 elements selected").arg(selectedCount);
        selection.detail = QString("%1 rooms/exits/stairs, %2 openings/doors, %3 walls/obstructions selected. Right-click the selection to delete.")
            .arg(selectedZoneIds_.size())
            .arg(selectedConnectionIds_.size())
            .arg(selectedBarrierIds_.size());
        return selection;
    }

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
    selection.detail = "Use the top and left toolbars to select, draw rooms, exits, walls, obstructions, and doors.";
    return selection;
}

}  // namespace safecrowd::application


