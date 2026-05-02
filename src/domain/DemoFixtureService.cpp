#include "domain/DemoFixtureService.h"

#include "domain/DemoLayouts.h"

namespace safecrowd::domain {

DemoFixture DemoFixtureService::createSprint1DemoFixture() const {
    DemoFixture fixture;
    fixture.layout = DemoLayouts::demoFacility();

    fixture.population.initialPlacements.push_back({
        .id = "placement-1",
        .zoneId = DemoLayouts::Sprint1FacilityIds::MainRoomZoneId,
        .area = {
            .outline = {
                {1.0, 1.0},
                {4.0, 1.0},
                {4.0, 4.0},
                {1.0, 4.0},
            },
        },
        .targetAgentCount = 100,
    });

    return fixture;
}

DemoFixture DemoFixtureService::create2FDemoFixture() const {
    DemoFixture fixture;
    fixture.layout = DemoLayouts::demoTwoFloorFacility();

    fixture.population.initialPlacements.push_back({
        .id = "placement-1",
        .zoneId = DemoLayouts::TwoFloorFacilityIds::HallZoneL2Id,
        .area = {
            .outline = {
                {6.0, 10.0},
                {10.0, 10.0},
                {10.0, 14.0},
                {6.0, 14.0},
            },
        },
        .targetAgentCount = 80,
    });

    return fixture;
}

}  // namespace safecrowd::domain
