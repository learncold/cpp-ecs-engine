#include "application/SimulationCanvasWidget.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

constexpr double kViewportPadding = 32.0;
constexpr double kVelocityIndicatorSeconds = 0.75;
constexpr double kAgentMarkerRadius = 5.0;
constexpr double kDefaultHotspotCellSize = 1.5;
constexpr int kHotspotMinCoreAlpha = 72;
constexpr int kHotspotMaxCoreAlpha = 190;

}  // namespace

SimulationCanvasWidget::SimulationCanvasWidget(safecrowd::domain::FacilityLayout2D layout, QWidget* parent)
    : QWidget(parent),
      layout_(std::move(layout)) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(520, 360);
    setStyleSheet("QWidget { background: #f4f7fb; }");
    layoutBounds_ = collectLayoutCanvasBounds(layout_);
    QCoreApplication::instance()->installEventFilter(this);
}

SimulationCanvasWidget::~SimulationCanvasWidget() {
    if (auto* app = QCoreApplication::instance(); app != nullptr) {
        app->removeEventFilter(this);
    }
}

void SimulationCanvasWidget::setFrame(safecrowd::domain::SimulationFrame frame) {
    frame_ = std::move(frame);
    update();
}

void SimulationCanvasWidget::setHotspotOverlay(std::vector<safecrowd::domain::ScenarioCongestionHotspot> hotspots) {
    hotspotOverlay_ = std::move(hotspots);
    update();
}

bool SimulationCanvasWidget::eventFilter(QObject* watched, QEvent* event) {
    (void)watched;
    camera_.handleGlobalKeyEvent(event);
    return QWidget::eventFilter(watched, event);
}

void SimulationCanvasWidget::keyPressEvent(QKeyEvent* event) {
    if (camera_.handleKeyPress(event)) {
        return;
    }
    QWidget::keyPressEvent(event);
}

void SimulationCanvasWidget::keyReleaseEvent(QKeyEvent* event) {
    if (camera_.handleKeyRelease(event)) {
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void SimulationCanvasWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        camera_.reset();
        layoutCacheValid_ = false;
        update();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void SimulationCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (camera_.updatePan(event)) {
        layoutCacheValid_ = false;
        update();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void SimulationCanvasWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (camera_.beginPan(event)) {
        return;
    }
    QWidget::mousePressEvent(event);
}

void SimulationCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (camera_.finishPan(event)) {
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void SimulationCanvasWidget::paintEvent(QPaintEvent* event) {
    (void)event;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#f4f7fb"));

    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        painter.setPen(QColor("#4f5d6b"));
        painter.drawText(rect(), Qt::AlignCenter, "No layout available");
        return;
    }

    refreshLayoutCache(*bounds);
    painter.drawPixmap(0, 0, layoutCache_);

    const auto transform = currentTransform(*bounds);
    drawHotspotOverlay(painter, transform);
    for (const auto& agent : frame_.agents) {
        const auto origin = transform.map(agent.position);
        const auto tip = transform.map({
            .x = agent.position.x + (agent.velocity.x * kVelocityIndicatorSeconds),
            .y = agent.position.y + (agent.velocity.y * kVelocityIndicatorSeconds),
        });
        painter.setPen(QPen(QColor("#0f4c8f"), 1.3, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(origin, tip);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#1f5fae"));
        painter.drawEllipse(origin, kAgentMarkerRadius, kAgentMarkerRadius);
    }
}

void SimulationCanvasWidget::wheelEvent(QWheelEvent* event) {
    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        QWidget::wheelEvent(event);
        return;
    }
    if (camera_.zoomAt(event, *bounds, previewViewport())) {
        layoutCacheValid_ = false;
        update();
        return;
    }
    QWidget::wheelEvent(event);
}

std::optional<LayoutCanvasBounds> SimulationCanvasWidget::collectBounds() const {
    return layoutBounds_;
}

LayoutCanvasTransform SimulationCanvasWidget::currentTransform(const LayoutCanvasBounds& bounds) const {
    return LayoutCanvasTransform(bounds, previewViewport(), camera_.zoom(), camera_.panOffset());
}

void SimulationCanvasWidget::refreshLayoutCache(const LayoutCanvasBounds& bounds) {
    const auto currentSize = size();
    if (layoutCacheValid_
        && layoutCacheSize_ == currentSize
        && layoutCacheZoom_ == camera_.zoom()
        && layoutCachePan_ == camera_.panOffset()) {
        return;
    }

    layoutCache_ = QPixmap(currentSize);
    layoutCache_.fill(QColor("#f4f7fb"));
    QPainter painter(&layoutCache_);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto viewport = previewViewport();
    const auto transform = currentTransform(bounds);
    painter.setPen(QPen(QColor("#d7e0ea"), 1));
    painter.setBrush(QColor("#ffffff"));
    painter.drawRoundedRect(viewport.adjusted(-18, -18, 18, 18), 14, 14);
    drawFacilityLayoutCanvas(painter, layout_, transform);

    layoutCacheSize_ = currentSize;
    layoutCacheZoom_ = camera_.zoom();
    layoutCachePan_ = camera_.panOffset();
    layoutCacheValid_ = true;
}

QRectF SimulationCanvasWidget::previewViewport() const {
    return QRectF(rect()).adjusted(kViewportPadding, kViewportPadding, -kViewportPadding, -kViewportPadding);
}

void SimulationCanvasWidget::drawHotspotOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (hotspotOverlay_.empty()) {
        return;
    }

    std::size_t maxAgentCount = 0;
    for (const auto& hotspot : hotspotOverlay_) {
        maxAgentCount = std::max(maxAgentCount, hotspot.agentCount);
    }
    if (maxAgentCount == 0) {
        return;
    }

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    QPainterPath walkableClip;
    for (const auto& zone : layout_.zones) {
        walkableClip.addPath(layoutCanvasPolygonPath(zone.area, transform));
    }
    if (!walkableClip.isEmpty()) {
        painter.setClipPath(walkableClip);
    }

    for (const auto& hotspot : hotspotOverlay_) {
        if (hotspot.agentCount == 0) {
            continue;
        }
        const auto intensity = static_cast<double>(hotspot.agentCount) / static_cast<double>(maxAgentCount);
        const auto center = transform.map(hotspot.center);
        const auto cellWidth = hotspot.cellMax.x > hotspot.cellMin.x
            ? hotspot.cellMax.x - hotspot.cellMin.x
            : kDefaultHotspotCellSize;
        const auto cellHeight = hotspot.cellMax.y > hotspot.cellMin.y
            ? hotspot.cellMax.y - hotspot.cellMin.y
            : kDefaultHotspotCellSize;
        const auto sourceRadiusWorld = std::max(cellWidth, cellHeight) * (1.2 + (0.85 * std::sqrt(intensity)));
        const auto radiusAnchor = transform.map({.x = hotspot.center.x + sourceRadiusWorld, .y = hotspot.center.y});
        const auto radius = std::max(12.0, std::hypot(radiusAnchor.x() - center.x(), radiusAnchor.y() - center.y()));
        const auto coreAlpha = kHotspotMinCoreAlpha
            + static_cast<int>((kHotspotMaxCoreAlpha - kHotspotMinCoreAlpha) * intensity);

        QRadialGradient gradient(center, radius);
        gradient.setColorAt(0.0, QColor(185, 28, 28, std::clamp(coreAlpha, kHotspotMinCoreAlpha, kHotspotMaxCoreAlpha)));
        gradient.setColorAt(0.28, QColor(220, 38, 38, static_cast<int>(coreAlpha * 0.62)));
        gradient.setColorAt(0.58, QColor(249, 115, 22, static_cast<int>(coreAlpha * 0.28)));
        gradient.setColorAt(1.0, QColor(249, 115, 22, 0));
        painter.setBrush(gradient);
        painter.drawEllipse(center, radius, radius);
    }
    painter.restore();
}

}  // namespace safecrowd::application
