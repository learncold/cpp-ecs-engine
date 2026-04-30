#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <QPointF>
#include <QString>
#include <QWidget>

#include "application/LayoutCanvasRendering.h"
#include "domain/FacilityLayout2D.h"

class QFrame;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QLabel;
class QSpinBox;
class QToolButton;
class QWheelEvent;

namespace safecrowd::application {

enum class ScenarioCrowdPlacementKind {
    Individual,
    Group,
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
};

class ScenarioCanvasWidget : public QWidget {
public:
    explicit ScenarioCanvasWidget(
        safecrowd::domain::FacilityLayout2D layout,
        QWidget* parent = nullptr);
    ~ScenarioCanvasWidget() override;

    void setPlacements(std::vector<ScenarioCrowdPlacement> placements);
    void setPlacementsChangedHandler(std::function<void(const std::vector<ScenarioCrowdPlacement>&)> handler);
    void focusLayoutElement(const QString& elementId);
    void focusPlacement(const QString& placementId);

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
        IndividualPlacement,
        GroupPlacement,
    };

    std::optional<LayoutCanvasBounds> collectBounds() const;
    LayoutCanvasTransform currentTransform(const LayoutCanvasBounds& bounds) const;
    QRectF previewViewport() const;
    void selectFloorForElement(const QString& elementId);
    safecrowd::domain::Point2D unmapPoint(const QPointF& point) const;
    QString zoneAt(const safecrowd::domain::Point2D& point) const;
    bool placementAreaBlocked(const std::vector<safecrowd::domain::Point2D>& area, int occupantCount) const;
    bool placementPointBlocked(const safecrowd::domain::Point2D& point) const;
    safecrowd::domain::Point2D defaultVelocityFrom(const safecrowd::domain::Point2D& point) const;
    QString nextPlacementId(ScenarioCrowdPlacementKind kind) const;
    void addGroupPlacement(const QPointF& start, const QPointF& end);
    void addIndividualPlacement(const QPointF& position);
    void drawFocusedLayoutElement(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void drawFocusedPlacement(QPainter& painter, const LayoutCanvasTransform& transform) const;
    void emitPlacementsChanged();
    void repositionToolbars();
    void setToolMode(ToolMode mode);
    void setupToolbars();
    bool switchFloorByWheel(QWheelEvent* event);

    safecrowd::domain::FacilityLayout2D layout_{};
    std::vector<ScenarioCrowdPlacement> placements_{};
    QString currentFloorId_{};
    QString focusedLayoutElementId_{};
    QString focusedPlacementId_{};
    ToolMode toolMode_{ToolMode::Select};
    LayoutCanvasCamera camera_{};
    QPointF dragStart_{};
    QPointF dragCurrent_{};
    bool dragging_{false};
    QFrame* topToolbar_{nullptr};
    QFrame* propertyPanel_{nullptr};
    QToolButton* selectToolButton_{nullptr};
    QToolButton* individualToolButton_{nullptr};
    QToolButton* groupToolButton_{nullptr};
    QLabel* groupCountLabel_{nullptr};
    QSpinBox* groupCountSpinBox_{nullptr};
    std::function<void(const std::vector<ScenarioCrowdPlacement>&)> placementsChangedHandler_{};
};

}  // namespace safecrowd::application
