#include "application/SimulationCanvasWidget.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <QCoreApplication>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QSignalBlocker>
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
constexpr int kFloorSelectorMargin = 14;

std::string defaultFloorId(const safecrowd::domain::FacilityLayout2D& layout) {
    if (!layout.floors.empty() && !layout.floors.front().id.empty()) {
        return layout.floors.front().id;
    }
    return layout.levelId;
}

bool matchesFloor(const std::string& elementFloorId, const std::string& floorId) {
    return floorId.empty() || elementFloorId.empty() || elementFloorId == floorId;
}

bool intervalContains(const safecrowd::domain::ConnectionBlockIntervalDraft& interval, double timeSeconds) {
    const auto start = std::max(0.0, interval.startSeconds);
    const auto end = std::max(start, interval.endSeconds);
    return timeSeconds + 1e-9 >= start && timeSeconds <= end + 1e-9;
}

bool connectionShouldBeBlocked(const safecrowd::domain::ConnectionBlockDraft& block, double timeSeconds) {
    if (block.connectionId.empty()) {
        return false;
    }
    if (block.intervals.empty()) {
        return true;
    }
    for (const auto& interval : block.intervals) {
        if (intervalContains(interval, timeSeconds)) {
            return true;
        }
    }
    return false;
}

safecrowd::domain::Point2D connectionCenter(const safecrowd::domain::Connection2D& connection) {
    return {
        .x = (connection.centerSpan.start.x + connection.centerSpan.end.x) * 0.5,
        .y = (connection.centerSpan.start.y + connection.centerSpan.end.y) * 0.5,
    };
}

}  // namespace

SimulationCanvasWidget::SimulationCanvasWidget(safecrowd::domain::FacilityLayout2D layout, QWidget* parent)
    : QWidget(parent),
      layout_(std::move(layout)) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(520, 360);
    setStyleSheet("QWidget { background: #f4f7fb; }");
    currentFloorId_ = defaultFloorId(layout_);
    layoutBounds_ = collectLayoutCanvasBounds(layout_, currentFloorId_);
    QCoreApplication::instance()->installEventFilter(this);
    setupFloorSelector();
}

SimulationCanvasWidget::~SimulationCanvasWidget() {
    if (auto* app = QCoreApplication::instance(); app != nullptr) {
        app->removeEventFilter(this);
    }
}

void SimulationCanvasWidget::setFrame(safecrowd::domain::SimulationFrame frame) {
    frame_ = std::move(frame);
    const auto firstAgentFloor = std::find_if(frame_.agents.begin(), frame_.agents.end(), [](const auto& agent) {
        return !agent.floorId.empty();
    });
    const auto hasAgentOnCurrentFloor = std::any_of(frame_.agents.begin(), frame_.agents.end(), [this](const auto& agent) {
        return matchesFloor(agent.floorId, currentFloorId_);
    });
    if (!manualFloorSelection_
        && !hasAgentOnCurrentFloor
        && firstAgentFloor != frame_.agents.end()
        && firstAgentFloor->floorId != currentFloorId_) {
        setCurrentFloorId(firstAgentFloor->floorId, false);
    }
    update();
}

void SimulationCanvasWidget::setConnectionBlocks(std::vector<safecrowd::domain::ConnectionBlockDraft> blocks) {
    connectionBlocks_ = std::move(blocks);
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
    drawConnectionBlockOverlay(painter, transform);
    drawHotspotOverlay(painter, transform);
    drawBottleneckOverlay(painter, transform);
    for (const auto& agent : frame_.agents) {
        if (!matchesFloor(agent.floorId, currentFloorId_)) {
            continue;
        }
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
    repositionFloorSelector();
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

    const auto transform = currentTransform(bounds);
    drawLayoutCanvasSurface(painter, QRectF(rect()));
    drawFacilityLayoutCanvas(painter, layout_, transform, currentFloorId_);

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
    layoutCacheValid_ = false;
    update();
}

void SimulationCanvasWidget::drawConnectionBlockOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (connectionBlocks_.empty()) {
        return;
    }

    const auto elapsedSeconds = std::max(0.0, frame_.elapsedSeconds);

    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor("#c0392b"), 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    for (const auto& block : connectionBlocks_) {
        if (!connectionShouldBeBlocked(block, elapsedSeconds)) {
            continue;
        }

        const auto it = std::find_if(layout_.connections.begin(), layout_.connections.end(), [&](const auto& connection) {
            return connection.id == block.connectionId;
        });
        if (it == layout_.connections.end()) {
            continue;
        }
        if (!matchesFloor(it->floorId, currentFloorId_)) {
            continue;
        }

        const auto center = transform.map(connectionCenter(*it));
        const double r = 10.0;
        painter.drawEllipse(center, r, r);
        painter.drawLine(QPointF(center.x() - 6.5, center.y() + 6.5), QPointF(center.x() + 6.5, center.y() - 6.5));
    }

    painter.restore();
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
        if (!matchesFloor(zone.floorId, currentFloorId_)) {
            continue;
        }
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

void SimulationCanvasWidget::setCurrentFloorId(std::string floorId, bool manualSelection) {
    if (floorId == currentFloorId_ && manualSelection == manualFloorSelection_) {
        return;
    }

    currentFloorId_ = std::move(floorId);
    manualFloorSelection_ = manualSelection;
    layoutBounds_ = collectLayoutCanvasBounds(layout_, currentFloorId_);
    layoutCacheValid_ = false;
    camera_.reset();

    if (floorComboBox_ != nullptr) {
        const auto index = floorComboBox_->findData(QString::fromStdString(currentFloorId_));
        if (index >= 0 && floorComboBox_->currentIndex() != index) {
            const QSignalBlocker blocker(floorComboBox_);
            floorComboBox_->setCurrentIndex(index);
        }
    }
    update();
}

void SimulationCanvasWidget::setupFloorSelector() {
    if (layout_.floors.size() <= 1) {
        return;
    }

    floorSelectorFrame_ = new QFrame(this);
    floorSelectorFrame_->setObjectName("simulationFloorSelector");
    floorSelectorFrame_->setStyleSheet(
        "QFrame#simulationFloorSelector {"
        " background: rgba(255, 255, 255, 238);"
        " border: 1px solid #d8e2ee;"
        " border-radius: 10px;"
        "}"
        "QLabel { color: #4f5d6b; background: transparent; font-size: 12px; }"
        "QComboBox {"
        " background: #ffffff;"
        " border: 1px solid #cad6e3;"
        " border-radius: 7px;"
        " padding: 4px 24px 4px 8px;"
        " color: #16202b;"
        " min-width: 116px;"
        "}");

    auto* layout = new QHBoxLayout(floorSelectorFrame_);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);
    auto* label = new QLabel("Floor", floorSelectorFrame_);
    floorComboBox_ = new QComboBox(floorSelectorFrame_);
    for (const auto& floor : layout_.floors) {
        const auto id = QString::fromStdString(floor.id);
        const auto labelText = floor.label.empty()
            ? id
            : QString("%1 (%2)").arg(QString::fromStdString(floor.label), id);
        floorComboBox_->addItem(labelText, id);
    }
    const auto currentIndex = floorComboBox_->findData(QString::fromStdString(currentFloorId_));
    if (currentIndex >= 0) {
        floorComboBox_->setCurrentIndex(currentIndex);
    }
    layout->addWidget(label);
    layout->addWidget(floorComboBox_);

    connect(floorComboBox_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (floorComboBox_ == nullptr || index < 0) {
            return;
        }
        setCurrentFloorId(floorComboBox_->itemData(index).toString().toStdString(), true);
    });

    floorSelectorFrame_->adjustSize();
    repositionFloorSelector();
    floorSelectorFrame_->raise();
}

void SimulationCanvasWidget::repositionFloorSelector() {
    if (floorSelectorFrame_ == nullptr) {
        return;
    }
    floorSelectorFrame_->adjustSize();
    floorSelectorFrame_->move(kFloorSelectorMargin, kFloorSelectorMargin);
    floorSelectorFrame_->raise();
}

}  // namespace safecrowd::application
