#include "application/LayoutPreviewEditing.h"

#include "application/LayoutPreviewGeometry.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "application/LayoutCanvasRendering.h"

#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>

namespace safecrowd::application {
namespace {

constexpr double kConnectionWidth = 1.2;
constexpr double kDraftMinimumSize = 0.2;
constexpr double kGeometryEpsilon = 1e-4;
constexpr double kMinimumDoorWidth = 0.9;

LayoutPreviewSelectionState emptySelectionState() {
    return {};
}

LayoutPreviewSelectionState selectZoneState(const QString& zoneId) {
    LayoutPreviewSelectionState selection;
    selection.selectedZoneId = zoneId;
    selection.selectedZoneIds = QStringList{zoneId};
    selection.focusedTargetId = zoneId;
    return selection;
}

LayoutPreviewSelectionState selectConnectionState(const QString& connectionId) {
    LayoutPreviewSelectionState selection;
    selection.selectedConnectionId = connectionId;
    selection.selectedConnectionIds = QStringList{connectionId};
    selection.focusedTargetId = connectionId;
    return selection;
}

LayoutPreviewSelectionState selectBarrierState(const QString& barrierId) {
    LayoutPreviewSelectionState selection;
    selection.selectedBarrierId = barrierId;
    selection.selectedBarrierIds = QStringList{barrierId};
    selection.focusedTargetId = barrierId;
    return selection;
}

void selectPrimaryFromLists(LayoutPreviewSelectionState& selection) {
    selection.selectedZoneId = selection.selectedZoneIds.isEmpty() ? QString{} : selection.selectedZoneIds.front();
    selection.selectedConnectionId = selection.selectedConnectionIds.isEmpty() ? QString{} : selection.selectedConnectionIds.front();
    selection.selectedBarrierId = selection.selectedBarrierIds.isEmpty() ? QString{} : selection.selectedBarrierIds.front();

    if (!selection.selectedZoneId.isEmpty()) {
        selection.focusedTargetId = selection.selectedZoneId;
    } else if (!selection.selectedConnectionId.isEmpty()) {
        selection.focusedTargetId = selection.selectedConnectionId;
    } else if (!selection.selectedBarrierId.isEmpty()) {
        selection.focusedTargetId = selection.selectedBarrierId;
    } else {
        selection.focusedTargetId.clear();
    }
}

bool hasSelection(const LayoutPreviewSelectionState& selection) {
    return !selection.selectedZoneIds.isEmpty()
        || !selection.selectedConnectionIds.isEmpty()
        || !selection.selectedBarrierIds.isEmpty();
}

LayoutPreviewEditResult selectionResult(const LayoutPreviewSelectionState& selection) {
    return {
        .layoutChanged = true,
        .selectionChanged = true,
        .selection = selection,
    };
}

LayoutPreviewEditResult selectionOnlyResult(const LayoutPreviewSelectionState& selection) {
    return {
        .selectionChanged = true,
        .selection = selection,
    };
}


}  // namespace

LayoutPreviewEditResult createLayoutPreviewRoomPolygon(
    safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<QPointF>& points,
    const LayoutPreviewEditOptions& options) {
    if (points.size() < 3) {
        return {};
    }

    safecrowd::domain::Polygon2D roomPolygon;
    roomPolygon.outline.reserve(points.size());
    for (const auto& point : points) {
        roomPolygon.outline.push_back({.x = point.x(), .y = point.y()});
    }

    QPolygonF polygonForArea;
    for (const auto& point : points) {
        polygonForArea.append(point);
    }
    if (polygonArea(polygonForArea) < kDraftMinimumSize) {
        return {};
    }

    const auto currentFloor = options.currentFloorId;
    QPainterPath candidatePath = worldPolygonPath(roomPolygon);
    QPainterPath occupiedRooms;
    for (const auto& zone : layout.zones) {
        if (!matchesFloor(zone.floorId, currentFloor)) {
            continue;
        }
        if (zone.kind != safecrowd::domain::ZoneKind::Room) {
            continue;
        }
        occupiedRooms = occupiedRooms.united(worldPolygonPath(zone.area));
    }

    auto polygonsToCreate = polygonsFromFillPath(candidatePath.subtracted(occupiedRooms).simplified());
    if (polygonsToCreate.empty()) {
        return {};
    }

    QString lastZoneId;
    for (const auto& polygon : polygonsToCreate) {
        const auto floorId = options.currentFloorId.toStdString();
        const auto zoneId = nextZoneId(layout);
        const auto zoneNumber = static_cast<int>(layout.zones.size()) + 1;
        layout.zones.push_back({
            .id = zoneId.toStdString(),
            .floorId = floorId,
            .kind = safecrowd::domain::ZoneKind::Room,
            .label = QString("Room %1").arg(zoneNumber).toStdString(),
            .area = polygon,
            .defaultCapacity = 0u,
        });

        if (options.roomAutoWallsEnabled) {
            appendAutoWallsForPolygon(layout, polygon, floorId);
        }
        autoConnectRoomToStairEntries(layout, zoneId, QString::fromStdString(floorId));

        lastZoneId = zoneId;
    }

    return selectionResult(selectZoneState(lastZoneId));
}
LayoutPreviewEditResult createLayoutPreviewZone(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    safecrowd::domain::ZoneKind kind,
    const LayoutPreviewEditOptions& options) {
    const auto rectangle = rectFromWorldPoints(startWorld, endWorld);
    if (rectangle.width() < kDraftMinimumSize || rectangle.height() < kDraftMinimumSize) {
        return {};
    }

    QString zoneLabel = "Room";
    if (kind == safecrowd::domain::ZoneKind::Exit) {
        zoneLabel = "Exit";
    }

    std::vector<safecrowd::domain::Polygon2D> polygonsToCreate;
    const safecrowd::domain::Polygon2D rectanglePolygon{
        .outline = {
            {.x = rectangle.left(), .y = rectangle.top()},
            {.x = rectangle.right(), .y = rectangle.top()},
            {.x = rectangle.right(), .y = rectangle.bottom()},
            {.x = rectangle.left(), .y = rectangle.bottom()},
        },
    };

    if (kind == safecrowd::domain::ZoneKind::Room) {
        const auto currentFloor = options.currentFloorId;
        QPainterPath candidatePath = worldPolygonPath(rectanglePolygon);
        QPainterPath occupiedRooms;
        for (const auto& zone : layout.zones) {
            if (!matchesFloor(zone.floorId, currentFloor)) {
                continue;
            }
            if (zone.kind != safecrowd::domain::ZoneKind::Room) {
                continue;
            }
            occupiedRooms = occupiedRooms.united(worldPolygonPath(zone.area));
        }
        polygonsToCreate = polygonsFromFillPath(candidatePath.subtracted(occupiedRooms).simplified());
    } else {
        polygonsToCreate.push_back(rectanglePolygon);
    }

    if (polygonsToCreate.empty()) {
        return {};
    }

    QString lastZoneId;
    for (const auto& polygon : polygonsToCreate) {
        const auto floorId = options.currentFloorId.toStdString();
        if (kind == safecrowd::domain::ZoneKind::Exit) {
            if (const auto mergedZoneId = mergeExitZonePolygon(layout, polygon, worldPolygonPath(polygon), floorId)) {
                lastZoneId = *mergedZoneId;
                continue;
            }
        }

        const auto zoneId = nextZoneId(layout);
        const auto zoneNumber = static_cast<int>(layout.zones.size()) + 1;
        layout.zones.push_back({
            .id = zoneId.toStdString(),
            .floorId = floorId,
            .kind = kind,
            .label = QString("%1 %2").arg(zoneLabel).arg(zoneNumber).toStdString(),
            .area = polygon,
            .defaultCapacity = kind == safecrowd::domain::ZoneKind::Exit ? 20u : 0u,
        });

        if (kind == safecrowd::domain::ZoneKind::Room && options.roomAutoWallsEnabled) {
            appendAutoWallsForPolygon(layout, polygon, floorId);
        }
        if (kind == safecrowd::domain::ZoneKind::Room) {
            autoConnectRoomToStairEntries(layout, zoneId, QString::fromStdString(floorId));
        }

        lastZoneId = zoneId;
    }

    return selectionResult(selectZoneState(lastZoneId));
}
LayoutPreviewEditResult createLayoutPreviewWallSegment(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options) {
    if (std::hypot(endWorld.x() - startWorld.x(), endWorld.y() - startWorld.y()) < kDraftMinimumSize) {
        return {};
    }

    const auto barrierId = nextWallId(layout);
    layout.barriers.push_back({
        .id = barrierId.toStdString(),
        .floorId = options.currentFloorId.toStdString(),
        .geometry = safecrowd::domain::Polyline2D{
            .vertices = {
                {.x = startWorld.x(), .y = startWorld.y()},
                {.x = endWorld.x(), .y = endWorld.y()},
            },
            .closed = false,
        },
        .blocksMovement = true,
    });
    normalizeOpenWallBarriers(layout, QStringList{barrierId});

    return selectionResult(selectBarrierState(barrierId));
}
LayoutPreviewEditResult createLayoutPreviewObstructionPolygon(
    safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<QPointF>& points,
    const LayoutPreviewEditOptions& options) {
    if (points.size() < 3) {
        return {};
    }

    QPolygonF polygonForArea;
    for (const auto& point : points) {
        polygonForArea.append(point);
    }
    if (polygonArea(polygonForArea) < kDraftMinimumSize) {
        return {};
    }

    const auto barrierId = nextObstructionId(layout);
    std::vector<safecrowd::domain::Point2D> vertices;
    vertices.reserve(points.size());
    for (const auto& point : points) {
        vertices.push_back({.x = point.x(), .y = point.y()});
    }

    layout.barriers.push_back({
        .id = barrierId.toStdString(),
        .floorId = options.currentFloorId.toStdString(),
        .geometry = safecrowd::domain::Polyline2D{
            .vertices = std::move(vertices),
            .closed = true,
        },
        .blocksMovement = true,
    });

    return selectionResult(selectBarrierState(barrierId));
}
LayoutPreviewEditResult createLayoutPreviewObstructionRectangle(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options) {
    const auto rectangle = rectFromWorldPoints(startWorld, endWorld);
    if (rectangle.width() < kDraftMinimumSize || rectangle.height() < kDraftMinimumSize) {
        return {};
    }

    const auto barrierId = nextObstructionId(layout);
    layout.barriers.push_back({
        .id = barrierId.toStdString(),
        .floorId = options.currentFloorId.toStdString(),
        .geometry = safecrowd::domain::Polyline2D{
            .vertices = {
                {.x = rectangle.left(), .y = rectangle.top()},
                {.x = rectangle.right(), .y = rectangle.top()},
                {.x = rectangle.right(), .y = rectangle.bottom()},
                {.x = rectangle.left(), .y = rectangle.bottom()},
            },
            .closed = true,
        },
        .blocksMovement = true,
    });

    return selectionResult(selectBarrierState(barrierId));
}
LayoutPreviewEditResult createLayoutPreviewConnection(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options) {
    const auto length = std::hypot(endWorld.x() - startWorld.x(), endWorld.y() - startWorld.y());
    if (length < kDraftMinimumSize) {
        return {};
    }

    const auto floorId = options.currentFloorId;
    const auto startCandidates = zonesNearPoint(layout, startWorld, floorId);
    const auto endCandidates = zonesNearPoint(layout, endWorld, floorId);

    std::vector<std::size_t> candidates = startCandidates;
    for (const auto index : endCandidates) {
        if (std::find(candidates.begin(), candidates.end(), index) == candidates.end()) {
            candidates.push_back(index);
        }
    }

    if (candidates.size() < 2) {
        return {};
    }

    const auto fromZoneId = QString::fromStdString(layout.zones[candidates[0]].id);
    const auto toZoneId = QString::fromStdString(layout.zones[candidates[1]].id);
    if (fromZoneId == toZoneId || hasConnectionPair(layout, fromZoneId, toZoneId)) {
        return {};
    }

    const auto connectionId = nextConnectionId(layout);
    layout.connections.push_back({
        .id = connectionId.toStdString(),
        .floorId = options.currentFloorId.toStdString(),
        .kind = (layout.zones[candidates[0]].kind == safecrowd::domain::ZoneKind::Exit
                || layout.zones[candidates[1]].kind == safecrowd::domain::ZoneKind::Exit)
            ? safecrowd::domain::ConnectionKind::Exit
            : safecrowd::domain::ConnectionKind::Opening,
        .fromZoneId = fromZoneId.toStdString(),
        .toZoneId = toZoneId.toStdString(),
        .effectiveWidth = kConnectionWidth,
        .directionality = safecrowd::domain::TravelDirection::Bidirectional,
        .centerSpan = {
            .start = {.x = startWorld.x(), .y = startWorld.y()},
            .end = {.x = endWorld.x(), .y = endWorld.y()},
        },
    });

    return selectionResult(selectConnectionState(connectionId));
}
LayoutPreviewEditResult createLayoutPreviewVerticalLink(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options) {
    const auto sourceFloorId = options.currentFloorId;
    const auto targetFloorId = options.targetFloorId;
    if (sourceFloorId.isEmpty() || targetFloorId.isEmpty() || sourceFloorId == targetFloorId) {
        return {};
    }

    const auto rectangle = rectFromWorldPoints(startWorld, endWorld);
    if (rectangle.width() < kDraftMinimumSize || rectangle.height() < kDraftMinimumSize) {
        return {};
    }

    const safecrowd::domain::Polygon2D footprint{
        .outline = {
            {.x = rectangle.left(), .y = rectangle.top()},
            {.x = rectangle.right(), .y = rectangle.top()},
            {.x = rectangle.right(), .y = rectangle.bottom()},
            {.x = rectangle.left(), .y = rectangle.bottom()},
        },
    };
    const QPointF center = rectangle.center();
    auto floorElevation = [&](const QString& floorId) {
        const auto it = std::find_if(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
            return QString::fromStdString(floor.id) == floorId;
        });
        return it == layout.floors.end() ? 0.0 : it->elevationMeters;
    };
    const bool sourceIsLower = floorElevation(sourceFloorId) <= floorElevation(targetFloorId);
    const auto sourceEntryDirection = options.stairEntryDirection;
    const auto targetEntryDirection = oppositeStairEntryDirection(sourceEntryDirection);
    const auto lowerEntryDirection = sourceIsLower ? sourceEntryDirection : targetEntryDirection;
    const auto upperEntryDirection = sourceIsLower ? targetEntryDirection : sourceEntryDirection;
    const auto sourceEntrySpan = entrySpanForRectangle(rectangle, sourceEntryDirection);
    const auto targetEntrySpan = entrySpanForRectangle(rectangle, targetEntryDirection);
    const auto sourceOutsideSample = entryOutsideSample(rectangle, sourceEntryDirection);
    const auto targetOutsideSample = entryOutsideSample(rectangle, targetEntryDirection);
    const auto sourceZoneCandidates = zonesContainingPoint(layout, sourceOutsideSample, sourceFloorId, 0.35);
    const auto targetZoneCandidates = zonesContainingPoint(layout, targetOutsideSample, targetFloorId, 0.35);
    const auto sourceZone = choosePrimaryZone(layout, sourceZoneCandidates);
    const auto targetZone = choosePrimaryZone(layout, targetZoneCandidates);

    const auto sourceStairZoneId = nextZoneId(layout);
    const auto sourceZoneNumber = static_cast<int>(layout.zones.size()) + 1;
    const auto linkKind = options.verticalLinkCreatesRamp
        ? safecrowd::domain::ConnectionKind::Ramp
        : safecrowd::domain::ConnectionKind::Stair;
    const auto zoneLabel = options.verticalLinkCreatesRamp ? QString("Ramp") : QString("Stair");
    const auto effectiveWidth = std::max(0.9, std::min(rectangle.width(), rectangle.height()));
    const auto span = verticalConnectionSpanForRectangle(rectangle, sourceEntryDirection, effectiveWidth);

    layout.zones.push_back({
        .id = sourceStairZoneId.toStdString(),
        .floorId = sourceFloorId.toStdString(),
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = QString("%1 %2").arg(zoneLabel).arg(sourceZoneNumber).toStdString(),
        .area = footprint,
        .defaultCapacity = 8,
        .isStair = !options.verticalLinkCreatesRamp,
        .isRamp = options.verticalLinkCreatesRamp,
    });
    appendStairWallsExceptEntry(layout, rectangle, sourceEntryDirection, sourceFloorId.toStdString());

    const auto targetStairZoneId = nextZoneId(layout);
    const auto targetZoneNumber = static_cast<int>(layout.zones.size()) + 1;
    layout.zones.push_back({
        .id = targetStairZoneId.toStdString(),
        .floorId = targetFloorId.toStdString(),
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = QString("%1 %2").arg(zoneLabel).arg(targetZoneNumber).toStdString(),
        .area = footprint,
        .defaultCapacity = 8,
        .isStair = !options.verticalLinkCreatesRamp,
        .isRamp = options.verticalLinkCreatesRamp,
    });
    appendStairWallsExceptEntry(layout, rectangle, targetEntryDirection, targetFloorId.toStdString());

    if (sourceZone.has_value()) {
        cutBarriersAtSpan(layout, sourceEntrySpan, sourceFloorId.toStdString());
        layout.connections.push_back({
            .id = nextConnectionId(layout).toStdString(),
            .floorId = sourceFloorId.toStdString(),
            .kind = safecrowd::domain::ConnectionKind::Opening,
            .fromZoneId = layout.zones[*sourceZone].id,
            .toZoneId = sourceStairZoneId.toStdString(),
            .effectiveWidth = effectiveWidth,
            .directionality = safecrowd::domain::TravelDirection::Bidirectional,
            .centerSpan = sourceEntrySpan,
        });
    }

    const auto verticalConnectionId = nextVerticalConnectionId(layout);
    layout.connections.push_back({
        .id = verticalConnectionId.toStdString(),
        .floorId = sourceFloorId.toStdString(),
        .kind = linkKind,
        .fromZoneId = sourceStairZoneId.toStdString(),
        .toZoneId = targetStairZoneId.toStdString(),
        .effectiveWidth = effectiveWidth,
        .directionality = safecrowd::domain::TravelDirection::Bidirectional,
        .isStair = !options.verticalLinkCreatesRamp,
        .isRamp = options.verticalLinkCreatesRamp,
        .lowerEntryDirection = lowerEntryDirection,
        .upperEntryDirection = upperEntryDirection,
        .centerSpan = span,
    });

    if (targetZone.has_value()) {
        cutBarriersAtSpan(layout, targetEntrySpan, targetFloorId.toStdString());
        layout.connections.push_back({
            .id = nextConnectionId(layout).toStdString(),
            .floorId = targetFloorId.toStdString(),
            .kind = safecrowd::domain::ConnectionKind::Opening,
            .fromZoneId = targetStairZoneId.toStdString(),
            .toZoneId = layout.zones[*targetZone].id,
            .effectiveWidth = effectiveWidth,
            .directionality = safecrowd::domain::TravelDirection::Bidirectional,
            .centerSpan = targetEntrySpan,
        });
    }

    return selectionResult(selectConnectionState(verticalConnectionId));
}
LayoutPreviewEditResult createLayoutPreviewUShapedStairLink(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options) {
    const auto sourceFloorId = options.currentFloorId;
    const auto targetFloorId = options.targetFloorId;
    if (sourceFloorId.isEmpty() || targetFloorId.isEmpty() || sourceFloorId == targetFloorId) {
        return {};
    }

    const auto rectangle = rectFromWorldPoints(startWorld, endWorld);
    if (rectangle.width() < kDraftMinimumSize || rectangle.height() < kDraftMinimumSize) {
        return {};
    }

    const auto geometry = uShapedStairGeometryForRectangle(rectangle, options.stairEntryDirection);
    if (geometry.laneWidth <= kGeometryEpsilon) {
        return {};
    }

    const bool sourceIsLower = floorElevation(layout, sourceFloorId.toStdString())
        <= floorElevation(layout, targetFloorId.toStdString());
    const auto sourceEntryDirection = options.stairEntryDirection;
    const auto targetEntryDirection = options.stairEntryDirection;
    const auto lowerEntryDirection = sourceIsLower ? sourceEntryDirection : targetEntryDirection;
    const auto upperEntryDirection = sourceIsLower ? targetEntryDirection : sourceEntryDirection;

    const auto sourceZoneCandidates = zonesContainingPoint(
        layout,
        QPointF(geometry.sourceOutsideSample.x, geometry.sourceOutsideSample.y),
        sourceFloorId,
        0.35);
    const auto targetZoneCandidates = zonesContainingPoint(
        layout,
        QPointF(geometry.targetOutsideSample.x, geometry.targetOutsideSample.y),
        targetFloorId,
        0.35);
    const auto sourceZone = choosePrimaryZone(layout, sourceZoneCandidates);
    const auto targetZone = choosePrimaryZone(layout, targetZoneCandidates);

    const auto sourceStairZoneId = nextZoneId(layout);
    const auto sourceZoneNumber = static_cast<int>(layout.zones.size()) + 1;
    layout.zones.push_back({
        .id = sourceStairZoneId.toStdString(),
        .floorId = sourceFloorId.toStdString(),
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = QString("U Stair %1").arg(sourceZoneNumber).toStdString(),
        .area = geometry.sourceFootprint,
        .defaultCapacity = 8,
        .isStair = true,
    });

    const auto targetStairZoneId = nextZoneId(layout);
    const auto targetZoneNumber = static_cast<int>(layout.zones.size()) + 1;
    layout.zones.push_back({
        .id = targetStairZoneId.toStdString(),
        .floorId = targetFloorId.toStdString(),
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = QString("U Stair %1").arg(targetZoneNumber).toStdString(),
        .area = geometry.targetFootprint,
        .defaultCapacity = 8,
        .isStair = true,
    });

    QStringList preferredWallIds;
    preferredWallIds.append(appendWallsForPolygonExceptGaps(
        layout,
        geometry.sourceFootprint,
        {geometry.sourceEntrySpan, geometry.verticalSpan},
        sourceFloorId.toStdString()));
    preferredWallIds.append(appendWallsForPolygonExceptGaps(
        layout,
        geometry.targetFootprint,
        {geometry.targetEntrySpan, geometry.verticalSpan},
        targetFloorId.toStdString()));

    if (sourceZone.has_value()) {
        cutBarriersAtSpan(layout, geometry.sourceEntrySpan, sourceFloorId.toStdString());
        layout.connections.push_back({
            .id = nextConnectionId(layout).toStdString(),
            .floorId = sourceFloorId.toStdString(),
            .kind = safecrowd::domain::ConnectionKind::Opening,
            .fromZoneId = layout.zones[*sourceZone].id,
            .toZoneId = sourceStairZoneId.toStdString(),
            .effectiveWidth = geometry.laneWidth,
            .directionality = safecrowd::domain::TravelDirection::Bidirectional,
            .centerSpan = geometry.sourceEntrySpan,
        });
    }

    const auto verticalConnectionId = nextVerticalConnectionId(layout);
    layout.connections.push_back({
        .id = verticalConnectionId.toStdString(),
        .floorId = sourceFloorId.toStdString(),
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = sourceStairZoneId.toStdString(),
        .toZoneId = targetStairZoneId.toStdString(),
        .effectiveWidth = geometry.laneWidth,
        .directionality = safecrowd::domain::TravelDirection::Bidirectional,
        .isStair = true,
        .lowerEntryDirection = lowerEntryDirection,
        .upperEntryDirection = upperEntryDirection,
        .centerSpan = geometry.verticalSpan,
    });

    if (targetZone.has_value()) {
        cutBarriersAtSpan(layout, geometry.targetEntrySpan, targetFloorId.toStdString());
        layout.connections.push_back({
            .id = nextConnectionId(layout).toStdString(),
            .floorId = targetFloorId.toStdString(),
            .kind = safecrowd::domain::ConnectionKind::Opening,
            .fromZoneId = targetStairZoneId.toStdString(),
            .toZoneId = layout.zones[*targetZone].id,
            .effectiveWidth = geometry.laneWidth,
            .directionality = safecrowd::domain::TravelDirection::Bidirectional,
            .centerSpan = geometry.targetEntrySpan,
        });
    }

    normalizeOpenWallBarriers(layout, preferredWallIds);

    return selectionResult(selectConnectionState(verticalConnectionId));
}
LayoutPreviewEditResult createLayoutPreviewDoorAt(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& barrierId,
    const QPointF& position,
    const LayoutPreviewEditOptions& options) {
    if (barrierId.isEmpty()) {
        return {};
    }

    const auto barrierIt = std::find_if(layout.barriers.begin(), layout.barriers.end(), [&](const auto& barrier) {
        return QString::fromStdString(barrier.id) == barrierId && barrier.geometry.vertices.size() == 2;
    });
    if (barrierIt == layout.barriers.end()) {
        return {};
    }

    const auto& barrierStart = barrierIt->geometry.vertices[0];
    const auto& barrierEnd = barrierIt->geometry.vertices[1];
    const auto projected = projectOntoSegment(position, barrierStart, barrierEnd);
    if (!projected.has_value()) {
        return {};
    }

    const auto segmentLength = std::hypot(barrierEnd.x - barrierStart.x, barrierEnd.y - barrierStart.y);
    const auto openingWidth = std::clamp(options.doorWidth, kMinimumDoorWidth, segmentLength - kGeometryEpsilon);
    if (segmentLength <= openingWidth + kGeometryEpsilon) {
        return {};
    }

    const bool vertical = nearlyEqual(barrierStart.x, barrierEnd.x);
    const bool horizontal = nearlyEqual(barrierStart.y, barrierEnd.y);
    if (!vertical && !horizontal) {
        return {};
    }

    safecrowd::domain::Point2D gapStart{};
    safecrowd::domain::Point2D gapEnd{};
    if (vertical) {
        const auto minY = std::min(barrierStart.y, barrierEnd.y);
        const auto maxY = std::max(barrierStart.y, barrierEnd.y);
        const auto centerY = std::clamp(projected->y(), minY + openingWidth * 0.5, maxY - openingWidth * 0.5);
        gapStart = {.x = barrierStart.x, .y = centerY - openingWidth * 0.5};
        gapEnd = {.x = barrierStart.x, .y = centerY + openingWidth * 0.5};
    } else {
        const auto minX = std::min(barrierStart.x, barrierEnd.x);
        const auto maxX = std::max(barrierStart.x, barrierEnd.x);
        const auto centerX = std::clamp(projected->x(), minX + openingWidth * 0.5, maxX - openingWidth * 0.5);
        gapStart = {.x = centerX - openingWidth * 0.5, .y = barrierStart.y};
        gapEnd = {.x = centerX + openingWidth * 0.5, .y = barrierStart.y};
    }

    return createLayoutPreviewDoorSpan(layout, barrierId, gapStart, gapEnd, options);
}
LayoutPreviewEditResult createLayoutPreviewDoorSegment(
    safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& startWorld,
    const QPointF& endWorld,
    const LayoutPreviewEditOptions& options) {
    const safecrowd::domain::LineSegment2D rawSpan{
        .start = {.x = startWorld.x(), .y = startWorld.y()},
        .end = {.x = endWorld.x(), .y = endWorld.y()},
    };
    if (std::hypot(rawSpan.end.x - rawSpan.start.x, rawSpan.end.y - rawSpan.start.y) < kMinimumDoorWidth) {
        return {};
    }

    const auto corrected = correctedDoorSpanForPlacement(layout, options.currentFloorId, rawSpan);
    if (!corrected.has_value()) {
        return {};
    }

    const auto& span = corrected->span;
    if (!corrected->barrierId.isEmpty()) {
        return createLayoutPreviewDoorSpan(layout, corrected->barrierId, span.start, span.end, options);
    }

    const auto barrierIndex = barrierIndexCoveringSpan(layout, span, options.currentFloorId);
    if (barrierIndex.has_value()) {
        const auto barrierId = QString::fromStdString(layout.barriers[*barrierIndex].id);
        return createLayoutPreviewDoorSpan(layout, barrierId, span.start, span.end, options);
    }

    return createLayoutPreviewDoorSpan(layout, {}, span.start, span.end, options);
}
LayoutPreviewEditResult createLayoutPreviewDoorSpan(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& barrierId,
    const safecrowd::domain::Point2D& gapStart,
    const safecrowd::domain::Point2D& gapEnd,
    const LayoutPreviewEditOptions& options) {
    const auto openingWidth = std::hypot(gapEnd.x - gapStart.x, gapEnd.y - gapStart.y);
    if (openingWidth < kMinimumDoorWidth) {
        return {};
    }

    const bool vertical = nearlyEqual(gapStart.x, gapEnd.x);
    const bool horizontal = nearlyEqual(gapStart.y, gapEnd.y);
    if (!vertical && !horizontal) {
        return {};
    }

    std::optional<std::size_t> barrierIndex;
    std::optional<safecrowd::domain::Point2D> barrierStart;
    std::optional<safecrowd::domain::Point2D> barrierEnd;
    if (!barrierId.isEmpty()) {
        const auto barrierIt = std::find_if(layout.barriers.begin(), layout.barriers.end(), [&](const auto& barrier) {
            return QString::fromStdString(barrier.id) == barrierId && barrier.geometry.vertices.size() == 2;
        });
        if (barrierIt == layout.barriers.end()) {
            return {};
        }

        barrierIndex = static_cast<std::size_t>(std::distance(layout.barriers.begin(), barrierIt));
        barrierStart = barrierIt->geometry.vertices[0];
        barrierEnd = barrierIt->geometry.vertices[1];
        const bool barrierVertical = nearlyEqual(barrierStart->x, barrierEnd->x);
        const bool barrierHorizontal = nearlyEqual(barrierStart->y, barrierEnd->y);
        if (barrierVertical != vertical || barrierHorizontal != horizontal) {
            return {};
        }

        if (vertical) {
            if (!nearlyEqual(barrierStart->x, gapStart.x)
                || !intervalContainsSpan(barrierStart->y, barrierEnd->y, gapStart.y, gapEnd.y)) {
                return {};
            }
        } else if (!nearlyEqual(barrierStart->y, gapStart.y)
            || !intervalContainsSpan(barrierStart->x, barrierEnd->x, gapStart.x, gapEnd.x)) {
            return {};
        }

        const auto segmentLength = std::hypot(barrierEnd->x - barrierStart->x, barrierEnd->y - barrierStart->y);
        if (segmentLength <= openingWidth + kGeometryEpsilon) {
            return {};
        }
    }

    const auto neighbors = doorNeighborsAcrossSegment(layout, gapStart, gapEnd, options.currentFloorId);
    const auto firstZone = choosePrimaryZone(layout, neighbors.firstSide);
    const auto secondZone = choosePrimaryZone(layout, neighbors.secondSide, firstZone);

    QString fromZoneId;
    QString toZoneId;
    safecrowd::domain::ConnectionKind connectionKind =
        options.doorCreatesLeaf ? safecrowd::domain::ConnectionKind::Doorway : safecrowd::domain::ConnectionKind::Opening;

    if (firstZone.has_value() && secondZone.has_value()) {
        fromZoneId = QString::fromStdString(layout.zones[*firstZone].id);
        toZoneId = QString::fromStdString(layout.zones[*secondZone].id);
        if (layout.zones[*firstZone].kind == safecrowd::domain::ZoneKind::Exit
            || layout.zones[*secondZone].kind == safecrowd::domain::ZoneKind::Exit) {
            connectionKind = safecrowd::domain::ConnectionKind::Exit;
        }
    } else {
        const auto interiorZone = firstZone.has_value() ? firstZone : secondZone;
        if (!interiorZone.has_value()) {
            return {};
        }

        const bool useFirstOutside = !firstZone.has_value();
        const QPointF outsideSample = useFirstOutside ? neighbors.firstSample : neighbors.secondSample;
        const QPointF insideSample = useFirstOutside ? neighbors.secondSample : neighbors.firstSample;
        QPointF outsideDirection = outsideSample - insideSample;
        const auto directionLength = std::hypot(outsideDirection.x(), outsideDirection.y());
        if (directionLength <= kGeometryEpsilon) {
            return {};
        }
        outsideDirection /= directionLength;

        fromZoneId = QString::fromStdString(layout.zones[*interiorZone].id);
        toZoneId = createExitZoneAtDoor(layout, gapStart, gapEnd, outsideDirection, options.currentFloorId.toStdString());
        connectionKind = safecrowd::domain::ConnectionKind::Exit;
    }

    const safecrowd::domain::LineSegment2D gapSpan{
        .start = gapStart,
        .end = gapEnd,
    };
    const bool canMergeWithExistingDoor = !mergeableDoorConnectionIndices(
        layout,
        options.currentFloorId,
        connectionKind,
        fromZoneId,
        toZoneId,
        gapSpan).empty();

    if (fromZoneId.isEmpty()
        || toZoneId.isEmpty()
        || fromZoneId == toZoneId
        || (!canMergeWithExistingDoor && hasConnectionPairAtSpan(layout, fromZoneId, toZoneId, gapStart, gapEnd))) {
        return {};
    }

    if (barrierIndex.has_value() && barrierStart.has_value() && barrierEnd.has_value()) {
        std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>> remainingSegments;
        if (vertical) {
            const auto lowerStart = std::min(barrierStart->y, barrierEnd->y);
            const auto lowerEnd = std::max(barrierStart->y, barrierEnd->y);
            if (gapStart.y - lowerStart > kGeometryEpsilon) {
                remainingSegments.push_back({
                    {.x = barrierStart->x, .y = lowerStart},
                    {.x = barrierStart->x, .y = gapStart.y},
                });
            }
            if (lowerEnd - gapEnd.y > kGeometryEpsilon) {
                remainingSegments.push_back({
                    {.x = barrierStart->x, .y = gapEnd.y},
                    {.x = barrierStart->x, .y = lowerEnd},
                });
            }
        } else {
            const auto lowerStart = std::min(barrierStart->x, barrierEnd->x);
            const auto lowerEnd = std::max(barrierStart->x, barrierEnd->x);
            if (gapStart.x - lowerStart > kGeometryEpsilon) {
                remainingSegments.push_back({
                    {.x = lowerStart, .y = barrierStart->y},
                    {.x = gapStart.x, .y = barrierStart->y},
                });
            }
            if (lowerEnd - gapEnd.x > kGeometryEpsilon) {
                remainingSegments.push_back({
                    {.x = gapEnd.x, .y = barrierStart->y},
                    {.x = lowerEnd, .y = barrierStart->y},
                });
            }
        }

        replaceBarrierWithSegments(layout, *barrierIndex, remainingSegments, options.currentFloorId.toStdString());
    }

    if (canMergeWithExistingDoor) {
        const auto mergedConnectionId = mergeDoorConnectionSpan(
            layout,
            options.currentFloorId,
            connectionKind,
            fromZoneId,
            toZoneId,
            gapSpan);
        if (!mergedConnectionId.has_value()) {
            return {};
        }

        return selectionResult(selectConnectionState(*mergedConnectionId));
    }

    const auto connectionId = nextConnectionId(layout);
    layout.connections.push_back({
        .id = connectionId.toStdString(),
        .floorId = options.currentFloorId.toStdString(),
        .kind = connectionKind,
        .fromZoneId = fromZoneId.toStdString(),
        .toZoneId = toZoneId.toStdString(),
        .effectiveWidth = openingWidth,
        .directionality = safecrowd::domain::TravelDirection::Bidirectional,
        .centerSpan = {.start = gapStart, .end = gapEnd},
    });

    return selectionResult(selectConnectionState(connectionId));
}

LayoutPreviewEditResult deleteLayoutPreviewConnection(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& connectionId,
    const LayoutPreviewSelectionState& selection) {
    if (connectionId.isEmpty()) {
        return {};
    }

    auto& connections = layout.connections;
    const auto it = std::remove_if(connections.begin(), connections.end(), [&](const auto& connection) {
        return QString::fromStdString(connection.id) == connectionId;
    });
    if (it == connections.end()) {
        return {};
    }

    connections.erase(it, connections.end());
    auto nextSelection = selection;
    nextSelection.selectedConnectionId.clear();
    nextSelection.selectedConnectionIds.removeAll(connectionId);
    selectPrimaryFromLists(nextSelection);
    nextSelection.focusedTargetId.clear();
    return selectionResult(nextSelection);
}

LayoutPreviewEditResult deleteLayoutPreviewBarrier(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& barrierId,
    const LayoutPreviewSelectionState& selection) {
    if (barrierId.isEmpty()) {
        return {};
    }

    auto& barriers = layout.barriers;
    const auto it = std::remove_if(barriers.begin(), barriers.end(), [&](const auto& barrier) {
        return QString::fromStdString(barrier.id) == barrierId;
    });
    if (it == barriers.end()) {
        return {};
    }

    barriers.erase(it, barriers.end());
    auto nextSelection = selection;
    nextSelection.selectedBarrierId.clear();
    nextSelection.selectedBarrierIds.removeAll(barrierId);
    selectPrimaryFromLists(nextSelection);
    nextSelection.focusedTargetId.clear();
    return selectionResult(nextSelection);
}

LayoutPreviewEditResult deleteSelectedLayoutPreviewElements(
    safecrowd::domain::FacilityLayout2D& layout,
    const LayoutPreviewSelectionState& selection) {
    if (!hasSelection(selection)) {
        return {};
    }

    bool changed = false;

    const auto selectedZoneId = [&](const std::string& id) {
        return selection.selectedZoneIds.contains(QString::fromStdString(id));
    };

    auto& connections = layout.connections;
    const auto connectionIt = std::remove_if(connections.begin(), connections.end(), [&](const auto& connection) {
        return selection.selectedConnectionIds.contains(QString::fromStdString(connection.id))
            || selectedZoneId(connection.fromZoneId)
            || selectedZoneId(connection.toZoneId);
    });
    if (connectionIt != connections.end()) {
        connections.erase(connectionIt, connections.end());
        changed = true;
    }

    auto& barriers = layout.barriers;
    const auto barrierIt = std::remove_if(barriers.begin(), barriers.end(), [&](const auto& barrier) {
        return selection.selectedBarrierIds.contains(QString::fromStdString(barrier.id));
    });
    if (barrierIt != barriers.end()) {
        barriers.erase(barrierIt, barriers.end());
        changed = true;
    }

    auto& zones = layout.zones;
    const auto zoneIt = std::remove_if(zones.begin(), zones.end(), [&](const auto& zone) {
        return selection.selectedZoneIds.contains(QString::fromStdString(zone.id));
    });
    if (zoneIt != zones.end()) {
        zones.erase(zoneIt, zones.end());
        changed = true;
    }

    const auto clearedSelection = emptySelectionState();
    if (!changed) {
        return selectionOnlyResult(clearedSelection);
    }

    return selectionResult(clearedSelection);
}

LayoutPreviewEditResult deleteLayoutPreviewFloor(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& floorId,
    const QString& currentFloorId) {
    if (floorId.isEmpty()) {
        return {};
    }

    ensureLayoutFloors(layout);
    const bool deletedLevelId = layout.levelId.empty() || QString::fromStdString(layout.levelId) == floorId;
    const auto floorIt = std::remove_if(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
        return QString::fromStdString(floor.id) == floorId;
    });
    if (floorIt == layout.floors.end()) {
        return {};
    }
    layout.floors.erase(floorIt, layout.floors.end());

    QStringList removedZoneIds;
    const auto zoneIt = std::remove_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        const bool remove = QString::fromStdString(zone.floorId) == floorId;
        if (remove) {
            removedZoneIds.append(QString::fromStdString(zone.id));
        }
        return remove;
    });
    layout.zones.erase(zoneIt, layout.zones.end());

    QStringList removedConnectionIds;
    const auto connectionIt = std::remove_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        const bool remove = QString::fromStdString(connection.floorId) == floorId
            || removedZoneIds.contains(QString::fromStdString(connection.fromZoneId))
            || removedZoneIds.contains(QString::fromStdString(connection.toZoneId));
        if (remove) {
            removedConnectionIds.append(QString::fromStdString(connection.id));
        }
        return remove;
    });
    layout.connections.erase(connectionIt, layout.connections.end());

    QStringList removedBarrierIds;
    const auto barrierIt = std::remove_if(layout.barriers.begin(), layout.barriers.end(), [&](const auto& barrier) {
        const bool remove = QString::fromStdString(barrier.floorId) == floorId;
        if (remove) {
            removedBarrierIds.append(QString::fromStdString(barrier.id));
        }
        return remove;
    });
    layout.barriers.erase(barrierIt, layout.barriers.end());

    const auto controlIt = std::remove_if(layout.controls.begin(), layout.controls.end(), [&](const auto& control) {
        const auto targetId = QString::fromStdString(control.targetId);
        return QString::fromStdString(control.floorId) == floorId
            || removedZoneIds.contains(targetId)
            || removedConnectionIds.contains(targetId)
            || removedBarrierIds.contains(targetId);
    });
    layout.controls.erase(controlIt, layout.controls.end());

    QString nextCurrentFloorId = currentFloorId;
    const auto floorExists = [&](const QString& candidate) {
        return std::any_of(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
            return QString::fromStdString(floor.id) == candidate;
        });
    };
    if (!floorExists(nextCurrentFloorId)) {
        nextCurrentFloorId = layout.floors.empty() ? QString{} : QString::fromStdString(layout.floors.front().id);
    }

    if (layout.floors.empty()) {
        nextCurrentFloorId = QStringLiteral("L1");
        layout.floors.push_back({
            .id = nextCurrentFloorId.toStdString(),
            .label = nextCurrentFloorId.toStdString(),
        });
    }
    if (deletedLevelId) {
        layout.levelId = nextCurrentFloorId.toStdString();
    }

    return {
        .layoutChanged = true,
        .selectionChanged = true,
        .floorChanged = true,
        .floorSelectorChanged = true,
        .currentFloorId = nextCurrentFloorId,
        .selection = emptySelectionState(),
    };
}

LayoutPreviewEditResult addLayoutPreviewFloor(safecrowd::domain::FacilityLayout2D& layout) {
    ensureLayoutFloors(layout);
    const auto floorId = nextFloorId(layout);
    layout.floors.push_back({
        .id = floorId.toStdString(),
        .label = QString("Floor %1").arg(layout.floors.size() + 1).toStdString(),
    });

    return {
        .layoutChanged = true,
        .selectionChanged = true,
        .floorChanged = true,
        .floorSelectorChanged = true,
        .currentFloorId = floorId,
        .selection = emptySelectionState(),
    };
}

}  // namespace safecrowd::application
