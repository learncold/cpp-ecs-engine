#include "domain/DemoLayouts.h"

namespace safecrowd::domain::DemoLayouts {

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
            {10.0, 0.0},
            {10.0, 10.0},
            {0.0, 10.0},
        },
    };
    mainRoom.defaultCapacity = 200;
    layout.zones.push_back(mainRoom);

    Zone2D sideRoom;
    sideRoom.id = Sprint1FacilityIds::SideRoomZoneId;
    sideRoom.kind = ZoneKind::Room;
    sideRoom.label = "Side Demo Room";
    sideRoom.area = Polygon2D{
        .outline = {
            {10.0, 0.0},
            {20.0, 0.0},
            {20.0, 10.0},
            {10.0, 10.0},
        },
    };
    sideRoom.defaultCapacity = 80;
    layout.zones.push_back(sideRoom);

    Zone2D exitZone;
    exitZone.id = Sprint1FacilityIds::ExitZoneId;
    exitZone.kind = ZoneKind::Exit;
    exitZone.label = "Main Exit";
    exitZone.area = Polygon2D{
        .outline = {
            {18.0, 10.0},
            {20.0, 10.0},
            {20.0, 12.0},
            {18.0, 12.0},
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
    roomConnection.centerSpan = LineSegment2D{{10.0, 4.0}, {10.0, 6.0}};
    layout.connections.push_back(roomConnection);

    Connection2D exitConnection;
    exitConnection.id = Sprint1FacilityIds::ExitConnectionId;
    exitConnection.kind = ConnectionKind::Exit;
    exitConnection.fromZoneId = sideRoom.id;
    exitConnection.toZoneId = exitZone.id;
    exitConnection.effectiveWidth = 2.0;
    exitConnection.centerSpan = LineSegment2D{{18.0, 10.0}, {20.0, 10.0}};
    layout.connections.push_back(exitConnection);

    Barrier2D centerObstacle;
    centerObstacle.id = Sprint1FacilityIds::BarrierId;
    centerObstacle.blocksMovement = true;
    centerObstacle.geometry = Polyline2D{
        .vertices = {
            {4.0, 4.0},
            {6.0, 4.0},
            {6.0, 5.0},
            {4.0, 5.0},
        },
        .closed = true,
    };
    layout.barriers.push_back(centerObstacle);

    return layout;
}

}  // namespace safecrowd::domain::DemoLayouts
