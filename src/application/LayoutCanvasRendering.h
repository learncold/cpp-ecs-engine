#pragma once

#include <limits>
#include <optional>

#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <Qt>

#include "domain/FacilityLayout2D.h"
#include "domain/ImportResult.h"

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPainter;
class QWheelEvent;

namespace safecrowd::application {

struct LayoutCanvasBounds {
    double minX{std::numeric_limits<double>::max()};
    double minY{std::numeric_limits<double>::max()};
    double maxX{std::numeric_limits<double>::lowest()};
    double maxY{std::numeric_limits<double>::lowest()};

    bool valid() const noexcept {
        return minX <= maxX && minY <= maxY;
    }
};

class LayoutCanvasTransform {
public:
    LayoutCanvasTransform(const LayoutCanvasBounds& bounds, const QRectF& viewport, double zoom, QPointF panOffset);

    QPointF map(const safecrowd::domain::Point2D& point) const;
    safecrowd::domain::Point2D unmap(const QPointF& point) const;

private:
    LayoutCanvasBounds bounds_{};
    QRectF viewport_{};
    double zoom_{1.0};
    QPointF panOffset_{};
    double scale_{1.0};
    double offsetX_{0.0};
    double offsetY_{0.0};
};

class LayoutCanvasCamera {
public:
    bool handleGlobalKeyEvent(QEvent* event);
    bool handleKeyPress(QKeyEvent* event);
    bool handleKeyRelease(QKeyEvent* event);
    bool beginPan(QMouseEvent* event);
    bool updatePan(QMouseEvent* event);
    bool finishPan(QMouseEvent* event);
    bool zoomAt(QWheelEvent* event, const LayoutCanvasBounds& bounds, const QRectF& viewport);
    void reset();

    double zoom() const noexcept {
        return zoom_;
    }

    QPointF panOffset() const noexcept {
        return panOffset_;
    }

    void setZoom(double zoom) noexcept {
        zoom_ = zoom;
    }

    void setPanOffset(QPointF panOffset) noexcept {
        panOffset_ = panOffset;
    }

    void setPrimaryButtonPanEnabled(bool enabled) noexcept {
        primaryButtonPanEnabled_ = enabled;
    }

    bool panning() const noexcept {
        return panning_;
    }

private:
    double zoom_{1.0};
    QPointF panOffset_{};
    QPointF lastMousePosition_{};
    Qt::MouseButton panButton_{Qt::NoButton};
    bool panning_{false};
    bool spacePressed_{false};
    bool primaryButtonPanEnabled_{false};
};

void includeLayoutCanvasPoint(LayoutCanvasBounds& bounds, const safecrowd::domain::Point2D& point);
void includeLayoutCanvasPolygon(LayoutCanvasBounds& bounds, const safecrowd::domain::Polygon2D& polygon);
void includeLayoutCanvasPolyline(LayoutCanvasBounds& bounds, const safecrowd::domain::Polyline2D& polyline);
void includeLayoutCanvasLine(LayoutCanvasBounds& bounds, const safecrowd::domain::LineSegment2D& line);

std::optional<LayoutCanvasBounds> collectLayoutCanvasBounds(const safecrowd::domain::FacilityLayout2D& layout);
std::optional<LayoutCanvasBounds> collectLayoutCanvasBounds(const safecrowd::domain::ImportResult& importResult);

QRectF layoutCanvasViewport(const QRect& widgetRect, int leftInset, int topInset, int rightInset, int bottomInset);
QPainterPath layoutCanvasPolygonPath(const safecrowd::domain::Polygon2D& polygon, const LayoutCanvasTransform& transform);
QPolygonF layoutCanvasPolylinePath(const safecrowd::domain::Polyline2D& polyline, const LayoutCanvasTransform& transform);
void drawLayoutCanvasLine(QPainter& painter, const safecrowd::domain::LineSegment2D& line, const LayoutCanvasTransform& transform);
void drawLayoutCanvasPolyline(QPainter& painter, const safecrowd::domain::Polyline2D& polyline, const LayoutCanvasTransform& transform);
void drawLayoutCanvasGrid(QPainter& painter, const QRectF& viewport);
void drawLayoutCanvasSurface(QPainter& painter, const QRectF& viewport);
void drawFacilityLayoutCanvas(QPainter& painter, const safecrowd::domain::FacilityLayout2D& layout, const LayoutCanvasTransform& transform);

}  // namespace safecrowd::application
