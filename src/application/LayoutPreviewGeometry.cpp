#include "application/LayoutPreviewGeometry.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace safecrowd::application {

constexpr double kConnectionWidth = 1.2;
constexpr double kDraftMinimumSize = 0.2;
constexpr double kGeometryEpsilon = 1e-4;
constexpr double kMinimumDoorWidth = 0.9;

using Bounds2D = LayoutCanvasBounds;
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

void includePolygon(Bounds2D& bounds, const safecrowd::domain::Polygon2D& polygon) {
    includeLayoutCanvasPolygon(bounds, polygon);
}

Bounds2D polygonBounds(const safecrowd::domain::Polygon2D& polygon) {
    Bounds2D bounds;
    includePolygon(bounds, polygon);
    return bounds;
}

bool nearlyEqual(double a, double b, double epsilon);

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

bool sameZonePair(
    const safecrowd::domain::Connection2D& connection,
    const QString& fromZoneId,
    const QString& toZoneId) {
    const auto from = QString::fromStdString(connection.fromZoneId);
    const auto to = QString::fromStdString(connection.toZoneId);
    return (from == fromZoneId && to == toZoneId) || (from == toZoneId && to == fromZoneId);
}

struct AxisAlignedSpan {
    bool vertical{false};
    double fixedCoordinate{0.0};
    double start{0.0};
    double end{0.0};
};

std::optional<AxisAlignedSpan> axisAlignedSpan(const safecrowd::domain::LineSegment2D& span) {
    const bool vertical = nearlyEqual(span.start.x, span.end.x, kGeometryEpsilon);
    const bool horizontal = nearlyEqual(span.start.y, span.end.y, kGeometryEpsilon);
    if (!vertical && !horizontal) {
        return std::nullopt;
    }

    if (vertical) {
        return AxisAlignedSpan{
            .vertical = true,
            .fixedCoordinate = span.start.x,
            .start = std::min(span.start.y, span.end.y),
            .end = std::max(span.start.y, span.end.y),
        };
    }

    return AxisAlignedSpan{
        .vertical = false,
        .fixedCoordinate = span.start.y,
        .start = std::min(span.start.x, span.end.x),
        .end = std::max(span.start.x, span.end.x),
    };
}

bool intervalsTouchOrOverlap(double firstStart, double firstEnd, double secondStart, double secondEnd, double tolerance) {
    return std::max(firstStart, secondStart) <= std::min(firstEnd, secondEnd) + tolerance;
}

safecrowd::domain::LineSegment2D spanFromAxisAlignedSpan(const AxisAlignedSpan& span) {
    if (span.vertical) {
        return {
            .start = {.x = span.fixedCoordinate, .y = span.start},
            .end = {.x = span.fixedCoordinate, .y = span.end},
        };
    }

    return {
        .start = {.x = span.start, .y = span.fixedCoordinate},
        .end = {.x = span.end, .y = span.fixedCoordinate},
    };
}

constexpr double kDoorMergeEndpointToleranceMeters = 0.05;

bool connectionCanMergeDoorSpan(
    const safecrowd::domain::Connection2D& connection,
    const QString& floorId,
    safecrowd::domain::ConnectionKind kind,
    const QString& fromZoneId,
    const QString& toZoneId,
    const safecrowd::domain::LineSegment2D& span) {
    if (!matchesFloor(connection.floorId, floorId)
        || connection.kind != kind
        || connection.isStair
        || connection.isRamp
        || !sameZonePair(connection, fromZoneId, toZoneId)) {
        return false;
    }

    const auto existing = axisAlignedSpan(connection.centerSpan);
    const auto candidate = axisAlignedSpan(span);
    if (!existing.has_value()
        || !candidate.has_value()
        || existing->vertical != candidate->vertical
        || !nearlyEqual(existing->fixedCoordinate, candidate->fixedCoordinate, kGeometryEpsilon)) {
        return false;
    }

    return intervalsTouchOrOverlap(
        existing->start,
        existing->end,
        candidate->start,
        candidate->end,
        kDoorMergeEndpointToleranceMeters);
}

std::vector<std::size_t> mergeableDoorConnectionIndices(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& floorId,
    safecrowd::domain::ConnectionKind kind,
    const QString& fromZoneId,
    const QString& toZoneId,
    const safecrowd::domain::LineSegment2D& span) {
    std::vector<std::size_t> indices;
    for (std::size_t index = 0; index < layout.connections.size(); ++index) {
        if (connectionCanMergeDoorSpan(layout.connections[index], floorId, kind, fromZoneId, toZoneId, span)) {
            indices.push_back(index);
        }
    }
    return indices;
}

void retargetControls(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& oldTargetId,
    const QString& newTargetId) {
    if (oldTargetId.isEmpty() || newTargetId.isEmpty() || oldTargetId == newTargetId) {
        return;
    }

    for (auto& control : layout.controls) {
        if (QString::fromStdString(control.targetId) == oldTargetId) {
            control.targetId = newTargetId.toStdString();
        }
    }
}

void retargetConnectionsFromZone(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& oldZoneId,
    const QString& newZoneId) {
    if (oldZoneId.isEmpty() || newZoneId.isEmpty() || oldZoneId == newZoneId) {
        return;
    }

    for (auto& connection : layout.connections) {
        if (QString::fromStdString(connection.fromZoneId) == oldZoneId) {
            connection.fromZoneId = newZoneId.toStdString();
        }
        if (QString::fromStdString(connection.toZoneId) == oldZoneId) {
            connection.toZoneId = newZoneId.toStdString();
        }
    }
    retargetControls(layout, oldZoneId, newZoneId);
}

std::optional<QString> mergeDoorConnectionSpan(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& floorId,
    safecrowd::domain::ConnectionKind kind,
    const QString& fromZoneId,
    const QString& toZoneId,
    const safecrowd::domain::LineSegment2D& span) {
    auto indices = mergeableDoorConnectionIndices(layout, floorId, kind, fromZoneId, toZoneId, span);
    if (indices.empty()) {
        return std::nullopt;
    }

    const auto candidate = axisAlignedSpan(span);
    if (!candidate.has_value()) {
        return std::nullopt;
    }

    const auto targetIndex = indices.front();
    const auto targetId = QString::fromStdString(layout.connections[targetIndex].id);
    AxisAlignedSpan merged = *candidate;
    for (const auto index : indices) {
        const auto existing = axisAlignedSpan(layout.connections[index].centerSpan);
        if (!existing.has_value()) {
            continue;
        }
        merged.start = std::min(merged.start, existing->start);
        merged.end = std::max(merged.end, existing->end);
    }

    layout.connections[targetIndex].centerSpan = spanFromAxisAlignedSpan(merged);
    layout.connections[targetIndex].effectiveWidth = std::hypot(
        layout.connections[targetIndex].centerSpan.end.x - layout.connections[targetIndex].centerSpan.start.x,
        layout.connections[targetIndex].centerSpan.end.y - layout.connections[targetIndex].centerSpan.start.y);

    std::sort(indices.begin(), indices.end(), std::greater<>());
    for (const auto index : indices) {
        if (index == targetIndex || index >= layout.connections.size()) {
            continue;
        }
        const auto removedId = QString::fromStdString(layout.connections[index].id);
        retargetControls(layout, removedId, targetId);
        layout.connections.erase(layout.connections.begin() + static_cast<std::ptrdiff_t>(index));
    }

    return targetId;
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

double distanceToSegment(const QPointF& point, const QPointF& start, const QPointF& end) {
    const auto dx = end.x() - start.x();
    const auto dy = end.y() - start.y();
    const auto lengthSquared = (dx * dx) + (dy * dy);
    if (lengthSquared <= kGeometryEpsilon) {
        return std::hypot(point.x() - start.x(), point.y() - start.y());
    }

    const auto t = std::clamp(
        ((point.x() - start.x()) * dx + (point.y() - start.y()) * dy) / lengthSquared,
        0.0,
        1.0);
    const QPointF projection(start.x() + (t * dx), start.y() + (t * dy));
    return std::hypot(point.x() - projection.x(), point.y() - projection.y());
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
    double tolerance) {
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

double painterFillArea(const QPainterPath& path) {
    double area = 0.0;
    for (const auto& polygon : path.toFillPolygons()) {
        area += std::abs(polygonArea(polygon));
    }
    return area;
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

bool nearlyEqual(double a, double b, double epsilon) {
    return std::abs(a - b) <= epsilon;
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

void normalizeOpenWallBarriers(safecrowd::domain::FacilityLayout2D& layout, const QStringList& preferredIds) {
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
    double tolerance) {
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
    std::optional<std::size_t> preferredOther) {
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

bool ringsShareBoundarySpan(
    const std::vector<safecrowd::domain::Point2D>& first,
    const std::vector<safecrowd::domain::Point2D>& second) {
    if (first.size() < 2 || second.size() < 2) {
        return false;
    }

    for (std::size_t firstIndex = 0; firstIndex < first.size(); ++firstIndex) {
        const safecrowd::domain::LineSegment2D firstEdge{
            .start = first[firstIndex],
            .end = first[(firstIndex + 1) % first.size()],
        };
        const auto firstSpan = axisAlignedSpan(firstEdge);
        if (!firstSpan.has_value()) {
            continue;
        }

        for (std::size_t secondIndex = 0; secondIndex < second.size(); ++secondIndex) {
            const safecrowd::domain::LineSegment2D secondEdge{
                .start = second[secondIndex],
                .end = second[(secondIndex + 1) % second.size()],
            };
            const auto secondSpan = axisAlignedSpan(secondEdge);
            if (!secondSpan.has_value()
                || firstSpan->vertical != secondSpan->vertical
                || !nearlyEqual(firstSpan->fixedCoordinate, secondSpan->fixedCoordinate)) {
                continue;
            }

            const auto overlapStart = std::max(firstSpan->start, secondSpan->start);
            const auto overlapEnd = std::min(firstSpan->end, secondSpan->end);
            if (overlapEnd - overlapStart > kGeometryEpsilon) {
                return true;
            }
        }
    }

    return false;
}

bool polygonsShareBoundarySpan(
    const safecrowd::domain::Polygon2D& first,
    const safecrowd::domain::Polygon2D& second) {
    if (ringsShareBoundarySpan(first.outline, second.outline)) {
        return true;
    }

    for (const auto& hole : first.holes) {
        if (ringsShareBoundarySpan(hole, second.outline)) {
            return true;
        }
    }
    for (const auto& hole : second.holes) {
        if (ringsShareBoundarySpan(first.outline, hole)) {
            return true;
        }
    }
    for (const auto& firstHole : first.holes) {
        for (const auto& secondHole : second.holes) {
            if (ringsShareBoundarySpan(firstHole, secondHole)) {
                return true;
            }
        }
    }

    return false;
}

std::optional<safecrowd::domain::Polygon2D> largestPolygonFromPath(const QPainterPath& path) {
    auto polygons = polygonsFromFillPath(path.simplified());
    if (polygons.empty()) {
        return std::nullopt;
    }

    return *std::max_element(polygons.begin(), polygons.end(), [](const auto& lhs, const auto& rhs) {
        return painterFillArea(worldPolygonPath(lhs)) < painterFillArea(worldPolygonPath(rhs));
    });
}

bool ringIsAxisAligned(const std::vector<safecrowd::domain::Point2D>& ring) {
    if (ring.size() < 2) {
        return false;
    }

    for (std::size_t index = 0; index < ring.size(); ++index) {
        const auto& a = ring[index];
        const auto& b = ring[(index + 1) % ring.size()];
        if (!nearlyEqual(a.x, b.x) && !nearlyEqual(a.y, b.y)) {
            return false;
        }
    }
    return true;
}

bool polygonIsAxisAligned(const safecrowd::domain::Polygon2D& polygon) {
    if (!ringIsAxisAligned(polygon.outline)) {
        return false;
    }

    return std::all_of(polygon.holes.begin(), polygon.holes.end(), ringIsAxisAligned);
}

safecrowd::domain::Polygon2D rectanglePolygonForBounds(const Bounds2D& bounds) {
    return {
        .outline = {
            {.x = bounds.minX, .y = bounds.minY},
            {.x = bounds.maxX, .y = bounds.minY},
            {.x = bounds.maxX, .y = bounds.maxY},
            {.x = bounds.minX, .y = bounds.maxY},
        },
    };
}

std::optional<QString> mergeExitZonePolygon(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Polygon2D& candidatePolygon,
    const QPainterPath& candidatePath,
    const std::string& floorId) {
    const double candidateArea = painterFillArea(candidatePath);
    constexpr double kExitZoneOverlapAreaThreshold = 0.02;
    constexpr double kExitZoneOverlapRatioThreshold = 0.08;

    std::vector<std::size_t> mergeZoneIndices;
    QPainterPath mergedPath = candidatePath;
    Bounds2D mergedBounds = polygonBounds(candidatePolygon);
    bool canRectangularize = polygonIsAxisAligned(candidatePolygon);
    for (std::size_t index = 0; index < layout.zones.size(); ++index) {
        const auto& zone = layout.zones[index];
        if (zone.kind != safecrowd::domain::ZoneKind::Exit || zone.floorId != floorId) {
            continue;
        }
        const auto zonePath = worldPolygonPath(zone.area);
        const QPainterPath overlap = candidatePath.intersected(zonePath);
        const double overlapArea = painterFillArea(overlap);
        const double ratio = candidateArea > kGeometryEpsilon ? overlapArea / candidateArea : 1.0;
        const bool overlapsEnough = overlapArea > kExitZoneOverlapAreaThreshold && ratio >= kExitZoneOverlapRatioThreshold;
        if (overlapsEnough || polygonsShareBoundarySpan(candidatePolygon, zone.area)) {
            mergeZoneIndices.push_back(index);
            mergedPath = mergedPath.united(zonePath);
            includePolygon(mergedBounds, zone.area);
            canRectangularize = canRectangularize && polygonIsAxisAligned(zone.area);
        }
    }

    if (mergeZoneIndices.empty()) {
        return std::nullopt;
    }

    const auto keepIndex = mergeZoneIndices.front();
    const auto keepZoneId = QString::fromStdString(layout.zones[keepIndex].id);
    std::size_t mergedCapacity = 0u;
    for (const auto index : mergeZoneIndices) {
        mergedCapacity += layout.zones[index].defaultCapacity;
    }

    if (canRectangularize && mergedBounds.valid()) {
        layout.zones[keepIndex].area = rectanglePolygonForBounds(mergedBounds);
    } else if (const auto mergedPolygon = largestPolygonFromPath(mergedPath)) {
        layout.zones[keepIndex].area = *mergedPolygon;
    }
    layout.zones[keepIndex].defaultCapacity = std::max<std::size_t>(mergedCapacity, layout.zones[keepIndex].defaultCapacity);

    std::sort(mergeZoneIndices.begin(), mergeZoneIndices.end(), std::greater<>());
    for (const auto index : mergeZoneIndices) {
        if (index == keepIndex || index >= layout.zones.size()) {
            continue;
        }
        const auto removedZoneId = QString::fromStdString(layout.zones[index].id);
        retargetConnectionsFromZone(layout, removedZoneId, keepZoneId);
        layout.zones.erase(layout.zones.begin() + static_cast<std::ptrdiff_t>(index));
    }

    return keepZoneId;
}

QString createExitZoneAtDoor(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& gapStart,
    const safecrowd::domain::Point2D& gapEnd,
    const QPointF& outsideDirection,
    const std::string& floorId) {
    const auto width = std::hypot(gapEnd.x - gapStart.x, gapEnd.y - gapStart.y);
    const auto depth = std::max(0.75, width * 0.6);

    const QPointF a(gapStart.x, gapStart.y);
    const QPointF b(gapEnd.x, gapEnd.y);
    const QPointF offset = outsideDirection * depth;

    const safecrowd::domain::Polygon2D candidatePolygon{
        .outline = {
            {.x = a.x(), .y = a.y()},
            {.x = b.x(), .y = b.y()},
            {.x = b.x() + offset.x(), .y = b.y() + offset.y()},
            {.x = a.x() + offset.x(), .y = a.y() + offset.y()},
        },
    };

    const QPainterPath candidatePath = worldPolygonPath(candidatePolygon);
    if (const auto mergedZoneId = mergeExitZonePolygon(layout, candidatePolygon, candidatePath, floorId)) {
        return *mergedZoneId;
    }

    const auto zoneId = nextZoneId(layout);
    const auto zoneNumber = static_cast<int>(layout.zones.size()) + 1;
    layout.zones.push_back({
        .id = zoneId.toStdString(),
        .floorId = floorId,
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = QString("Exit %1").arg(zoneNumber).toStdString(),
        .area = candidatePolygon,
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

constexpr double kDoorSpanSupportSnapToleranceMeters = 0.35;

std::optional<DoorSpanCorrectionResult> correctedDoorSpanOnAxisAlignedSupport(
    const safecrowd::domain::Point2D& supportStart,
    const safecrowd::domain::Point2D& supportEnd,
    const QString& barrierId,
    const safecrowd::domain::LineSegment2D& rawSpan) {
    const bool supportVertical = nearlyEqual(supportStart.x, supportEnd.x);
    const bool supportHorizontal = nearlyEqual(supportStart.y, supportEnd.y);
    if (!supportVertical && !supportHorizontal) {
        return std::nullopt;
    }

    const QPointF startPoint(rawSpan.start.x, rawSpan.start.y);
    const QPointF endPoint(rawSpan.end.x, rawSpan.end.y);
    const auto projectedStart = projectOntoSegment(startPoint, supportStart, supportEnd);
    const auto projectedEnd = projectOntoSegment(endPoint, supportStart, supportEnd);
    if (!projectedStart.has_value() || !projectedEnd.has_value()) {
        return std::nullopt;
    }

    safecrowd::domain::LineSegment2D corrected{
        .start = {.x = projectedStart->x(), .y = projectedStart->y()},
        .end = {.x = projectedEnd->x(), .y = projectedEnd->y()},
    };
    if (supportVertical) {
        corrected.start.x = supportStart.x;
        corrected.end.x = supportStart.x;
        if (corrected.end.y < corrected.start.y) {
            std::swap(corrected.start, corrected.end);
        }
    } else {
        corrected.start.y = supportStart.y;
        corrected.end.y = supportStart.y;
        if (corrected.end.x < corrected.start.x) {
            std::swap(corrected.start, corrected.end);
        }
    }

    const auto length = std::hypot(corrected.end.x - corrected.start.x, corrected.end.y - corrected.start.y);
    if (length < kMinimumDoorWidth) {
        return std::nullopt;
    }

    return DoorSpanCorrectionResult{
        .span = corrected,
        .barrierId = barrierId,
    };
}

std::optional<DoorSpanCorrectionResult> correctedDoorSpanForPlacement(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& floorId,
    const safecrowd::domain::LineSegment2D& rawSpan) {
    const QPointF midpoint(
        (rawSpan.start.x + rawSpan.end.x) * 0.5,
        (rawSpan.start.y + rawSpan.end.y) * 0.5);

    double bestDistance = std::numeric_limits<double>::max();
    std::optional<DoorSpanCorrectionResult> best;

    const auto considerSupport = [&](const safecrowd::domain::Point2D& a, const safecrowd::domain::Point2D& b, const QString& barrierId) {
        const auto distance = distanceToLineSegmentWorld(midpoint, a, b);
        if (distance > kDoorSpanSupportSnapToleranceMeters || distance >= bestDistance) {
            return;
        }

        const auto candidate = correctedDoorSpanOnAxisAlignedSupport(a, b, barrierId, rawSpan);
        if (!candidate.has_value()) {
            return;
        }

        bestDistance = distance;
        best = *candidate;
    };

    const auto considerExitBoundarySupport = [&](const safecrowd::domain::Point2D& a, const safecrowd::domain::Point2D& b) {
        const auto distance = distanceToLineSegmentWorld(midpoint, a, b);
        if (distance > kDoorSpanSupportSnapToleranceMeters || distance >= bestDistance) {
            return;
        }

        const auto candidate = correctedDoorSpanOnAxisAlignedSupport(a, b, {}, rawSpan);
        if (!candidate.has_value()) {
            return;
        }

        const auto neighbors = doorNeighborsAcrossSegment(layout, candidate->span.start, candidate->span.end, floorId);
        if (neighbors.firstSide.empty() || neighbors.secondSide.empty()) {
            return;
        }

        const auto sideHasExit = [&](const std::vector<std::size_t>& indices) {
            return std::any_of(indices.begin(), indices.end(), [&](std::size_t index) {
                return layout.zones[index].kind == safecrowd::domain::ZoneKind::Exit;
            });
        };
        const auto sideHasNonExit = [&](const std::vector<std::size_t>& indices) {
            return std::any_of(indices.begin(), indices.end(), [&](std::size_t index) {
                return layout.zones[index].kind != safecrowd::domain::ZoneKind::Exit;
            });
        };

        const bool hasExit = sideHasExit(neighbors.firstSide) || sideHasExit(neighbors.secondSide);
        const bool hasNonExit = sideHasNonExit(neighbors.firstSide) || sideHasNonExit(neighbors.secondSide);
        if (!hasExit || !hasNonExit) {
            return;
        }

        bestDistance = distance;
        best = *candidate;
    };

    // Prefer aligning to existing Exit zone boundaries when present.
    for (const auto& zone : layout.zones) {
        if (!matchesFloor(zone.floorId, floorId) || zone.kind != safecrowd::domain::ZoneKind::Exit) {
            continue;
        }
        const auto& outline = zone.area.outline;
        if (outline.size() < 2) {
            continue;
        }
        for (std::size_t i = 0; i < outline.size(); ++i) {
            const auto& a = outline[i];
            const auto& b = outline[(i + 1) % outline.size()];
            if (std::hypot(b.x - a.x, b.y - a.y) <= kGeometryEpsilon) {
                continue;
            }
            considerExitBoundarySupport(a, b);
        }
    }

    // Fallback: align to nearest axis-aligned wall barrier segment.
    for (const auto& barrier : layout.barriers) {
        if (!matchesFloor(barrier.floorId, floorId)
            || barrier.geometry.closed
            || barrier.geometry.vertices.size() != 2) {
            continue;
        }
        const auto& a = barrier.geometry.vertices[0];
        const auto& b = barrier.geometry.vertices[1];
        considerSupport(a, b, QString::fromStdString(barrier.id));
    }

    if (best.has_value()) {
        return best;
    }

    // Last resort: if the user drew a slightly tilted span, coerce it to the dominant axis.
    const auto dx = rawSpan.end.x - rawSpan.start.x;
    const auto dy = rawSpan.end.y - rawSpan.start.y;
    if (std::hypot(dx, dy) < kMinimumDoorWidth) {
        return std::nullopt;
    }

    const bool preferHorizontal = std::abs(dx) >= std::abs(dy);
    safecrowd::domain::LineSegment2D corrected = rawSpan;
    if (preferHorizontal) {
        const auto y = (rawSpan.start.y + rawSpan.end.y) * 0.5;
        corrected.start.y = y;
        corrected.end.y = y;
        if (corrected.end.x < corrected.start.x) {
            std::swap(corrected.start, corrected.end);
        }
    } else {
        const auto x = (rawSpan.start.x + rawSpan.end.x) * 0.5;
        corrected.start.x = x;
        corrected.end.x = x;
        if (corrected.end.y < corrected.start.y) {
            std::swap(corrected.start, corrected.end);
        }
    }

    if (std::hypot(corrected.end.x - corrected.start.x, corrected.end.y - corrected.start.y) < kMinimumDoorWidth) {
        return std::nullopt;
    }

    return DoorSpanCorrectionResult{
        .span = corrected,
        .barrierId = {},
    };
}

}  // namespace safecrowd::application
