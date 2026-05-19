#pragma once

#include <vector>

#include <QPointF>
#include <QString>
#include <QStringList>

#include "domain/FacilityLayout2D.h"

namespace safecrowd::application {

struct LayoutPreviewEditOptions {
    QString currentFloorId{};
    QString targetFloorId{};
    bool roomAutoWallsEnabled{true};
    bool doorCreatesLeaf{true};
    bool verticalLinkCreatesRamp{false};
    safecrowd::domain::StairEntryDirection stairEntryDirection{safecrowd::domain::StairEntryDirection::West};
    double doorWidth{1.2};
};

struct LayoutPreviewSelectionState {
    QString selectedBarrierId{};
    QStringList selectedBarrierIds{};
    QString selectedConnectionId{};
    QStringList selectedConnectionIds{};
    QString selectedZoneId{};
    QStringList selectedZoneIds{};
    QString focusedTargetId{};
};

struct LayoutPreviewEditResult {
    bool layoutChanged{false};
    bool selectionChanged{false};
    bool floorChanged{false};
    bool floorSelectorChanged{false};
    QString currentFloorId{};
    LayoutPreviewSelectionState selection{};
};

LayoutPreviewEditResult createLayoutPreviewRoomPolygon(
    safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<QPointF>& points,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewZone(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    safecrowd::domain::ZoneKind kind,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewWallSegment(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewObstructionPolygon(
    safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<QPointF>& points,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewObstructionRectangle(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewConnection(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewVerticalLink(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewUShapedStairLink(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewDoorAt(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& barrierId,
    const QPointF& position,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewDoorSegment(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult createLayoutPreviewDoorSpan(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& barrierId,
    const safecrowd::domain::Point2D& gapStart,
    const safecrowd::domain::Point2D& gapEnd,
    const LayoutPreviewEditOptions& options);

LayoutPreviewEditResult deleteLayoutPreviewConnection(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& connectionId,
    const LayoutPreviewSelectionState& selection);

LayoutPreviewEditResult deleteLayoutPreviewBarrier(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& barrierId,
    const LayoutPreviewSelectionState& selection);

LayoutPreviewEditResult deleteSelectedLayoutPreviewElements(
    safecrowd::domain::FacilityLayout2D& layout,
    const LayoutPreviewSelectionState& selection);

LayoutPreviewEditResult addLayoutPreviewFloor(safecrowd::domain::FacilityLayout2D& layout);

}  // namespace safecrowd::application
