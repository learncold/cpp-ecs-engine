#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QPointF>
#include <QStringList>
#include <QString>
#include <QWidget>

#include "application/LayoutCanvasRendering.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"

class QFrame;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QLabel;
class QComboBox;
class QSpinBox;
class QToolButton;
class QWheelEvent;

namespace safecrowd::application {

enum class ScenarioCrowdPlacementKind {
    Individual,
    Group,
    Source,
};

struct ScenarioCrowdPlacement {
    QString id{};
    QString name{};
    ScenarioCrowdPlacementKind kind{ScenarioCrowdPlacementKind::Individual};
    QString zoneId{};
    QString floorId{};
    std::vector<safecrowd::domain::Point2D> area{};
    int occupantCount{1};
    safecrowd::domain::Point2D velocity{};
    safecrowd::domain::InitialPlacementDistribution distribution{
        safecrowd::domain::InitialPlacementDistribution::Uniform};
    std::vector<safecrowd::domain::Point2D> generatedPositions{};
    int sourceAgentsPerSpawn{1};
    double sourceStartSeconds{0.0};
    double sourceEndSeconds{180.0};
    double sourceIntervalSeconds{5.0};
};

enum class ScenarioCanvasSelectionKind {
    None,
    LayoutElement,
    CrowdPlacement,
    ConnectionBlock,
    EnvironmentHazard,
    RouteGuidance,
};

struct ScenarioCanvasSelection {
    ScenarioCanvasSelectionKind kind{ScenarioCanvasSelectionKind::None};
    QString id{};
};

class ScenarioCanvasWidget : public QWidget {
public:
    explicit ScenarioCanvasWidget(
        safecrowd::domain::FacilityLayout2D layout,
        QWidget* parent = nullptr);
    ~ScenarioCanvasWidget() override;

    void setPlacements(std::vector<ScenarioCrowdPlacement> placements);
    void setPlacementsChangedHandler(std::function<void(const std::vector<ScenarioCrowdPlacement>&)> handler);
    void setConnectionBlocks(std::vector<safecrowd::domain::ConnectionBlockDraft> blocks);
    void setConnectionBlocksChangedHandler(std::function<void(const std::vector<safecrowd::domain::ConnectionBlockDraft>&)> handler);
    void setEnvironmentHazards(std::vector<safecrowd::domain::EnvironmentHazardDraft> hazards);
    void setEnvironmentHazardsChangedHandler(std::function<void(const std::vector<safecrowd::domain::EnvironmentHazardDraft>&)> handler);
    void setRouteGuidances(std::vector<safecrowd::domain::RouteGuidanceDraft> guidances);
    void setRouteGuidancesChangedHandler(std::function<void(const std::vector<safecrowd::domain::RouteGuidanceDraft>&)> handler);
    void setScenarioElementSelectionChangedHandler(std::function<void(const ScenarioCanvasSelection&)> handler);
    void setLayoutElementActivatedHandler(std::function<void(const QString&)> handler);
    void setCrowdSelectionChangedHandler(std::function<void(const QString&)> handler);
    void focusLayoutElement(const QString& elementId);
    void activateLayoutElement(const QString& elementId);
    void focusPlacement(const QString& placementId);
    bool deleteCrowdElementById(const QString& crowdElementId);
    bool deleteConnectionBlockById(const QString& blockId);
    bool editConnectionBlockScheduleById(const QString& blockId);
    bool deleteEnvironmentHazardById(const QString& hazardId);
    bool deleteRouteGuidanceById(const QString& guidanceId);
    bool editRouteGuidanceById(const QString& guidanceId);

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
    enum class ToolMode {
        Select,
        IndividualPlacement,
        GroupPlacement,
        SourcePlacement,
        BlockDoor,
        FireHazard,
        SmokeHazard,
        RouteGuidance,
    };

    enum class DraggableEventKind {
        ConnectionBlock,
        EnvironmentHazard,
        RouteGuidance,
    };

    struct EventDragState {
        DraggableEventKind kind{DraggableEventKind::EnvironmentHazard};
        std::size_t index{0};
        std::optional<safecrowd::domain::ConnectionBlockDraft> originalBlock{};
        std::optional<safecrowd::domain::EnvironmentHazardDraft> originalHazard{};
        std::optional<safecrowd::domain::RouteGuidanceDraft> originalGuidance{};
        QPointF previewScreenPosition{};
        bool hasValidPreview{true};
        QString invalidReason{};
    };

    std::optional<LayoutCanvasBounds> collectBounds() const;
    LayoutCanvasTransform currentTransform(const LayoutCanvasBounds& bounds) const;
    QRectF previewViewport() const;
    bool switchFloorByWheel(QWheelEvent* event);
    void selectFloorForElement(const QString& elementId);
    safecrowd::domain::Point2D unmapPoint(const QPointF& point) const;
    QString zoneAt(const safecrowd::domain::Point2D& point) const;
    const safecrowd::domain::Connection2D* connectionAt(const safecrowd::domain::Point2D& point, double toleranceWorldUnits) const;
    const safecrowd::domain::Connection2D* controlConnectionAt(const safecrowd::domain::Point2D& point, double toleranceWorldUnits) const;
    const safecrowd::domain::Barrier2D* barrierAt(const safecrowd::domain::Point2D& point, double toleranceWorldUnits) const;
    safecrowd::domain::Point2D connectionCenter(const safecrowd::domain::Connection2D& connection) const;
    QString placementAt(const QPointF& position, const LayoutCanvasTransform& transform, double pickPadding = 0.0) const;
    QString selectedPlacementAt(const QPointF& position, const LayoutCanvasTransform& transform) const;
    void addGroupPlacement(const QPointF& start, const QPointF& end);
    void addIndividualPlacement(const QPointF& position);
    void addSourcePlacement(const QPointF& position);
    void addConnectionBlock(const QPointF& position);
    void addConnectionBlockForConnection(const safecrowd::domain::Connection2D& connection);
    void addEnvironmentHazard(const QPointF& position, safecrowd::domain::EnvironmentHazardKind kind);
    void addEnvironmentHazardForZone(
        const safecrowd::domain::Zone2D& zone,
        safecrowd::domain::Point2D position,
        safecrowd::domain::EnvironmentHazardKind kind);
    void addRouteGuidance(const QPointF& position);
    void addRouteGuidanceForZonePosition(
        const safecrowd::domain::Zone2D& zone,
        safecrowd::domain::Point2D position);
    void addRouteGuidanceForExitZone(const safecrowd::domain::Zone2D& zone);
    void addRouteGuidanceForConnection(const safecrowd::domain::Connection2D& connection);
    bool beginEventDrag(const QPointF& position, const LayoutCanvasTransform& transform);
    void restoreDraggedEventOriginal();
    void updateEventDragPreview(const QPointF& position);
    void finishEventDrag();
    bool tryMoveConnectionBlock(std::size_t index, const QPointF& position, QString* errorMessage);
    bool tryMoveEnvironmentHazard(std::size_t index, const QPointF& position, QString* errorMessage);
    bool tryMoveRouteGuidance(std::size_t index, const QPointF& position, QString* errorMessage);
    void openRouteGuidanceEditor(const QString& guidanceId, const QPoint& screenPosition);
    void selectSingleAt(const QPointF& position, const LayoutCanvasTransform& transform);
    void selectPlacementsInRect(const QRectF& screenRect, const LayoutCanvasTransform& transform);
    void selectLayoutElementAt(const QPointF& position);
    void openCrowdPlacementContextMenu(const QString& crowdElementId, const QPoint& screenPosition);
    void openConnectionBlockScheduleEditor(const QString& blockId, const QPoint& screenPosition);
    bool editOccupantSourceById(const QString& sourceId, const QPoint& screenPosition);
    bool deleteCrowdElement(const QString& crowdElementId);
    void drawFocusedLayoutElement(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawFocusedPlacement(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawConnectionBlocks(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawEnvironmentHazards(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawRouteGuidances(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawDraggedEventPreview(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void emitScenarioSelection(ScenarioCanvasSelectionKind kind, const QString& id);
    void emitPlacementsChanged();
    void emitConnectionBlocksChanged();
    void emitEnvironmentHazardsChanged();
    void emitRouteGuidancesChanged();
    bool configureSourcePlacementTool(const QPoint& screenPosition);
    void repositionToolbars();
    void setToolMode(ToolMode mode);
    void setupToolbars();

    safecrowd::domain::FacilityLayout2D layout_{};
    std::vector<ScenarioCrowdPlacement> placements_{};
    std::vector<safecrowd::domain::ConnectionBlockDraft> connectionBlocks_{};
    std::vector<safecrowd::domain::EnvironmentHazardDraft> environmentHazards_{};
    std::vector<safecrowd::domain::RouteGuidanceDraft> routeGuidances_{};
    QString currentFloorId_{};
    QString focusedLayoutElementId_{};
    QString focusedCrowdElementId_{};
    QString focusedPlacementId_{};
    QStringList selectedPlacementIds_{};
    ToolMode toolMode_{ToolMode::Select};
    LayoutCanvasCamera camera_{};
    QPointF dragStart_{};
    QPointF dragCurrent_{};
    QPointF selectionDragStart_{};
    QPointF selectionDragCurrent_{};
    bool dragging_{false};
    bool selectionDragging_{false};
    bool eventDragging_{false};
    std::optional<EventDragState> eventDragState_{};
    QFrame* topToolbar_{nullptr};
    QFrame* propertyPanel_{nullptr};
    QToolButton* selectToolButton_{nullptr};
    QToolButton* individualToolButton_{nullptr};
    QToolButton* groupToolButton_{nullptr};
    QToolButton* sourceToolButton_{nullptr};
    QToolButton* blockDoorToolButton_{nullptr};
    QToolButton* fireHazardToolButton_{nullptr};
    QToolButton* smokeHazardToolButton_{nullptr};
    QToolButton* routeGuidanceToolButton_{nullptr};
    QLabel* groupCountLabel_{nullptr};
    QSpinBox* groupCountSpinBox_{nullptr};
    QLabel* groupDistributionLabel_{nullptr};
    QComboBox* groupDistributionComboBox_{nullptr};
    int sourceAgentsPerSpawn_{1};
    double sourceStartSeconds_{0.0};
    double sourceDurationSeconds_{180.0};
    double sourceIntervalSeconds_{5.0};
    QString hoveredConnectionBlockId_{};
    QString hoveredEnvironmentHazardId_{};
    QString hoveredRouteGuidanceId_{};
    std::function<void(const ScenarioCanvasSelection&)> scenarioElementSelectionChangedHandler_{};
    std::function<void(const QString&)> layoutElementActivatedHandler_{};
    std::function<void(const QString&)> crowdSelectionChangedHandler_{};
    std::function<void(const std::vector<ScenarioCrowdPlacement>&)> placementsChangedHandler_{};
    std::function<void(const std::vector<safecrowd::domain::ConnectionBlockDraft>&)> connectionBlocksChangedHandler_{};
    std::function<void(const std::vector<safecrowd::domain::EnvironmentHazardDraft>&)> environmentHazardsChangedHandler_{};
    std::function<void(const std::vector<safecrowd::domain::RouteGuidanceDraft>&)> routeGuidancesChangedHandler_{};
};

}  // namespace safecrowd::application
