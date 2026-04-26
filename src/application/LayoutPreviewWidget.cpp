#include "application/LayoutPreviewWidget.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPathStroker>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

constexpr double kConnectionWidth = 1.2;
constexpr double kConnectionHitTolerance = 10.0;
constexpr double kDraftMinimumSize = 0.2;
constexpr int kTopToolbarHeight = 44;
constexpr int kSideToolbarWidth = 44;
constexpr int kToolbarButtonSize = 44;

QRectF previewViewport(const QRect& widgetRect) {
    return QRectF(widgetRect).adjusted(kSideToolbarWidth + 16, kTopToolbarHeight + 16, -16, -16);
}

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

    safecrowd::domain::Point2D unmap(const QPointF& point) const {
        return {
            .x = bounds_.minX + ((point.x() - offsetX_) / scale_),
            .y = bounds_.maxY - ((point.y() - offsetY_) / scale_),
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

void drawPolyline(QPainter& painter, const safecrowd::domain::Polyline2D& polyline, const LayoutTransform& transform) {
    const auto path = polylinePath(polyline, transform);
    if (path.size() <= 1) {
        return;
    }

    if (polyline.closed && path.size() > 2) {
        painter.drawPolygon(path);
        return;
    }

    painter.drawPolyline(path);
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
    case ZoneKind::Corridor:
        return "Corridor";
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

QString zoneTitle(const safecrowd::domain::Zone2D& zone) {
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty()
        ? QString("Zone %1").arg(QString::fromStdString(zone.id))
        : QString("%1 (%2)").arg(label, QString::fromStdString(zone.id));
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

std::vector<std::size_t> zonesNearPoint(const safecrowd::domain::FacilityLayout2D& layout, const QPointF& point) {
    std::vector<std::size_t> matches;
    for (std::size_t index = 0; index < layout.zones.size(); ++index) {
        const auto& zone = layout.zones[index];
        if (pointInPolygon(zone.area, point) || distanceToPolygonBoundary(zone.area, point) <= 0.35) {
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
    } else if (glyph == "corridor") {
        painter.drawRoundedRect(QRectF(4, 8, 16, 8), 2, 2);
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
    }

    return QIcon(pixmap);
}

std::optional<QString> hitTestZone(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& position,
    const LayoutTransform& transform) {
    for (auto it = layout.zones.rbegin(); it != layout.zones.rend(); ++it) {
        if (polygonPath(it->area, transform).contains(position)) {
            return QString::fromStdString(it->id);
        }
    }

    return std::nullopt;
}

std::optional<QString> hitTestConnection(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& position,
    const LayoutTransform& transform) {
    double bestDistance = std::numeric_limits<double>::max();
    std::optional<QString> bestId;

    for (const auto& connection : layout.connections) {
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
    const LayoutTransform& transform) {
    double bestDistance = std::numeric_limits<double>::max();
    std::optional<QString> bestId;

    for (const auto& barrier : layout.barriers) {
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
    setMinimumSize(520, 360);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    QCoreApplication::instance()->installEventFilter(this);
    setupToolbars();
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
            focusIssueTarget(elementId);
            selectZone(elementId);
            return;
        }
        if (containsConnection(*importResult_.layout, elementId)) {
            focusIssueTarget(elementId);
            selectConnection(elementId);
            return;
        }
        if (containsBarrier(*importResult_.layout, elementId)) {
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
    const auto worldBounds = collectBounds(importResult_);
    if (targetBounds.valid() && worldBounds.has_value()) {
        const QRectF viewport = previewViewport(rect());
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

void LayoutPreviewWidget::setImportResult(safecrowd::domain::ImportResult importResult) {
    importResult_ = std::move(importResult);

    if (!importResult_.layout.has_value()) {
        clearSelection();
        if (topToolbar_ != nullptr) {
            topToolbar_->hide();
        }
        if (sideToolbar_ != nullptr) {
            sideToolbar_->hide();
        }
        return;
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

    if (topToolbar_ != nullptr) {
        topToolbar_->setVisible(true);
    }
    if (sideToolbar_ != nullptr) {
        sideToolbar_->setVisible(true);
    }

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

    return QWidget::eventFilter(watched, event);
}

void LayoutPreviewWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spacePressed_ = true;
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void LayoutPreviewWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        spacePressed_ = false;
        event->accept();
        return;
    }

    QWidget::keyReleaseEvent(event);
}

void LayoutPreviewWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    QWidget::mouseDoubleClickEvent(event);
    resetView();
}

void LayoutPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!panning_ && !drafting_) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (drafting_) {
        const auto bounds = collectBounds(importResult_);
        if (bounds.has_value()) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), zoom_, panOffset_);
            const auto world = transform.unmap(event->position());
            draftCurrentWorld_ = QPointF(world.x, world.y);
            update();
            event->accept();
            return;
        }
    }

    const auto currentPosition = event->position();
    panOffset_ += currentPosition - lastMousePosition_;
    lastMousePosition_ = currentPosition;
    update();
}

void LayoutPreviewWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);

    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && spacePressed_)) {
        panning_ = true;
        panButton_ = event->button();
        lastMousePosition_ = event->position();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const auto bounds = collectBounds(importResult_);
        if (!bounds.has_value()) {
            QWidget::mousePressEvent(event);
            return;
        }

        if (toolMode_ != ToolMode::Select && toolMode_ != ToolMode::Delete) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), zoom_, panOffset_);
            const auto world = transform.unmap(event->position());
            drafting_ = true;
            draftStartWorld_ = QPointF(world.x, world.y);
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
    if (panning_ && event->button() == panButton_) {
        panning_ = false;
        panButton_ = Qt::NoButton;
        event->accept();
        return;
    }

    if (drafting_ && event->button() == Qt::LeftButton) {
        drafting_ = false;
        const auto bounds = collectBounds(importResult_);
        if (bounds.has_value()) {
            const LayoutTransform transform(*bounds, previewViewport(rect()), zoom_, panOffset_);
            const auto world = transform.unmap(event->position());
            draftCurrentWorld_ = QPointF(world.x, world.y);
        }

        switch (toolMode_) {
        case ToolMode::DrawRoom:
            createZone(draftStartWorld_, draftCurrentWorld_, safecrowd::domain::ZoneKind::Room);
            break;
        case ToolMode::DrawCorridor:
            createZone(draftStartWorld_, draftCurrentWorld_, safecrowd::domain::ZoneKind::Corridor);
            break;
        case ToolMode::DrawExit:
            createZone(draftStartWorld_, draftCurrentWorld_, safecrowd::domain::ZoneKind::Exit);
            break;
        case ToolMode::DrawWall:
            createBarrier(draftStartWorld_, draftCurrentWorld_);
            break;
        case ToolMode::DrawDoor:
            createConnection(draftStartWorld_, draftCurrentWorld_);
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

    const auto bounds = collectBounds(importResult_);
    if (!bounds.has_value()) {
        painter.setPen(QPen(QColor(80, 80, 80), 1));
        painter.setFont(QFont("Segoe UI", 14, QFont::DemiBold));
        painter.drawText(rect(), Qt::AlignCenter, "No layout geometry imported");
        return;
    }

    const QRectF viewport = previewViewport(rect());
    const LayoutTransform transform(*bounds, viewport, zoom_, panOffset_);

    painter.setPen(QPen(QColor(238, 243, 248), 1));
    for (int x = static_cast<int>(viewport.left()); x < static_cast<int>(viewport.right()); x += 32) {
        painter.drawLine(QPointF(x, viewport.top()), QPointF(x, viewport.bottom()));
    }
    for (int y = static_cast<int>(viewport.top()); y < static_cast<int>(viewport.bottom()); y += 32) {
        painter.drawLine(QPointF(viewport.left(), y), QPointF(viewport.right(), y));
    }

    painter.setPen(Qt::NoPen);
    if (importResult_.layout.has_value()) {
        for (const auto& zone : importResult_.layout->zones) {
            painter.setBrush(zone.kind == safecrowd::domain::ZoneKind::Exit
                    ? QColor(214, 239, 226)
                    : QColor(231, 238, 246));
            painter.drawPath(polygonPath(zone.area, transform));
        }
    } else if (importResult_.canonicalGeometry.has_value()) {
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

    if (importResult_.layout.has_value()) {
        painter.setPen(QPen(QColor(56, 122, 186), 2.5));
        for (const auto& connection : importResult_.layout->connections) {
            drawLine(painter, connection.centerSpan, transform);
        }

        painter.setPen(QPen(QColor(82, 92, 105), 2.5));
        painter.setBrush(Qt::NoBrush);
        for (const auto& barrier : importResult_.layout->barriers) {
            drawPolyline(painter, barrier.geometry, transform);
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
                if (QString::fromStdString(zone.id) == highlightTargetId || traceMatches(zone.provenance, highlightTargetId)) {
                    painter.drawPath(polygonPath(zone.area, transform));
                }
            }
            for (const auto& connection : importResult_.layout->connections) {
                if (QString::fromStdString(connection.id) == highlightTargetId || traceMatches(connection.provenance, highlightTargetId)) {
                    drawLine(painter, connection.centerSpan, transform);
                }
            }
            for (const auto& barrier : importResult_.layout->barriers) {
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
    painter.drawText(viewport.adjusted(0, -10, 0, 0), Qt::AlignTop | Qt::AlignRight, QString("Zoom %1%").arg(static_cast<int>(zoom_ * 100.0)));
    painter.drawText(viewport.adjusted(0, -10, 0, 0), Qt::AlignTop | Qt::AlignLeft, "Layout Preview");

}

void LayoutPreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    repositionToolbars();
}

void LayoutPreviewWidget::wheelEvent(QWheelEvent* event) {
    const auto bounds = collectBounds(importResult_);
    if (!bounds.has_value()) {
        QWidget::wheelEvent(event);
        return;
    }

    const QRectF viewport = previewViewport(rect());
    const LayoutTransform currentTransform(*bounds, viewport, zoom_, panOffset_);
    const auto anchorPoint = event->position();
    const auto anchorWorld = currentTransform.unmap(anchorPoint);

    const auto factor = event->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    zoom_ = std::clamp(zoom_ * factor, 0.1, 50.0);

    const LayoutTransform updatedTransform(*bounds, viewport, zoom_, panOffset_);
    panOffset_ += anchorPoint - updatedTransform.map(anchorWorld);
    update();
    event->accept();
}

void LayoutPreviewWidget::applyToolAt(const QPointF& position) {
    if (!importResult_.layout.has_value()) {
        return;
    }

    const auto bounds = collectBounds(importResult_);
    if (!bounds.has_value()) {
        return;
    }

    const LayoutTransform transform(*bounds, previewViewport(rect()), zoom_, panOffset_);
    const auto zoneId = hitTestZone(*importResult_.layout, position, transform);
    const auto connectionId = hitTestConnection(*importResult_.layout, position, transform);
    const auto barrierId = hitTestBarrier(*importResult_.layout, position, transform);

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
    case ToolMode::DrawCorridor:
    case ToolMode::DrawExit:
    case ToolMode::DrawWall:
    case ToolMode::DrawDoor:
        return;
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

    const auto zoneId = nextZoneId(layout);
    const auto zoneNumber = static_cast<int>(layout.zones.size()) + 1;
    QString zoneLabel = "Room";
    if (kind == safecrowd::domain::ZoneKind::Corridor) {
        zoneLabel = "Corridor";
    } else if (kind == safecrowd::domain::ZoneKind::Exit) {
        zoneLabel = "Exit";
    }

    layout.zones.push_back({
        .id = zoneId.toStdString(),
        .kind = kind,
        .label = QString("%1 %2").arg(zoneLabel).arg(zoneNumber).toStdString(),
        .area = safecrowd::domain::Polygon2D{
            .outline = {
                {.x = rectangle.left(), .y = rectangle.top()},
                {.x = rectangle.right(), .y = rectangle.top()},
                {.x = rectangle.right(), .y = rectangle.bottom()},
                {.x = rectangle.left(), .y = rectangle.bottom()},
            },
        },
        .defaultCapacity = kind == safecrowd::domain::ZoneKind::Exit ? 20u : 0u,
    });

    selectedZoneId_ = zoneId;
    selectedConnectionId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_ = zoneId;
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
    const auto startCandidates = zonesNearPoint(layout, startWorld);
    const auto endCandidates = zonesNearPoint(layout, endWorld);

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

void LayoutPreviewWidget::notifyLayoutEdited() {
    if (layoutEditedHandler_ && importResult_.layout.has_value()) {
        layoutEditedHandler_(*importResult_.layout);
    }
}

void LayoutPreviewWidget::repositionToolbars() {
    if (topToolbar_ != nullptr) {
        topToolbar_->setGeometry(kSideToolbarWidth, 0, width() - kSideToolbarWidth, kTopToolbarHeight);
        topToolbar_->raise();
    }
    if (sideToolbar_ != nullptr) {
        sideToolbar_->setGeometry(0, kTopToolbarHeight, kSideToolbarWidth, std::max(0, height() - kTopToolbarHeight));
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

void LayoutPreviewWidget::selectZone(const QString& zoneId) {
    selectedZoneId_ = zoneId;
    selectedConnectionId_.clear();
    selectedBarrierId_.clear();
    focusedTargetId_ = zoneId;
    emitCurrentSelection();
    update();
}

void LayoutPreviewWidget::setToolMode(ToolMode mode) {
    toolMode_ = mode;
    if (selectToolButton_ != nullptr) {
        selectToolButton_->setChecked(toolMode_ == ToolMode::Select);
    }
    if (roomToolButton_ != nullptr) {
        roomToolButton_->setChecked(toolMode_ == ToolMode::DrawRoom);
    }
    if (corridorToolButton_ != nullptr) {
        corridorToolButton_->setChecked(toolMode_ == ToolMode::DrawCorridor);
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
    if (deleteToolButton_ != nullptr) {
        deleteToolButton_->setChecked(toolMode_ == ToolMode::Delete);
    }

    update();
}

void LayoutPreviewWidget::setupToolbars() {
    const QString frameStyle =
        "QFrame { background: rgba(255, 255, 255, 245); border: 1px solid #d7e0ea; border-radius: 0px; }"
        "QToolButton { background: transparent; border: 0; border-radius: 0px; }"
        "QToolButton:hover { background: #eef3f8; }"
        "QToolButton:checked { background: #dce9f9; }";

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
    topLayout->addStretch(1);

    roomToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("room", QColor("#2f5d8a")), "Draw Room");
    corridorToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("corridor", QColor("#4b7282")), "Draw Corridor");
    exitToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("exit", QColor("#2d8f5b")), "Draw Exit");
    wallToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("wall", QColor("#6c4f38")), "Draw Wall");
    doorToolButton_ = makeButton(sideToolbar_, sideLayout, makeToolIcon("door", QColor("#8e6b23")), "Draw Door");
    sideLayout->addStretch(1);

    connect(selectToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::Select); });
    connect(deleteToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::Delete); });
    connect(resetViewButton_, &QToolButton::clicked, this, [this]() { resetView(); });
    connect(roomToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawRoom); });
    connect(corridorToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawCorridor); });
    connect(exitToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawExit); });
    connect(wallToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawWall); });
    connect(doorToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::DrawDoor); });

    const auto visible = importResult_.layout.has_value();
    topToolbar_->setVisible(visible);
    sideToolbar_->setVisible(visible);
    setToolMode(ToolMode::Select);
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
    selection.detail = "Use the top and left toolbars to select, draw rooms, corridors, exits, walls, and doors.";
    return selection;
}

}  // namespace safecrowd::application
