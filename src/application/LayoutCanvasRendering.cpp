#include "application/LayoutCanvasRendering.h"

#include <algorithm>

#include <QColor>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>

namespace safecrowd::application {

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

void drawLayoutCanvasGrid(QPainter& painter, const QRectF& viewport) {
    painter.setPen(QPen(QColor(238, 243, 248), 1));
    for (int x = static_cast<int>(viewport.left()); x < static_cast<int>(viewport.right()); x += 32) {
        painter.drawLine(QPointF(x, viewport.top()), QPointF(x, viewport.bottom()));
    }
    for (int y = static_cast<int>(viewport.top()); y < static_cast<int>(viewport.bottom()); y += 32) {
        painter.drawLine(QPointF(viewport.left(), y), QPointF(viewport.right(), y));
    }
}

void drawFacilityLayoutCanvas(QPainter& painter, const safecrowd::domain::FacilityLayout2D& layout, const LayoutCanvasTransform& transform) {
    painter.setPen(Qt::NoPen);
    for (const auto& zone : layout.zones) {
        painter.setBrush(zone.kind == safecrowd::domain::ZoneKind::Exit
                ? QColor(214, 239, 226)
                : QColor(231, 238, 246));
        painter.drawPath(layoutCanvasPolygonPath(zone.area, transform));
    }

    painter.setPen(QPen(QColor(56, 122, 186), 2.5));
    for (const auto& connection : layout.connections) {
        drawLayoutCanvasLine(painter, connection.centerSpan, transform);
    }

    painter.setPen(QPen(QColor(82, 92, 105), 2.5));
    painter.setBrush(Qt::NoBrush);
    for (const auto& barrier : layout.barriers) {
        drawLayoutCanvasPolyline(painter, barrier.geometry, transform);
    }
}

}  // namespace safecrowd::application
