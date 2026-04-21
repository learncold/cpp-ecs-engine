#include "application/LayoutPreviewWidget.h"

#include <algorithm>
#include <limits>
#include <optional>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

struct Bounds2D {
    double minX{std::numeric_limits<double>::max()};
    double minY{std::numeric_limits<double>::max()};
    double maxX{std::numeric_limits<double>::lowest()};
    double maxY{std::numeric_limits<double>::lowest()};

    bool valid() const noexcept {
        return minX <= maxX && minY <= maxY;
    }
};

void includePoint(Bounds2D& bounds, const safecrowd::domain::Point2D& point) {
    bounds.minX = std::min(bounds.minX, point.x);
    bounds.minY = std::min(bounds.minY, point.y);
    bounds.maxX = std::max(bounds.maxX, point.x);
    bounds.maxY = std::max(bounds.maxY, point.y);
}

void includePolygon(Bounds2D& bounds, const safecrowd::domain::Polygon2D& polygon) {
    for (const auto& point : polygon.outline) {
        includePoint(bounds, point);
    }
    for (const auto& hole : polygon.holes) {
        for (const auto& point : hole) {
            includePoint(bounds, point);
        }
    }
}

void includePolyline(Bounds2D& bounds, const safecrowd::domain::Polyline2D& polyline) {
    for (const auto& point : polyline.vertices) {
        includePoint(bounds, point);
    }
}

void includeLine(Bounds2D& bounds, const safecrowd::domain::LineSegment2D& line) {
    includePoint(bounds, line.start);
    includePoint(bounds, line.end);
}

std::optional<Bounds2D> collectBounds(const safecrowd::domain::ImportResult& importResult) {
    Bounds2D bounds;

    if (importResult.layout.has_value()) {
        for (const auto& zone : importResult.layout->zones) {
            includePolygon(bounds, zone.area);
        }
        for (const auto& barrier : importResult.layout->barriers) {
            includePolyline(bounds, barrier.geometry);
        }
        for (const auto& connection : importResult.layout->connections) {
            includeLine(bounds, connection.centerSpan);
        }
    }

    if (importResult.canonicalGeometry.has_value()) {
        for (const auto& walkable : importResult.canonicalGeometry->walkableAreas) {
            includePolygon(bounds, walkable.polygon);
        }
        for (const auto& obstacle : importResult.canonicalGeometry->obstacles) {
            includePolygon(bounds, obstacle.footprint);
        }
        for (const auto& wall : importResult.canonicalGeometry->walls) {
            includeLine(bounds, wall.segment);
        }
        for (const auto& opening : importResult.canonicalGeometry->openings) {
            includeLine(bounds, opening.span);
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

class LayoutTransform {
public:
    LayoutTransform(const Bounds2D& bounds, const QRectF& viewport, double zoom, QPointF panOffset)
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

    QPointF map(const safecrowd::domain::Point2D& point) const {
        return {
            offsetX_ + ((point.x - bounds_.minX) * scale_),
            offsetY_ + ((bounds_.maxY - point.y) * scale_),
        };
    }

private:
    Bounds2D bounds_{};
    QRectF viewport_{};
    double zoom_{1.0};
    QPointF panOffset_{};
    double scale_{1.0};
    double offsetX_{0.0};
    double offsetY_{0.0};
};

QPainterPath polygonPath(const safecrowd::domain::Polygon2D& polygon, const LayoutTransform& transform) {
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

QPolygonF polylinePath(const safecrowd::domain::Polyline2D& polyline, const LayoutTransform& transform) {
    QPolygonF path;
    for (const auto& point : polyline.vertices) {
        path.append(transform.map(point));
    }
    return path;
}

void drawLine(QPainter& painter, const safecrowd::domain::LineSegment2D& line, const LayoutTransform& transform) {
    painter.drawLine(transform.map(line.start), transform.map(line.end));
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

}  // namespace

LayoutPreviewWidget::LayoutPreviewWidget(safecrowd::domain::ImportResult importResult, QWidget* parent)
    : QWidget(parent),
      importResult_(std::move(importResult)) {
    setMinimumSize(520, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void LayoutPreviewWidget::focusIssueTarget(const QString& targetId) {
    focusedTargetId_ = targetId;

    Bounds2D targetBounds;
    includeMatchingGeometryBounds(importResult_, targetId, targetBounds);
    const auto worldBounds = collectBounds(importResult_);
    if (targetBounds.valid() && worldBounds.has_value()) {
        const QRectF viewport = rect().adjusted(width() / 12, height() / 12, -width() / 12, -height() / 12);
        const auto targetWidth = std::max(targetBounds.maxX - targetBounds.minX, 1.0);
        const auto targetHeight = std::max(targetBounds.maxY - targetBounds.minY, 1.0);
        const auto targetZoom = 0.55 * std::min(viewport.width() / targetWidth, viewport.height() / targetHeight)
            / std::min(viewport.width() / (worldBounds->maxX - worldBounds->minX), viewport.height() / (worldBounds->maxY - worldBounds->minY));
        zoom_ = std::clamp(targetZoom, 1.0, 30.0);

        const LayoutTransform transform(*worldBounds, viewport, zoom_, {});
        const safecrowd::domain::Point2D center{
            .x = (targetBounds.minX + targetBounds.maxX) / 2.0,
            .y = (targetBounds.minY + targetBounds.maxY) / 2.0,
        };
        panOffset_ = viewport.center() - transform.map(center);
    }

    update();
}

void LayoutPreviewWidget::resetView() {
    zoom_ = 1.0;
    panOffset_ = {};
    update();
}

void LayoutPreviewWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    QWidget::mouseDoubleClickEvent(event);
    resetView();
}

void LayoutPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!panning_) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const auto currentPosition = event->position();
    panOffset_ += currentPosition - lastMousePosition_;
    lastMousePosition_ = currentPosition;
    update();
}

void LayoutPreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastMousePosition_ = event->position();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void LayoutPreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (panning_ && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        panning_ = false;
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void LayoutPreviewWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::white);

    const auto bounds = collectBounds(importResult_);
    if (!bounds.has_value()) {
        painter.setPen(QPen(QColor(80, 80, 80), 1));
        painter.setFont(QFont("Arial", 14));
        painter.drawText(rect(), Qt::AlignCenter, "No layout geometry imported");
        return;
    }

    const QRectF viewport = rect().adjusted(width() / 12, height() / 12, -width() / 12, -height() / 12);
    const LayoutTransform transform(*bounds, viewport, zoom_, panOffset_);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(235, 235, 235));
    if (importResult_.layout.has_value()) {
        for (const auto& zone : importResult_.layout->zones) {
            painter.drawPath(polygonPath(zone.area, transform));
        }
    } else if (importResult_.canonicalGeometry.has_value()) {
        for (const auto& walkable : importResult_.canonicalGeometry->walkableAreas) {
            painter.drawPath(polygonPath(walkable.polygon, transform));
        }
    }

    if (importResult_.canonicalGeometry.has_value()) {
        painter.setBrush(QColor(230, 160, 70, 150));
        painter.setPen(QPen(QColor(180, 118, 38), 1));
        for (const auto& obstacle : importResult_.canonicalGeometry->obstacles) {
            painter.drawPath(polygonPath(obstacle.footprint, transform));
        }

        painter.setPen(QPen(QColor(70, 70, 70), 3));
        for (const auto& wall : importResult_.canonicalGeometry->walls) {
            drawLine(painter, wall.segment, transform);
        }

        painter.setPen(QPen(QColor(58, 174, 74), 3, Qt::DashLine));
        for (const auto& opening : importResult_.canonicalGeometry->openings) {
            drawLine(painter, opening.span, transform);
        }
    }

    if (importResult_.layout.has_value()) {
        painter.setPen(QPen(QColor(62, 150, 210), 2));
        for (const auto& connection : importResult_.layout->connections) {
            drawLine(painter, connection.centerSpan, transform);
        }

        painter.setPen(QPen(QColor(70, 70, 70), 3));
        for (const auto& barrier : importResult_.layout->barriers) {
            const auto path = polylinePath(barrier.geometry, transform);
            if (path.size() > 1) {
                painter.drawPolyline(path);
            }
        }
    }

    if (!focusedTargetId_.isEmpty()) {
        painter.setBrush(QColor(255, 217, 64, 90));
        painter.setPen(QPen(QColor(210, 55, 55), 4));

        if (importResult_.layout.has_value()) {
            for (const auto& zone : importResult_.layout->zones) {
                if (QString::fromStdString(zone.id) == focusedTargetId_ || traceMatches(zone.provenance, focusedTargetId_)) {
                    painter.drawPath(polygonPath(zone.area, transform));
                }
            }
            for (const auto& connection : importResult_.layout->connections) {
                if (QString::fromStdString(connection.id) == focusedTargetId_ || traceMatches(connection.provenance, focusedTargetId_)) {
                    drawLine(painter, connection.centerSpan, transform);
                }
            }
            for (const auto& barrier : importResult_.layout->barriers) {
                if (QString::fromStdString(barrier.id) == focusedTargetId_ || traceMatches(barrier.provenance, focusedTargetId_)) {
                    const auto path = polylinePath(barrier.geometry, transform);
                    if (path.size() > 1) {
                        painter.drawPolyline(path);
                    }
                }
            }
        }

        if (importResult_.canonicalGeometry.has_value()) {
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

    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.setFont(QFont("Arial", 10));
    painter.drawText(rect().adjusted(8, 8, -8, -8), Qt::AlignTop | Qt::AlignRight, QString("Zoom %1%").arg(static_cast<int>(zoom_ * 100.0)));
}

void LayoutPreviewWidget::wheelEvent(QWheelEvent* event) {
    const auto factor = event->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    zoom_ = std::clamp(zoom_ * factor, 0.1, 50.0);
    update();
    event->accept();
}

}  // namespace safecrowd::application
