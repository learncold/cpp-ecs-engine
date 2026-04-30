#include "application/SimulationCanvasWidget.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

constexpr double kViewportPadding = 32.0;
constexpr double kVelocityIndicatorSeconds = 0.75;
constexpr double kAgentMarkerRadius = 5.0;
constexpr double kDefaultHotspotCellSize = 1.5;
constexpr double kHotspotFocusZoom = 2.8;
constexpr double kBottleneckFocusZoom = 2.4;
constexpr int kHotspotMinCoreAlpha = 72;
constexpr int kHotspotMaxCoreAlpha = 190;

QPushButton* createViewControlButton(const QString& text, const QString& tooltip, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setFixedSize(text == "Fit" ? QSize(42, 30) : QSize(30, 30));
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setStyleSheet(
        "QPushButton {"
        " background: #ffffff;"
        " border: 1px solid #d7e0ea;"
        " border-radius: 8px;"
        " color: #16202b;"
        " font-weight: 700;"
        " padding: 0;"
        "}"
        "QPushButton:hover {"
        " background: #eef3f8;"
        " border-color: #b8c6d6;"
        "}");
    return button;
}

}  // namespace

SimulationCanvasWidget::SimulationCanvasWidget(safecrowd::domain::FacilityLayout2D layout, QWidget* parent)
    : QWidget(parent),
      layout_(std::move(layout)) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(520, 360);
    setCursor(Qt::OpenHandCursor);
    setStyleSheet("QWidget { background: #f4f7fb; }");
    camera_.setPrimaryButtonPanEnabled(true);
    layoutBounds_ = collectLayoutCanvasBounds(layout_);
    createViewControls();
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
    if (focusedHotspotIndex_.has_value() && *focusedHotspotIndex_ >= hotspotOverlay_.size()) {
        focusedHotspotIndex_.reset();
    }
    update();
}

void SimulationCanvasWidget::setBottleneckOverlay(std::vector<safecrowd::domain::ScenarioBottleneckMetric> bottlenecks) {
    bottleneckOverlay_ = std::move(bottlenecks);
    if (focusedBottleneckIndex_.has_value() && *focusedBottleneckIndex_ >= bottleneckOverlay_.size()) {
        focusedBottleneckIndex_.reset();
    }
    update();
}

void SimulationCanvasWidget::focusHotspot(std::size_t index) {
    if (index >= hotspotOverlay_.size()) {
        return;
    }

    focusedHotspotIndex_ = index;
    focusedBottleneckIndex_.reset();
    focusWorldPoint(hotspotOverlay_[index].center, std::max(camera_.zoom(), kHotspotFocusZoom));
}

void SimulationCanvasWidget::focusBottleneck(std::size_t index) {
    if (index >= bottleneckOverlay_.size()) {
        return;
    }

    const auto& passage = bottleneckOverlay_[index].passage;
    focusedBottleneckIndex_ = index;
    focusedHotspotIndex_.reset();
    focusWorldPoint(
        {.x = (passage.start.x + passage.end.x) / 2.0, .y = (passage.start.y + passage.end.y) / 2.0},
        std::max(camera_.zoom(), kBottleneckFocusZoom));
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
        resetView();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void SimulationCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (camera_.updatePan(event)) {
        invalidateLayoutCache();
        update();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void SimulationCanvasWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (camera_.beginPan(event)) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    QWidget::mousePressEvent(event);
}

void SimulationCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (camera_.finishPan(event)) {
        setCursor(Qt::OpenHandCursor);
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
    drawBottleneckOverlay(painter, transform);
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

void SimulationCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    positionViewControls();
}

void SimulationCanvasWidget::wheelEvent(QWheelEvent* event) {
    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        QWidget::wheelEvent(event);
        return;
    }
    if (camera_.zoomAt(event, *bounds, previewViewport())) {
        invalidateLayoutCache();
        update();
        return;
    }
    QWidget::wheelEvent(event);
}

void SimulationCanvasWidget::createViewControls() {
    viewControls_ = new QFrame(this);
    viewControls_->setObjectName("simulationViewControls");
    viewControls_->setStyleSheet(
        "#simulationViewControls {"
        " background: rgba(255, 255, 255, 230);"
        " border: 1px solid #d7e0ea;"
        " border-radius: 12px;"
        "}"
    );
    auto* layout = new QHBoxLayout(viewControls_);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* zoomInButton = createViewControlButton("+", "Zoom in", viewControls_);
    auto* zoomOutButton = createViewControlButton("-", "Zoom out", viewControls_);
    auto* fitButton = createViewControlButton("Fit", "Fit to view", viewControls_);
    layout->addWidget(zoomInButton);
    layout->addWidget(zoomOutButton);
    layout->addWidget(fitButton);

    connect(zoomInButton, &QPushButton::clicked, this, [this]() {
        zoomAtCanvasPoint(rect().center(), 1.2);
    });
    connect(zoomOutButton, &QPushButton::clicked, this, [this]() {
        zoomAtCanvasPoint(rect().center(), 1.0 / 1.2);
    });
    connect(fitButton, &QPushButton::clicked, this, [this]() {
        resetView();
    });

    viewControls_->adjustSize();
    positionViewControls();
}

void SimulationCanvasWidget::positionViewControls() {
    if (viewControls_ == nullptr) {
        return;
    }

    viewControls_->adjustSize();
    const int margin = 14;
    viewControls_->move(width() - viewControls_->width() - margin, margin);
    viewControls_->raise();
}

void SimulationCanvasWidget::resetView() {
    camera_.reset();
    invalidateLayoutCache();
    update();
}

void SimulationCanvasWidget::zoomAtCanvasPoint(const QPointF& anchorPoint, double factor) {
    const auto bounds = collectBounds();
    if (!bounds.has_value() || factor <= 0.0) {
        return;
    }

    const auto viewport = previewViewport();
    if (viewport.width() <= 0.0 || viewport.height() <= 0.0) {
        return;
    }

    const LayoutCanvasTransform currentTransform(*bounds, viewport, camera_.zoom(), camera_.panOffset());
    const auto anchorWorld = currentTransform.unmap(anchorPoint);
    camera_.setZoom(std::clamp(camera_.zoom() * factor, 0.1, 50.0));

    const LayoutCanvasTransform updatedTransform(*bounds, viewport, camera_.zoom(), camera_.panOffset());
    camera_.setPanOffset(camera_.panOffset() + anchorPoint - updatedTransform.map(anchorWorld));
    invalidateLayoutCache();
    update();
}

void SimulationCanvasWidget::invalidateLayoutCache() {
    layoutCacheValid_ = false;
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

    const auto transform = currentTransform(bounds);
    drawLayoutCanvasSurface(painter, QRectF(rect()));
    drawFacilityLayoutCanvas(painter, layout_, transform);

    layoutCacheSize_ = currentSize;
    layoutCacheZoom_ = camera_.zoom();
    layoutCachePan_ = camera_.panOffset();
    layoutCacheValid_ = true;
}

QRectF SimulationCanvasWidget::previewViewport() const {
    return QRectF(rect()).adjusted(kViewportPadding, kViewportPadding, -kViewportPadding, -kViewportPadding);
}

void SimulationCanvasWidget::focusWorldPoint(const safecrowd::domain::Point2D& point, double zoom) {
    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        return;
    }

    const auto viewport = previewViewport();
    if (viewport.width() <= 0.0 || viewport.height() <= 0.0) {
        return;
    }

    camera_.setZoom(std::clamp(zoom, 0.1, 50.0));
    camera_.setPanOffset({});

    const LayoutCanvasTransform transform(*bounds, viewport, camera_.zoom(), {});
    camera_.setPanOffset(viewport.center() - transform.map(point));
    invalidateLayoutCache();
    update();
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

        if (focusedHotspotIndex_.has_value()
            && *focusedHotspotIndex_ < hotspotOverlay_.size()
            && &hotspot == &hotspotOverlay_[*focusedHotspotIndex_]) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(127, 29, 29, 220), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawEllipse(center, radius + 4.0, radius + 4.0);
            painter.setPen(Qt::NoPen);
        }
    }
    painter.restore();
}

void SimulationCanvasWidget::drawBottleneckOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (bottleneckOverlay_.empty()) {
        return;
    }

    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (std::size_t index = 0; index < bottleneckOverlay_.size(); ++index) {
        const auto focused = focusedBottleneckIndex_.has_value() && *focusedBottleneckIndex_ == index;
        painter.setPen(QPen(
            focused ? QColor(127, 29, 29, 235) : QColor(220, 38, 38, 150),
            focused ? 6.0 : 4.0,
            Qt::SolidLine,
            Qt::RoundCap));
        painter.drawLine(
            transform.map(bottleneckOverlay_[index].passage.start),
            transform.map(bottleneckOverlay_[index].passage.end));
    }
    painter.restore();
}

}  // namespace safecrowd::application
