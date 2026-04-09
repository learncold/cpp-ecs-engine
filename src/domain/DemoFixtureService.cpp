#include "domain/DemoFixtureService.h"

namespace safecrowd::domain {

DemoFixture DemoFixtureService::createSprint1DemoFixture() const {
    DemoFixture fixture;
    auto& layout = fixture.layout;
    layout.id = "demo-fixture-01";
    layout.name = "Sprint 1 Demo Layout";
    layout.levelId = "L1";

    Zone2D mainRoom;
    mainRoom.id = "zone-room-1";
    mainRoom.kind = ZoneKind::Room;
    mainRoom.label = "Main Demo Room";
    mainRoom.area = Polygon2D{
        .outline = {
            {0.0, 0.0},
            {20.0, 0.0},
            {20.0, 20.0},
            {0.0, 20.0},
        },
    };
    mainRoom.defaultCapacity = 200;
    layout.zones.push_back(mainRoom);

    Zone2D exitZone;
    exitZone.id = "zone-exit-1";
    exitZone.kind = ZoneKind::Exit;
    exitZone.label = "Main Exit";
    exitZone.area = Polygon2D{
        .outline = {
            {18.0, 20.0},
            {20.0, 20.0},
            {20.0, 22.0},
            {18.0, 22.0},
        },
    };
    exitZone.defaultCapacity = 20;
    layout.zones.push_back(exitZone);

    Connection2D exitConnection;
    exitConnection.id = "conn-exit-1";
    exitConnection.kind = ConnectionKind::Exit;
    exitConnection.fromZoneId = mainRoom.id;
    exitConnection.toZoneId = exitZone.id;
    exitConnection.effectiveWidth = 2.0;
    exitConnection.centerSpan = LineSegment2D{{18.0, 20.0}, {20.0, 20.0}};
    layout.connections.push_back(exitConnection);

    Barrier2D centerObstacle;
    centerObstacle.id = "barrier-1";
    centerObstacle.blocksMovement = true;
    centerObstacle.geometry = Polyline2D{
        .vertices = {
            {8.0, 10.0},
            {12.0, 10.0},
            {12.0, 11.0},
            {8.0, 11.0},
        },
        .closed = true,
    };
    layout.barriers.push_back(centerObstacle);

    fixture.population.initialPlacements.push_back({
        .id = "placement-1",
        .zoneId = mainRoom.id,
        .area = {
            .outline = {
                {1.0, 1.0},
                {5.0, 1.0},
                {5.0, 5.0},
                {1.0, 5.0},
            },
        },
        .targetAgentCount = 100,
    });

    return fixture;
}

}  // namespace safecrowd::domain
