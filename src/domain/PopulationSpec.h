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

struct PopulationSpec {
    std::vector<InitialPlacement2D> initialPlacements{};
};

}  // namespace safecrowd::domain
