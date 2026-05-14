#include "domain/CompressionSystem.h"

#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
#include "domain/Metrics.h"
#include "domain/PressureTuning.h"
#include "domain/ScenarioSimulationInternal.h"
#include "domain/ScenarioSimulationSystems.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace safecrowd::domain {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kReferenceDistanceMeters = kPressureReferenceDistanceMeters;

struct SpatialCell {
    int x{0};
    int y{0};
};

double distanceBetween(const Point2D& lhs, const Point2D& rhs) {
    const double dx = lhs.x - rhs.x;
    const double dy = lhs.y - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
}

double distancePointToSegment(const Point2D& point, const Point2D& start, const Point2D& end) {
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double lengthSquared = (dx * dx) + (dy * dy);

    if (lengthSquared == 0.0) {
        return distanceBetween(point, start);
    }

    const double t = std::clamp(
        (((point.x - start.x) * dx) + ((point.y - start.y) * dy)) / lengthSquared,
        0.0,
        1.0);
    const Point2D projection{
        .x = start.x + (t * dx),
        .y = start.y + (t * dy),
    };
    return distanceBetween(point, projection);
}

double barrierCompression(const Barrier2D& barrier, const Point2D& position, double referenceDistance) {
    if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
        return 0.0;
    }

    double force = 0.0;
    const auto& vertices = barrier.geometry.vertices;

    for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
        const double distance = distancePointToSegment(position, vertices[index], vertices[index + 1]);
        if (distance < referenceDistance) {
            force += (referenceDistance - distance) / referenceDistance;
        }
    }

    if (barrier.geometry.closed) {
        const double distance = distancePointToSegment(position, vertices.back(), vertices.front());
        if (distance < referenceDistance) {
            force += (referenceDistance - distance) / referenceDistance;
        }
    }

    return force;
}

double localDensityRatio(std::size_t nearbyCount) {
    const auto areaSquareMeters = kPi * kReferenceDistanceMeters * kReferenceDistanceMeters;
    const auto densityPeoplePerSquareMeter =
        static_cast<double>(nearbyCount) / areaSquareMeters;
    return densityPeoplePerSquareMeter / kPressureHighDensityThresholdPeoplePerSquareMeter;
}

long long spatialKey(const SpatialCell& cell) {
    return (static_cast<long long>(cell.x) << 32)
        ^ static_cast<unsigned int>(cell.y);
}

SpatialCell spatialCellFor(const Point2D& point, double cellSize) {
    return {
        .x = static_cast<int>(std::floor(point.x / cellSize)),
        .y = static_cast<int>(std::floor(point.y / cellSize)),
    };
}

}  // namespace

CompressionSystem::CompressionSystem(double timeStepSeconds)
    : timeStepSeconds_(static_cast<float>(std::max(0.0, timeStepSeconds))) {
}

void CompressionSystem::update(engine::EngineWorld& world,
                               const engine::EngineStepContext& step) {
    (void)step;

    auto& query = world.query();
    auto& resources = world.resources();
    const auto agentEntities = query.view<Position, Agent, CompressionData>();
    const auto barrierEntities = query.view<Barrier2D>();
    const auto* spatialIndex = resources.contains<ScenarioAgentSpatialIndexResource>()
        ? &resources.get<ScenarioAgentSpatialIndexResource>()
        : nullptr;
    std::unordered_map<long long, std::vector<engine::Entity>> agentCells;
    if (spatialIndex == nullptr) {
        agentCells.reserve(agentEntities.size());
        for (const auto entity : agentEntities) {
            const auto& position = query.get<Position>(entity);
            agentCells[spatialKey(spatialCellFor(position.value, kReferenceDistanceMeters))].push_back(entity);
        }
    }

    for (const auto entity : agentEntities) {
        const auto& position = query.get<Position>(entity);
        auto& compression = query.get<CompressionData>(entity);

        std::size_t nearbyCount = 0;
        double proximityScore = 0.0;
        if (spatialIndex != nullptr) {
            const auto floorId = query.contains<EvacuationRoute>(entity)
                ? simulation_internal::agentCollisionFloorId(query.get<EvacuationRoute>(entity))
                : std::string{};
            const auto nearbyAgents = scenarioNearbyAgents(
                query,
                *spatialIndex,
                position.value,
                floorId,
                kReferenceDistanceMeters);
            for (const auto otherEntity : nearbyAgents) {
                if (otherEntity == entity) {
                    continue;
                }

                const auto& otherPosition = query.get<Position>(otherEntity);
                const double distance = distanceBetween(position.value, otherPosition.value);
                if (distance < kReferenceDistanceMeters) {
                    proximityScore += (kReferenceDistanceMeters - distance) / kReferenceDistanceMeters;
                    ++nearbyCount;
                }
            }
        } else {
            const auto centerCell = spatialCellFor(position.value, kReferenceDistanceMeters);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const auto cellIt = agentCells.find(spatialKey({
                        .x = centerCell.x + dx,
                        .y = centerCell.y + dy,
                    }));
                    if (cellIt == agentCells.end()) {
                        continue;
                    }
                    for (const auto otherEntity : cellIt->second) {
                        if (otherEntity == entity) {
                            continue;
                        }

                        const auto& otherPosition = query.get<Position>(otherEntity);
                        const double distance = distanceBetween(position.value, otherPosition.value);
                        if (distance < kReferenceDistanceMeters) {
                            proximityScore += (kReferenceDistanceMeters - distance) / kReferenceDistanceMeters;
                            ++nearbyCount;
                        }
                    }
                }
            }
        }

        double currentForce = std::max(proximityScore, localDensityRatio(nearbyCount));
        for (const auto barrierEntity : barrierEntities) {
            currentForce = std::max(
                currentForce,
                proximityScore + barrierCompression(
                    query.get<Barrier2D>(barrierEntity),
                    position.value,
                    kReferenceDistanceMeters));
        }

        compression.force = static_cast<float>(currentForce);
        if (compression.force >= kPressureCriticalScoreThreshold) {
            compression.exposure += timeStepSeconds_;
        }

        compression.isCritical =
            compression.force >= kPressureCriticalScoreThreshold &&
            compression.exposure >= kPressureCriticalExposureThresholdSeconds;
    }
}

}  // namespace safecrowd::domain
