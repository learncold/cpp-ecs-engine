#include "application/LayoutCanvasRendering.h"

#include <algorithm>
#include <cmath>

#include <QColor>
#include <QEvent>
#include <QFont>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

const QColor kDoorStrokeColor("#ff8c00");
const QColor kOpeningStrokeColor("#2f6fb2");

bool matchesFloor(const std::string& elementFloorId, const std::string& floorId) {
    return floorId.empty() || elementFloorId.empty() || elementFloorId == floorId;
}

bool isVerticalConnection(const safecrowd::domain::Connection2D& connection) {
    return connection.kind == safecrowd::domain::ConnectionKind::Stair
        || connection.kind == safecrowd::domain::ConnectionKind::Ramp
        || connection.isStair
        || connection.isRamp;
}

QColor connectionStrokeColor(const safecrowd::domain::Connection2D& connection) {
    if (connection.kind == safecrowd::domain::ConnectionKind::Doorway) {
        return kDoorStrokeColor;
    }
    return kOpeningStrokeColor;
}

const safecrowd::domain::Zone2D* findZone(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

bool isVerticalZone(const safecrowd::domain::Zone2D* zone) {
    return zone != nullptr
        && (zone->kind == safecrowd::domain::ZoneKind::Stair
            || zone->isStair
            || zone->isRamp);
}

bool isStairAdjacentOpening(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection) {
    if (connection.kind != safecrowd::domain::ConnectionKind::Opening) {
        return false;
    }
    return isVerticalZone(findZone(layout, connection.fromZoneId))
        || isVerticalZone(findZone(layout, connection.toZoneId));
}

double floorElevation(const safecrowd::domain::FacilityLayout2D& layout, const std::string& floorId) {
    const auto it = std::find_if(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
        return floor.id == floorId;
    });
    return it == layout.floors.end() ? 0.0 : it->elevationMeters;
}

LayoutCanvasBounds zoneBounds(const safecrowd::domain::Zone2D& zone) {
    LayoutCanvasBounds bounds;
    for (const auto& point : zone.area.outline) {
        bounds.minX = std::min(bounds.minX, point.x);
        bounds.minY = std::min(bounds.minY, point.y);
        bounds.maxX = std::max(bounds.maxX, point.x);
        bounds.maxY = std::max(bounds.maxY, point.y);
    }
    return bounds;
}

QPointF entrySideMidpoint(
    const LayoutCanvasBounds& bounds,
    safecrowd::domain::StairEntryDirection direction) {
    switch (direction) {
    case safecrowd::domain::StairEntryDirection::North:
        return {(bounds.minX + bounds.maxX) * 0.5, bounds.maxY};
    case safecrowd::domain::StairEntryDirection::East:
        return {bounds.maxX, (bounds.minY + bounds.maxY) * 0.5};
    case safecrowd::domain::StairEntryDirection::South:
        return {(bounds.minX + bounds.maxX) * 0.5, bounds.minY};
    case safecrowd::domain::StairEntryDirection::West:
        return {bounds.minX, (bounds.minY + bounds.maxY) * 0.5};
    case safecrowd::domain::StairEntryDirection::Unspecified:
        return {(bounds.minX + bounds.maxX) * 0.5, (bounds.minY + bounds.maxY) * 0.5};
    }
    return {};
}

QPointF entryOutsidePoint(
    const LayoutCanvasBounds& bounds,
    safecrowd::domain::StairEntryDirection direction) {
    const auto margin = std::max(bounds.maxX - bounds.minX, bounds.maxY - bounds.minY) * 0.22;
    auto point = entrySideMidpoint(bounds, direction);
    switch (direction) {
    case safecrowd::domain::StairEntryDirection::North:
        point.ry() += margin;
        break;
    case safecrowd::domain::StairEntryDirection::East:
        point.rx() += margin;
        break;
    case safecrowd::domain::StairEntryDirection::South:
        point.ry() -= margin;
        break;
    case safecrowd::domain::StairEntryDirection::West:
        point.rx() -= margin;
        break;
    case safecrowd::domain::StairEntryDirection::Unspecified:
        break;
    }
    return point;
}

struct StairFloorView {
    const safecrowd::domain::Zone2D* zone{nullptr};
    safecrowd::domain::StairEntryDirection entryDirection{safecrowd::domain::StairEntryDirection::Unspecified};
    QString label{};
};

struct AdjacentStairGhostView {
    const safecrowd::domain::Zone2D* currentZone{nullptr};
    const safecrowd::domain::Zone2D* adjacentZone{nullptr};
    safecrowd::domain::StairEntryDirection currentEntryDirection{safecrowd::domain::StairEntryDirection::Unspecified};
    bool straightFootprint{false};
};

std::optional<StairFloorView> stairFloorView(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId) {
    if (!isVerticalConnection(connection)) {
        return std::nullopt;
    }

    const auto* fromZone = findZone(layout, connection.fromZoneId);
    const auto* toZone = findZone(layout, connection.toZoneId);
    if (fromZone == nullptr || toZone == nullptr || fromZone->floorId == toZone->floorId) {
        return std::nullopt;
    }

    const auto fromElevation = floorElevation(layout, fromZone->floorId);
    const auto toElevation = floorElevation(layout, toZone->floorId);
    const bool fromIsLower = fromElevation <= toElevation;
    const auto* currentZone = matchesFloor(fromZone->floorId, floorId) ? fromZone
        : (matchesFloor(toZone->floorId, floorId) ? toZone : nullptr);
    if (currentZone == nullptr) {
        return std::nullopt;
    }

    const bool currentIsLower = currentZone == fromZone ? fromIsLower : !fromIsLower;
    return StairFloorView{
        .zone = currentZone,
        .entryDirection = currentIsLower ? connection.lowerEntryDirection : connection.upperEntryDirection,
        .label = currentIsLower ? QString("Up") : QString("Down"),
    };
}

bool nearlyEqual(double lhs, double rhs, double tolerance = 1e-4) {
    return std::abs(lhs - rhs) <= tolerance;
}

bool nearlySameBounds(const LayoutCanvasBounds& lhs, const LayoutCanvasBounds& rhs) {
    return lhs.valid() && rhs.valid()
        && nearlyEqual(lhs.minX, rhs.minX)
        && nearlyEqual(lhs.minY, rhs.minY)
        && nearlyEqual(lhs.maxX, rhs.maxX)
        && nearlyEqual(lhs.maxY, rhs.maxY);
}

std::optional<AdjacentStairGhostView> adjacentStairGhostView(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId) {
    if (floorId.empty() || !isVerticalConnection(connection)) {
        return std::nullopt;
    }

    const auto* fromZone = findZone(layout, connection.fromZoneId);
    const auto* toZone = findZone(layout, connection.toZoneId);
    if (fromZone == nullptr || toZone == nullptr || fromZone->floorId == toZone->floorId) {
        return std::nullopt;
    }

    const auto* currentZone = matchesFloor(fromZone->floorId, floorId) ? fromZone
        : (matchesFloor(toZone->floorId, floorId) ? toZone : nullptr);
    if (currentZone == nullptr) {
        return std::nullopt;
    }

    const auto* adjacentZone = currentZone == fromZone ? toZone : fromZone;
    if (!isVerticalZone(currentZone) || !isVerticalZone(adjacentZone)) {
        return std::nullopt;
    }

    const auto fromElevation = floorElevation(layout, fromZone->floorId);
    const auto toElevation = floorElevation(layout, toZone->floorId);
    const bool fromIsLower = fromElevation <= toElevation;
    const bool currentIsLower = currentZone == fromZone ? fromIsLower : !fromIsLower;
    const auto currentEntryDirection = currentIsLower ? connection.lowerEntryDirection : connection.upperEntryDirection;

    return AdjacentStairGhostView{
        .currentZone = currentZone,
        .adjacentZone = adjacentZone,
        .currentEntryDirection = currentEntryDirection,
        .straightFootprint = nearlySameBounds(zoneBounds(*currentZone), zoneBounds(*adjacentZone)),
    };
}

std::optional<LayoutCanvasBounds> straightStairNextFloorHalfBounds(const AdjacentStairGhostView& view) {
    if (view.currentZone == nullptr || !view.straightFootprint) {
        return std::nullopt;
    }

    auto bounds = zoneBounds(*view.currentZone);
    if (!bounds.valid()) {
        return std::nullopt;
    }

    const double midX = (bounds.minX + bounds.maxX) * 0.5;
    const double midY = (bounds.minY + bounds.maxY) * 0.5;
    switch (view.currentEntryDirection) {
    case safecrowd::domain::StairEntryDirection::North:
        bounds.maxY = midY;
        break;
    case safecrowd::domain::StairEntryDirection::East:
        bounds.maxX = midX;
        break;
    case safecrowd::domain::StairEntryDirection::South:
        bounds.minY = midY;
        break;
    case safecrowd::domain::StairEntryDirection::West:
        bounds.minX = midX;
        break;
    case safecrowd::domain::StairEntryDirection::Unspecified:
        return std::nullopt;
    }

    return bounds;
}

QPainterPath boundsPath(const LayoutCanvasBounds& bounds, const LayoutCanvasTransform& transform) {
    QPainterPath path;
    if (!bounds.valid()) {
        return path;
    }

    const auto minPoint = transform.map({.x = bounds.minX, .y = bounds.minY});
    const auto maxPoint = transform.map({.x = bounds.maxX, .y = bounds.maxY});
    path.addRect(QRectF(minPoint, maxPoint).normalized());
    return path;
}

void drawStraightStairTransitionLine(
    QPainter& painter,
    const AdjacentStairGhostView& view,
    const LayoutCanvasTransform& transform) {
    if (view.currentZone == nullptr || !view.straightFootprint) {
        return;
    }

    const auto bounds = zoneBounds(*view.currentZone);
    if (!bounds.valid()) {
        return;
    }

    const double midX = (bounds.minX + bounds.maxX) * 0.5;
    const double midY = (bounds.minY + bounds.maxY) * 0.5;
    switch (view.currentEntryDirection) {
    case safecrowd::domain::StairEntryDirection::North:
    case safecrowd::domain::StairEntryDirection::South:
        painter.drawLine(
            transform.map({.x = bounds.minX, .y = midY}),
            transform.map({.x = bounds.maxX, .y = midY}));
        break;
    case safecrowd::domain::StairEntryDirection::East:
    case safecrowd::domain::StairEntryDirection::West:
        painter.drawLine(
            transform.map({.x = midX, .y = bounds.minY}),
            transform.map({.x = midX, .y = bounds.maxY}));
        break;
    case safecrowd::domain::StairEntryDirection::Unspecified:
        break;
    }
}

void drawAdjacentStairGhost(
    QPainter& painter,
    const AdjacentStairGhostView& view,
    const LayoutCanvasTransform& transform) {
    if (view.adjacentZone == nullptr) {
        return;
    }

    painter.save();
    painter.setBrush(QColor(112, 141, 176, 56));
    painter.setPen(QPen(QColor(76, 92, 118, 112), 1.4, Qt::DashLine));

    if (const auto halfBounds = straightStairNextFloorHalfBounds(view); halfBounds.has_value()) {
        painter.setClipPath(boundsPath(*halfBounds, transform), Qt::IntersectClip);
    }

    painter.drawPath(layoutCanvasPolygonPath(view.adjacentZone->area, transform));
    painter.restore();

    if (view.straightFootprint) {
        painter.save();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(76, 92, 118, 116), 1.2, Qt::DotLine));
        drawStraightStairTransitionLine(painter, view, transform);
        painter.restore();
    }
}

void drawArrowHead(QPainter& painter, const QPointF& start, const QPointF& end) {
    const QLineF line(start, end);
    if (line.length() <= 1.0) {
        return;
    }

    const double angle = std::atan2(start.y() - end.y(), start.x() - end.x());
    constexpr double arrowSize = 8.0;
    const QPointF first = end + QPointF(std::cos(angle + 0.45) * arrowSize, std::sin(angle + 0.45) * arrowSize);
    const QPointF second = end + QPointF(std::cos(angle - 0.45) * arrowSize, std::sin(angle - 0.45) * arrowSize);
    painter.drawLine(end, first);
    painter.drawLine(end, second);
}

void drawStairEntryOverlay(
    QPainter& painter,
    const StairFloorView& view,
    const LayoutCanvasTransform& transform) {
    if (view.zone == nullptr) {
        return;
    }

    const auto bounds = zoneBounds(*view.zone);
    if (!bounds.valid()) {
        return;
    }

    const safecrowd::domain::Point2D center{
        .x = (bounds.minX + bounds.maxX) * 0.5,
        .y = (bounds.minY + bounds.maxY) * 0.5,
    };
    const auto centerScreen = transform.map(center);

    painter.save();
    painter.setFont(QFont("Segoe UI", 9, QFont::DemiBold));
    painter.setPen(QPen(QColor("#5a4b9a"), 2.0));
    painter.setBrush(Qt::NoBrush);

    if (view.entryDirection != safecrowd::domain::StairEntryDirection::Unspecified) {
        const auto outsideWorld = entryOutsidePoint(bounds, view.entryDirection);
        const auto sideWorld = entrySideMidpoint(bounds, view.entryDirection);
        const auto start = transform.map({.x = outsideWorld.x(), .y = outsideWorld.y()});
        const auto end = transform.map({.x = (sideWorld.x() + center.x) * 0.5, .y = (sideWorld.y() + center.y) * 0.5});
        painter.drawLine(start, end);
        drawArrowHead(painter, start, end);
    }

    painter.setPen(QPen(QColor("#3f347b"), 1.0));
    painter.drawText(QRectF(centerScreen.x() - 24.0, centerScreen.y() - 12.0, 48.0, 24.0), Qt::AlignCenter, view.label);
    painter.restore();
}

}  // namespace

LayoutCanvasTransform::LayoutCanvasTransform(
    const LayoutCanvasBounds& bounds,
    const QRectF& viewport,
    double zoom,
    QPointF panOffset)
    : bounds_(bounds),
      viewport_(viewport),
      zoom_(zoom),
      panOffset_(panOffset) {
    const auto sx = viewport_.width() / (bounds_.maxX - bounds_.minX);
    const auto sy = viewport_.height() / (bounds_.maxY - bounds_.minY);
    scale_ = std::min(sx, sy) * zoom_;

    const auto drawingWidth = (bounds_.maxX - bounds_.minX) * scale_;
    const auto drawingHeight = (bounds_.maxY - bounds_.minY) * scale_;
    offsetX_ = viewport_.left() + (viewport_.width() - drawingWidth) / 2.0 + panOffset_.x();
    offsetY_ = viewport_.top() + (viewport_.height() - drawingHeight) / 2.0 + panOffset_.y();
}

QPointF LayoutCanvasTransform::map(const safecrowd::domain::Point2D& point) const {
    return {
        offsetX_ + ((point.x - bounds_.minX) * scale_),
        offsetY_ + ((bounds_.maxY - point.y) * scale_),
    };
}

safecrowd::domain::Point2D LayoutCanvasTransform::unmap(const QPointF& point) const {
    return {
        .x = bounds_.minX + ((point.x() - offsetX_) / scale_),
        .y = bounds_.maxY - ((point.y() - offsetY_) / scale_),
    };
}

bool LayoutCanvasCamera::handleGlobalKeyEvent(QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Space && !keyEvent->isAutoRepeat()) {
            spacePressed_ = true;
        }
    } else if (event->type() == QEvent::KeyRelease) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Space && !keyEvent->isAutoRepeat()) {
            spacePressed_ = false;
            if (!panning_) {
                panButton_ = Qt::NoButton;
            }
        }
    }

    return false;
}

bool LayoutCanvasCamera::handleKeyPress(QKeyEvent* event) {
    if (event->key() != Qt::Key_Space || event->isAutoRepeat()) {
        return false;
    }

    spacePressed_ = true;
    event->accept();
    return true;
}

bool LayoutCanvasCamera::handleKeyRelease(QKeyEvent* event) {
    if (event->key() != Qt::Key_Space || event->isAutoRepeat()) {
        return false;
    }

    spacePressed_ = false;
    event->accept();
    return true;
}

bool LayoutCanvasCamera::beginPan(QMouseEvent* event) {
    if (event->button() != Qt::MiddleButton && !(event->button() == Qt::LeftButton && spacePressed_)) {
        return false;
    }

    panning_ = true;
    panButton_ = event->button();
    lastMousePosition_ = event->position();
    event->accept();
    return true;
}

bool LayoutCanvasCamera::updatePan(QMouseEvent* event) {
    if (!panning_) {
        return false;
    }

    const auto currentPosition = event->position();
    panOffset_ += currentPosition - lastMousePosition_;
    lastMousePosition_ = currentPosition;
    event->accept();
    return true;
}

bool LayoutCanvasCamera::finishPan(QMouseEvent* event) {
    if (!panning_ || event->button() != panButton_) {
        return false;
    }

    panning_ = false;
    panButton_ = Qt::NoButton;
    event->accept();
    return true;
}

bool LayoutCanvasCamera::zoomAt(QWheelEvent* event, const LayoutCanvasBounds& bounds, const QRectF& viewport) {
    if (!bounds.valid() || viewport.width() <= 0.0 || viewport.height() <= 0.0) {
        return false;
    }

    const LayoutCanvasTransform currentTransform(bounds, viewport, zoom_, panOffset_);
    const auto anchorPoint = event->position();
    const auto anchorWorld = currentTransform.unmap(anchorPoint);

    const auto factor = event->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    zoom_ = std::clamp(zoom_ * factor, 0.1, 50.0);

    const LayoutCanvasTransform updatedTransform(bounds, viewport, zoom_, panOffset_);
    panOffset_ += anchorPoint - updatedTransform.map(anchorWorld);
    event->accept();
    return true;
}

void LayoutCanvasCamera::reset() {
    zoom_ = 1.0;
    panOffset_ = {};
}

void includeLayoutCanvasPoint(LayoutCanvasBounds& bounds, const safecrowd::domain::Point2D& point) {
    bounds.minX = std::min(bounds.minX, point.x);
    bounds.minY = std::min(bounds.minY, point.y);
    bounds.maxX = std::max(bounds.maxX, point.x);
    bounds.maxY = std::max(bounds.maxY, point.y);
}

void includeLayoutCanvasPolygon(LayoutCanvasBounds& bounds, const safecrowd::domain::Polygon2D& polygon) {
    for (const auto& point : polygon.outline) {
        includeLayoutCanvasPoint(bounds, point);
    }
    for (const auto& hole : polygon.holes) {
        for (const auto& point : hole) {
            includeLayoutCanvasPoint(bounds, point);
        }
    }
}

void includeLayoutCanvasPolyline(LayoutCanvasBounds& bounds, const safecrowd::domain::Polyline2D& polyline) {
    for (const auto& point : polyline.vertices) {
        includeLayoutCanvasPoint(bounds, point);
    }
}

void includeLayoutCanvasLine(LayoutCanvasBounds& bounds, const safecrowd::domain::LineSegment2D& line) {
    includeLayoutCanvasPoint(bounds, line.start);
    includeLayoutCanvasPoint(bounds, line.end);
}

std::optional<LayoutCanvasBounds> collectLayoutCanvasBounds(const safecrowd::domain::FacilityLayout2D& layout) {
    LayoutCanvasBounds bounds;

    for (const auto& zone : layout.zones) {
        includeLayoutCanvasPolygon(bounds, zone.area);
    }
    for (const auto& barrier : layout.barriers) {
        includeLayoutCanvasPolyline(bounds, barrier.geometry);
    }
    for (const auto& connection : layout.connections) {
        includeLayoutCanvasLine(bounds, connection.centerSpan);
    }

    if (!bounds.valid()) {
        return std::nullopt;
    }

    if (bounds.maxX == bounds.minX) {
        bounds.maxX += 1.0;
        bounds.minX -= 1.0;
    }
    if (bounds.maxY == bounds.minY) {
        bounds.maxY += 1.0;
        bounds.minY -= 1.0;
    }

    return bounds;
}

std::optional<LayoutCanvasBounds> collectLayoutCanvasBounds(const safecrowd::domain::FacilityLayout2D& layout, const std::string& floorId) {
    LayoutCanvasBounds bounds;

    for (const auto& zone : layout.zones) {
        if (matchesFloor(zone.floorId, floorId)) {
            includeLayoutCanvasPolygon(bounds, zone.area);
        }
    }
    for (const auto& connection : layout.connections) {
        if (const auto view = adjacentStairGhostView(layout, connection, floorId); view.has_value()) {
            if (view->straightFootprint) {
                if (const auto halfBounds = straightStairNextFloorHalfBounds(*view); halfBounds.has_value()) {
                    includeLayoutCanvasPoint(bounds, {.x = halfBounds->minX, .y = halfBounds->minY});
                    includeLayoutCanvasPoint(bounds, {.x = halfBounds->maxX, .y = halfBounds->maxY});
                }
            } else if (view->adjacentZone != nullptr) {
                includeLayoutCanvasPolygon(bounds, view->adjacentZone->area);
            }
        }
    }
    for (const auto& barrier : layout.barriers) {
        if (matchesFloor(barrier.floorId, floorId)) {
            includeLayoutCanvasPolyline(bounds, barrier.geometry);
        }
    }
    for (const auto& connection : layout.connections) {
        if (matchesFloor(connection.floorId, floorId)) {
            includeLayoutCanvasLine(bounds, connection.centerSpan);
        }
    }

    if (!bounds.valid()) {
        return std::nullopt;
    }

    if (bounds.maxX == bounds.minX) {
        bounds.maxX += 1.0;
        bounds.minX -= 1.0;
    }
    if (bounds.maxY == bounds.minY) {
        bounds.maxY += 1.0;
        bounds.minY -= 1.0;
    }

    return bounds;
}

std::optional<LayoutCanvasBounds> collectLayoutCanvasBounds(const safecrowd::domain::ImportResult& importResult) {
    LayoutCanvasBounds bounds;

    if (importResult.layout.has_value()) {
        for (const auto& zone : importResult.layout->zones) {
            includeLayoutCanvasPolygon(bounds, zone.area);
        }
        for (const auto& barrier : importResult.layout->barriers) {
            includeLayoutCanvasPolyline(bounds, barrier.geometry);
        }
        for (const auto& connection : importResult.layout->connections) {
            includeLayoutCanvasLine(bounds, connection.centerSpan);
        }
    }

    if (importResult.canonicalGeometry.has_value()) {
        for (const auto& walkable : importResult.canonicalGeometry->walkableAreas) {
            includeLayoutCanvasPolygon(bounds, walkable.polygon);
        }
        for (const auto& obstacle : importResult.canonicalGeometry->obstacles) {
            includeLayoutCanvasPolygon(bounds, obstacle.footprint);
        }
        for (const auto& wall : importResult.canonicalGeometry->walls) {
            includeLayoutCanvasLine(bounds, wall.segment);
        }
        for (const auto& opening : importResult.canonicalGeometry->openings) {
            includeLayoutCanvasLine(bounds, opening.span);
        }
    }

    if (!bounds.valid()) {
        return std::nullopt;
    }

    if (bounds.maxX == bounds.minX) {
        bounds.maxX += 1.0;
        bounds.minX -= 1.0;
    }
    if (bounds.maxY == bounds.minY) {
        bounds.maxY += 1.0;
        bounds.minY -= 1.0;
    }

    return bounds;
}

QRectF layoutCanvasViewport(const QRect& widgetRect, int leftInset, int topInset, int rightInset, int bottomInset) {
    return QRectF(widgetRect).adjusted(leftInset, topInset, -rightInset, -bottomInset);
}

QPainterPath layoutCanvasPolygonPath(const safecrowd::domain::Polygon2D& polygon, const LayoutCanvasTransform& transform) {
    QPainterPath path;

    const auto addRing = [&](const std::vector<safecrowd::domain::Point2D>& ring) {
        if (ring.empty()) {
            return;
        }

        path.moveTo(transform.map(ring.front()));
        for (std::size_t i = 1; i < ring.size(); ++i) {
            path.lineTo(transform.map(ring[i]));
        }
        path.closeSubpath();
    };

    addRing(polygon.outline);
    for (const auto& hole : polygon.holes) {
        addRing(hole);
    }

    return path;
}

QPolygonF layoutCanvasPolylinePath(const safecrowd::domain::Polyline2D& polyline, const LayoutCanvasTransform& transform) {
    QPolygonF path;
    for (const auto& point : polyline.vertices) {
        path.append(transform.map(point));
    }
    return path;
}

void drawLayoutCanvasLine(QPainter& painter, const safecrowd::domain::LineSegment2D& line, const LayoutCanvasTransform& transform) {
    painter.drawLine(transform.map(line.start), transform.map(line.end));
}

void drawLayoutCanvasPolyline(QPainter& painter, const safecrowd::domain::Polyline2D& polyline, const LayoutCanvasTransform& transform) {
    const auto path = layoutCanvasPolylinePath(polyline, transform);
    if (path.size() <= 1) {
        return;
    }

    if (polyline.closed && path.size() > 2) {
        painter.drawPolygon(path);
        return;
    }

    painter.drawPolyline(path);
}

void drawLayoutCanvasGrid(
    QPainter& painter,
    const QRectF& viewport,
    const LayoutCanvasTransform& transform,
    double spacingMeters) {
    if (spacingMeters <= 0.0 || viewport.isEmpty()) {
        return;
    }

    double renderSpacing = spacingMeters;
    auto screenSpacing = [&]() {
        const auto start = transform.map({.x = 0.0, .y = 0.0});
        const auto end = transform.map({.x = renderSpacing, .y = 0.0});
        return std::abs(end.x() - start.x());
    };
    while (screenSpacing() < 8.0 && renderSpacing < 1000.0) {
        renderSpacing *= 2.0;
    }

    const auto topLeft = transform.unmap(viewport.topLeft());
    const auto bottomRight = transform.unmap(viewport.bottomRight());
    const double minX = std::min(topLeft.x, bottomRight.x);
    const double maxX = std::max(topLeft.x, bottomRight.x);
    const double minY = std::min(topLeft.y, bottomRight.y);
    const double maxY = std::max(topLeft.y, bottomRight.y);

    painter.save();
    painter.setClipRect(viewport);
    painter.setPen(QPen(QColor(224, 233, 242), 1));

    const auto firstX = std::floor(minX / renderSpacing) * renderSpacing;
    for (double x = firstX; x <= maxX + 1e-9; x += renderSpacing) {
        const auto start = transform.map({.x = x, .y = minY});
        const auto end = transform.map({.x = x, .y = maxY});
        painter.drawLine(QPointF(start.x(), viewport.top()), QPointF(end.x(), viewport.bottom()));
    }

    const auto firstY = std::floor(minY / renderSpacing) * renderSpacing;
    for (double y = firstY; y <= maxY + 1e-9; y += renderSpacing) {
        const auto start = transform.map({.x = minX, .y = y});
        const auto end = transform.map({.x = maxX, .y = y});
        painter.drawLine(QPointF(viewport.left(), start.y()), QPointF(viewport.right(), end.y()));
    }

    painter.restore();
}

void drawLayoutCanvasSurface(QPainter& painter, const QRectF& viewport) {
    painter.setPen(QPen(QColor("#d7e0ea"), 1));
    painter.setBrush(QColor("#ffffff"));
    painter.drawRect(viewport.adjusted(0.5, 0.5, -0.5, -0.5));
}

void drawFacilityLayoutCanvas(QPainter& painter, const safecrowd::domain::FacilityLayout2D& layout, const LayoutCanvasTransform& transform) {
    painter.setPen(Qt::NoPen);
    for (const auto& zone : layout.zones) {
        painter.setBrush(zone.kind == safecrowd::domain::ZoneKind::Exit
                ? QColor(214, 239, 226)
                : QColor(231, 238, 246));
        painter.drawPath(layoutCanvasPolygonPath(zone.area, transform));
    }

    for (const auto& connection : layout.connections) {
        if (isVerticalConnection(connection) || isStairAdjacentOpening(layout, connection)) {
            continue;
        }
        painter.setPen(QPen(connectionStrokeColor(connection), 3.0));
        drawLayoutCanvasLine(painter, connection.centerSpan, transform);
    }

    for (const auto& connection : layout.connections) {
        if (const auto view = stairFloorView(layout, connection, std::string{}); view.has_value()) {
            drawStairEntryOverlay(painter, *view, transform);
        }
    }

    painter.setPen(QPen(QColor(82, 92, 105), 2.5));
    painter.setBrush(Qt::NoBrush);
    for (const auto& barrier : layout.barriers) {
        drawLayoutCanvasPolyline(painter, barrier.geometry, transform);
    }
}

void drawFacilityLayoutCanvas(
    QPainter& painter,
    const safecrowd::domain::FacilityLayout2D& layout,
    const LayoutCanvasTransform& transform,
    const std::string& floorId) {
    painter.setPen(Qt::NoPen);
    for (const auto& zone : layout.zones) {
        if (!matchesFloor(zone.floorId, floorId)) {
            continue;
        }
        painter.setBrush(zone.kind == safecrowd::domain::ZoneKind::Exit
                ? QColor(214, 239, 226)
                : QColor(231, 238, 246));
        painter.drawPath(layoutCanvasPolygonPath(zone.area, transform));
    }

    for (const auto& connection : layout.connections) {
        if (const auto view = adjacentStairGhostView(layout, connection, floorId); view.has_value()) {
            drawAdjacentStairGhost(painter, *view, transform);
        }
    }

    for (const auto& connection : layout.connections) {
        if (!isVerticalConnection(connection)
            && !isStairAdjacentOpening(layout, connection)
            && matchesFloor(connection.floorId, floorId)) {
            painter.setPen(QPen(connectionStrokeColor(connection), 3.0));
            drawLayoutCanvasLine(painter, connection.centerSpan, transform);
        }
    }

    for (const auto& connection : layout.connections) {
        if (const auto view = stairFloorView(layout, connection, floorId); view.has_value()) {
            drawStairEntryOverlay(painter, *view, transform);
        }
    }

    painter.setPen(QPen(QColor(82, 92, 105), 2.5));
    painter.setBrush(Qt::NoBrush);
    for (const auto& barrier : layout.barriers) {
        if (matchesFloor(barrier.floorId, floorId)) {
            drawLayoutCanvasPolyline(painter, barrier.geometry, transform);
        }
    }
}

}  // namespace safecrowd::application
