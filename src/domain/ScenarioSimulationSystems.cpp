#include "domain/ScenarioSimulationSystems.h"

#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <utility>

#include "engine/EngineWorld.h"

namespace safecrowd::domain {
namespace {

constexpr double kReplaySampleIntervalSeconds = 0.5;
constexpr std::size_t kMaxReplayFrames = 600;

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

void appendReplayFrame(
    ScenarioResultArtifactsResource& result,
    const SimulationFrame& frame) {
    auto& frames = result.artifacts.replayFrames;
    if (!frames.empty() && std::abs(frames.back().elapsedSeconds - frame.elapsedSeconds) <= 1e-9) {
        frames.back() = frame;
        return;
    }

    if (frames.size() >= result.maxReplayFrames) {
        if (frame.complete && !frames.empty()) {
            frames.back() = frame;
        }
        return;
    }

    frames.push_back(frame);
}

std::optional<double> percentileCompletionTime(
    std::vector<double> completionTimes,
    std::size_t totalAgentCount,
    double percentile) {
    if (totalAgentCount == 0 || completionTimes.empty()) {
        return std::nullopt;
    }

    const auto targetCount = static_cast<std::size_t>(
        std::ceil(static_cast<double>(totalAgentCount) * percentile));
    if (targetCount == 0 || completionTimes.size() < targetCount) {
        return std::nullopt;
    }

    std::sort(completionTimes.begin(), completionTimes.end());
    return completionTimes[targetCount - 1];
}

bool intervalContains(const ConnectionBlockIntervalDraft& interval, double timeSeconds) {
    const auto start = std::max(0.0, interval.startSeconds);
    const auto end = std::max(start, interval.endSeconds);
    return timeSeconds + 1e-9 >= start && timeSeconds <= end + 1e-9;
}

bool connectionShouldBeBlocked(const ConnectionBlockDraft& block, double timeSeconds) {
    if (block.connectionId.empty()) {
        return false;
    }
    if (block.intervals.empty()) {
        return true;
    }
    return std::any_of(block.intervals.begin(), block.intervals.end(), [&](const auto& interval) {
        return intervalContains(interval, timeSeconds);
    });
}

Connection2D* findConnectionById(FacilityLayout2D& layout, const std::string& connectionId) {
    const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        return connection.id == connectionId;
    });
    return it == layout.connections.end() ? nullptr : &(*it);
}

std::unordered_set<std::string> activeBlockedConnectionIds(
    const FacilityLayout2D& layout,
    const std::vector<ConnectionBlockDraft>& blocks,
    double elapsedSeconds) {
    std::unordered_set<std::string> ids;
    ids.reserve(blocks.size());
    for (const auto& block : blocks) {
        if (!connectionShouldBeBlocked(block, elapsedSeconds)) {
            continue;
        }
        if (std::any_of(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
                return connection.id == block.connectionId;
            })) {
            ids.insert(block.connectionId);
        }
    }
    return ids;
}

FacilityLayout2D layoutWithConnectionBlocks(
    FacilityLayout2D layout,
    const std::unordered_set<std::string>& blockedConnectionIds) {
    for (const auto& connectionId : blockedConnectionIds) {
        auto* connection = findConnectionById(layout, connectionId);
        if (connection == nullptr) {
            continue;
        }
        connection->directionality = TravelDirection::Closed;
        layout.barriers.push_back(Barrier2D{
            .id = "control-block-" + connectionId,
            .floorId = connection->floorId,
            .geometry = Polyline2D{.vertices = {connection->centerSpan.start, connection->centerSpan.end}, .closed = false},
            .blocksMovement = true,
        });
    }
    return layout;
}

class ScenarioControlSystem final : public engine::EngineSystem {
public:
    ScenarioControlSystem(FacilityLayout2D baseLayout, std::vector<ConnectionBlockDraft> blocks)
        : baseLayout_(std::move(baseLayout)),
          blocks_(std::move(blocks)) {
    }

    void configure(engine::EngineWorld& world) override {
        world.resources().set(simulation_internal::buildScenarioLayoutCache(baseLayout_));
        world.resources().set(ScenarioLayoutRevisionResource{.revision = revision_});
    }

    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override {
        (void)step;

        auto& resources = world.resources();
        double elapsedSeconds = 0.0;
        if (resources.contains<ScenarioSimulationClockResource>()) {
            elapsedSeconds = std::max(0.0, resources.get<ScenarioSimulationClockResource>().elapsedSeconds);
        }

        auto blockedConnectionIds = activeBlockedConnectionIds(baseLayout_, blocks_, elapsedSeconds);
        const bool changed = blockedConnectionIds != previousBlockedConnectionIds_;
        const bool cacheMissing = !resources.contains<ScenarioLayoutCacheResource>();
        if (!changed && !cacheMissing) {
            if (!resources.contains<ScenarioLayoutRevisionResource>()
                || resources.get<ScenarioLayoutRevisionResource>().revision != revision_) {
                resources.set(ScenarioLayoutRevisionResource{.revision = revision_});
            }
            return;
        }

        if (changed) {
            ++revision_;
            previousBlockedConnectionIds_ = blockedConnectionIds;
        }

        auto controlledLayout = layoutWithConnectionBlocks(baseLayout_, blockedConnectionIds);
        resources.set(simulation_internal::buildScenarioLayoutCache(std::move(controlledLayout)));
        resources.set(ScenarioLayoutRevisionResource{.revision = revision_});
    }

private:
    FacilityLayout2D baseLayout_{};
    std::vector<ConnectionBlockDraft> blocks_{};
    std::unordered_set<std::string> previousBlockedConnectionIds_{};
    std::uint64_t revision_{0};
};

}  // namespace

ScenarioAgentSpawnSystem::ScenarioAgentSpawnSystem(std::vector<ScenarioAgentSeed> seeds, double timeLimitSeconds)
    : seeds_(std::move(seeds)),
      timeLimitSeconds_(std::max(0.0, timeLimitSeconds)) {
}

void ScenarioAgentSpawnSystem::configure(engine::EngineWorld& world) {
    world.resources().set(ScenarioSimulationClockResource{
        .elapsedSeconds = 0.0,
        .timeLimitSeconds = timeLimitSeconds_ > 0.0 ? timeLimitSeconds_ : 60.0,
        .complete = false,
    });

    for (const auto& seed : seeds_) {
        world.commands().spawnEntity(
            seed.position,
            seed.agent,
            seed.velocity,
            seed.route,
            seed.status);
    }
}

void ScenarioAgentSpawnSystem::update(engine::EngineWorld& world, const engine::EngineStepContext& step) {
    (void)world;
    (void)step;
}

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

ScenarioClockSystem::ScenarioClockSystem(double fixedDeltaSeconds)
    : fixedDeltaSeconds_(std::max(0.0, fixedDeltaSeconds)) {
}

void ScenarioClockSystem::update(engine::EngineWorld& world, const engine::EngineStepContext& step) {
    (void)step;

    auto& resources = world.resources();
    if (!resources.contains<ScenarioSimulationClockResource>()) {
        resources.set(ScenarioSimulationClockResource{});
    }

    auto& clock = resources.get<ScenarioSimulationClockResource>();
    if (clock.complete) {
        return;
    }

    const auto remaining = std::max(0.0, clock.timeLimitSeconds - clock.elapsedSeconds);
    const auto delta = std::min(fixedDeltaSeconds_, remaining);
    clock.elapsedSeconds += delta;
    clock.complete = clock.elapsedSeconds >= clock.timeLimitSeconds;
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
            .floorId = query.contains<EvacuationRoute>(entity)
                ? (!query.get<EvacuationRoute>(entity).displayFloorId.empty()
                    ? query.get<EvacuationRoute>(entity).displayFloorId
                    : query.get<EvacuationRoute>(entity).currentFloorId)
                : std::string{},
        });
    }

    if (resources.contains<ScenarioResultArtifactsResource>()) {
        auto& result = resources.get<ScenarioResultArtifactsResource>();
        const auto shouldRecordReplay =
            result.artifacts.replayFrames.empty()
            || frame.elapsedSeconds + 1e-9 >= result.nextReplaySampleTimeSeconds
            || frame.complete;
        if (shouldRecordReplay) {
            appendReplayFrame(result, frame);
            while (result.nextReplaySampleTimeSeconds <= frame.elapsedSeconds + 1e-9) {
                result.nextReplaySampleTimeSeconds += result.replaySampleIntervalSeconds;
            }
        }
    }

    resources.set(ScenarioSimulationFrameResource{.frame = std::move(frame)});
}

ScenarioResultArtifactsSystem::ScenarioResultArtifactsSystem(double sampleIntervalSeconds)
    : sampleIntervalSeconds_(sampleIntervalSeconds > 0.0 ? sampleIntervalSeconds : 1.0) {
}

void ScenarioResultArtifactsSystem::configure(engine::EngineWorld& world) {
    world.resources().set(ScenarioResultArtifactsResource{
        .artifacts = {},
        .lastRecordedEvacuatedCount = std::numeric_limits<std::size_t>::max(),
        .nextSampleTimeSeconds = 0.0,
        .sampleIntervalSeconds = sampleIntervalSeconds_,
        .nextReplaySampleTimeSeconds = 0.0,
        .replaySampleIntervalSeconds = kReplaySampleIntervalSeconds,
        .maxReplayFrames = kMaxReplayFrames,
    });
}

void ScenarioResultArtifactsSystem::update(engine::EngineWorld& world, const engine::EngineStepContext& step) {
    (void)step;

    auto& query = world.query();
    auto& resources = world.resources();
    if (!resources.contains<ScenarioResultArtifactsResource>()) {
        configure(world);
    }

    auto& result = resources.get<ScenarioResultArtifactsResource>();
    double elapsedSeconds = 0.0;
    bool complete = false;
    if (resources.contains<ScenarioSimulationClockResource>()) {
        const auto& clock = resources.get<ScenarioSimulationClockResource>();
        elapsedSeconds = clock.elapsedSeconds;
        complete = clock.complete;
    }

    std::size_t totalAgentCount = 0;
    std::size_t evacuatedCount = 0;
    std::vector<double> completionTimes;
    const auto entities = query.view<EvacuationStatus>();
    completionTimes.reserve(entities.size());
    for (const auto entity : entities) {
        ++totalAgentCount;
        const auto& status = query.get<EvacuationStatus>(entity);
        if (!status.evacuated) {
            continue;
        }
        ++evacuatedCount;
        completionTimes.push_back(status.completionTimeSeconds);
    }

    result.artifacts.timingSummary.t50Seconds =
        percentileCompletionTime(completionTimes, totalAgentCount, 0.50);
    result.artifacts.timingSummary.t90Seconds =
        percentileCompletionTime(completionTimes, totalAgentCount, 0.90);
    result.artifacts.timingSummary.t95Seconds =
        percentileCompletionTime(completionTimes, totalAgentCount, 0.95);
    if (totalAgentCount > 0 && completionTimes.size() == totalAgentCount) {
        result.artifacts.timingSummary.finalEvacuationTimeSeconds =
            *std::max_element(completionTimes.begin(), completionTimes.end());
    } else {
        result.artifacts.timingSummary.finalEvacuationTimeSeconds = std::nullopt;
    }

    const auto shouldRecordSample =
        result.artifacts.evacuationProgress.empty()
        || evacuatedCount != result.lastRecordedEvacuatedCount
        || elapsedSeconds + 1e-9 >= result.nextSampleTimeSeconds
        || complete;
    if (!shouldRecordSample) {
        return;
    }

    result.artifacts.evacuationProgress.push_back({
        .timeSeconds = elapsedSeconds,
        .evacuatedCount = evacuatedCount,
        .totalCount = totalAgentCount,
        .evacuatedRatio = totalAgentCount == 0
            ? 0.0
            : static_cast<double>(evacuatedCount) / static_cast<double>(totalAgentCount),
    });
    result.lastRecordedEvacuatedCount = evacuatedCount;
    while (result.nextSampleTimeSeconds <= elapsedSeconds + 1e-9) {
        result.nextSampleTimeSeconds += result.sampleIntervalSeconds;
    }
}

std::unique_ptr<engine::EngineSystem> makeScenarioControlSystem(
    FacilityLayout2D baseLayout,
    std::vector<ConnectionBlockDraft> blocks) {
    return std::make_unique<ScenarioControlSystem>(std::move(baseLayout), std::move(blocks));
}

}  // namespace safecrowd::domain
