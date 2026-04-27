#include "application/SimulationCanvasWidget.h"

#include <utility>

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

constexpr double kViewportPadding = 32.0;
constexpr double kVelocityIndicatorSeconds = 0.75;
constexpr double kAgentMarkerRadius = 5.0;

}  // namespace

SimulationCanvasWidget::SimulationCanvasWidget(safecrowd::domain::FacilityLayout2D layout, QWidget* parent)
    : QWidget(parent),
      layout_(std::move(layout)) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(520, 360);
    setStyleSheet("QWidget { background: #f4f7fb; }");
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
        update();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void SimulationCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (camera_.updatePan(event)) {
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
    const auto viewport = previewViewport();
    if (!bounds.has_value()) {
        painter.setPen(QColor("#4f5d6b"));
        painter.drawText(rect(), Qt::AlignCenter, "No layout available");
        return;
    }

    const auto transform = currentTransform(*bounds);
    painter.setPen(QPen(QColor("#d7e0ea"), 1));
    painter.setBrush(QColor("#ffffff"));
    painter.drawRoundedRect(viewport.adjusted(-18, -18, 18, 18), 14, 14);

    drawFacilityLayoutCanvas(painter, layout_, transform);

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
        update();
        return;
    }
    QWidget::wheelEvent(event);
}

std::optional<LayoutCanvasBounds> SimulationCanvasWidget::collectBounds() const {
    return collectLayoutCanvasBounds(layout_);
}

LayoutCanvasTransform SimulationCanvasWidget::currentTransform(const LayoutCanvasBounds& bounds) const {
    return LayoutCanvasTransform(bounds, previewViewport(), camera_.zoom(), camera_.panOffset());
}

QRectF SimulationCanvasWidget::previewViewport() const {
    return QRectF(rect()).adjusted(kViewportPadding, kViewportPadding, -kViewportPadding, -kViewportPadding);
}

}  // namespace safecrowd::application
