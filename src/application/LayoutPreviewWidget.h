#pragma once

#include <functional>
#include <vector>

#include <QString>
#include <QStringList>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QWidget>

#include "domain/FacilityLayout2D.h"
#include "domain/ImportResult.h"
#include "application/LayoutCanvasRendering.h"

class QFrame;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QKeyEvent;
class QLabel;
class QMouseEvent;
class QToolButton;
class QWheelEvent;

namespace safecrowd::application {

enum class PreviewSelectionKind {
    None,
    Multiple,
    Zone,
    Connection,
    Barrier,
};

struct PreviewSelection {
    PreviewSelectionKind kind{PreviewSelectionKind::None};
    QString id{};
    QString title{};
    QString detail{};

    bool empty() const noexcept {
        return kind == PreviewSelectionKind::None || id.isEmpty();
    }
};

class LayoutPreviewWidget : public QWidget {
public:
    explicit LayoutPreviewWidget(safecrowd::domain::ImportResult importResult, QWidget* parent = nullptr);
    ~LayoutPreviewWidget() override;

    void focusElement(const QString& elementId);
    void focusIssueTarget(const QString& targetId);
    void resetView();
    void setImportResult(safecrowd::domain::ImportResult importResult);
    void setSelectionChangedHandler(std::function<void(const PreviewSelection&)> handler);
    void setLayoutEditedHandler(std::function<void(const safecrowd::domain::FacilityLayout2D&)> handler);
    bool updateElementVertices(
        PreviewSelectionKind kind,
        const QString& elementId,
        const std::vector<safecrowd::domain::Point2D>& vertices);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
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
        DrawRoom,
        DrawExit,
        DrawWall,
        DrawObstruction,
        DrawDoor,
        DrawStair,
        DrawUStair,
    };

    enum class ShapeDrawMode {
        Rectangle,
        Polygon,
    };

    void applyToolAt(const QPointF& position);
    void clearSelection();
    void createConnection(const QPointF& startWorld, const QPointF& endWorld);
    void createDoorAt(const QString& barrierId, const QPointF& position);
    void createObstructionPolygon(const std::vector<QPointF>& points);
    void createObstructionRectangle(const QPointF& startWorld, const QPointF& endWorld);
    void createRoomPolygon(const std::vector<QPointF>& points);
    void createUShapedStairLink(const QPointF& startWorld, const QPointF& endWorld);
    void createVerticalLink(const QPointF& startWorld, const QPointF& endWorld);
    void createWallSegment(const QPointF& startWorld, const QPointF& endWorld);
    void createZone(const QPointF& startWorld, const QPointF& endWorld, safecrowd::domain::ZoneKind kind);
    void deleteConnection(const QString& connectionId);
    void deleteBarrier(const QString& barrierId);
    void deleteSelectedElements();
    void emitCurrentSelection();
    void finishPolygonDraft();
    bool hasSelection() const;
    bool isSelected(PreviewSelectionKind kind, const QString& id) const;
    void pruneSelection();
    void beginSelectionMove(const QPointF& position, const LayoutCanvasTransform& transform, const LayoutCanvasBounds& bounds);
    void updateSelectionMove(const QPointF& position);
    void finishSelectionMove(const QPointF& position);
    void applySelectedTranslation(double dx, double dy);
    safecrowd::domain::Point2D selectionMoveDeltaForPosition(const QPointF& position) const;
    safecrowd::domain::FacilityLayout2D selectionMoveSnapTargetLayout(const safecrowd::domain::FacilityLayout2D& layout) const;
    std::vector<safecrowd::domain::Point2D> selectionMoveAnchors(const safecrowd::domain::FacilityLayout2D& layout) const;
    QStringList splitZoneBoundaryBarriersForSelection(safecrowd::domain::FacilityLayout2D& layout) const;
    QStringList zoneBoundaryBarrierIdsForSelection(const safecrowd::domain::FacilityLayout2D& layout) const;
    void updateHoverCursor(const QPointF& position);
    QPointF snapDragWorldPoint(
        const QPointF& anchorWorldPoint,
        const QPointF& worldPoint,
        const LayoutCanvasTransform& transform) const;
    QPointF snapWorldPoint(const QPointF& worldPoint, const LayoutCanvasTransform& transform) const;
    void notifyLayoutEdited();
    void repositionToolbars();
    void refreshFloorSelector();
    void refreshPropertyPanel();
    void selectBarrier(const QString& barrierId);
    void selectConnection(const QString& connectionId);
    void selectElementsInRect(const QRectF& screenRect, const LayoutCanvasTransform& transform);
    void selectFloorForElement(const QString& elementId);
    void selectPrimaryFromLists();
    void selectSingleAt(const QPointF& position, const LayoutCanvasTransform& transform);
    void selectZone(const QString& zoneId);
    void addFloor();
    QString currentFloorId() const;
    bool switchFloorByWheel(QWheelEvent* event);
    QString verticalTargetFloorId() const;
    void setToolMode(ToolMode mode);
    void showSelectionContextMenu(const QPoint& globalPosition);
    void setupToolbars();
    PreviewSelection currentSelection() const;

    safecrowd::domain::ImportResult importResult_{};
    QString selectedBarrierId_{};
    QStringList selectedBarrierIds_{};
    QString focusedTargetId_{};
    QString selectedConnectionId_{};
    QStringList selectedConnectionIds_{};
    QString selectedZoneId_{};
    QStringList selectedZoneIds_{};
    QPointF draftStartWorld_{};
    QPointF draftCurrentWorld_{};
    QPointF selectionDragStart_{};
    QPointF selectionDragCurrent_{};
    QPointF selectionMoveStart_{};
    QPointF selectionMoveCurrent_{};
    QPointF selectionMoveStartWorld_{};
    LayoutCanvasBounds selectionMoveBounds_{};
    QRectF selectionMoveViewport_{};
    double selectionMoveZoom_{1.0};
    QPointF selectionMovePanOffset_{};
    safecrowd::domain::FacilityLayout2D selectionMoveOriginalLayout_{};
    safecrowd::domain::FacilityLayout2D selectionMoveBaseLayout_{};
    safecrowd::domain::FacilityLayout2D selectionMoveSnapTargetLayout_{};
    std::vector<safecrowd::domain::Point2D> selectionMoveAnchors_{};
    QStringList selectionMoveAttachedBarrierIds_{};
    std::vector<QPointF> polygonDraftPoints_{};
    LayoutCanvasCamera camera_{};
    bool drafting_{false};
    bool selectionDragging_{false};
    bool selectionMoveDragging_{false};
    ToolMode toolMode_{ToolMode::Select};
    ShapeDrawMode shapeDrawMode_{ShapeDrawMode::Rectangle};
    bool roomAutoWallsEnabled_{true};
    bool doorCreatesLeaf_{true};
    bool verticalLinkCreatesRamp_{false};
    safecrowd::domain::StairEntryDirection stairEntryDirection_{safecrowd::domain::StairEntryDirection::West};
    double doorWidth_{1.2};
    bool gridSnapEnabled_{false};
    double gridSpacingMeters_{0.5};
    QString currentFloorId_{};
    QFrame* toolbarCorner_{nullptr};
    QFrame* topToolbar_{nullptr};
    QFrame* propertyPanel_{nullptr};
    QFrame* sideToolbar_{nullptr};
    QCheckBox* roomAutoWallsCheckBox_{nullptr};
    QComboBox* shapeDrawModeComboBox_{nullptr};
    QDoubleSpinBox* doorWidthSpinBox_{nullptr};
    QCheckBox* doorLeafCheckBox_{nullptr};
    QComboBox* verticalTargetFloorComboBox_{nullptr};
    QLabel* stairEntryLabel_{nullptr};
    QComboBox* stairEntryComboBox_{nullptr};
    QCheckBox* rampLinkCheckBox_{nullptr};
    QComboBox* floorComboBox_{nullptr};
    QComboBox* gridSpacingComboBox_{nullptr};
    QToolButton* selectToolButton_{nullptr};
    QToolButton* roomToolButton_{nullptr};
    QToolButton* exitToolButton_{nullptr};
    QToolButton* wallToolButton_{nullptr};
    QToolButton* obstructionToolButton_{nullptr};
    QToolButton* doorToolButton_{nullptr};
    QToolButton* stairToolButton_{nullptr};
    QToolButton* uStairToolButton_{nullptr};
    QToolButton* addFloorButton_{nullptr};
    QToolButton* gridToolButton_{nullptr};
    QToolButton* resetViewButton_{nullptr};
    std::function<void(const PreviewSelection&)> selectionChangedHandler_{};
    std::function<void(const safecrowd::domain::FacilityLayout2D&)> layoutEditedHandler_{};
};

}  // namespace safecrowd::application
