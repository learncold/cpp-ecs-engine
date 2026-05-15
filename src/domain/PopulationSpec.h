#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

enum class InitialPlacementDistribution {
    Uniform,
    Random,
};

struct InitialPlacement2D {
    std::string id{};
    std::string zoneId{};
    std::string floorId{};
    Polygon2D area{};
    std::size_t targetAgentCount{0};
    Point2D initialVelocity{};
    InitialPlacementDistribution distribution{InitialPlacementDistribution::Uniform};
    std::vector<Point2D> explicitPositions{};
};

struct OccupantSource2D {
    std::string id{};
    std::string zoneId{};
    std::string floorId{};
    Point2D position{};
    std::size_t targetAgentCount{0};
    std::size_t agentsPerSpawn{1};
    double startSeconds{0.0};
    double endSeconds{180.0};
    double spawnIntervalSeconds{5.0};
    Point2D initialVelocity{};
};

struct PopulationSpec {
    std::vector<InitialPlacement2D> initialPlacements{};
    std::vector<OccupantSource2D> occupantSources{};
};

}  // namespace safecrowd::domain
