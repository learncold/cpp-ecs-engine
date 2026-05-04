#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include "domain/DemoLayouts.h"
#include "domain/ScenarioSimulationInternal.h"
#include "domain/ScenarioSimulationRunner.h"
#include "domain/ScenarioSimulationSystems.h"

namespace {

bool testPointInRing(
    const std::vector<safecrowd::domain::Point2D>& ring,
    const safecrowd::domain::Point2D& point) {
    if (ring.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const auto& a = ring[i];
        const auto& b = ring[j];
        const auto intersects = ((a.y > point.y) != (b.y > point.y))
            && (point.x < ((b.x - a.x) * (point.y - a.y) / ((b.y - a.y) == 0.0 ? 1e-9 : (b.y - a.y)) + a.x));
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool agentInsideAnyZoneOnFrameFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::SimulationAgentFrame& agent) {
    return std::any_of(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return (zone.floorId.empty() || zone.floorId == agent.floorId)
            && testPointInRing(zone.area.outline, agent.position);
    });
}

double testDistanceBetween(const safecrowd::domain::Point2D& lhs, const safecrowd::domain::Point2D& rhs) {
    const auto dx = lhs.x - rhs.x;
    const auto dy = lhs.y - rhs.y;
    return std::sqrt((dx * dx) + (dy * dy));
}

safecrowd::domain::Point2D testClosestPointOnSegment(
    const safecrowd::domain::Point2D& point,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end) {
    const auto dx = end.x - start.x;
    const auto dy = end.y - start.y;
    const auto lengthSquared = (dx * dx) + (dy * dy);
    if (lengthSquared <= 1e-9) {
        return start;
    }

    const auto t = std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared, 0.0, 1.0);
    return {.x = start.x + (dx * t), .y = start.y + (dy * t)};
}

double testDistancePointToSegment(
    const safecrowd::domain::Point2D& point,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end) {
    return testDistanceBetween(point, testClosestPointOnSegment(point, start, end));
}

bool agentKeepsBarrierClearance(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::SimulationAgentFrame& agent,
    double clearance) {
    for (const auto& barrier : layout.barriers) {
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }
        if (!barrier.floorId.empty() && barrier.floorId != agent.floorId) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
            if (testDistancePointToSegment(agent.position, vertices[index], vertices[index + 1]) + 1e-6 < clearance) {
                return false;
            }
        }
        if (barrier.geometry.closed
            && testDistancePointToSegment(agent.position, vertices.back(), vertices.front()) + 1e-6 < clearance) {
            return false;
        }
    }
    return true;
}

safecrowd::domain::FacilityLayout2D blockedDoorLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "left",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Left",
        .area = {.outline = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {1.0, 1.0}}},
    });
    layout.connections.push_back({
        .id = "conn-left-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "left",
        .toZoneId = "exit",
        .effectiveWidth = 1.0,
        .centerSpan = {{1.0, 0.4}, {1.0, 0.6}},
    });
    layout.barriers.push_back({
        .id = "blocking-wall",
        .geometry = {.vertices = {{1.0, 0.0}, {1.0, 1.0}}},
        .blocksMovement = true,
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D wideDoorLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{4.0, 0.0}, {8.0, 0.0}, {8.0, 4.0}, {4.0, 4.0}}},
    });
    layout.connections.push_back({
        .id = "wide-door",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "exit",
        .effectiveWidth = 3.0,
        .centerSpan = {{4.0, 0.5}, {4.0, 3.5}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D wideDoorToPassageLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "passage",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Passage",
        .area = {.outline = {{4.0, 0.0}, {8.0, 0.0}, {8.0, 4.0}, {4.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{8.0, 1.0}, {10.0, 1.0}, {10.0, 3.0}, {8.0, 3.0}}},
    });
    layout.connections.push_back({
        .id = "wide-door",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "room",
        .toZoneId = "passage",
        .effectiveWidth = 3.0,
        .centerSpan = {{4.0, 0.5}, {4.0, 3.5}},
    });
    layout.connections.push_back({
        .id = "exit-door",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "passage",
        .toZoneId = "exit",
        .effectiveWidth = 2.0,
        .centerSpan = {{8.0, 1.0}, {8.0, 3.0}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D narrowDoorCrowdLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "passage",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Passage",
        .area = {.outline = {{4.0, 0.0}, {7.0, 0.0}, {7.0, 4.0}, {4.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{7.0, 1.0}, {9.0, 1.0}, {9.0, 3.0}, {7.0, 3.0}}},
    });
    layout.connections.push_back({
        .id = "narrow-door",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "room",
        .toZoneId = "passage",
        .effectiveWidth = 0.9,
        .centerSpan = {{4.0, 1.55}, {4.0, 2.45}},
    });
    layout.connections.push_back({
        .id = "exit-door",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "passage",
        .toZoneId = "exit",
        .effectiveWidth = 2.0,
        .centerSpan = {{7.0, 1.0}, {7.0, 3.0}},
    });
    layout.barriers.push_back({
        .id = "lower-door-jamb",
        .geometry = {.vertices = {{4.0, 0.0}, {4.0, 1.55}}},
        .blocksMovement = true,
    });
    layout.barriers.push_back({
        .id = "upper-door-jamb",
        .geometry = {.vertices = {{4.0, 2.45}, {4.0, 4.0}}},
        .blocksMovement = true,
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D twoFloorStairExitLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "two-floor";
    layout.levelId = "L1";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "room-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L1",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "exit-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L2",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "stair-l1-l2",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "room-l1",
        .toZoneId = "exit-l2",
        .effectiveWidth = 1.2,
        .isStair = true,
        .centerSpan = {{2.0, 0.6}, {2.0, 1.4}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D overlappingTwoFloorExitLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "overlapping-two-floor";
    layout.levelId = "L1";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "room-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L1",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "exit-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L1",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "room-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L2",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "exit-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L2",
        .area = {.outline = {{-2.0, 0.0}, {0.0, 0.0}, {0.0, 2.0}, {-2.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "door-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room-l1",
        .toZoneId = "exit-l1",
        .effectiveWidth = 1.2,
        .centerSpan = {{2.0, 0.4}, {2.0, 1.6}},
    });
    layout.connections.push_back({
        .id = "door-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room-l2",
        .toZoneId = "exit-l2",
        .effectiveWidth = 1.2,
        .centerSpan = {{0.0, 0.4}, {0.0, 1.6}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D directedStairExitLayout(
    safecrowd::domain::StairEntryDirection lowerEntryDirection,
    safecrowd::domain::StairEntryDirection upperEntryDirection) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "directed-stair";
    layout.levelId = "L1";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "room-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L1",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "stair-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Stair L1",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "stair-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Stair L2",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "exit-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L2",
        .area = {.outline = {{4.0, 0.0}, {6.0, 0.0}, {6.0, 2.0}, {4.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "room-to-stair",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Opening,
        .fromZoneId = "room-l1",
        .toZoneId = "stair-l1",
        .effectiveWidth = 1.2,
        .centerSpan = {{2.0, 0.4}, {2.0, 1.6}},
    });
    layout.connections.push_back({
        .id = "stair-l1-l2",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "stair-l1",
        .toZoneId = "stair-l2",
        .effectiveWidth = 1.2,
        .isStair = true,
        .lowerEntryDirection = lowerEntryDirection,
        .upperEntryDirection = upperEntryDirection,
        .centerSpan = lowerEntryDirection == safecrowd::domain::StairEntryDirection::North
                || lowerEntryDirection == safecrowd::domain::StairEntryDirection::South
            ? safecrowd::domain::LineSegment2D{{2.8, 1.0}, {3.2, 1.0}}
            : safecrowd::domain::LineSegment2D{{3.0, 0.4}, {3.0, 1.6}},
    });
    layout.connections.push_back({
        .id = "stair-to-exit",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "stair-l2",
        .toZoneId = "exit-l2",
        .effectiveWidth = 1.2,
        .centerSpan = {{4.0, 0.4}, {4.0, 1.6}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D blockedUShapedStairTransitionLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "blocked-u-stair";
    layout.levelId = "L1";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "stair-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "U Stair L1",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {2.0, 4.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "stair-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "U Stair L2",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 4.0}, {0.0, 4.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "exit-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L2",
        .area = {.outline = {{-2.0, 1.0}, {0.0, 1.0}, {0.0, 3.0}, {-2.0, 3.0}}},
    });
    layout.connections.push_back({
        .id = "u-stair-l1-l2",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "stair-l1",
        .toZoneId = "stair-l2",
        .effectiveWidth = 1.2,
        .isStair = true,
        .lowerEntryDirection = safecrowd::domain::StairEntryDirection::West,
        .upperEntryDirection = safecrowd::domain::StairEntryDirection::West,
        .centerSpan = {{2.0, 2.8}, {2.0, 4.0}},
    });
    layout.connections.push_back({
        .id = "stair-to-exit",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "stair-l2",
        .toZoneId = "exit-l2",
        .effectiveWidth = 1.2,
        .centerSpan = {{0.0, 1.2}, {0.0, 2.8}},
    });
    layout.barriers.push_back({
        .id = "blocked-platform-turn",
        .floorId = "L1",
        .geometry = {.vertices = {{2.0, 0.0}, {2.0, 4.0}}},
        .blocksMovement = true,
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D uShapedStairTransitionLayout() {
    auto layout = blockedUShapedStairTransitionLayout();
    layout.id = "u-stair";
    layout.barriers.clear();
    return layout;
}

safecrowd::domain::FacilityLayout2D westEntryUShapedStairTransitionLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "west-entry-u-stair";
    layout.levelId = "L1";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "stair-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "U Stair L1",
        .area = {.outline = {{0.0, 2.0}, {0.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "stair-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "U Stair L2",
        .area = {.outline = {{0.0, 4.0}, {0.0, 2.0}, {4.0, 2.0}, {4.0, 4.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "exit-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L2",
        .area = {.outline = {{-2.0, 2.0}, {0.0, 2.0}, {0.0, 4.0}, {-2.0, 4.0}}},
    });
    layout.connections.push_back({
        .id = "u-stair-l1-l2",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "stair-l1",
        .toZoneId = "stair-l2",
        .effectiveWidth = 1.2,
        .isStair = true,
        .lowerEntryDirection = safecrowd::domain::StairEntryDirection::West,
        .upperEntryDirection = safecrowd::domain::StairEntryDirection::West,
        .centerSpan = {{2.8, 2.0}, {4.0, 2.0}},
    });
    layout.connections.push_back({
        .id = "stair-to-exit",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "stair-l2",
        .toZoneId = "exit-l2",
        .effectiveWidth = 1.2,
        .centerSpan = {{0.0, 2.4}, {0.0, 3.6}},
    });
    layout.barriers.push_back({
        .id = "source-south-wall",
        .floorId = "L1",
        .geometry = {.vertices = {{0.0, 0.0}, {4.0, 0.0}}},
        .blocksMovement = true,
    });
    layout.barriers.push_back({
        .id = "source-east-wall",
        .floorId = "L1",
        .geometry = {.vertices = {{4.0, 0.0}, {4.0, 2.0}}},
        .blocksMovement = true,
    });
    layout.barriers.push_back({
        .id = "source-north-wall-before-turn",
        .floorId = "L1",
        .geometry = {.vertices = {{0.0, 2.0}, {2.8, 2.0}}},
        .blocksMovement = true,
    });
    layout.barriers.push_back({
        .id = "target-south-wall-before-turn",
        .floorId = "L2",
        .geometry = {.vertices = {{0.0, 2.0}, {2.8, 2.0}}},
        .blocksMovement = true,
    });
    layout.barriers.push_back({
        .id = "target-east-wall",
        .floorId = "L2",
        .geometry = {.vertices = {{4.0, 2.0}, {4.0, 4.0}}},
        .blocksMovement = true,
    });
    layout.barriers.push_back({
        .id = "target-north-wall",
        .floorId = "L2",
        .geometry = {.vertices = {{4.0, 4.0}, {0.0, 4.0}}},
        .blocksMovement = true,
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D descendingWestEntryUShapedStairTransitionLayout() {
    auto layout = westEntryUShapedStairTransitionLayout();
    layout.id = "descending-west-entry-u-stair";
    layout.zones[2] = {
        .id = "exit-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L1",
        .area = {.outline = {{-2.0, 0.0}, {0.0, 0.0}, {0.0, 2.0}, {-2.0, 2.0}}},
    };
    layout.connections[1] = {
        .id = "stair-to-exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "stair-l1",
        .toZoneId = "exit-l1",
        .effectiveWidth = 1.2,
        .centerSpan = {{0.0, 0.4}, {0.0, 1.6}},
    };
    return layout;
}

safecrowd::domain::FacilityLayout2D competingStairExitLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "competing-stair-exit";
    layout.levelId = "L1";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "upper-room",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Upper Room",
        .area = {.outline = {{0.0, 0.0}, {20.0, 0.0}, {20.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "left-stair-upper",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Left Stair Upper",
        .area = {.outline = {{0.0, 4.0}, {2.0, 4.0}, {2.0, 6.0}, {0.0, 6.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "right-stair-upper",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Right Stair Upper",
        .area = {.outline = {{18.0, 4.0}, {20.0, 4.0}, {20.0, 6.0}, {18.0, 6.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "left-stair-lower",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Left Stair Lower",
        .area = {.outline = {{0.0, 4.0}, {2.0, 4.0}, {2.0, 6.0}, {0.0, 6.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "right-stair-lower",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Right Stair Lower",
        .area = {.outline = {{18.0, 4.0}, {20.0, 4.0}, {20.0, 6.0}, {18.0, 6.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "lower-corridor",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Lower Corridor",
        .area = {.outline = {{0.0, 0.0}, {20.0, 0.0}, {20.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "exit-left",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Left Exit",
        .area = {.outline = {{-2.0, 0.8}, {0.0, 0.8}, {0.0, 3.2}, {-2.0, 3.2}}},
    });
    layout.connections.push_back({
        .id = "upper-room-left-stair",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Opening,
        .fromZoneId = "upper-room",
        .toZoneId = "left-stair-upper",
        .effectiveWidth = 1.2,
        .centerSpan = {{1.0, 4.0}, {1.8, 4.0}},
    });
    layout.connections.push_back({
        .id = "upper-room-right-stair",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Opening,
        .fromZoneId = "upper-room",
        .toZoneId = "right-stair-upper",
        .effectiveWidth = 1.2,
        .centerSpan = {{18.2, 4.0}, {19.0, 4.0}},
    });
    layout.connections.push_back({
        .id = "left-stair-vertical",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "left-stair-upper",
        .toZoneId = "left-stair-lower",
        .effectiveWidth = 1.2,
        .isStair = true,
        .centerSpan = {{1.0, 4.8}, {1.8, 4.8}},
    });
    layout.connections.push_back({
        .id = "right-stair-vertical",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "right-stair-upper",
        .toZoneId = "right-stair-lower",
        .effectiveWidth = 1.2,
        .isStair = true,
        .centerSpan = {{18.2, 4.8}, {19.0, 4.8}},
    });
    layout.connections.push_back({
        .id = "left-stair-lower-corridor",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Opening,
        .fromZoneId = "left-stair-lower",
        .toZoneId = "lower-corridor",
        .effectiveWidth = 1.2,
        .centerSpan = {{1.0, 4.0}, {1.8, 4.0}},
    });
    layout.connections.push_back({
        .id = "right-stair-lower-corridor",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Opening,
        .fromZoneId = "right-stair-lower",
        .toZoneId = "lower-corridor",
        .effectiveWidth = 1.2,
        .centerSpan = {{18.2, 4.0}, {19.0, 4.0}},
    });
    layout.connections.push_back({
        .id = "corridor-exit-left",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "lower-corridor",
        .toZoneId = "exit-left",
        .effectiveWidth = 1.8,
        .centerSpan = {{0.0, 1.2}, {0.0, 2.8}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D adjacentExitPortalChoiceLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "adjacent-exit-portal-choice";
    layout.zones.push_back({
        .id = "lower-hall",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Lower Hall",
        .area = {.outline = {{0.0, 0.0}, {6.0, 0.0}, {6.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "near-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Near Exit",
        .area = {.outline = {{0.5, -4.0}, {1.5, -4.0}, {1.5, 0.0}, {0.5, 0.0}}},
    });
    layout.zones.push_back({
        .id = "adjacent-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Adjacent Exit",
        .area = {.outline = {{3.5, -0.2}, {4.5, -0.2}, {4.5, 0.0}, {3.5, 0.0}}},
    });
    layout.connections.push_back({
        .id = "hall-near-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "lower-hall",
        .toZoneId = "near-exit",
        .effectiveWidth = 1.0,
        .centerSpan = {{0.6, 0.0}, {1.4, 0.0}},
    });
    layout.connections.push_back({
        .id = "hall-adjacent-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "lower-hall",
        .toZoneId = "adjacent-exit",
        .effectiveWidth = 1.0,
        .centerSpan = {{3.6, 0.0}, {4.4, 0.0}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D verticalLandingExitChoiceLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "vertical-landing-exit-choice";
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "upper-room",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Upper Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 1.5}, {0.0, 1.5}}},
    });
    layout.zones.push_back({
        .id = "stair-upper",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Upper Stair",
        .area = {.outline = {{0.0, 1.5}, {4.0, 1.5}, {4.0, 4.5}, {0.0, 4.5}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "stair-lower",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Lower Stair",
        .area = {.outline = {{0.0, 1.5}, {4.0, 1.5}, {4.0, 4.5}, {0.0, 4.5}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "near-exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Near Exit",
        .area = {.outline = {{4.0, 2.2}, {5.0, 2.2}, {5.0, 2.7}, {4.0, 2.7}}},
    });
    layout.zones.push_back({
        .id = "far-exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Far Exit",
        .area = {.outline = {{4.0, 1.75}, {5.0, 1.75}, {5.0, 2.05}, {4.0, 2.05}}},
    });
    layout.connections.push_back({
        .id = "room-to-stair",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Opening,
        .fromZoneId = "upper-room",
        .toZoneId = "stair-upper",
        .effectiveWidth = 1.0,
        .centerSpan = {{1.8, 1.5}, {2.8, 1.5}},
    });
    layout.connections.push_back({
        .id = "stair-vertical",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "stair-upper",
        .toZoneId = "stair-lower",
        .effectiveWidth = 1.0,
        .isStair = true,
        .centerSpan = {{1.8, 2.0}, {2.8, 2.0}},
    });
    layout.connections.push_back({
        .id = "stair-to-near-exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "stair-lower",
        .toZoneId = "near-exit",
        .effectiveWidth = 0.5,
        .centerSpan = {{4.0, 2.25}, {4.0, 2.65}},
    });
    layout.connections.push_back({
        .id = "stair-to-far-exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "stair-lower",
        .toZoneId = "far-exit",
        .effectiveWidth = 0.3,
        .centerSpan = {{4.0, 1.82}, {4.0, 1.98}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D sharedExitMultiPortalLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = "shared-exit-multi-portal";
    layout.zones.push_back({
        .id = "lower-hall",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Lower Hall",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 8.0}, {0.0, 8.0}}},
    });
    layout.zones.push_back({
        .id = "shared-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Shared Exit",
        .area = {.outline = {{4.0, 0.0}, {8.0, 0.0}, {8.0, 8.0}, {4.0, 8.0}}},
    });
    layout.connections.push_back({
        .id = "hall-to-shared-exit-far",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "lower-hall",
        .toZoneId = "shared-exit",
        .effectiveWidth = 0.4,
        .centerSpan = {{4.0, 1.0}, {4.0, 1.4}},
    });
    layout.connections.push_back({
        .id = "hall-to-shared-exit-near",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "lower-hall",
        .toZoneId = "shared-exit",
        .effectiveWidth = 0.4,
        .centerSpan = {{4.0, 6.4}, {4.0, 6.8}},
    });
    return layout;
}

safecrowd::domain::InitialPlacement2D groupPlacement() {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "group-1";
    placement.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    placement.targetAgentCount = 4;
    placement.initialVelocity = {.x = 1.0, .y = 0.5};
    placement.area.outline = {
        {.x = 1.0, .y = 4.0},
        {.x = 3.0, .y = 4.0},
        {.x = 3.0, .y = 6.0},
        {.x = 1.0, .y = 6.0},
    };
    return placement;
}

}  // namespace

SC_TEST(ScenarioSimulationRunnerInitializesAndRoutesAgentsThroughLayoutConnections) {
    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(groupPlacement());

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(4));

    runner.step(2.0);

    SC_EXPECT_NEAR(runner.frame().elapsedSeconds, 2.0, 1e-9);
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x > 2.0);
    SC_EXPECT_TRUE(runner.frame().agents.front().position.y > 4.0);
    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, static_cast<std::size_t>(0));
    SC_EXPECT_TRUE(!runner.complete());
}

SC_TEST(ScenarioSimulationRunnerCompletesAtTimeLimit) {
    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 1.0;
    scenario.population.initialPlacements.push_back(groupPlacement());

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    runner.step(2.0);

    SC_EXPECT_NEAR(runner.frame().elapsedSeconds, 1.0, 1e-9);
    SC_EXPECT_TRUE(runner.complete());
}

SC_TEST(ScenarioSimulationRunnerMarksEvacuationSuccessAtExitZone) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::ExitPassageZoneId;
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 3.0, .y = 0.0};
    placement.area.outline = {{.x = 23.5, .y = 6.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    for (int i = 0; i < 20 && !runner.complete(); ++i) {
        runner.step(0.5);
    }

    SC_EXPECT_EQ(runner.frame().totalAgentCount, static_cast<std::size_t>(1));
    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, static_cast<std::size_t>(1));
    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(0));
    SC_EXPECT_TRUE(runner.complete());
}

SC_TEST(ScenarioSimulationRunnerSeparatesOverlappingAgents) {
    safecrowd::domain::InitialPlacement2D first;
    first.id = "agent-1";
    first.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    first.targetAgentCount = 1;
    first.initialVelocity = {.x = 1.0, .y = 0.0};
    first.area.outline = {{.x = 2.0, .y = 5.0}};

    safecrowd::domain::InitialPlacement2D second = first;
    second.id = "agent-2";

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(first);
    scenario.population.initialPlacements.push_back(second);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    runner.step(0.1);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(2));
    const auto dx = runner.frame().agents[0].position.x - runner.frame().agents[1].position.x;
    const auto dy = runner.frame().agents[0].position.y - runner.frame().agents[1].position.y;
    SC_EXPECT_TRUE(std::hypot(dx, dy) >= 0.49);
}

SC_TEST(ScenarioSimulationRunnerAvoidanceDoesNotReverseSharedRouteDirection) {
    safecrowd::domain::InitialPlacement2D first;
    first.id = "agent-1";
    first.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    first.targetAgentCount = 1;
    first.initialVelocity = {.x = 1.5, .y = 0.0};
    first.area.outline = {{.x = 2.0, .y = 5.0}};

    safecrowd::domain::InitialPlacement2D second = first;
    second.id = "agent-2";
    second.area.outline = {{.x = 2.18, .y = 5.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(first);
    scenario.population.initialPlacements.push_back(second);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    for (int i = 0; i < 6; ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(2));
    for (const auto& agent : runner.frame().agents) {
        SC_EXPECT_TRUE(agent.velocity.x >= -1e-6);
    }
}

SC_TEST(ScenarioSimulationRunnerAdvancesDoorWaypointAfterAgentEntersNextZone) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 12.4, .y = 5.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    runner.step(0.2);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x > 12.4);
    SC_EXPECT_TRUE(runner.frame().agents.front().velocity.x > 0.0);
}

SC_TEST(ScenarioSimulationRunnerUsesDoorSpanInsteadOfOnlyCenterPoint) {
    safecrowd::domain::InitialPlacement2D lower;
    lower.id = "agent-lower";
    lower.zoneId = "room";
    lower.targetAgentCount = 1;
    lower.initialVelocity = {.x = 2.0, .y = 0.0};
    lower.area.outline = {{.x = 1.0, .y = 1.0}};

    safecrowd::domain::InitialPlacement2D upper = lower;
    upper.id = "agent-upper";
    upper.area.outline = {{.x = 1.0, .y = 3.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 5.0;
    scenario.population.initialPlacements.push_back(lower);
    scenario.population.initialPlacements.push_back(upper);

    safecrowd::domain::ScenarioSimulationRunner runner(wideDoorLayout(), scenario);
    runner.step(0.25);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(2));
    for (const auto& agent : runner.frame().agents) {
        SC_EXPECT_TRUE(agent.velocity.x > 0.0);
        SC_EXPECT_TRUE(std::fabs(agent.velocity.y) < 0.05);
    }
}

SC_TEST(ScenarioSimulationRunnerAdvancesWideDoorPassageAfterCrossingNearEndpoint) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-near-endpoint";
    placement.zoneId = "room";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 2.0, .y = 0.0};
    placement.area.outline = {{.x = 4.08, .y = 3.25}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 5.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(wideDoorToPassageLayout(), scenario);
    runner.step(0.1);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    SC_EXPECT_TRUE(runner.frame().agents.front().velocity.x > 0.0);
}

SC_TEST(ScenarioSimulationRunnerSkipsPassedDoorwaysWhenAgentIsAlreadyInLaterZone) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-pushed-ahead";
    placement.zoneId = "room";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 2.0, .y = 0.0};
    placement.area.outline = {{.x = 4.4, .y = 3.25}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 5.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(wideDoorToPassageLayout(), scenario);
    runner.step(0.1);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    SC_EXPECT_TRUE(runner.frame().agents.front().velocity.x > 0.0);
}

SC_TEST(ScenarioSimulationRunnerKeepsCrowdedNarrowDoorAgentsOutOfWalls) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "crowded-door";
    placement.zoneId = "room";
    placement.initialVelocity = {.x = 1.4, .y = 0.0};
    placement.explicitPositions = {
        {.x = 3.35, .y = 1.35},
        {.x = 3.35, .y = 1.75},
        {.x = 3.35, .y = 2.15},
        {.x = 3.35, .y = 2.55},
        {.x = 2.85, .y = 1.25},
        {.x = 2.85, .y = 1.65},
        {.x = 2.85, .y = 2.05},
        {.x = 2.85, .y = 2.45},
        {.x = 2.35, .y = 1.45},
        {.x = 2.35, .y = 1.85},
        {.x = 2.35, .y = 2.25},
        {.x = 2.35, .y = 2.65},
    };

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 10.0;
    scenario.population.initialPlacements.push_back(placement);

    const auto layout = narrowDoorCrowdLayout();
    safecrowd::domain::ScenarioSimulationRunner runner(layout, scenario);
    bool anyAgentEnteredPassage = false;
    for (int i = 0; i < 70 && !runner.complete(); ++i) {
        runner.step(0.1);
        for (const auto& agent : runner.frame().agents) {
            anyAgentEnteredPassage = anyAgentEnteredPassage || agent.position.x > 4.0;
            SC_EXPECT_TRUE(agentInsideAnyZoneOnFrameFloor(layout, agent));
            SC_EXPECT_TRUE(agentKeepsBarrierClearance(layout, agent, agent.radius * 0.95));
        }
    }

    SC_EXPECT_TRUE(anyAgentEnteredPassage);
}

SC_TEST(ScenarioSimulationRunnerMovesWallContactAgentBackIntoInterior) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "wall-contact";
    placement.zoneId = "room";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.4, .y = 0.0};
    placement.area.outline = {{.x = 3.74, .y = 1.36}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 4.0;
    scenario.population.initialPlacements.push_back(placement);

    const auto layout = narrowDoorCrowdLayout();
    safecrowd::domain::ScenarioSimulationRunner runner(layout, scenario);
    runner.step(0.1);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    const auto firstFrameAgent = runner.frame().agents.front();
    SC_EXPECT_TRUE(firstFrameAgent.position.x < 3.74);
    SC_EXPECT_TRUE(agentKeepsBarrierClearance(layout, firstFrameAgent, firstFrameAgent.radius * 0.95));

    for (int i = 0; i < 12 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    const auto recoveredAgent = runner.frame().agents.front();
    SC_EXPECT_TRUE(!recoveredAgent.stalled);
    SC_EXPECT_TRUE(agentKeepsBarrierClearance(layout, recoveredAgent, recoveredAgent.radius * 0.95));
}

SC_TEST(ScenarioSimulationRunnerTraversesStairConnectionBetweenFloors) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "room-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 0.5, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 10.0;

    safecrowd::domain::ScenarioSimulationRunner runner(twoFloorStairExitLayout(), scenario);
    for (int i = 0; i < 80 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, std::size_t{1});
}

SC_TEST(ScenarioSimulationRunnerUsesPlacementFloorForOverlappingCoordinates) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.floorId = "L2";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 1.0, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 5.0;

    safecrowd::domain::ScenarioSimulationRunner runner(overlappingTwoFloorExitLayout(), scenario);
    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L2"});

    runner.step(0.25);

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L2"});
    SC_EXPECT_TRUE(runner.frame().agents.front().velocity.x < 0.0);
}

SC_TEST(ScenarioSimulationRunnerDoesNotSlowAgentsOnDifferentFloorsAtSameCoordinates) {
    safecrowd::domain::InitialPlacement2D lower;
    lower.id = "agent-l1";
    lower.floorId = "L1";
    lower.zoneId = "room-l1";
    lower.targetAgentCount = 1;
    lower.initialVelocity = {.x = 1.5, .y = 0.0};
    lower.area.outline = {{.x = 1.0, .y = 1.0}};

    safecrowd::domain::InitialPlacement2D upper;
    upper.id = "agent-l2";
    upper.floorId = "L2";
    upper.zoneId = "room-l2";
    upper.targetAgentCount = 1;
    upper.initialVelocity = {.x = 1.5, .y = 0.0};
    upper.area.outline = {{.x = 1.0, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(lower);
    scenario.population.initialPlacements.push_back(upper);
    scenario.execution.timeLimitSeconds = 5.0;

    safecrowd::domain::ScenarioSimulationRunner runner(overlappingTwoFloorExitLayout(), scenario);
    runner.step(0.1);

    const auto& agents = runner.frame().agents;
    SC_EXPECT_EQ(agents.size(), std::size_t{2});
    const auto& first = agents[0].floorId == "L1" ? agents[0] : agents[1];
    const auto& second = agents[0].floorId == "L2" ? agents[0] : agents[1];
    SC_EXPECT_EQ(first.floorId, std::string{"L1"});
    SC_EXPECT_EQ(second.floorId, std::string{"L2"});
    SC_EXPECT_TRUE(first.velocity.x > 1.0);
    SC_EXPECT_TRUE(second.velocity.x < -1.0);
}

SC_TEST(ScenarioRouteToNearestExitUsesStartPositionAndLowerFloorExitDistanceForStairChoice) {
    const auto layout = competingStairExitLayout();
    const auto cache = safecrowd::domain::simulation_internal::buildScenarioLayoutCache(layout);

    const auto lowerExitPreferred = safecrowd::domain::simulation_internal::zoneRouteToNearestExit(
        cache,
        {.x = 18.0, .y = 3.2},
        "upper-room");
    SC_EXPECT_TRUE(lowerExitPreferred.has_value());
    const auto lowerExitRoute = lowerExitPreferred.has_value()
        ? lowerExitPreferred->zoneIds
        : std::vector<std::string>{};
    SC_EXPECT_TRUE(lowerExitRoute.size() >= 2);
    SC_EXPECT_EQ(lowerExitRoute.size() >= 2 ? lowerExitRoute[1] : std::string{}, std::string{"left-stair-upper"});
    SC_EXPECT_EQ(lowerExitRoute.empty() ? std::string{} : lowerExitRoute.back(), std::string{"exit-left"});

    const auto startPositionPreferred = safecrowd::domain::simulation_internal::zoneRouteToNearestExit(
        cache,
        {.x = 19.7, .y = 3.2},
        "upper-room");
    SC_EXPECT_TRUE(startPositionPreferred.has_value());
    const auto startPositionRoute = startPositionPreferred.has_value()
        ? startPositionPreferred->zoneIds
        : std::vector<std::string>{};
    SC_EXPECT_TRUE(startPositionRoute.size() >= 2);
    SC_EXPECT_EQ(startPositionRoute.size() >= 2 ? startPositionRoute[1] : std::string{}, std::string{"right-stair-upper"});
    SC_EXPECT_EQ(startPositionRoute.empty() ? std::string{} : startPositionRoute.back(), std::string{"exit-left"});
}

SC_TEST(ScenarioRouteToNearestExitPrefersNearestExitPortalOverExitZoneCenter) {
    const auto layout = adjacentExitPortalChoiceLayout();
    const auto cache = safecrowd::domain::simulation_internal::buildScenarioLayoutCache(layout);

    const auto route = safecrowd::domain::simulation_internal::zoneRouteToNearestExit(
        cache,
        {.x = 1.0, .y = 2.0},
        "lower-hall");

    SC_EXPECT_TRUE(route.has_value());
    const auto zoneRoute = route.has_value() ? route->zoneIds : std::vector<std::string>{};
    SC_EXPECT_EQ(zoneRoute.empty() ? std::string{} : zoneRoute.back(), std::string{"near-exit"});
}

SC_TEST(ScenarioRouteToNearestExitPreservesSelectedPortalForSharedExitZone) {
    const auto layout = sharedExitMultiPortalLayout();
    const auto cache = safecrowd::domain::simulation_internal::buildScenarioLayoutCache(layout);

    const auto route = safecrowd::domain::simulation_internal::zoneRouteToNearestExit(
        cache,
        {.x = 2.0, .y = 6.6},
        "lower-hall");

    SC_EXPECT_TRUE(route.has_value());
    SC_EXPECT_EQ(route.has_value() ? route->zoneIds.back() : std::string{}, std::string{"shared-exit"});
    SC_EXPECT_EQ(route.has_value() ? route->connectionIndices.size() : std::size_t{0}, std::size_t{1});
    const auto connectionIndex = route.has_value() && !route->connectionIndices.empty()
        ? route->connectionIndices.front()
        : std::size_t{0};
    SC_EXPECT_TRUE(connectionIndex < layout.connections.size());
    SC_EXPECT_EQ(
        connectionIndex < layout.connections.size() ? layout.connections[connectionIndex].id : std::string{},
        std::string{"hall-to-shared-exit-near"});
}

SC_TEST(ScenarioSimulationRunnerUsesSelectedPortalWhenExitZoneHasMultipleConnections) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "lower-hall";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.0, .y = 0.0};
    placement.area.outline = {{.x = 2.0, .y = 6.6}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 4.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(sharedExitMultiPortalLayout(), scenario);
    runner.step(0.1);

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    const auto& agent = runner.frame().agents.front();
    SC_EXPECT_TRUE(agent.velocity.x > 1.0);
    SC_EXPECT_TRUE(std::fabs(agent.velocity.y) < 0.15);
}

SC_TEST(ScenarioSimulationRunnerReplansExitChoiceAfterVerticalLanding) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.floorId = "L2";
    placement.zoneId = "upper-room";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 0.0, .y = 1.0};
    placement.area.outline = {{.x = 2.3, .y = 0.75}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 12.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(verticalLandingExitChoiceLayout(), scenario);
    for (int i = 0; i < 120 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_TRUE(runner.complete());
    SC_EXPECT_EQ(runner.resultArtifacts().exitUsage.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.resultArtifacts().exitUsage.front().exitZoneId, std::string{"near-exit"});
}

SC_TEST(ScenarioSimulationRunnerHonorsAllowedStairEntryDirection) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "room-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 0.5, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 12.0;

    safecrowd::domain::ScenarioSimulationRunner runner(directedStairExitLayout(
        safecrowd::domain::StairEntryDirection::West,
        safecrowd::domain::StairEntryDirection::East), scenario);
    for (int i = 0; i < 120 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, std::size_t{1});
}

SC_TEST(ScenarioSimulationRunnerBlocksDisallowedStairEntryDirection) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "room-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 0.5, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 4.0;

    safecrowd::domain::ScenarioSimulationRunner runner(directedStairExitLayout(
        safecrowd::domain::StairEntryDirection::East,
        safecrowd::domain::StairEntryDirection::East), scenario);
    for (int i = 0; i < 40 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, std::size_t{0});
    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x < 2.0);
}

SC_TEST(ScenarioSimulationRunnerDisplaysNextFloorAfterCrossingStairMidpoint) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "stair-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.5, .y = 0.0};
    placement.area.outline = {{.x = 2.2, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 4.0;

    safecrowd::domain::ScenarioSimulationRunner runner(directedStairExitLayout(
        safecrowd::domain::StairEntryDirection::West,
        safecrowd::domain::StairEntryDirection::East), scenario);
    runner.step(0.55);

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L1"});

    runner.step(0.5);

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L2"});
}

SC_TEST(ScenarioSimulationRunnerDoesNotAdvanceBlockedUShapedStairBeforeConnection) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.floorId = "L1";
    placement.zoneId = "stair-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = -0.8, .y = 1.2};
    placement.area.outline = {{.x = 3.4, .y = 0.6}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 6.0;

    safecrowd::domain::ScenarioSimulationRunner runner(blockedUShapedStairTransitionLayout(), scenario);
    for (int i = 0; i < 40 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L1"});
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x > 2.0);
}

SC_TEST(ScenarioSimulationRunnerLandsOnOppositeUShapedStairLaneAfterFloorTransition) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.floorId = "L1";
    placement.zoneId = "stair-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = -0.8, .y = 1.2};
    placement.area.outline = {{.x = 3.4, .y = 0.6}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 10.0;

    safecrowd::domain::ScenarioSimulationRunner runner(uShapedStairTransitionLayout(), scenario);
    for (int i = 0; i < 80 && !runner.complete(); ++i) {
        runner.step(0.1);
        if (!runner.frame().agents.empty() && runner.frame().agents.front().floorId == "L2") {
            break;
        }
    }

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L2"});
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x < 2.0);
}

SC_TEST(ScenarioSimulationRunnerKeepsUShapedStairVerticalOpeningClearOfEntryWalls) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.floorId = "L1";
    placement.zoneId = "stair-l1";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 1.0, .y = 1.0};
    placement.area.outline = {{.x = 1.0, .y = 1.0}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 10.0;

    safecrowd::domain::ScenarioSimulationRunner runner(westEntryUShapedStairTransitionLayout(), scenario);
    for (int i = 0; i < 80 && !runner.complete(); ++i) {
        runner.step(0.1);
        if (!runner.frame().agents.empty() && runner.frame().agents.front().floorId == "L2") {
            break;
        }
    }

    SC_EXPECT_EQ(runner.frame().agents.size(), std::size_t{1});
    SC_EXPECT_EQ(runner.frame().agents.front().floorId, std::string{"L2"});
    SC_EXPECT_TRUE(runner.frame().agents.front().position.y > 2.0);
}

SC_TEST(ScenarioSimulationRunnerQueuesCrowdedUShapedStairFloorTransitions) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "descending-crowd";
    placement.floorId = "L2";
    placement.zoneId = "stair-l2";
    placement.initialVelocity = {.x = 0.0, .y = -1.0};
    placement.explicitPositions = {
        {.x = 2.9, .y = 2.35},
        {.x = 3.25, .y = 2.35},
        {.x = 3.6, .y = 2.35},
        {.x = 2.9, .y = 2.75},
        {.x = 3.25, .y = 2.75},
        {.x = 3.6, .y = 2.75},
    };

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 8.0;

    const auto layout = descendingWestEntryUShapedStairTransitionLayout();
    safecrowd::domain::ScenarioSimulationRunner runner(layout, scenario);
    for (int i = 0; i < 12 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    std::size_t agentsOnLowerFloor = 0;
    std::size_t agentsQueuedOnUpperFloor = 0;
    for (const auto& agent : runner.frame().agents) {
        if (agent.floorId == "L1") {
            ++agentsOnLowerFloor;
        }
        if (agent.floorId == "L2") {
            ++agentsQueuedOnUpperFloor;
        }
        SC_EXPECT_TRUE(agentInsideAnyZoneOnFrameFloor(layout, agent));
    }

    SC_EXPECT_TRUE(agentsOnLowerFloor >= 2);
    SC_EXPECT_TRUE(agentsQueuedOnUpperFloor >= 1);
}

SC_TEST(ScenarioSimulationRunnerMovesFollowingAgentsThroughDescendingUShapedStair) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "descending-group";
    placement.floorId = "L2";
    placement.zoneId = "stair-l2";
    placement.initialVelocity = {.x = 1.0, .y = -1.0};
    placement.explicitPositions = {
        {.x = 0.8, .y = 3.5},
        {.x = 1.4, .y = 3.5},
        {.x = 2.0, .y = 3.5},
        {.x = 2.6, .y = 3.5},
        {.x = 0.8, .y = 3.0},
        {.x = 1.4, .y = 3.0},
        {.x = 2.0, .y = 3.0},
        {.x = 2.6, .y = 3.0},
        {.x = 0.8, .y = 2.5},
        {.x = 1.4, .y = 2.5},
    };

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 16.0;

    const auto layout = descendingWestEntryUShapedStairTransitionLayout();
    safecrowd::domain::ScenarioSimulationRunner runner(layout, scenario);
    for (int i = 0; i < 160 && !runner.complete(); ++i) {
        runner.step(0.1);
    }

    std::size_t agentsStillOnUpperFloor = 0;
    std::size_t stalledAgents = 0;
    for (const auto& agent : runner.frame().agents) {
        if (agent.floorId == "L2") {
            ++agentsStillOnUpperFloor;
        }
        if (agent.stalled) {
            ++stalledAgents;
        }
        SC_EXPECT_TRUE(agentInsideAnyZoneOnFrameFloor(layout, agent));
    }

    SC_EXPECT_EQ(agentsStillOnUpperFloor, std::size_t{0});
    SC_EXPECT_EQ(stalledAgents, std::size_t{0});
}

SC_TEST(ScenarioSimulationRunnerKeepsCrowdedUShapedStairConnectionClearOfWalls) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "tight-descending-group";
    placement.floorId = "L2";
    placement.zoneId = "stair-l2";
    placement.initialVelocity = {.x = 0.0, .y = -1.0};
    placement.explicitPositions = {
        {.x = 3.65, .y = 2.35},
        {.x = 3.35, .y = 2.35},
        {.x = 3.05, .y = 2.35},
        {.x = 3.65, .y = 2.75},
        {.x = 3.35, .y = 2.75},
        {.x = 3.05, .y = 2.75},
        {.x = 3.65, .y = 3.15},
        {.x = 3.35, .y = 3.15},
    };

    safecrowd::domain::ScenarioDraft scenario;
    scenario.population.initialPlacements.push_back(placement);
    scenario.execution.timeLimitSeconds = 10.0;

    const auto layout = descendingWestEntryUShapedStairTransitionLayout();
    safecrowd::domain::ScenarioSimulationRunner runner(layout, scenario);
    bool anyAgentReachedLowerFloor = false;
    for (int i = 0; i < 100 && !runner.complete(); ++i) {
        runner.step(0.1);
        for (const auto& agent : runner.frame().agents) {
            anyAgentReachedLowerFloor = anyAgentReachedLowerFloor || agent.floorId == "L1";
            SC_EXPECT_TRUE(agentInsideAnyZoneOnFrameFloor(layout, agent));
            SC_EXPECT_TRUE(agentKeepsBarrierClearance(layout, agent, agent.radius * 0.95));
        }
    }

    std::size_t stalledAgents = 0;
    for (const auto& agent : runner.frame().agents) {
        if (agent.stalled) {
            ++stalledAgents;
        }
    }

    SC_EXPECT_TRUE(anyAgentReachedLowerFloor);
    SC_EXPECT_TRUE(runner.frame().agents.empty() || stalledAgents < runner.frame().agents.size());
}

SC_TEST(ScenarioSimulationRunnerBlocksMovementAcrossBarrierSegments) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = "left";
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 2.0, .y = 0.0};
    placement.area.outline = {{.x = 0.5, .y = 0.5}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 5.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(blockedDoorLayout(), scenario);
    runner.step(1.0);

    SC_EXPECT_EQ(runner.frame().agents.size(), static_cast<std::size_t>(1));
    SC_EXPECT_TRUE(runner.frame().agents.front().position.x < 1.0);
}

SC_TEST(ScenarioSimulationRunnerRoutesAroundClosedObstructions) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = "agent-1";
    placement.zoneId = safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId;
    placement.targetAgentCount = 1;
    placement.initialVelocity = {.x = 3.0, .y = 0.0};
    placement.area.outline = {{.x = 2.0, .y = 4.2}};

    safecrowd::domain::ScenarioDraft scenario;
    scenario.execution.timeLimitSeconds = 40.0;
    scenario.population.initialPlacements.push_back(placement);

    safecrowd::domain::ScenarioSimulationRunner runner(safecrowd::domain::DemoLayouts::demoFacility(), scenario);
    for (int i = 0; i < 160 && !runner.complete(); ++i) {
        runner.step(0.25);
    }

    SC_EXPECT_EQ(runner.frame().totalAgentCount, static_cast<std::size_t>(1));
    SC_EXPECT_EQ(runner.frame().evacuatedAgentCount, static_cast<std::size_t>(1));
}
