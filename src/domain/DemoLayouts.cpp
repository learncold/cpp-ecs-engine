#include "domain/DemoLayouts.h"

#include <utility>

namespace safecrowd::domain::DemoLayouts {
namespace {

Barrier2D makeBarrier(const char* id, std::vector<Point2D> vertices, bool closed = false) {
    Barrier2D barrier;
    barrier.id = id;
    barrier.blocksMovement = true;
    barrier.geometry = Polyline2D{
        .vertices = std::move(vertices),
        .closed = closed,
    };
    return barrier;
}

}  // namespace

FacilityLayout2D demoFacility() {
    FacilityLayout2D layout{};
    layout.id = Sprint1FacilityIds::LayoutId;
    layout.name = "Sprint 1 Demo Layout";
    layout.levelId = "L1";

    Zone2D mainRoom;
    mainRoom.id = Sprint1FacilityIds::MainRoomZoneId;
    mainRoom.kind = ZoneKind::Room;
    mainRoom.label = "Main Demo Room";
    mainRoom.area = Polygon2D{
        .outline = {
            {0.0, 0.0},
            {12.0, 0.0},
            {12.0, 10.0},
            {0.0, 10.0},
        },
    };
    mainRoom.defaultCapacity = 120;
    layout.zones.push_back(mainRoom);

    Zone2D sideRoom;
    sideRoom.id = Sprint1FacilityIds::SideRoomZoneId;
    sideRoom.kind = ZoneKind::Room;
    sideRoom.label = "Side Demo Room";
    sideRoom.area = Polygon2D{
        .outline = {
            {12.0, 0.0},
            {20.0, 0.0},
            {20.0, 10.0},
            {12.0, 10.0},
        },
    };
    sideRoom.defaultCapacity = 80;
    layout.zones.push_back(sideRoom);

    Zone2D exitCorridor;
    exitCorridor.id = Sprint1FacilityIds::ExitCorridorZoneId;
    exitCorridor.kind = ZoneKind::Corridor;
    exitCorridor.label = "Exit Corridor";
    exitCorridor.area = Polygon2D{
        .outline = {
            {20.0, 4.0},
            {24.0, 4.0},
            {24.0, 8.0},
            {20.0, 8.0},
        },
    };
    exitCorridor.defaultCapacity = 24;
    layout.zones.push_back(exitCorridor);

    Zone2D exitZone;
    exitZone.id = Sprint1FacilityIds::ExitZoneId;
    exitZone.kind = ZoneKind::Exit;
    exitZone.label = "Main Exit";
    exitZone.area = Polygon2D{
        .outline = {
            {24.0, 5.0},
            {26.0, 5.0},
            {26.0, 7.0},
            {24.0, 7.0},
        },
    };
    exitZone.defaultCapacity = 20;
    layout.zones.push_back(exitZone);

    Connection2D roomConnection;
    roomConnection.id = Sprint1FacilityIds::OpeningConnectionId;
    roomConnection.kind = ConnectionKind::Opening;
    roomConnection.fromZoneId = mainRoom.id;
    roomConnection.toZoneId = sideRoom.id;
    roomConnection.effectiveWidth = 3.0;
    roomConnection.centerSpan = LineSegment2D{{12.0, 3.5}, {12.0, 6.5}};
    layout.connections.push_back(roomConnection);

    Connection2D corridorConnection;
    corridorConnection.id = Sprint1FacilityIds::DoorwayConnectionId;
    corridorConnection.kind = ConnectionKind::Doorway;
    corridorConnection.fromZoneId = sideRoom.id;
    corridorConnection.toZoneId = exitCorridor.id;
    corridorConnection.effectiveWidth = 2.0;
    corridorConnection.centerSpan = LineSegment2D{{20.0, 5.0}, {20.0, 7.0}};
    layout.connections.push_back(corridorConnection);

    Connection2D exitConnection;
    exitConnection.id = Sprint1FacilityIds::ExitConnectionId;
    exitConnection.kind = ConnectionKind::Exit;
    exitConnection.fromZoneId = exitCorridor.id;
    exitConnection.toZoneId = exitZone.id;
    exitConnection.effectiveWidth = 2.0;
    exitConnection.centerSpan = LineSegment2D{{24.0, 5.0}, {24.0, 7.0}};
    layout.connections.push_back(exitConnection);

    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::WestWallId, {{0.0, 0.0}, {0.0, 10.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::NorthWallId, {{0.0, 10.0}, {20.0, 10.0}, {20.0, 8.0}, {24.0, 8.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::SouthWallId, {{24.0, 4.0}, {20.0, 4.0}, {20.0, 0.0}, {0.0, 0.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::CorridorExitWallUpperId, {{24.0, 8.0}, {24.0, 7.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::CorridorExitWallLowerId, {{24.0, 5.0}, {24.0, 4.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::MainSideWallLowerId, {{12.0, 0.0}, {12.0, 3.5}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::MainSideWallUpperId, {{12.0, 6.5}, {12.0, 10.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::SideCorridorWallLowerId, {{20.0, 0.0}, {20.0, 5.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::SideCorridorWallUpperId, {{20.0, 7.0}, {20.0, 10.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::BarrierId, {{5.0, 3.5}, {7.0, 3.5}, {7.0, 5.0}, {5.0, 5.0}}, true));

    return layout;
}

}  // namespace safecrowd::domain::DemoLayouts
