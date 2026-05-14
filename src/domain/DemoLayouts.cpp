#include "domain/DemoLayouts.h"

#include <utility>

namespace safecrowd::domain::DemoLayouts {
namespace {

Barrier2D makeBarrier(const char* id, const char* floorId, std::vector<Point2D> vertices, bool closed = false) {
    Barrier2D barrier;
    barrier.id = id;
    barrier.floorId = floorId;
    barrier.blocksMovement = true;
    barrier.geometry = Polyline2D{
        .vertices = std::move(vertices),
        .closed = closed,
    };
    return barrier;
}

Barrier2D makeBarrier(const char* id, std::vector<Point2D> vertices, bool closed = false) {
    return makeBarrier(id, Sprint1FacilityIds::FloorId, std::move(vertices), closed);
}

Zone2D makeRectZone(
    const char* id,
    const char* floorId,
    ZoneKind kind,
    const char* label,
    double minX,
    double minY,
    double maxX,
    double maxY,
    std::size_t capacity,
    bool isStair = false) {
    Zone2D zone;
    zone.id = id;
    zone.floorId = floorId;
    zone.kind = kind;
    zone.label = label;
    zone.area = Polygon2D{
        .outline = {
            {minX, minY},
            {maxX, minY},
            {maxX, maxY},
            {minX, maxY},
        },
    };
    zone.defaultCapacity = capacity;
    zone.isStair = isStair;
    return zone;
}

Connection2D makeConnection(
    const char* id,
    const char* floorId,
    ConnectionKind kind,
    const char* fromZoneId,
    const char* toZoneId,
    double effectiveWidth,
    LineSegment2D centerSpan,
    bool isStair = false) {
    Connection2D connection;
    connection.id = id;
    connection.floorId = floorId;
    connection.kind = kind;
    connection.fromZoneId = fromZoneId;
    connection.toZoneId = toZoneId;
    connection.effectiveWidth = effectiveWidth;
    connection.centerSpan = centerSpan;
    connection.isStair = isStair;
    return connection;
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

FacilityLayout2D twoFloorEvacuationFacility() {
    using Ids = TwoFloorEvacuationIds;

    FacilityLayout2D layout{};
    layout.id = Ids::LayoutId;
    layout.name = "Two-floor Evacuation Demo Layout";
    layout.levelId = Ids::LowerFloorId;
    layout.floors.push_back({
        .id = Ids::LowerFloorId,
        .label = "Floor 1",
    });
    layout.floors.push_back({
        .id = Ids::UpperFloorId,
        .label = "Floor 2",
        .elevationMeters = 3.5,
    });

    layout.zones.push_back(makeRectZone(
        Ids::UpperWestTrainingZoneId,
        Ids::UpperFloorId,
        ZoneKind::Room,
        "West Training Room",
        1.0,
        1.0,
        10.0,
        6.0,
        60));
    layout.zones.push_back(makeRectZone(
        Ids::UpperBriefingZoneId,
        Ids::UpperFloorId,
        ZoneKind::Room,
        "Briefing Room",
        10.0,
        1.0,
        18.0,
        6.0,
        48));
    layout.zones.push_back(makeRectZone(
        Ids::UpperEastTrainingZoneId,
        Ids::UpperFloorId,
        ZoneKind::Room,
        "East Training Room",
        18.0,
        1.0,
        27.0,
        6.0,
        60));
    layout.zones.push_back(makeRectZone(
        Ids::UpperCorridorZoneId,
        Ids::UpperFloorId,
        ZoneKind::Room,
        "Upper Corridor",
        1.0,
        6.0,
        27.0,
        9.0,
        90));
    layout.zones.push_back(makeRectZone(
        Ids::UpperWestStairZoneId,
        Ids::UpperFloorId,
        ZoneKind::Stair,
        "Upper West Stair",
        3.0,
        9.0,
        5.0,
        13.0,
        24,
        true));
    layout.zones.push_back(makeRectZone(
        Ids::UpperEastStairZoneId,
        Ids::UpperFloorId,
        ZoneKind::Stair,
        "Upper East Stair",
        23.0,
        9.0,
        25.0,
        13.0,
        24,
        true));
    layout.zones.push_back(makeRectZone(
        Ids::LowerWestStairZoneId,
        Ids::LowerFloorId,
        ZoneKind::Stair,
        "Lower West Stair",
        1.0,
        9.0,
        3.0,
        13.0,
        24,
        true));
    layout.zones.push_back(makeRectZone(
        Ids::LowerEastStairZoneId,
        Ids::LowerFloorId,
        ZoneKind::Stair,
        "Lower East Stair",
        25.0,
        9.0,
        27.0,
        13.0,
        24,
        true));
    layout.zones.push_back(makeRectZone(
        Ids::LowerWestVestibuleZoneId,
        Ids::LowerFloorId,
        ZoneKind::Room,
        "West Exit Vestibule",
        1.0,
        4.0,
        6.0,
        9.0,
        36));
    layout.zones.push_back(makeRectZone(
        Ids::LowerLobbyZoneId,
        Ids::LowerFloorId,
        ZoneKind::Room,
        "Ground Floor Lobby",
        6.0,
        4.0,
        22.0,
        10.0,
        120));
    layout.zones.push_back(makeRectZone(
        Ids::LowerEastVestibuleZoneId,
        Ids::LowerFloorId,
        ZoneKind::Room,
        "East Exit Vestibule",
        22.0,
        4.0,
        27.0,
        9.0,
        36));
    layout.zones.push_back(makeRectZone(
        Ids::WestExitZoneId,
        Ids::LowerFloorId,
        ZoneKind::Exit,
        "West Exit",
        -2.0,
        5.0,
        1.0,
        8.0,
        40));
    layout.zones.push_back(makeRectZone(
        Ids::EastExitZoneId,
        Ids::LowerFloorId,
        ZoneKind::Exit,
        "East Exit",
        27.0,
        5.0,
        30.0,
        8.0,
        40));

    layout.connections.push_back(makeConnection(
        Ids::UpperWestTrainingToCorridorConnectionId,
        Ids::UpperFloorId,
        ConnectionKind::Doorway,
        Ids::UpperWestTrainingZoneId,
        Ids::UpperCorridorZoneId,
        3.0,
        {{4.0, 6.0}, {7.0, 6.0}}));
    layout.connections.push_back(makeConnection(
        Ids::UpperBriefingToCorridorConnectionId,
        Ids::UpperFloorId,
        ConnectionKind::Doorway,
        Ids::UpperBriefingZoneId,
        Ids::UpperCorridorZoneId,
        3.0,
        {{12.5, 6.0}, {15.5, 6.0}}));
    layout.connections.push_back(makeConnection(
        Ids::UpperEastTrainingToCorridorConnectionId,
        Ids::UpperFloorId,
        ConnectionKind::Doorway,
        Ids::UpperEastTrainingZoneId,
        Ids::UpperCorridorZoneId,
        3.0,
        {{21.0, 6.0}, {24.0, 6.0}}));
    layout.connections.push_back(makeConnection(
        Ids::UpperCorridorWestStairConnectionId,
        Ids::UpperFloorId,
        ConnectionKind::Opening,
        Ids::UpperCorridorZoneId,
        Ids::UpperWestStairZoneId,
        1.6,
        {{3.2, 9.0}, {4.8, 9.0}}));
    layout.connections.push_back(makeConnection(
        Ids::UpperCorridorEastStairConnectionId,
        Ids::UpperFloorId,
        ConnectionKind::Opening,
        Ids::UpperCorridorZoneId,
        Ids::UpperEastStairZoneId,
        1.6,
        {{23.2, 9.0}, {24.8, 9.0}}));
    auto westVerticalStair = makeConnection(
        Ids::WestStairVerticalConnectionId,
        Ids::LowerFloorId,
        ConnectionKind::Stair,
        Ids::UpperWestStairZoneId,
        Ids::LowerWestStairZoneId,
        1.2,
        {{3.0, 11.6}, {3.0, 12.8}},
        true);
    westVerticalStair.lowerEntryDirection = StairEntryDirection::South;
    westVerticalStair.upperEntryDirection = StairEntryDirection::South;
    layout.connections.push_back(westVerticalStair);
    auto eastVerticalStair = makeConnection(
        Ids::EastStairVerticalConnectionId,
        Ids::LowerFloorId,
        ConnectionKind::Stair,
        Ids::UpperEastStairZoneId,
        Ids::LowerEastStairZoneId,
        1.2,
        {{25.0, 11.6}, {25.0, 12.8}},
        true);
    eastVerticalStair.lowerEntryDirection = StairEntryDirection::South;
    eastVerticalStair.upperEntryDirection = StairEntryDirection::South;
    layout.connections.push_back(eastVerticalStair);
    layout.connections.push_back(makeConnection(
        Ids::LowerWestStairVestibuleConnectionId,
        Ids::LowerFloorId,
        ConnectionKind::Opening,
        Ids::LowerWestStairZoneId,
        Ids::LowerWestVestibuleZoneId,
        1.6,
        {{1.2, 9.0}, {2.8, 9.0}}));
    layout.connections.push_back(makeConnection(
        Ids::LowerEastStairVestibuleConnectionId,
        Ids::LowerFloorId,
        ConnectionKind::Opening,
        Ids::LowerEastStairZoneId,
        Ids::LowerEastVestibuleZoneId,
        1.6,
        {{25.2, 9.0}, {26.8, 9.0}}));
    layout.connections.push_back(makeConnection(
        Ids::LowerWestVestibuleLobbyConnectionId,
        Ids::LowerFloorId,
        ConnectionKind::Opening,
        Ids::LowerWestVestibuleZoneId,
        Ids::LowerLobbyZoneId,
        2.0,
        {{6.0, 5.5}, {6.0, 7.5}}));
    layout.connections.push_back(makeConnection(
        Ids::LowerLobbyEastVestibuleConnectionId,
        Ids::LowerFloorId,
        ConnectionKind::Opening,
        Ids::LowerLobbyZoneId,
        Ids::LowerEastVestibuleZoneId,
        2.0,
        {{22.0, 5.5}, {22.0, 7.5}}));
    layout.connections.push_back(makeConnection(
        Ids::LowerWestExitConnectionId,
        Ids::LowerFloorId,
        ConnectionKind::Exit,
        Ids::LowerWestVestibuleZoneId,
        Ids::WestExitZoneId,
        2.0,
        {{1.0, 5.5}, {1.0, 7.5}}));
    layout.connections.push_back(makeConnection(
        Ids::LowerEastExitConnectionId,
        Ids::LowerFloorId,
        ConnectionKind::Exit,
        Ids::LowerEastVestibuleZoneId,
        Ids::EastExitZoneId,
        2.0,
        {{27.0, 5.5}, {27.0, 7.5}}));

    layout.barriers.push_back(makeBarrier("two-floor-upper-south-wall", Ids::UpperFloorId, {{1.0, 1.0}, {27.0, 1.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-west-wall", Ids::UpperFloorId, {{1.0, 1.0}, {1.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-east-wall", Ids::UpperFloorId, {{27.0, 1.0}, {27.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-west-room-divider", Ids::UpperFloorId, {{10.0, 1.0}, {10.0, 6.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-east-room-divider", Ids::UpperFloorId, {{18.0, 1.0}, {18.0, 6.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-room-corridor-wall-1", Ids::UpperFloorId, {{1.0, 6.0}, {4.0, 6.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-room-corridor-wall-2", Ids::UpperFloorId, {{7.0, 6.0}, {12.5, 6.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-room-corridor-wall-3", Ids::UpperFloorId, {{15.5, 6.0}, {21.0, 6.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-room-corridor-wall-4", Ids::UpperFloorId, {{24.0, 6.0}, {27.0, 6.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-corridor-north-1", Ids::UpperFloorId, {{1.0, 9.0}, {3.2, 9.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-corridor-north-2", Ids::UpperFloorId, {{4.8, 9.0}, {23.2, 9.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-corridor-north-3", Ids::UpperFloorId, {{24.8, 9.0}, {27.0, 9.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-west-stair-west-1", Ids::UpperFloorId, {{3.0, 9.0}, {3.0, 11.6}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-west-stair-west-2", Ids::UpperFloorId, {{3.0, 12.8}, {3.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-west-stair-east", Ids::UpperFloorId, {{5.0, 9.0}, {5.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-west-stair-north", Ids::UpperFloorId, {{3.0, 13.0}, {5.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-east-stair-west", Ids::UpperFloorId, {{23.0, 9.0}, {23.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-east-stair-east-1", Ids::UpperFloorId, {{25.0, 9.0}, {25.0, 11.6}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-east-stair-east-2", Ids::UpperFloorId, {{25.0, 12.8}, {25.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-upper-east-stair-north", Ids::UpperFloorId, {{23.0, 13.0}, {25.0, 13.0}}));
    layout.barriers.push_back(makeBarrier(
        "two-floor-upper-corridor-column",
        Ids::UpperFloorId,
        {{13.0, 7.0}, {15.0, 7.0}, {15.0, 8.0}, {13.0, 8.0}},
        true));

    layout.barriers.push_back(makeBarrier("two-floor-lower-west-stair-west", Ids::LowerFloorId, {{1.0, 9.0}, {1.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-west-stair-east-1", Ids::LowerFloorId, {{3.0, 9.0}, {3.0, 11.6}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-west-stair-east-2", Ids::LowerFloorId, {{3.0, 12.8}, {3.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-west-stair-north", Ids::LowerFloorId, {{1.0, 13.0}, {3.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-east-stair-west-1", Ids::LowerFloorId, {{25.0, 9.0}, {25.0, 11.6}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-east-stair-west-2", Ids::LowerFloorId, {{25.0, 12.8}, {25.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-east-stair-east", Ids::LowerFloorId, {{27.0, 9.0}, {27.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-east-stair-north", Ids::LowerFloorId, {{25.0, 13.0}, {27.0, 13.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-vestibule-south-west", Ids::LowerFloorId, {{1.0, 4.0}, {6.0, 4.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-vestibule-west-wall-1", Ids::LowerFloorId, {{1.0, 4.0}, {1.0, 5.5}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-vestibule-west-wall-2", Ids::LowerFloorId, {{1.0, 7.5}, {1.0, 9.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-west-vestibule-lobby-wall-1", Ids::LowerFloorId, {{6.0, 4.0}, {6.0, 5.5}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-west-vestibule-lobby-wall-2", Ids::LowerFloorId, {{6.0, 7.5}, {6.0, 9.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-west-stair-south-1", Ids::LowerFloorId, {{1.0, 9.0}, {1.2, 9.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-west-stair-south-2", Ids::LowerFloorId, {{2.8, 9.0}, {6.0, 9.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-lobby-south", Ids::LowerFloorId, {{6.0, 4.0}, {22.0, 4.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-lobby-north", Ids::LowerFloorId, {{6.0, 10.0}, {22.0, 10.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-lobby-east-wall-1", Ids::LowerFloorId, {{22.0, 4.0}, {22.0, 5.5}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-lobby-east-wall-2", Ids::LowerFloorId, {{22.0, 7.5}, {22.0, 10.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-vestibule-south-east", Ids::LowerFloorId, {{22.0, 4.0}, {27.0, 4.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-vestibule-east-wall-1", Ids::LowerFloorId, {{27.0, 4.0}, {27.0, 5.5}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-vestibule-east-wall-2", Ids::LowerFloorId, {{27.0, 7.5}, {27.0, 9.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-east-stair-south-1", Ids::LowerFloorId, {{22.0, 9.0}, {25.2, 9.0}}));
    layout.barriers.push_back(makeBarrier("two-floor-lower-east-stair-south-2", Ids::LowerFloorId, {{26.8, 9.0}, {27.0, 9.0}}));
    layout.barriers.push_back(makeBarrier(
        "two-floor-lower-info-desk",
        Ids::LowerFloorId,
        {{13.0, 5.8}, {15.0, 5.8}, {15.0, 7.0}, {13.0, 7.0}},
        true));

    return layout;
}

}  // namespace safecrowd::domain::DemoLayouts
