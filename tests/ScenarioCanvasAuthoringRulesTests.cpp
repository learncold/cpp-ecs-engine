#include "TestSupport.h"
#include "application/ScenarioCanvasAuthoringRules.h"

using namespace safecrowd::application;
using namespace safecrowd::domain;

namespace {

FacilityLayout2D makeRoomLayout() {
    FacilityLayout2D layout;
    layout.floors.push_back(Floor2D{
        .id = "L1",
        .label = "Level 1",
    });
    layout.zones.push_back(Zone2D{
        .id = "room-a",
        .floorId = "L1",
        .kind = ZoneKind::Room,
        .label = "Room A",
        .area = Polygon2D{
            .outline = {
                {.x = 0.0, .y = 0.0},
                {.x = 10.0, .y = 0.0},
                {.x = 10.0, .y = 10.0},
                {.x = 0.0, .y = 10.0},
            },
        },
    });
    layout.zones.push_back(Zone2D{
        .id = "exit-a",
        .floorId = "L1",
        .kind = ZoneKind::Exit,
        .label = "Exit A",
        .area = Polygon2D{
            .outline = {
                {.x = 12.0, .y = 4.0},
                {.x = 14.0, .y = 4.0},
                {.x = 14.0, .y = 6.0},
                {.x = 12.0, .y = 6.0},
            },
        },
    });
    return layout;
}

std::vector<Point2D> groupArea() {
    return {
        {.x = 1.0, .y = 1.0},
        {.x = 9.0, .y = 1.0},
        {.x = 9.0, .y = 9.0},
        {.x = 1.0, .y = 9.0},
    };
}

}  // namespace

SC_TEST(ScenarioCanvasAuthoringRules_regeneratesGroupPositionsWhenCountChanges) {
    const auto layout = makeRoomLayout();
    const auto created = createScenarioGroupPlacement(
        layout,
        "L1",
        {},
        groupArea(),
        4,
        InitialPlacementDistribution::Uniform);
    SC_EXPECT_TRUE(created.placement.has_value());
    SC_EXPECT_EQ(created.placement->generatedPositions.size(), std::size_t{4});

    const std::vector<ScenarioCrowdPlacement> placements{*created.placement};
    const auto regenerated = regenerateScenarioGroupPlacement(
        layout,
        placements,
        *created.placement,
        9,
        InitialPlacementDistribution::Random);

    SC_EXPECT_TRUE(regenerated.placement.has_value());
    SC_EXPECT_EQ(regenerated.placement->id, created.placement->id);
    SC_EXPECT_EQ(regenerated.placement->occupantCount, 9);
    SC_EXPECT_TRUE(regenerated.placement->distribution == InitialPlacementDistribution::Random);
    SC_EXPECT_EQ(regenerated.placement->generatedPositions.size(), std::size_t{9});
}

SC_TEST(ScenarioCanvasAuthoringRules_regenerationIgnoresOriginalGroupForCollisionChecks) {
    const auto layout = makeRoomLayout();
    const auto created = createScenarioGroupPlacement(
        layout,
        "L1",
        {},
        groupArea(),
        4,
        InitialPlacementDistribution::Uniform);
    SC_EXPECT_TRUE(created.placement.has_value());

    const std::vector<ScenarioCrowdPlacement> placements{*created.placement};
    const auto regenerated = regenerateScenarioGroupPlacement(
        layout,
        placements,
        *created.placement,
        4,
        InitialPlacementDistribution::Uniform);

    SC_EXPECT_TRUE(regenerated.placement.has_value());
    SC_EXPECT_EQ(regenerated.placement->generatedPositions.size(), std::size_t{4});
}

/*
Unused code: smoke hazard is currently not used.

SC_TEST(ScenarioCanvasAuthoringRules_createsSmokeHazardFromCanvasToolRules) {
    const auto layout = makeRoomLayout();
    const auto result = createScenarioEnvironmentHazard(
        layout,
        "L1",
        {},
        Point2D{.x = 5.0, .y = 5.0},
        EnvironmentHazardKind::Smoke);

    SC_EXPECT_TRUE(result.hazard.has_value());
    SC_EXPECT_TRUE(result.hazard->kind == EnvironmentHazardKind::Smoke);
    SC_EXPECT_EQ(result.hazard->affectedZoneId, std::string{"room-a"});
    SC_EXPECT_EQ(result.hazard->floorId, std::string{"L1"});
}
*/
