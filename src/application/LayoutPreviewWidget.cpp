#include "application/LayoutPreviewWidget.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <utility>

#include "application/LayoutCanvasRendering.h"
#include "application/LayoutCanvasSnapping.h"
#include "application/LayoutPreviewGeometry.h"
#include "application/LayoutPreviewEditing.h"
#include "application/ToolIconResources.h"
#include "application/UiStyle.h"

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
const QColor kExitAccentColor("#2d8f5b");
const QColor kDoorAccentColor("#ff8c00");

QRectF previewViewport(const QRect& widgetRect) {
    return layoutCanvasViewport(widgetRect, kSideToolbarWidth + 16, kTopToolbarHeight + kPropertyPanelHeight + 16, 16, 16);
}

using Bounds2D = LayoutCanvasBounds;
using LayoutTransform = LayoutCanvasTransform;

void includePoint(Bounds2D& bounds, const safecrowd::domain::Point2D& point) {
    includeLayoutCanvasPoint(bounds, point);
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

QString floorDisplayLabel(const safecrowd::domain::Floor2D& floor) {
    const auto label = QString::fromStdString(floor.label);
    const auto id = QString::fromStdString(floor.id);
    if (label.isEmpty() || label == id) {
        return id;
    }
    return QString("%1  -  %2").arg(label, id);
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

bool layoutMatchesTarget(const safecrowd::domain::FacilityLayout2D& layout, const QString& targetId) {
    return !targetId.isEmpty() && QString::fromStdString(layout.id) == targetId;
}

bool zoneMatchesTarget(const safecrowd::domain::Zone2D& zone, const QString& targetId) {
    return !targetId.isEmpty() && (QString::fromStdString(zone.id) == targetId || traceMatches(zone.provenance, targetId));
}

bool connectionMatchesTarget(const safecrowd::domain::Connection2D& connection, const QString& targetId) {
    return !targetId.isEmpty()
        && (QString::fromStdString(connection.id) == targetId || traceMatches(connection.provenance, targetId));
}

bool barrierMatchesTarget(const safecrowd::domain::Barrier2D& barrier, const QString& targetId) {
    return !targetId.isEmpty()
        && (QString::fromStdString(barrier.id) == targetId || traceMatches(barrier.provenance, targetId));
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
        const bool includeWholeLayout = layoutMatchesTarget(*importResult.layout, targetId);
        for (const auto& zone : importResult.layout->zones) {
            if (includeWholeLayout || zoneMatchesTarget(zone, targetId)) {
                includePolygon(bounds, zone.area);
            }
        }
        for (const auto& connection : importResult.layout->connections) {
            if (includeWholeLayout || connectionMatchesTarget(connection, targetId)) {
                includeLine(bounds, connection.centerSpan);
            }
        }
        for (const auto& barrier : importResult.layout->barriers) {
            if (includeWholeLayout || barrierMatchesTarget(barrier, targetId)) {
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

bool containsFloor(const safecrowd::domain::FacilityLayout2D& layout, const QString& floorId) {
    return std::any_of(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
        return QString::fromStdString(floor.id) == floorId;
    });
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

void appendTargetFloorId(QStringList& floorIds, const std::string& floorId) {
    const auto floorIdText = QString::fromStdString(floorId);
    if (!floorIdText.isEmpty() && !floorIds.contains(floorIdText)) {
        floorIds.append(floorIdText);
    }
}

void appendTargetConnectionFloorIds(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    QStringList& floorIds) {
    appendTargetFloorId(floorIds, connection.floorId);

    if (!isVerticalLink(connection)) {
        return;
    }

    const auto* fromZone = findZoneById(layout, connection.fromZoneId);
    const auto* toZone = findZoneById(layout, connection.toZoneId);
    if (fromZone != nullptr) {
        appendTargetFloorId(floorIds, fromZone->floorId);
    }
    if (toZone != nullptr) {
        appendTargetFloorId(floorIds, toZone->floorId);
    }
}

std::optional<QString> floorIdForIssueTarget(
    const safecrowd::domain::ImportResult& importResult,
    const QString& targetId,
    const QString& preferredFloorId) {
    if (targetId.isEmpty() || !importResult.layout.has_value()) {
        return std::nullopt;
    }

    const auto& layout = *importResult.layout;
    for (const auto& floor : layout.floors) {
        if (QString::fromStdString(floor.id) == targetId || traceMatches(floor.provenance, targetId)) {
            return QString::fromStdString(floor.id);
        }
    }

    if (layoutMatchesTarget(layout, targetId)) {
        return preferredFloorId.isEmpty() ? defaultFloorId(layout) : preferredFloorId;
    }

    QStringList matchingFloorIds;
    for (const auto& zone : layout.zones) {
        if (zoneMatchesTarget(zone, targetId)) {
            appendTargetFloorId(matchingFloorIds, zone.floorId);
        }
    }
    for (const auto& connection : layout.connections) {
        if (connectionMatchesTarget(connection, targetId)) {
            appendTargetConnectionFloorIds(layout, connection, matchingFloorIds);
        }
    }
    for (const auto& barrier : layout.barriers) {
        if (barrierMatchesTarget(barrier, targetId)) {
            appendTargetFloorId(matchingFloorIds, barrier.floorId);
        }
    }
    for (const auto& control : layout.controls) {
        if (QString::fromStdString(control.id) == targetId
            || QString::fromStdString(control.targetId) == targetId
            || traceMatches(control.provenance, targetId)) {
            appendTargetFloorId(matchingFloorIds, control.floorId);
        }
    }

    if (matchingFloorIds.contains(preferredFloorId)) {
        return preferredFloorId;
    }
    if (!matchingFloorIds.isEmpty()) {
        return matchingFloorIds.front();
    }

    return std::nullopt;
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

QString floorDetail(const safecrowd::domain::FacilityLayout2D& layout, const QString& floorId) {
    int zoneCount = 0;
    int connectionCount = 0;
    int barrierCount = 0;
    int controlCount = 0;

    for (const auto& zone : layout.zones) {
        if (matchesFloor(zone.floorId, floorId)) {
            ++zoneCount;
        }
    }
    for (const auto& connection : layout.connections) {
        if (connectionVisibleOnFloor(layout, connection, floorId)) {
            ++connectionCount;
        }
    }
    for (const auto& barrier : layout.barriers) {
        if (matchesFloor(barrier.floorId, floorId)) {
            ++barrierCount;
        }
    }
    for (const auto& control : layout.controls) {
        if (matchesFloor(control.floorId, floorId)) {
            ++controlCount;
        }
    }

    return QString("Id: %1\nRooms/exits/stairs: %2\nOpenings/doors: %3\nWalls/obstructions: %4\nControls: %5")
        .arg(floorId)
        .arg(zoneCount)
        .arg(connectionCount)
        .arg(barrierCount)
        .arg(controlCount);
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

double distanceBetweenScreenPoints(const QPointF& lhs, const QPointF& rhs) {
    return std::hypot(lhs.x() - rhs.x(), lhs.y() - rhs.y());
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

QIcon makeToolIcon(const QString& glyph, const QColor& color) {
    const auto resourcePath = layoutToolIconResourcePath(glyph);
    return resourcePath.isEmpty() ? QIcon{} : makeSvgToolIcon(resourcePath, color, QSize(24, 24));
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
            refreshFloorSelector();
            selectFloor(floorId);
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

bool LayoutPreviewWidget::deleteElement(const QString& elementId) {
    if (!importResult_.layout.has_value() || elementId.isEmpty()) {
        return false;
    }

    if (elementId.startsWith("floor:")) {
        const auto floorId = elementId.mid(QString("floor:").size());
        if (floorId.isEmpty()) {
            return false;
        }
        applyEditResult(deleteLayoutPreviewFloor(*importResult_.layout, floorId, currentFloorId()));
        return true;
    }

    if (containsConnection(*importResult_.layout, elementId)) {
        deleteConnection(elementId);
        return true;
    }
    if (containsBarrier(*importResult_.layout, elementId)) {
        deleteBarrier(elementId);
        return true;
    }
    if (containsZone(*importResult_.layout, elementId)) {
        selectedBarrierId_.clear();
        selectedBarrierIds_.clear();
        selectedConnectionId_.clear();
        selectedConnectionIds_.clear();
        selectedZoneId_ = elementId;
        selectedZoneIds_ = QStringList{elementId};
        deleteSelectedElements();
        return true;
    }

    return false;
}

void LayoutPreviewWidget::focusIssueTarget(const QString& targetId) {
    selectedFloorId_.clear();
    selectedZoneId_.clear();
    selectedZoneIds_.clear();
    selectedConnectionId_.clear();
    selectedConnectionIds_.clear();
    selectedBarrierId_.clear();
    selectedBarrierIds_.clear();
    focusedTargetId_ = targetId;

    if (const auto targetFloorId = floorIdForIssueTarget(importResult_, targetId, currentFloorId());
        targetFloorId.has_value() && *targetFloorId != currentFloorId_) {
        currentFloorId_ = *targetFloorId;
        refreshFloorSelector();
    }

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
    case PreviewSelectionKind::Floor:
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

    if (event->key() == Qt::Key_Delete && hasSelection()) {
        deleteSelectedElements();
        event->accept();
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

    if (event->key() == Qt::Key_Escape && toolMode_ != ToolMode::Select) {
        setToolMode(ToolMode::Select);
        event->accept();
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

    if (event->button() == Qt::LeftButton
        && toolMode_ == ToolMode::Select
        && importResult_.layout.has_value()
        && hasSelection()) {
        const auto bounds = collectBounds(importResult_, currentFloorId());
        if (bounds.has_value()) {
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
    if (selectionMoveDragging_ && event->button() == Qt::LeftButton) {
        finishSelectionMove(event->position());
        event->accept();
        return;
    }

    if (camera_.finishPan(event)) {
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

    const auto liveBounds = collectBounds(importResult_, currentFloorId());
    if (!liveBounds.has_value()) {
        painter.setPen(QPen(QColor(80, 80, 80), 1));
        painter.setFont(QFont("Segoe UI", 14, QFont::DemiBold));
        painter.drawText(rect(), Qt::AlignCenter, "No layout geometry imported");
        return;
    }

    const bool freezeCamera = selectionMoveDragging_ && selectionMoveBounds_.valid();
    const auto bounds = freezeCamera ? selectionMoveBounds_ : *liveBounds;
    const QRectF viewport = freezeCamera ? selectionMoveViewport_ : previewViewport(rect());
    const auto zoom = freezeCamera ? selectionMoveZoom_ : camera_.zoom();
    const auto panOffset = freezeCamera ? selectionMovePanOffset_ : camera_.panOffset();
    const LayoutTransform transform(bounds, viewport, zoom, panOffset);

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

        for (const auto& opening : importResult_.canonicalGeometry->openings) {
            const auto openingColor = opening.kind == safecrowd::domain::OpeningKind::Doorway
                ? kDoorAccentColor
                : QColor(66, 156, 96);
            painter.setPen(QPen(openingColor, 3.0, Qt::DashLine));
            drawLine(painter, opening.span, transform);
        }
    }

    const bool hasExplicitSelection = hasSelection();
    if (hasExplicitSelection || !focusedTargetId_.isEmpty()) {
        painter.setBrush(QColor(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 44));
        painter.setPen(QPen(kSelectionHighlightColor, 2.25, Qt::DashLine));

        if (importResult_.layout.has_value()) {
            const bool focusedLayout = !hasExplicitSelection && layoutMatchesTarget(*importResult_.layout, focusedTargetId_);
            for (const auto& zone : importResult_.layout->zones) {
                if (!matchesFloor(zone.floorId, currentFloorId())) {
                    continue;
                }
                const auto id = QString::fromStdString(zone.id);
                const bool selected = selectedZoneIds_.contains(id);
                const bool focused = focusedLayout || (!hasExplicitSelection && zoneMatchesTarget(zone, focusedTargetId_));
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
                const bool focused = focusedLayout || (!hasExplicitSelection && connectionMatchesTarget(connection, focusedTargetId_));
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
                const bool focused = focusedLayout || (!hasExplicitSelection && barrierMatchesTarget(barrier, focusedTargetId_));
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
    painter.drawText(viewport.adjusted(0, -10, 0, 0), Qt::AlignTop | Qt::AlignRight, QString("Zoom %1%").arg(static_cast<int>(zoom * 100.0)));
    painter.drawText(viewport.adjusted(0, -10, 0, 0), Qt::AlignTop | Qt::AlignLeft, "Layout Preview");

}

void LayoutPreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    repositionToolbars();
}

void LayoutPreviewWidget::wheelEvent(QWheelEvent* event) {
    if (selectionMoveDragging_) {
        event->accept();
        return;
    }

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
    selectedFloorId_.clear();
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

LayoutPreviewEditOptions LayoutPreviewWidget::editOptions() const {
    return {
        .currentFloorId = currentFloorId(),
        .targetFloorId = verticalTargetFloorId(),
        .roomAutoWallsEnabled = roomAutoWallsEnabled_,
        .doorCreatesLeaf = doorCreatesLeaf_,
        .verticalLinkCreatesRamp = verticalLinkCreatesRamp_,
        .stairEntryDirection = stairEntryDirection_,
        .doorWidth = doorWidth_,
    };
}

LayoutPreviewSelectionState LayoutPreviewWidget::editSelectionState() const {
    LayoutPreviewSelectionState selection;
    selection.selectedBarrierId = selectedBarrierId_;
    selection.selectedBarrierIds = selectedBarrierIds_;
    selection.selectedConnectionId = selectedConnectionId_;
    selection.selectedConnectionIds = selectedConnectionIds_;
    selection.selectedZoneId = selectedZoneId_;
    selection.selectedZoneIds = selectedZoneIds_;
    selection.focusedTargetId = focusedTargetId_;
    return selection;
}

void LayoutPreviewWidget::applyEditResult(const LayoutPreviewEditResult& result) {
    if (!result.layoutChanged && !result.selectionChanged && !result.floorChanged && !result.floorSelectorChanged) {
        return;
    }

    if (result.selectionChanged) {
        selectedFloorId_.clear();
        selectedBarrierId_ = result.selection.selectedBarrierId;
        selectedBarrierIds_ = result.selection.selectedBarrierIds;
        selectedConnectionId_ = result.selection.selectedConnectionId;
        selectedConnectionIds_ = result.selection.selectedConnectionIds;
        selectedZoneId_ = result.selection.selectedZoneId;
        selectedZoneIds_ = result.selection.selectedZoneIds;
        focusedTargetId_ = result.selection.focusedTargetId;
        selectionDragging_ = false;
        selectionMoveDragging_ = false;
        selectionMoveAttachedBarrierIds_.clear();
        selectionMoveAnchors_.clear();
    }

    if (result.floorChanged) {
        currentFloorId_ = result.currentFloorId;
    }
    if (result.floorSelectorChanged) {
        refreshFloorSelector();
    }
    if (result.layoutChanged) {
        notifyLayoutEdited();
    }

    emitCurrentSelection();
    update();
}


void LayoutPreviewWidget::createRoomPolygon(const std::vector<QPointF>& points) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(createLayoutPreviewRoomPolygon(*importResult_.layout, points, editOptions()));
}


void LayoutPreviewWidget::createZone(const QPointF& startWorld, const QPointF& endWorld, safecrowd::domain::ZoneKind kind) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(createLayoutPreviewZone(*importResult_.layout, startWorld, endWorld, kind, editOptions()));
}


void LayoutPreviewWidget::createWallSegment(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(createLayoutPreviewWallSegment(*importResult_.layout, startWorld, endWorld, editOptions()));
}


void LayoutPreviewWidget::createObstructionPolygon(const std::vector<QPointF>& points) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(createLayoutPreviewObstructionPolygon(*importResult_.layout, points, editOptions()));
}


void LayoutPreviewWidget::createObstructionRectangle(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(createLayoutPreviewObstructionRectangle(*importResult_.layout, startWorld, endWorld, editOptions()));
}


void LayoutPreviewWidget::createConnection(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(createLayoutPreviewConnection(*importResult_.layout, startWorld, endWorld, editOptions()));
}


void LayoutPreviewWidget::createVerticalLink(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(createLayoutPreviewVerticalLink(*importResult_.layout, startWorld, endWorld, editOptions()));
}


void LayoutPreviewWidget::createUShapedStairLink(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(createLayoutPreviewUShapedStairLink(*importResult_.layout, startWorld, endWorld, editOptions()));
}


void LayoutPreviewWidget::createDoorAt(const QString& barrierId, const QPointF& position) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(createLayoutPreviewDoorAt(*importResult_.layout, barrierId, position, editOptions()));
}


bool LayoutPreviewWidget::createDoorSegment(const QPointF& startWorld, const QPointF& endWorld) {
    if (!importResult_.layout.has_value()) {
        return false;
    }

    const auto result = createLayoutPreviewDoorSegment(*importResult_.layout, startWorld, endWorld, editOptions());
    const bool changed = result.layoutChanged;
    applyEditResult(result);
    return changed;
}


bool LayoutPreviewWidget::createDoorSpan(
    const QString& barrierId,
    const safecrowd::domain::Point2D& gapStart,
    const safecrowd::domain::Point2D& gapEnd) {
    if (!importResult_.layout.has_value()) {
        return false;
    }

    const auto result = createLayoutPreviewDoorSpan(*importResult_.layout, barrierId, gapStart, gapEnd, editOptions());
    const bool changed = result.layoutChanged;
    applyEditResult(result);
    return changed;
}


void LayoutPreviewWidget::deleteConnection(const QString& connectionId) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(deleteLayoutPreviewConnection(*importResult_.layout, connectionId, editSelectionState()));
}


void LayoutPreviewWidget::deleteBarrier(const QString& barrierId) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    applyEditResult(deleteLayoutPreviewBarrier(*importResult_.layout, barrierId, editSelectionState()));
}


void LayoutPreviewWidget::deleteSelectedElements() {
    if (!importResult_.layout.has_value() || !hasSelection()) {
        return;
    }

    applyEditResult(deleteSelectedLayoutPreviewElements(*importResult_.layout, editSelectionState()));
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
    case PreviewSelectionKind::Floor:
        return id == QString("floor:%1").arg(selectedFloorId_);
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
    if (!selectedFloorId_.isEmpty() && !containsFloor(layout, selectedFloorId_)) {
        selectedFloorId_.clear();
    }
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
    selectedFloorId_.clear();
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
    selectedFloorId_.clear();
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
    selectedFloorId_.clear();

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

void LayoutPreviewWidget::selectFloor(const QString& floorId) {
    selectedFloorId_ = floorId;
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
    selectedFloorId_.clear();
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

    applyEditResult(addLayoutPreviewFloor(*importResult_.layout));
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
    toolbarCorner_ = new QFrame(this);
    ui::polishCanvasToolbar(toolbarCorner_);

    topToolbar_ = new QFrame(this);
    ui::polishCanvasToolbar(topToolbar_);
    auto* topLayout = new QHBoxLayout(topToolbar_);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    sideToolbar_ = new QFrame(this);
    ui::polishCanvasToolbar(sideToolbar_);
    auto* sideLayout = new QVBoxLayout(sideToolbar_);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    propertyPanel_ = new QFrame(this);
    ui::polishLayoutPreviewPropertyPanel(propertyPanel_);
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
    topLayout->addStretch(1);
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

    roomToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("room", QColor("#2f5d8a")), "Draw Room");
    exitToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("exit", kExitAccentColor), "Draw Exit");
    wallToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("wall", QColor("#4f5d6b")), "Draw Wall");
    obstructionToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("obstruction", QColor("#6c4f38")), "Draw Obstruction");
    doorToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("door", kDoorAccentColor), "Draw Door");
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
    const auto toggleToolMode = [this](ToolMode mode) {
        setToolMode(toolMode_ == mode ? ToolMode::Select : mode);
    };
    connect(roomToolButton_, &QToolButton::clicked, this, [this, toggleToolMode]() { toggleToolMode(ToolMode::DrawRoom); });
    connect(exitToolButton_, &QToolButton::clicked, this, [this, toggleToolMode]() { toggleToolMode(ToolMode::DrawExit); });
    connect(wallToolButton_, &QToolButton::clicked, this, [this, toggleToolMode]() { toggleToolMode(ToolMode::DrawWall); });
    connect(obstructionToolButton_, &QToolButton::clicked, this, [this, toggleToolMode]() { toggleToolMode(ToolMode::DrawObstruction); });
    connect(doorToolButton_, &QToolButton::clicked, this, [this, toggleToolMode]() { toggleToolMode(ToolMode::DrawDoor); });
    connect(stairToolButton_, &QToolButton::clicked, this, [this, toggleToolMode]() { toggleToolMode(ToolMode::DrawStair); });
    connect(uStairToolButton_, &QToolButton::clicked, this, [this, toggleToolMode]() { toggleToolMode(ToolMode::DrawUStair); });
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

    if (importResult_.layout.has_value() && !selectedFloorId_.isEmpty()) {
        const auto& layout = *importResult_.layout;
        selection.kind = PreviewSelectionKind::Floor;
        selection.id = QString("floor:%1").arg(selectedFloorId_);
        selection.title = floorLabelForId(layout, selectedFloorId_);
        selection.detail = floorDetail(layout, selectedFloorId_);
        return selection;
    }

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


