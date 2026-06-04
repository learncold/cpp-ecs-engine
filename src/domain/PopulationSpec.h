#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
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

inline std::size_t initialPlacementAgentCount(const InitialPlacement2D& placement) {
    return placement.explicitPositions.empty()
        ? placement.targetAgentCount
        : placement.explicitPositions.size();
}

inline std::size_t occupantSourceSpawnTickCount(const OccupantSource2D& source) {
    if (source.spawnIntervalSeconds <= 1e-9 || source.endSeconds <= source.startSeconds) {
        return 0;
    }

    const auto duration = source.endSeconds - source.startSeconds;
    return static_cast<std::size_t>(
        std::floor(std::max(0.0, duration - 1e-9) / source.spawnIntervalSeconds)) + 1;
}

inline std::size_t occupantSourceScheduledAgentCount(const OccupantSource2D& source) {
    if (source.targetAgentCount == 0) {
        return 0;
    }
    const auto scheduled = occupantSourceSpawnTickCount(source) * std::max<std::size_t>(1, source.agentsPerSpawn);
    return std::min(source.targetAgentCount, scheduled);
}

inline std::size_t scheduledPopulationAgentCount(const PopulationSpec& population) {
    std::size_t total = 0;
    for (const auto& placement : population.initialPlacements) {
        total += initialPlacementAgentCount(placement);
    }
    for (const auto& source : population.occupantSources) {
        total += occupantSourceScheduledAgentCount(source);
    }
    return total;
}

}  // namespace safecrowd::domain
