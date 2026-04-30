#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

struct Position {
    Point2D value;
};

struct Agent {
    float radius{0.25f};
    float maxSpeed{1.5f};
};

struct Velocity {
    Point2D value;
};

struct EvacuationRoute {
    std::vector<Point2D> waypoints{};
    std::vector<LineSegment2D> waypointPassages{};
    std::vector<std::string> waypointFromZoneIds{};
    std::vector<std::string> waypointZoneIds{};
    std::vector<std::string> waypointFloorIds{};
    std::vector<std::string> waypointConnectionIds{};
    std::vector<bool> waypointVerticalTransitions{};
    std::size_t nextWaypointIndex{0};
    Point2D currentSegmentStart{};
    double previousDistanceToWaypoint{0.0};
    double stalledSeconds{0.0};
    std::string destinationZoneId{};
    std::string currentFloorId{};
    std::string displayFloorId{};

    double nextExitReplanSeconds{0.0};
    double nextSegmentReplanSeconds{0.0};
    std::uint64_t observedLayoutRevision{0};
    bool noExitAvailable{false};
};

struct EvacuationStatus {
    bool evacuated{false};
    double completionTimeSeconds{0.0};
};

}  // namespace safecrowd::domain
