#include "TestSupport.h"

#include "domain/GeometryQueries.h"

namespace {

safecrowd::domain::Polygon2D rectangle(double minX, double minY, double maxX, double maxY) {
    return {
        .outline = {
            {.x = minX, .y = minY},
            {.x = maxX, .y = minY},
            {.x = maxX, .y = maxY},
            {.x = minX, .y = maxY},
        },
    };
}

}  // namespace

SC_TEST(GeometryQueries_PointInPolygonHandlesRectangleAndHole) {
    auto polygon = rectangle(0.0, 0.0, 10.0, 10.0);
    polygon.holes.push_back({
        {.x = 4.0, .y = 4.0},
        {.x = 6.0, .y = 4.0},
        {.x = 6.0, .y = 6.0},
        {.x = 4.0, .y = 6.0},
    });

    SC_EXPECT_TRUE(safecrowd::domain::pointInPolygon(polygon, {.x = 2.0, .y = 2.0}));
    SC_EXPECT_TRUE(!safecrowd::domain::pointInPolygon(polygon, {.x = 11.0, .y = 2.0}));
    SC_EXPECT_TRUE(!safecrowd::domain::pointInPolygon(polygon, {.x = 5.0, .y = 5.0}));
}

SC_TEST(GeometryQueries_DistanceToPolygonBoundaryIncludesHoles) {
    auto polygon = rectangle(0.0, 0.0, 10.0, 10.0);
    polygon.holes.push_back({
        {.x = 4.0, .y = 4.0},
        {.x = 6.0, .y = 4.0},
        {.x = 6.0, .y = 6.0},
        {.x = 4.0, .y = 6.0},
    });

    const auto distance = safecrowd::domain::distanceToPolygonBoundary(polygon, {.x = 5.0, .y = 3.75});

    SC_EXPECT_NEAR(distance, 0.25, 1e-9);
}

SC_TEST(GeometryQueries_RepresentativePointHandlesConcavePolygon) {
    const safecrowd::domain::Polygon2D polygon{
        .outline = {
            {.x = 0.0, .y = 0.0},
            {.x = 4.0, .y = 0.0},
            {.x = 4.0, .y = 1.0},
            {.x = 1.0, .y = 1.0},
            {.x = 1.0, .y = 4.0},
            {.x = 0.0, .y = 4.0},
        },
    };

    const auto point = safecrowd::domain::representativePointInPolygon(polygon);

    SC_EXPECT_TRUE(point.has_value());
    SC_EXPECT_TRUE(safecrowd::domain::pointInPolygon(polygon, *point));
}

SC_TEST(GeometryQueries_RepresentativePointAvoidsHole) {
    auto polygon = rectangle(0.0, 0.0, 10.0, 10.0);
    polygon.holes.push_back({
        {.x = 3.0, .y = 3.0},
        {.x = 7.0, .y = 3.0},
        {.x = 7.0, .y = 7.0},
        {.x = 3.0, .y = 7.0},
    });

    const auto point = safecrowd::domain::representativePointInPolygon(polygon);

    SC_EXPECT_TRUE(point.has_value());
    SC_EXPECT_TRUE(safecrowd::domain::pointInPolygon(polygon, *point));
    SC_EXPECT_TRUE(!(point->x > 3.0 && point->x < 7.0 && point->y > 3.0 && point->y < 7.0));
}

SC_TEST(GeometryQueries_RepresentativePointHandlesThinDiagonalPolygon) {
    const safecrowd::domain::Polygon2D polygon{
        .outline = {
            {.x = 0.0, .y = 0.0},
            {.x = 10.0, .y = 10.0},
            {.x = 10.2, .y = 10.0},
            {.x = 0.2, .y = 0.0},
        },
    };

    const auto point = safecrowd::domain::representativePointInPolygon(polygon);

    SC_EXPECT_TRUE(point.has_value());
    SC_EXPECT_TRUE(safecrowd::domain::pointInPolygon(polygon, *point));
}

SC_TEST(GeometryQueries_PointInsideWalkableZoneWithClearanceRejectsWallsAndOutsideSpace) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .area = rectangle(0.0, 0.0, 10.0, 10.0),
    });
    layout.barriers.push_back({
        .id = "wall-1",
        .floorId = "L1",
        .geometry = {.vertices = {{5.0, 0.0}, {5.0, 10.0}}},
        .blocksMovement = true,
    });

    SC_EXPECT_TRUE(safecrowd::domain::pointInsideWalkableZoneWithClearance(
        layout, {.x = 2.0, .y = 2.0}, "L1", 0.35));
    SC_EXPECT_TRUE(!safecrowd::domain::pointInsideWalkableZoneWithClearance(
        layout, {.x = 5.1, .y = 5.0}, "L1", 0.35));
    SC_EXPECT_TRUE(!safecrowd::domain::pointInsideWalkableZoneWithClearance(
        layout, {.x = 11.0, .y = 2.0}, "L1", 0.35));
}
