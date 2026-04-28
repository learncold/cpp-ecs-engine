#pragma once

#include <optional>
#include <vector>

#include <QPixmap>
#include <QWidget>

#include "application/LayoutCanvasRendering.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationRunner.h"

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QWheelEvent;

namespace safecrowd::application {

class SimulationCanvasWidget : public QWidget {
public:
    explicit SimulationCanvasWidget(safecrowd::domain::FacilityLayout2D layout, QWidget* parent = nullptr);
    ~SimulationCanvasWidget() override;

    void setFrame(safecrowd::domain::SimulationFrame frame);
    void setHotspotOverlay(std::vector<safecrowd::domain::ScenarioCongestionHotspot> hotspots);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    std::optional<LayoutCanvasBounds> collectBounds() const;
    LayoutCanvasTransform currentTransform(const LayoutCanvasBounds& bounds) const;
    void refreshLayoutCache(const LayoutCanvasBounds& bounds);
    QRectF previewViewport() const;
    void drawHotspotOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;

    safecrowd::domain::FacilityLayout2D layout_{};
    safecrowd::domain::SimulationFrame frame_{};
    std::vector<safecrowd::domain::ScenarioCongestionHotspot> hotspotOverlay_{};
    LayoutCanvasCamera camera_{};
    std::optional<LayoutCanvasBounds> layoutBounds_{};
    QPixmap layoutCache_{};
    QSize layoutCacheSize_{};
    QPointF layoutCachePan_{};
    double layoutCacheZoom_{0.0};
    bool layoutCacheValid_{false};
};

}  // namespace safecrowd::application
