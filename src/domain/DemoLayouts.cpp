#include "domain/DemoLayouts.h"

#include <utility>

namespace safecrowd::domain::DemoLayouts {
namespace {

Barrier2D makeBarrier(const char* id, std::vector<Point2D> vertices, bool closed = false) {
    Barrier2D barrier;
    barrier.id = id;
    barrier.floorId = Sprint1FacilityIds::FloorId;
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
    layout.levelId = Sprint1FacilityIds::FloorId;
    layout.floors.push_back({
        .id = Sprint1FacilityIds::FloorId,
        .label = "Floor 1",
    });

    Zone2D mainRoom;
    mainRoom.id = Sprint1FacilityIds::MainRoomZoneId;
    mainRoom.floorId = Sprint1FacilityIds::FloorId;
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
    sideRoom.floorId = Sprint1FacilityIds::FloorId;
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

    Zone2D exitPassage;
    exitPassage.id = Sprint1FacilityIds::ExitPassageZoneId;
    exitPassage.floorId = Sprint1FacilityIds::FloorId;
    exitPassage.kind = ZoneKind::Room;
    exitPassage.label = "Exit Passage";
    exitPassage.area = Polygon2D{
        .outline = {
            {20.0, 4.0},
            {24.0, 4.0},
            {24.0, 8.0},
            {20.0, 8.0},
        },
    };
    exitPassage.defaultCapacity = 24;
    layout.zones.push_back(exitPassage);

    Zone2D exitZone;
    exitZone.id = Sprint1FacilityIds::ExitZoneId;
    exitZone.floorId = Sprint1FacilityIds::FloorId;
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
    roomConnection.floorId = Sprint1FacilityIds::FloorId;
    roomConnection.kind = ConnectionKind::Opening;
    roomConnection.fromZoneId = mainRoom.id;
    roomConnection.toZoneId = sideRoom.id;
    roomConnection.effectiveWidth = 3.0;
    roomConnection.centerSpan = LineSegment2D{{12.0, 3.5}, {12.0, 6.5}};
    layout.connections.push_back(roomConnection);

    Connection2D passageConnection;
    passageConnection.id = Sprint1FacilityIds::DoorwayConnectionId;
    passageConnection.floorId = Sprint1FacilityIds::FloorId;
    passageConnection.kind = ConnectionKind::Doorway;
    passageConnection.fromZoneId = sideRoom.id;
    passageConnection.toZoneId = exitPassage.id;
    passageConnection.effectiveWidth = 2.0;
    passageConnection.centerSpan = LineSegment2D{{20.0, 5.0}, {20.0, 7.0}};
    layout.connections.push_back(passageConnection);

    Connection2D exitConnection;
    exitConnection.id = Sprint1FacilityIds::ExitConnectionId;
    exitConnection.floorId = Sprint1FacilityIds::FloorId;
    exitConnection.kind = ConnectionKind::Exit;
    exitConnection.fromZoneId = exitPassage.id;
    exitConnection.toZoneId = exitZone.id;
    exitConnection.effectiveWidth = 2.0;
    exitConnection.centerSpan = LineSegment2D{{24.0, 5.0}, {24.0, 7.0}};
    layout.connections.push_back(exitConnection);

    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::WestWallId, {{0.0, 0.0}, {0.0, 10.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::MainRoomNorthWallId, {{0.0, 10.0}, {12.0, 10.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::MainRoomSouthWallId, {{0.0, 0.0}, {12.0, 0.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::SideRoomNorthWallId, {{12.0, 10.0}, {20.0, 10.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::SideRoomSouthWallId, {{12.0, 0.0}, {20.0, 0.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::PassageNorthWallId, {{20.0, 8.0}, {24.0, 8.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::PassageSouthWallId, {{20.0, 4.0}, {24.0, 4.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::PassageExitWallUpperId, {{24.0, 8.0}, {24.0, 7.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::PassageExitWallLowerId, {{24.0, 5.0}, {24.0, 4.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::MainSideWallLowerId, {{12.0, 0.0}, {12.0, 3.5}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::MainSideWallUpperId, {{12.0, 6.5}, {12.0, 10.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::SidePassageWallLowerId, {{20.0, 0.0}, {20.0, 5.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::SidePassageWallUpperId, {{20.0, 7.0}, {20.0, 10.0}}));
    layout.barriers.push_back(makeBarrier(Sprint1FacilityIds::BarrierId, {{5.0, 3.5}, {7.0, 3.5}, {7.0, 5.0}, {5.0, 5.0}}, true));

    return layout;
}

}  // namespace safecrowd::domain::DemoLayouts
