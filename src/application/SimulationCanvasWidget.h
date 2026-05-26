#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <QPixmap>
#include <QWidget>

#include "application/LayoutCanvasRendering.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationRunner.h"

class QEvent;
class QComboBox;
class QFrame;
class QKeyEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace safecrowd::application {

enum class ResultOverlayMode {
    None,
    Occupancy,
    Density,
    Pressure,
    Hotspots,
    Bottlenecks,
    CrossFlow,
};

class SimulationCanvasWidget : public QWidget {
public:
    explicit SimulationCanvasWidget(safecrowd::domain::FacilityLayout2D layout, QWidget* parent = nullptr);
    ~SimulationCanvasWidget() override;

    void setFrame(safecrowd::domain::SimulationFrame frame);
    void setConnectionBlocks(std::vector<safecrowd::domain::ConnectionBlockDraft> blocks);
    void setEnvironmentHazards(std::vector<safecrowd::domain::EnvironmentHazardDraft> hazards);
    void setRouteGuidances(std::vector<safecrowd::domain::RouteGuidanceDraft> guidances);
    void setDensityOverlay(
        std::vector<safecrowd::domain::DensityCellMetric> densityCells,
        double scaleMaxPeoplePerSquareMeter = 4.0);
    void setOccupancyHeatmapOverlay(safecrowd::domain::OccupancyHeatmap heatmap);
    void setPressureOverlay(
        std::vector<safecrowd::domain::PressureCellMetric> pressureCells,
        double scaleMaxPressureScore = 1.0);
    void setHotspotOverlay(std::vector<safecrowd::domain::ScenarioCongestionHotspot> hotspots);
    void setBottleneckOverlay(std::vector<safecrowd::domain::ScenarioBottleneckMetric> bottlenecks);
    void setCrossFlowOverlay(std::vector<safecrowd::domain::ScenarioCrossFlowCellMetric> cells);
    void setResultOverlayMode(ResultOverlayMode mode);
    void focusHotspot(std::size_t index);
    void focusBottleneck(std::size_t index);
    void focusCrossFlowCell(std::size_t index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    std::optional<LayoutCanvasBounds> collectBounds() const;
    LayoutCanvasTransform currentTransform(const LayoutCanvasBounds& bounds) const;
    void refreshLayoutCache(const LayoutCanvasBounds& bounds);
    void refreshOverlayCache(const LayoutCanvasBounds& bounds);
    void invalidateOverlayCache();
    QRectF previewViewport() const;
    void focusWorldPoint(const safecrowd::domain::Point2D& point, double zoom);
    void drawConnectionBlockOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawEnvironmentHazardOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawRouteGuidanceOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawOccupancyHeatmapOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawDensityOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawPressureOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawHotspotOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawBottleneckOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawCrossFlowOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const;
    bool switchFloorByWheel(QWheelEvent* event);
    void setCurrentFloorId(std::string floorId, bool manualSelection);
    void setupFloorSelector();
    void repositionFloorSelector();

    safecrowd::domain::FacilityLayout2D layout_{};
    safecrowd::domain::SimulationFrame frame_{};
    std::vector<safecrowd::domain::ConnectionBlockDraft> connectionBlocks_{};
    std::vector<safecrowd::domain::EnvironmentHazardDraft> environmentHazards_{};
    std::vector<safecrowd::domain::RouteGuidanceDraft> routeGuidances_{};
    safecrowd::domain::OccupancyHeatmap occupancyHeatmapOverlay_{};
    std::vector<safecrowd::domain::DensityCellMetric> densityOverlay_{};
    double densityScaleMaxPeoplePerSquareMeter_{4.0};
    std::vector<safecrowd::domain::PressureCellMetric> pressureOverlay_{};
    double pressureScaleMaxScore_{1.0};
    std::vector<safecrowd::domain::ScenarioCongestionHotspot> hotspotOverlay_{};
    std::vector<safecrowd::domain::ScenarioBottleneckMetric> bottleneckOverlay_{};
    std::vector<safecrowd::domain::ScenarioCrossFlowCellMetric> crossFlowCellOverlay_{};
    ResultOverlayMode overlayMode_{ResultOverlayMode::None};
    std::optional<std::size_t> focusedHotspotIndex_{};
    std::optional<std::size_t> focusedBottleneckIndex_{};
    std::optional<std::size_t> focusedCrossFlowCellIndex_{};
    LayoutCanvasCamera camera_{};
    std::optional<LayoutCanvasBounds> layoutBounds_{};
    std::string currentFloorId_{};
    bool manualFloorSelection_{false};
    QFrame* floorSelectorFrame_{nullptr};
    QComboBox* floorComboBox_{nullptr};
    QPixmap layoutCache_{};
    QSize layoutCacheSize_{};
    QPointF layoutCachePan_{};
    double layoutCacheZoom_{0.0};
    double layoutCacheDevicePixelRatio_{0.0};
    bool layoutCacheValid_{false};
    QPixmap overlayCache_{};
    QSize overlayCacheSize_{};
    QPointF overlayCachePan_{};
    double overlayCacheZoom_{0.0};
    double overlayCacheDevicePixelRatio_{0.0};
    ResultOverlayMode overlayCacheMode_{ResultOverlayMode::None};
    std::string overlayCacheFloorId_{};
    bool overlayCacheValid_{false};

    std::string hoveredConnectionBlockId_{};
    std::string hoveredEnvironmentHazardId_{};
    std::string hoveredRouteGuidanceId_{};
};

}  // namespace safecrowd::application
