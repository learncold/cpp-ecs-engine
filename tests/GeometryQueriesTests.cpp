#include "TestSupport.h"

#include "domain/GeometryQueries.h"

#include <cstddef>

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

SC_TEST(GeometryQueries_MatchesFloorUsesEmptyFloorAsWildcard) {
    SC_EXPECT_TRUE(safecrowd::domain::matchesFloor("", ""));
    SC_EXPECT_TRUE(safecrowd::domain::matchesFloor("", "L1"));
    SC_EXPECT_TRUE(safecrowd::domain::matchesFloor("L1", ""));
    SC_EXPECT_TRUE(safecrowd::domain::matchesFloor("L1", "L1"));
    SC_EXPECT_TRUE(!safecrowd::domain::matchesFloor("L1", "L2"));
}

SC_TEST(GeometryQueries_DefaultFloorIdUsesFloorThenLevelThenFallback) {
    safecrowd::domain::FacilityLayout2D layout;
    SC_EXPECT_EQ(safecrowd::domain::defaultFloorId(layout), "");
    SC_EXPECT_EQ(safecrowd::domain::defaultFloorId(layout, "L1"), "L1");

    layout.levelId = "LevelA";
    SC_EXPECT_EQ(safecrowd::domain::defaultFloorId(layout, "L1"), "LevelA");

    layout.floors.push_back({.id = "FloorA", .label = "Floor A"});
    SC_EXPECT_EQ(safecrowd::domain::defaultFloorId(layout, "L1"), "FloorA");
}

SC_TEST(GeometryQueries_PolygonCenterAndSegmentHelpers) {
    const auto center = safecrowd::domain::polygonCenter(rectangle(0.0, 0.0, 4.0, 2.0));
    SC_EXPECT_NEAR(center.x, 2.0, 1e-9);
    SC_EXPECT_NEAR(center.y, 1.0, 1e-9);

    const safecrowd::domain::Point2D point{.x = 2.0, .y = 3.0};
    const safecrowd::domain::Point2D start{.x = 0.0, .y = 0.0};
    const safecrowd::domain::Point2D end{.x = 4.0, .y = 0.0};
    const auto closest = safecrowd::domain::closestPointOnSegment(point, start, end);
    SC_EXPECT_NEAR(closest.x, 2.0, 1e-9);
    SC_EXPECT_NEAR(closest.y, 0.0, 1e-9);
    SC_EXPECT_NEAR(safecrowd::domain::distancePointToSegment(point, start, end), 3.0, 1e-9);

    const safecrowd::domain::LineSegment2D segment{.start = start, .end = end};
    SC_EXPECT_NEAR(safecrowd::domain::distancePointToSegment(point, segment), 3.0, 1e-9);
    SC_EXPECT_TRUE(safecrowd::domain::lineSegmentsIntersect(
        {.x = 0.0, .y = 0.0},
        {.x = 4.0, .y = 4.0},
        {.x = 0.0, .y = 4.0},
        {.x = 4.0, .y = 0.0}));
    SC_EXPECT_TRUE(!safecrowd::domain::lineSegmentsIntersect(
        {.x = 0.0, .y = 0.0},
        {.x = 4.0, .y = 0.0},
        {.x = 0.0, .y = 1.0},
        {.x = 4.0, .y = 1.0}));
}

SC_TEST(GeometryQueries_SpatialCellHelpers) {
    const auto cell = safecrowd::domain::spatialCellFor({.x = 2.4, .y = -0.1}, 1.0);
    SC_EXPECT_EQ(cell.x, 2);
    SC_EXPECT_EQ(cell.y, -1);

    const auto min = safecrowd::domain::spatialCellMin(cell, 1.5);
    const auto max = safecrowd::domain::spatialCellMax(cell, 1.5);
    SC_EXPECT_NEAR(min.x, 3.0, 1e-9);
    SC_EXPECT_NEAR(min.y, -1.5, 1e-9);
    SC_EXPECT_NEAR(max.x, 4.5, 1e-9);
    SC_EXPECT_NEAR(max.y, 0.0, 1e-9);

    const auto sameCell = safecrowd::domain::SpatialCell{.x = 2, .y = -1};
    const auto otherCell = safecrowd::domain::SpatialCell{.x = 2, .y = 0};
    SC_EXPECT_EQ(safecrowd::domain::spatialKey(cell), safecrowd::domain::spatialKey(sameCell));
    SC_EXPECT_TRUE(safecrowd::domain::spatialKey(cell) != safecrowd::domain::spatialKey(otherCell));

    const auto cells = safecrowd::domain::spatialCellsForBounds(
        {.x = 0.1, .y = 0.1},
        {.x = 2.1, .y = 1.1},
        1.0);
    SC_EXPECT_EQ(cells.size(), static_cast<std::size_t>(6));
    SC_EXPECT_EQ(cells.front().x, 0);
    SC_EXPECT_EQ(cells.front().y, 0);
    SC_EXPECT_EQ(cells.back().x, 2);
    SC_EXPECT_EQ(cells.back().y, 1);
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
    layout.zones.front().area.holes.push_back({
        {.x = 7.0, .y = 7.0},
        {.x = 8.0, .y = 7.0},
        {.x = 8.0, .y = 8.0},
        {.x = 7.0, .y = 8.0},
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
        layout, {.x = 0.1, .y = 2.0}, "L1", 0.35));
    SC_EXPECT_TRUE(!safecrowd::domain::pointInsideWalkableZoneWithClearance(
        layout, {.x = 6.8, .y = 7.5}, "L1", 0.35));
    SC_EXPECT_TRUE(!safecrowd::domain::pointInsideWalkableZoneWithClearance(
        layout, {.x = 11.0, .y = 2.0}, "L1", 0.35));
}

SC_TEST(GeometryQueries_SegmentCrossesMovementBarrierRespectsFloorAndBlockingFlag) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.barriers.push_back({
        .id = "wall-1",
        .floorId = "L1",
        .geometry = {.vertices = {{5.0, 0.0}, {5.0, 10.0}}},
        .blocksMovement = true,
    });
    layout.barriers.push_back({
        .id = "decorative-line",
        .floorId = "L1",
        .geometry = {.vertices = {{8.0, 0.0}, {8.0, 10.0}}},
        .blocksMovement = false,
    });

    SC_EXPECT_TRUE(safecrowd::domain::segmentCrossesMovementBarrier(
        layout, {.x = 4.0, .y = 5.0}, {.x = 6.0, .y = 5.0}, "L1"));
    SC_EXPECT_TRUE(!safecrowd::domain::segmentCrossesMovementBarrier(
        layout, {.x = 4.0, .y = 5.0}, {.x = 4.5, .y = 5.0}, "L1"));
    SC_EXPECT_TRUE(!safecrowd::domain::segmentCrossesMovementBarrier(
        layout, {.x = 7.0, .y = 5.0}, {.x = 9.0, .y = 5.0}, "L1"));
    SC_EXPECT_TRUE(!safecrowd::domain::segmentCrossesMovementBarrier(
        layout, {.x = 4.0, .y = 5.0}, {.x = 6.0, .y = 5.0}, "L2"));
}
