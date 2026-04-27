#include "domain/ScenarioSimulationSystems.h"

#include <algorithm>
#include <cmath>

#include "engine/EngineWorld.h"

namespace safecrowd::domain {
namespace {

struct SpatialCell {
    int x{0};
    int y{0};
};

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

double distanceBetween(const Point2D& lhs, const Point2D& rhs) {
    return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y);
}

}  // namespace

std::vector<engine::Entity> scenarioNearbyAgents(
    engine::WorldQuery& query,
    const ScenarioAgentSpatialIndexResource& index,
    const Point2D& point,
    double radius) {
    std::vector<engine::Entity> candidates;
    const auto center = spatialCellFor(point, index.cellSize);
    const auto range = std::max(1, static_cast<int>(std::ceil(radius / index.cellSize)));
    for (int dy = -range; dy <= range; ++dy) {
        for (int dx = -range; dx <= range; ++dx) {
            const auto it = index.cells.find(spatialKey({.x = center.x + dx, .y = center.y + dy}));
            if (it == index.cells.end()) {
                continue;
            }
            for (const auto entity : it->second) {
                const auto& otherPosition = query.get<Position>(entity);
                if (distanceBetween(point, otherPosition.value) <= radius) {
                    candidates.push_back(entity);
                }
            }
        }
    }
    return candidates;
}

ScenarioSpatialIndexSystem::ScenarioSpatialIndexSystem(double cellSize)
    : cellSize_(std::max(0.1, cellSize)) {
}

void ScenarioSpatialIndexSystem::update(engine::EngineWorld& world, const engine::EngineStepContext& step) {
    (void)step;

    auto& query = world.query();
    auto& resources = world.resources();
    ScenarioAgentSpatialIndexResource index;
    index.cellSize = cellSize_;

    const auto entities = query.view<Position, Agent, EvacuationStatus>();
    index.cells.reserve(entities.size() * 2);
    for (const auto entity : entities) {
        const auto& status = query.get<EvacuationStatus>(entity);
        if (status.evacuated) {
            continue;
        }
        const auto& position = query.get<Position>(entity);
        index.cells[spatialKey(spatialCellFor(position.value, index.cellSize))].push_back(entity);
    }

    resources.set(std::move(index));
}

void ScenarioFrameSyncSystem::update(engine::EngineWorld& world, const engine::EngineStepContext& step) {
    (void)step;

    auto& query = world.query();
    auto& resources = world.resources();
    SimulationFrame frame;
    if (resources.contains<ScenarioSimulationClockResource>()) {
        const auto& clock = resources.get<ScenarioSimulationClockResource>();
        frame.elapsedSeconds = clock.elapsedSeconds;
        frame.complete = clock.complete;
    }

    const auto entities = query.view<Position, Agent, Velocity, EvacuationStatus>();
    frame.agents.reserve(entities.size());
    for (const auto entity : entities) {
        ++frame.totalAgentCount;
        const auto& status = query.get<EvacuationStatus>(entity);
        if (status.evacuated) {
            ++frame.evacuatedAgentCount;
            continue;
        }

        const auto& position = query.get<Position>(entity);
        const auto& velocity = query.get<Velocity>(entity);
        const auto& agent = query.get<Agent>(entity);
        frame.agents.push_back({
            .id = entity.index,
            .position = position.value,
            .velocity = velocity.value,
            .radius = agent.radius,
        });
    }

    resources.set(ScenarioSimulationFrameResource{.frame = std::move(frame)});
}

}  // namespace safecrowd::domain
