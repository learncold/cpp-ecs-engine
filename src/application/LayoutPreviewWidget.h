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
        DrawDoor,
        DrawStair,
    };

    enum class RoomDrawMode {
        Rectangle,
        Polygon,
    };

    void applyToolAt(const QPointF& position);
    void clearSelection();
    void createBarrier(const QPointF& startWorld, const QPointF& endWorld);
    void createConnection(const QPointF& startWorld, const QPointF& endWorld);
    void createDoorAt(const QString& barrierId, const QPointF& position);
    void createRoomPolygon(const std::vector<QPointF>& points);
    void createVerticalLink(const QPointF& startWorld, const QPointF& endWorld);
    void createZone(const QPointF& startWorld, const QPointF& endWorld, safecrowd::domain::ZoneKind kind);
    void deleteConnection(const QString& connectionId);
    void deleteBarrier(const QString& barrierId);
    void deleteSelectedElements();
    void emitCurrentSelection();
    void finishRoomPolygonDraft();
    bool hasSelection() const;
    bool isSelected(PreviewSelectionKind kind, const QString& id) const;
    void pruneSelection();
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
    std::vector<QPointF> roomPolygonDraftPoints_{};
    LayoutCanvasCamera camera_{};
    bool drafting_{false};
    bool selectionDragging_{false};
    ToolMode toolMode_{ToolMode::Select};
    RoomDrawMode roomDrawMode_{RoomDrawMode::Rectangle};
    bool roomAutoWallsEnabled_{true};
    bool doorCreatesLeaf_{true};
    bool verticalLinkCreatesRamp_{false};
    safecrowd::domain::StairEntryDirection stairEntryDirection_{safecrowd::domain::StairEntryDirection::West};
    safecrowd::domain::StairEntryDirection destinationStairEntryDirection_{safecrowd::domain::StairEntryDirection::East};
    double doorWidth_{1.2};
    QString currentFloorId_{};
    QFrame* toolbarCorner_{nullptr};
    QFrame* topToolbar_{nullptr};
    QFrame* propertyPanel_{nullptr};
    QFrame* sideToolbar_{nullptr};
    QCheckBox* roomAutoWallsCheckBox_{nullptr};
    QComboBox* roomDrawModeComboBox_{nullptr};
    QDoubleSpinBox* doorWidthSpinBox_{nullptr};
    QCheckBox* doorLeafCheckBox_{nullptr};
    QComboBox* verticalTargetFloorComboBox_{nullptr};
    QLabel* stairEntryLabel_{nullptr};
    QComboBox* stairEntryComboBox_{nullptr};
    QLabel* destinationStairEntryLabel_{nullptr};
    QComboBox* destinationStairEntryComboBox_{nullptr};
    QCheckBox* rampLinkCheckBox_{nullptr};
    QComboBox* floorComboBox_{nullptr};
    QToolButton* selectToolButton_{nullptr};
    QToolButton* roomToolButton_{nullptr};
    QToolButton* exitToolButton_{nullptr};
    QToolButton* wallToolButton_{nullptr};
    QToolButton* doorToolButton_{nullptr};
    QToolButton* stairToolButton_{nullptr};
    QToolButton* addFloorButton_{nullptr};
    QToolButton* resetViewButton_{nullptr};
    std::function<void(const PreviewSelection&)> selectionChangedHandler_{};
    std::function<void(const safecrowd::domain::FacilityLayout2D&)> layoutEditedHandler_{};
};

}  // namespace safecrowd::application
