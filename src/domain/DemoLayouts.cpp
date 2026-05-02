#include "domain/DemoLayouts.h"

#include <cmath>
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

namespace {

Barrier2D makeFloorOutlineBarrier(const char* id, const char* floorId, double width, double height) {
    Barrier2D barrier;
    barrier.id = id;
    barrier.floorId = floorId;
    barrier.blocksMovement = true;
    barrier.geometry = Polyline2D{
        .vertices = {
            {0.0, 0.0},
            {width, 0.0},
            {width, height},
            {0.0, height},
            {0.0, 0.0},
        },
        .closed = false,
    };
    return barrier;
}

}  // namespace

FacilityLayout2D demoTwoFloorFacility() {
    constexpr double kWidth = 30.0;
    constexpr double kHeight = 20.0;

    FacilityLayout2D layout{};
    layout.id = TwoFloorFacilityIds::LayoutId;
    layout.name = "2F demo";
    layout.levelId = TwoFloorFacilityIds::Floor1Id;
    layout.floors.push_back({
        .id = TwoFloorFacilityIds::Floor1Id,
        .label = "1F",
        .elevationMeters = 0.0,
    });
    layout.floors.push_back({
        .id = TwoFloorFacilityIds::Floor2Id,
        .label = "2F",
        .elevationMeters = 3.5,
    });

    const auto makeRoom = [](const std::string& id,
                             const std::string& floorId,
                             const std::string& label,
                             ZoneKind kind,
                             const std::vector<Point2D>& outline,
                             std::size_t capacity) {
        Zone2D zone;
        zone.id = id;
        zone.floorId = floorId;
        zone.kind = kind;
        zone.isStair = kind == ZoneKind::Stair;
        zone.label = label;
        zone.area = Polygon2D{.outline = outline};
        zone.defaultCapacity = capacity;
        return zone;
    };

    const auto addDoor = [&](const std::string& id,
                             const std::string& floorId,
                             ConnectionKind kind,
                             const std::string& fromZoneId,
                             const std::string& toZoneId,
                             Point2D start,
                             Point2D end) {
        Connection2D door;
        door.id = id;
        door.floorId = floorId;
        door.kind = kind;
        door.fromZoneId = fromZoneId;
        door.toZoneId = toZoneId;
        door.centerSpan = LineSegment2D{start, end};
        door.effectiveWidth = std::sqrt((end.x - start.x) * (end.x - start.x) + (end.y - start.y) * (end.y - start.y));
        layout.connections.push_back(std::move(door));
    };

    const auto addStairLink = [&](const std::string& id,
                                  const std::string& floorId,
                                  const std::string& fromZoneId,
                                  const std::string& toZoneId,
                                  Point2D start,
                                  Point2D end) {
        Connection2D link;
        link.id = id;
        link.floorId = floorId;
        link.kind = ConnectionKind::Stair;
        link.isStair = true;
        link.fromZoneId = fromZoneId;
        link.toZoneId = toZoneId;
        link.centerSpan = LineSegment2D{start, end};
        link.effectiveWidth = std::sqrt((end.x - start.x) * (end.x - start.x) + (end.y - start.y) * (end.y - start.y));
        link.lowerEntryDirection = StairEntryDirection::North;
        link.upperEntryDirection = StairEntryDirection::South;
        layout.connections.push_back(std::move(link));
    };

    const auto addWall = [&](const std::string& id,
                             const std::string& floorId,
                             Point2D start,
                             Point2D end) {
        Barrier2D barrier;
        barrier.id = id;
        barrier.floorId = floorId;
        barrier.geometry = Polyline2D{
            .vertices = {start, end},
            .closed = false,
        };
        barrier.blocksMovement = true;
        layout.barriers.push_back(std::move(barrier));
    };

    const double hallMinX = 2.0;
    const double hallMaxX = 28.0;
    const double hallMinY = 7.0;
    const double hallMaxY = 14.0;
    const double roomBandMinX = 4.5;
    const double roomBandMaxX = 25.5;
    const double bottomRoomMinY = 0.0;
    const double bottomRoomMaxY = 7.0;
    const double topRoomMinY = 14.0;
    const double topRoomMaxY = 20.0;
    constexpr int kBottomRoomCount = 3;
    constexpr int kTopRoomCountL2 = 3;
    const double bottomRoomWidth = (roomBandMaxX - roomBandMinX) / static_cast<double>(kBottomRoomCount);
    const double topRoomWidth = (roomBandMaxX - roomBandMinX) / static_cast<double>(kTopRoomCountL2);

    const auto addHall = [&](const std::string& id,
                             const std::string& floorId,
                             const std::string& label,
                             const std::vector<Point2D>& outline) {
        layout.zones.push_back(makeRoom(
            id,
            floorId,
            label,
            ZoneKind::Room,
            outline,
            500));
    };

    const auto addBottomRooms = [&](const std::string& floorId,
                                    const std::string& hallId,
                                    const std::string& roomPrefix,
                                    const std::string& doorPrefix) {
        for (int i = 0; i < kBottomRoomCount; ++i) {
            const double x0 = roomBandMinX + bottomRoomWidth * static_cast<double>(i);
            const double x1 = roomBandMinX + bottomRoomWidth * static_cast<double>(i + 1);
            const auto roomId = roomPrefix + std::to_string(i + 1);
            layout.zones.push_back(makeRoom(
                roomId,
                floorId,
                "Bottom Room " + std::to_string(i + 1),
                ZoneKind::Room,
                {{x0, bottomRoomMinY}, {x1, bottomRoomMinY}, {x1, bottomRoomMaxY}, {x0, bottomRoomMaxY}},
                50));

            const double doorCenterX = (x0 + x1) * 0.5;
            addDoor(
                doorPrefix + std::to_string(i + 1),
                floorId,
                ConnectionKind::Doorway,
                hallId,
                roomId,
                {.x = doorCenterX - 0.9, .y = hallMinY},
                {.x = doorCenterX + 0.9, .y = hallMinY});
        }
    };

    const auto addTopRooms2F = [&](const std::string& floorId, const std::string& hallId) {
        for (int i = 0; i < kTopRoomCountL2; ++i) {
            const double x0 = roomBandMinX + topRoomWidth * static_cast<double>(i);
            const double x1 = roomBandMinX + topRoomWidth * static_cast<double>(i + 1);
            const auto roomId = std::string(TwoFloorFacilityIds::TopRoomL2Prefix) + std::to_string(i + 1);
            layout.zones.push_back(makeRoom(
                roomId,
                floorId,
                "Top Room " + std::to_string(i + 1),
                ZoneKind::Room,
                {{x0, topRoomMinY}, {x1, topRoomMinY}, {x1, topRoomMaxY}, {x0, topRoomMaxY}},
                44));

            const double doorCenterX = (x0 + x1) * 0.5;
            addDoor(
                std::string(TwoFloorFacilityIds::TopDoorL2Prefix) + std::to_string(i + 1),
                floorId,
                ConnectionKind::Doorway,
                hallId,
                roomId,
                {.x = doorCenterX - 0.9, .y = hallMaxY},
                {.x = doorCenterX + 0.9, .y = hallMaxY});
        }

        layout.zones.push_back(makeRoom(
            TwoFloorFacilityIds::CornerRoomL2Id,
            floorId,
            "Top Left Corner",
            ZoneKind::Room,
            {{0.0, 14.0}, {2.2, 14.0}, {2.2, 20.0}, {0.0, 20.0}},
            18));
        addDoor(
            TwoFloorFacilityIds::CornerDoorL2Id,
            floorId,
            ConnectionKind::Doorway,
            hallId,
            TwoFloorFacilityIds::CornerRoomL2Id,
            {.x = 2.2, .y = 15.2},
            {.x = 2.2, .y = 17.0});

        layout.zones.push_back(makeRoom(
            TwoFloorFacilityIds::CornerRoomRightL2Id,
            floorId,
            "Top Right Corner",
            ZoneKind::Room,
            {{25.5, 14.0}, {28.0, 14.0}, {28.0, 20.0}, {25.5, 20.0}},
            18));
        addDoor(
            TwoFloorFacilityIds::CornerRightDoorL2Id,
            floorId,
            ConnectionKind::Doorway,
            hallId,
            TwoFloorFacilityIds::CornerRoomRightL2Id,
            {.x = 28.0, .y = 15.2},
            {.x = 28.0, .y = 17.0});
    };

    const auto addTopRooms1F = [&](const std::string& floorId, const std::string& hallId) {
        layout.zones.push_back(makeRoom(
            TwoFloorFacilityIds::CornerRoomL1Id,
            floorId,
            "Top Left Corner",
            ZoneKind::Room,
            {{0.0, 14.0}, {2.2, 14.0}, {2.2, 20.0}, {0.0, 20.0}},
            18));
        addDoor(
            TwoFloorFacilityIds::CornerDoorL1Id,
            floorId,
            ConnectionKind::Doorway,
            hallId,
            TwoFloorFacilityIds::CornerRoomL1Id,
            {.x = 2.2, .y = 15.2},
            {.x = 2.2, .y = 17.0});

        const std::vector<std::pair<double, double>> roomRanges{
            {4.0, 12.0},
            {18.0, 28.0},
        };
        for (std::size_t index = 0; index < roomRanges.size(); ++index) {
            const auto& [x0, x1] = roomRanges[index];
            const auto roomId = std::string(TwoFloorFacilityIds::TopRoomL1Prefix) + std::to_string(index + 1);
            layout.zones.push_back(makeRoom(
                roomId,
                floorId,
                "Top Room " + std::to_string(index + 1),
                ZoneKind::Room,
                {{x0, topRoomMinY}, {x1, topRoomMinY}, {x1, topRoomMaxY}, {x0, topRoomMaxY}},
                48));

            const double doorCenterX = (x0 + x1) * 0.5;
            addDoor(
                std::string(TwoFloorFacilityIds::TopDoorL1Prefix) + std::to_string(index + 1),
                floorId,
                ConnectionKind::Doorway,
                hallId,
                roomId,
                {.x = doorCenterX - 0.9, .y = hallMaxY},
                {.x = doorCenterX + 0.9, .y = hallMaxY});
        }
    };

    addHall(
        TwoFloorFacilityIds::HallZoneL1Id,
        TwoFloorFacilityIds::Floor1Id,
        "1F Hall",
        {
            {0.0, 8.0},
            {2.2, 8.0},
            {2.2, hallMinY},
            {hallMaxX, hallMinY},
            {hallMaxX, 8.0},
            {30.0, 8.0},
            {30.0, 20.0},
            {28.0, 20.0},
            {28.0, hallMaxY},
            {25.5, hallMaxY},
            {4.0, hallMaxY},
            {4.0, 20.0},
            {2.2, 20.0},
            {2.2, hallMaxY},
            {0.0, hallMaxY},
        });
    addBottomRooms(
        TwoFloorFacilityIds::Floor1Id,
        TwoFloorFacilityIds::HallZoneL1Id,
        TwoFloorFacilityIds::BottomRoomL1Prefix,
        TwoFloorFacilityIds::BottomDoorL1Prefix);
    addTopRooms1F(TwoFloorFacilityIds::Floor1Id, TwoFloorFacilityIds::HallZoneL1Id);

    addHall(
        TwoFloorFacilityIds::HallZoneL2Id,
        TwoFloorFacilityIds::Floor2Id,
        "2F Hall",
        {
            {2.2, hallMinY},
            {hallMaxX, hallMinY},
            {hallMaxX, hallMaxY},
            {30.0, hallMaxY},
            {30.0, 20.0},
            {28.0, 20.0},
            {28.0, hallMaxY},
            {25.5, hallMaxY},
            {4.5, hallMaxY},
            {4.5, 20.0},
            {2.2, 20.0},
        });
    addBottomRooms(
        TwoFloorFacilityIds::Floor2Id,
        TwoFloorFacilityIds::HallZoneL2Id,
        TwoFloorFacilityIds::BottomRoomL2Prefix,
        TwoFloorFacilityIds::BottomDoorL2Prefix);
    addTopRooms2F(TwoFloorFacilityIds::Floor2Id, TwoFloorFacilityIds::HallZoneL2Id);

    layout.zones.push_back(makeRoom(
        TwoFloorFacilityIds::LeftStairZoneL1Id,
        TwoFloorFacilityIds::Floor1Id,
        "Left Stairs",
        ZoneKind::Stair,
        {{0.0, 0.0}, {2.2, 0.0}, {2.2, 8.0}, {0.0, 8.0}},
        20));
    layout.zones.push_back(makeRoom(
        TwoFloorFacilityIds::RightStairZoneL1Id,
        TwoFloorFacilityIds::Floor1Id,
        "Right Stairs",
        ZoneKind::Stair,
        {{27.6, 0.0}, {30.0, 0.0}, {30.0, 8.0}, {27.6, 8.0}},
        20));
    layout.zones.push_back(makeRoom(
        TwoFloorFacilityIds::LeftStairZoneL2Id,
        TwoFloorFacilityIds::Floor2Id,
        "Left Stairs",
        ZoneKind::Stair,
        {{0.0, 2.0}, {2.4, 2.0}, {2.4, 8.5}, {0.0, 8.5}},
        20));
    layout.zones.push_back(makeRoom(
        TwoFloorFacilityIds::RightStairZoneL2Id,
        TwoFloorFacilityIds::Floor2Id,
        "Right Stairs",
        ZoneKind::Stair,
        {{27.6, 2.0}, {30.0, 2.0}, {30.0, 8.5}, {27.6, 8.5}},
        20));

    addDoor(
        TwoFloorFacilityIds::LeftStairDoorL1Id,
        TwoFloorFacilityIds::Floor1Id,
        ConnectionKind::Opening,
        TwoFloorFacilityIds::HallZoneL1Id,
        TwoFloorFacilityIds::LeftStairZoneL1Id,
        {.x = 2.2, .y = 7.0},
        {.x = 2.2, .y = 8.0});
    addDoor(
        TwoFloorFacilityIds::RightStairDoorL1Id,
        TwoFloorFacilityIds::Floor1Id,
        ConnectionKind::Opening,
        TwoFloorFacilityIds::HallZoneL1Id,
        TwoFloorFacilityIds::RightStairZoneL1Id,
        {.x = hallMaxX, .y = 7.0},
        {.x = hallMaxX, .y = 8.0});
    addDoor(
        TwoFloorFacilityIds::LeftStairDoorL2Id,
        TwoFloorFacilityIds::Floor2Id,
        ConnectionKind::Opening,
        TwoFloorFacilityIds::HallZoneL2Id,
        TwoFloorFacilityIds::LeftStairZoneL2Id,
        {.x = hallMinX, .y = 7.2},
        {.x = hallMinX, .y = 8.6});
    addDoor(
        TwoFloorFacilityIds::RightStairDoorL2Id,
        TwoFloorFacilityIds::Floor2Id,
        ConnectionKind::Opening,
        TwoFloorFacilityIds::HallZoneL2Id,
        TwoFloorFacilityIds::RightStairZoneL2Id,
        {.x = hallMaxX, .y = 7.2},
        {.x = hallMaxX, .y = 8.6});

    layout.zones.push_back(makeRoom(
        TwoFloorFacilityIds::ExitZoneL1Id,
        TwoFloorFacilityIds::Floor1Id,
        "Exit",
        ZoneKind::Exit,
        {{12.0, 14.0}, {18.0, 14.0}, {18.0, 20.0}, {12.0, 20.0}},
        60));
    addDoor(
        TwoFloorFacilityIds::ExitDoorL1Id,
        TwoFloorFacilityIds::Floor1Id,
        ConnectionKind::Exit,
        TwoFloorFacilityIds::HallZoneL1Id,
        TwoFloorFacilityIds::ExitZoneL1Id,
        {.x = 14.0, .y = hallMaxY},
        {.x = 16.0, .y = hallMaxY});

    const double roomDivider1 = roomBandMinX + bottomRoomWidth;
    const double roomDivider2 = roomBandMinX + bottomRoomWidth * 2.0;

    // 1F top walls
    addWall("barrier-l1-top-left-door-upper", TwoFloorFacilityIds::Floor1Id, {2.2, 17.0}, {2.2, 20.0});
    addWall("barrier-l1-top-left-door-lower", TwoFloorFacilityIds::Floor1Id, {2.2, 14.0}, {2.2, 15.2});
    addWall("barrier-l1-top-left-bottom", TwoFloorFacilityIds::Floor1Id, {0.0, 14.0}, {2.2, 14.0});
    addWall("barrier-l1-top-room-left-side", TwoFloorFacilityIds::Floor1Id, {4.0, 14.0}, {4.0, 20.0});
    addWall("barrier-l1-top-room-mid-left", TwoFloorFacilityIds::Floor1Id, {12.0, 14.0}, {12.0, 20.0});
    addWall("barrier-l1-top-room-mid-right", TwoFloorFacilityIds::Floor1Id, {18.0, 14.0}, {18.0, 20.0});
    addWall("barrier-l1-top-right-door-upper", TwoFloorFacilityIds::Floor1Id, {28.0, 17.0}, {28.0, 20.0});
    addWall("barrier-l1-top-right-door-lower", TwoFloorFacilityIds::Floor1Id, {28.0, 14.0}, {28.0, 15.2});
    addWall("barrier-l1-top-right-bottom", TwoFloorFacilityIds::Floor1Id, {28.0, 14.0}, {30.0, 14.0});
    addWall("barrier-l1-top-main-left-a", TwoFloorFacilityIds::Floor1Id, {4.0, 14.0}, {7.1, 14.0});
    addWall("barrier-l1-top-main-left-b", TwoFloorFacilityIds::Floor1Id, {8.9, 14.0}, {12.0, 14.0});
    addWall("barrier-l1-top-exit-left", TwoFloorFacilityIds::Floor1Id, {12.0, 14.0}, {14.0, 14.0});
    addWall("barrier-l1-top-exit-right", TwoFloorFacilityIds::Floor1Id, {16.0, 14.0}, {18.0, 14.0});
    addWall("barrier-l1-top-main-right-a", TwoFloorFacilityIds::Floor1Id, {18.0, 14.0}, {22.1, 14.0});
    addWall("barrier-l1-top-main-right-b", TwoFloorFacilityIds::Floor1Id, {23.9, 14.0}, {25.5, 14.0});
    addWall("barrier-l1-top-right-inner-divider", TwoFloorFacilityIds::Floor1Id, {25.5, 14.0}, {25.5, 20.0});

    // 1F bottom walls
    addWall("barrier-l1-bottom-left-side", TwoFloorFacilityIds::Floor1Id, {4.5, 0.0}, {4.5, 7.0});
    addWall("barrier-l1-bottom-divider-1", TwoFloorFacilityIds::Floor1Id, {roomDivider1, 0.0}, {roomDivider1, 7.0});
    addWall("barrier-l1-bottom-divider-2", TwoFloorFacilityIds::Floor1Id, {roomDivider2, 0.0}, {roomDivider2, 7.0});
    addWall("barrier-l1-bottom-right-side", TwoFloorFacilityIds::Floor1Id, {25.5, 0.0}, {25.5, 7.0});
    addWall("barrier-l1-bottom-top-a", TwoFloorFacilityIds::Floor1Id, {4.5, 7.0}, {7.1, 7.0});
    addWall("barrier-l1-bottom-top-b", TwoFloorFacilityIds::Floor1Id, {8.9, 7.0}, {14.1, 7.0});
    addWall("barrier-l1-bottom-top-c", TwoFloorFacilityIds::Floor1Id, {15.9, 7.0}, {21.1, 7.0});
    addWall("barrier-l1-bottom-top-d", TwoFloorFacilityIds::Floor1Id, {22.9, 7.0}, {25.5, 7.0});

    // 2F top walls
    addWall("barrier-l2-top-left-door-upper", TwoFloorFacilityIds::Floor2Id, {2.2, 17.0}, {2.2, 20.0});
    addWall("barrier-l2-top-left-door-lower", TwoFloorFacilityIds::Floor2Id, {2.2, 14.0}, {2.2, 15.2});
    addWall("barrier-l2-top-left-side", TwoFloorFacilityIds::Floor2Id, {4.5, 14.0}, {4.5, 20.0});
    addWall("barrier-l2-top-divider-1", TwoFloorFacilityIds::Floor2Id, {roomDivider1, 14.0}, {roomDivider1, 20.0});
    addWall("barrier-l2-top-divider-2", TwoFloorFacilityIds::Floor2Id, {roomDivider2, 14.0}, {roomDivider2, 20.0});
    addWall("barrier-l2-top-right-side", TwoFloorFacilityIds::Floor2Id, {25.5, 14.0}, {25.5, 20.0});
    addWall("barrier-l2-top-right-door-upper", TwoFloorFacilityIds::Floor2Id, {28.0, 17.0}, {28.0, 20.0});
    addWall("barrier-l2-top-right-door-lower", TwoFloorFacilityIds::Floor2Id, {28.0, 14.0}, {28.0, 15.2});
    addWall("barrier-l2-top-main-a", TwoFloorFacilityIds::Floor2Id, {4.5, 14.0}, {7.1, 14.0});
    addWall("barrier-l2-top-main-b", TwoFloorFacilityIds::Floor2Id, {8.9, 14.0}, {14.1, 14.0});
    addWall("barrier-l2-top-main-c", TwoFloorFacilityIds::Floor2Id, {15.9, 14.0}, {21.1, 14.0});
    addWall("barrier-l2-top-main-d", TwoFloorFacilityIds::Floor2Id, {22.9, 14.0}, {25.5, 14.0});
    addDoor(
        "conn-l1-top-room-2-side-door",
        TwoFloorFacilityIds::Floor1Id,
        ConnectionKind::Doorway,
        TwoFloorFacilityIds::HallZoneL1Id,
        std::string(TwoFloorFacilityIds::TopRoomL1Prefix) + "2",
        {.x = 28.0, .y = 15.2},
        {.x = 28.0, .y = 17.0});

    // 2F bottom walls
    addWall("barrier-l2-bottom-left-side", TwoFloorFacilityIds::Floor2Id, {4.5, 0.0}, {4.5, 7.0});
    addWall("barrier-l2-bottom-divider-1", TwoFloorFacilityIds::Floor2Id, {roomDivider1, 0.0}, {roomDivider1, 7.0});
    addWall("barrier-l2-bottom-divider-2", TwoFloorFacilityIds::Floor2Id, {roomDivider2, 0.0}, {roomDivider2, 7.0});
    addWall("barrier-l2-bottom-right-side", TwoFloorFacilityIds::Floor2Id, {25.5, 0.0}, {25.5, 7.0});
    addWall("barrier-l2-bottom-top-a", TwoFloorFacilityIds::Floor2Id, {4.5, 7.0}, {7.1, 7.0});
    addWall("barrier-l2-bottom-top-b", TwoFloorFacilityIds::Floor2Id, {8.9, 7.0}, {14.1, 7.0});
    addWall("barrier-l2-bottom-top-c", TwoFloorFacilityIds::Floor2Id, {15.9, 7.0}, {21.1, 7.0});
    addWall("barrier-l2-bottom-top-d", TwoFloorFacilityIds::Floor2Id, {22.9, 7.0}, {25.5, 7.0});

    // Stair links between floors (left and right).
    addStairLink(
        TwoFloorFacilityIds::LeftStairLinkId,
        TwoFloorFacilityIds::Floor1Id,
        TwoFloorFacilityIds::LeftStairZoneL1Id,
        TwoFloorFacilityIds::LeftStairZoneL2Id,
        {.x = 1.0, .y = 6.0 - 0.9},
        {.x = 1.0, .y = 6.0 + 0.9});
    addStairLink(
        TwoFloorFacilityIds::RightStairLinkId,
        TwoFloorFacilityIds::Floor1Id,
        TwoFloorFacilityIds::RightStairZoneL1Id,
        TwoFloorFacilityIds::RightStairZoneL2Id,
        {.x = 29.0, .y = 6.0 - 0.9},
        {.x = 29.0, .y = 6.0 + 0.9});

    layout.barriers.push_back(makeFloorOutlineBarrier(TwoFloorFacilityIds::OuterWallL1Id, TwoFloorFacilityIds::Floor1Id, kWidth, kHeight));
    layout.barriers.push_back(makeFloorOutlineBarrier(TwoFloorFacilityIds::OuterWallL2Id, TwoFloorFacilityIds::Floor2Id, kWidth, kHeight));

    return layout;
}

}  // namespace safecrowd::domain::DemoLayouts
