#include "domain/ScenarioSimulationSystems.h"

#include "domain/GeometryQueries.h"
#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace safecrowd::domain {
namespace {

using namespace simulation_internal;

constexpr std::size_t kMaxReportedHotspots = 5;
constexpr std::size_t kMaxReportedPressureHotspots = 5;
constexpr std::size_t kMaxReportedPressureAgents = 5;
constexpr std::size_t kMaxReportedCriticalPressureEvents = 5;
constexpr std::size_t kMaxReportedBottlenecks = 5;
constexpr std::size_t kMaxReportedCrossFlowCells = 5;
constexpr double kCrossFlowDirectionEpsilon = 1e-6;

template <typename T, typename Compare>
void sortAndTrimTop(std::vector<T>& values, std::size_t maxCount, Compare compare) {
    if (values.size() <= maxCount) {
        std::sort(values.begin(), values.end(), compare);
        return;
    }

    const auto trimEnd = values.begin() + static_cast<std::ptrdiff_t>(maxCount);
    std::partial_sort(values.begin(), trimEnd, values.end(), compare);
    values.resize(maxCount);
}

struct RiskCellAccumulator {
    Point2D positionSum{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::string floorId{};
    std::size_t agentCount{0};
    std::vector<engine::Entity> entities{};
};

struct CrossFlowCellAccumulator {
    Point2D positionSum{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::string floorId{};
    std::size_t movingAgentCount{0};
    double speedSum{0.0};
    std::array<std::size_t, kScenarioCrossFlowDirectionBinCount> directionCounts{};
};

struct ActiveAgentContext {
    engine::Entity entity{};
    std::uint64_t agentId{0};
    Point2D position{};
    Point2D velocity{};
    std::string floorId{};
    double radius{0.25};
    bool stalled{false};
};

struct CrossFlowCellAddress {
    SpatialCell cell{};
    std::string floorId{};
};

struct CrossFlowObservation {
    std::size_t movingAgentCount{0};
    std::size_t primaryFlowCount{0};
    std::size_t crossFlowCount{0};
    double averageSpeed{0.0};
    double crossFlowRatio{0.0};
    double speedDropRatio{0.0};
    double crossFlowScore{0.0};
};

struct ActivePressureFeedbackContext {
    engine::Entity entity{};
    std::uint64_t agentId{0};
    Point2D position{};
    std::string displayFloorId{};
    std::string collisionFloorId{};
    double radius{0.25};
};

struct ScenarioPressureAgentTrackingState {
    double currentForce{0.0};
    double exposureSeconds{0.0};
};

struct ScenarioActiveCriticalPressureEventState {
    double startedAtSeconds{0.0};
};

struct ScenarioPressureTrackingResource {
    bool hasPreviousElapsedSeconds{false};
    double previousElapsedSeconds{0.0};
    std::unordered_map<std::uint64_t, ScenarioPressureAgentTrackingState> agentStates{};
    std::unordered_map<long long, ScenarioActiveCriticalPressureEventState> activeEvents{};
};

struct RiskCellAddress {
    SpatialCell cell{};
    std::string floorId{};
};

RiskCellAddress riskCellAddress(const Point2D& point, const std::string& floorId) {
    return {
        .cell = spatialCellFor(point, kScenarioHotspotCellSize),
        .floorId = floorId,
    };
}

long long riskCellKey(const RiskCellAddress& cell) {
    const auto cellKey = spatialKey(cell.cell);
    return cellKey ^ (static_cast<long long>(std::hash<std::string>{}(cell.floorId)) << 1);
}

Point2D riskCellMin(const RiskCellAddress& cell) {
    return spatialCellMin(cell.cell, kScenarioHotspotCellSize);
}

Point2D riskCellMax(const RiskCellAddress& cell) {
    return spatialCellMax(cell.cell, kScenarioHotspotCellSize);
}

CrossFlowCellAddress crossFlowCellAddress(const Point2D& point, const std::string& floorId) {
    return {
        .cell = spatialCellFor(point, kScenarioCrossFlowCellSize),
        .floorId = floorId,
    };
}

long long crossFlowCellKey(const CrossFlowCellAddress& cell) {
    const auto cellKey = spatialKey(cell.cell);
    return cellKey ^ (static_cast<long long>(std::hash<std::string>{}(cell.floorId)) << 1);
}

Point2D crossFlowCellMin(const CrossFlowCellAddress& cell) {
    return spatialCellMin(cell.cell, kScenarioCrossFlowCellSize);
}

Point2D crossFlowCellMax(const CrossFlowCellAddress& cell) {
    return spatialCellMax(cell.cell, kScenarioCrossFlowCellSize);
}

bool isStalled(const Velocity& velocity, const EvacuationRoute& route) {
    return scenarioAgentStalled(lengthOf(velocity.value), route.stalledSeconds);
}

int riskSeverity(ScenarioRiskLevel level) {
    switch (level) {
    case ScenarioRiskLevel::Low:
        return 0;
    case ScenarioRiskLevel::Medium:
        return 1;
    case ScenarioRiskLevel::High:
        return 2;
    }
    return 0;
}

bool isHotspotSetWorse(
    const std::vector<ScenarioCongestionHotspot>& candidate,
    const std::vector<ScenarioCongestionHotspot>& currentPeak) {
    if (candidate.empty()) {
        return false;
    }
    if (currentPeak.empty()) {
        return true;
    }
    return candidate.front().agentCount > currentPeak.front().agentCount;
}

bool isPressureHotspotSetWorse(
    const std::vector<ScenarioPressureHotspot>& candidate,
    const std::vector<ScenarioPressureHotspot>& currentPeak) {
    if (candidate.empty()) {
        return false;
    }
    if (currentPeak.empty()) {
        return true;
    }

    const auto& lhs = candidate.front();
    const auto& rhs = currentPeak.front();
    if (std::fabs(lhs.pressureScore - rhs.pressureScore) > 1e-9) {
        return lhs.pressureScore > rhs.pressureScore;
    }
    if (lhs.intrudingPairCount != rhs.intrudingPairCount) {
        return lhs.intrudingPairCount > rhs.intrudingPairCount;
    }
    return lhs.agentCount > rhs.agentCount;
}

bool isPressureAgentSetWorse(
    const std::vector<ScenarioPressureAgentMetric>& candidate,
    const std::vector<ScenarioPressureAgentMetric>& currentPeak) {
    if (candidate.empty()) {
        return false;
    }
    if (currentPeak.empty()) {
        return true;
    }

    const auto& lhs = candidate.front();
    const auto& rhs = currentPeak.front();
    if (lhs.critical != rhs.critical) {
        return lhs.critical;
    }
    if (std::fabs(lhs.exposureSeconds - rhs.exposureSeconds) > 1e-9) {
        return lhs.exposureSeconds > rhs.exposureSeconds;
    }
    return lhs.compressionForce > rhs.compressionForce;
}

bool isCriticalPressureEventSetWorse(
    const std::vector<ScenarioCriticalPressureEvent>& candidate,
    const std::vector<ScenarioCriticalPressureEvent>& currentPeak) {
    if (candidate.empty()) {
        return false;
    }
    if (currentPeak.empty()) {
        return true;
    }

    const auto& lhs = candidate.front();
    const auto& rhs = currentPeak.front();
    if (lhs.criticalAgentCount != rhs.criticalAgentCount) {
        return lhs.criticalAgentCount > rhs.criticalAgentCount;
    }
    if (std::fabs(lhs.durationSeconds - rhs.durationSeconds) > 1e-9) {
        return lhs.durationSeconds > rhs.durationSeconds;
    }
    return lhs.pressureScore > rhs.pressureScore;
}

bool isBottleneckSetWorse(
    const std::vector<ScenarioBottleneckMetric>& candidate,
    const std::vector<ScenarioBottleneckMetric>& currentPeak) {
    if (candidate.empty()) {
        return false;
    }
    if (currentPeak.empty()) {
        return true;
    }

    const auto& lhs = candidate.front();
    const auto& rhs = currentPeak.front();
    if (lhs.stalledAgentCount != rhs.stalledAgentCount) {
        return lhs.stalledAgentCount > rhs.stalledAgentCount;
    }
    if (lhs.nearbyAgentCount != rhs.nearbyAgentCount) {
        return lhs.nearbyAgentCount > rhs.nearbyAgentCount;
    }
    return lhs.averageSpeed < rhs.averageSpeed;
}

bool isCrossFlowCellSetWorse(
    const std::vector<ScenarioCrossFlowCellMetric>& candidate,
    const std::vector<ScenarioCrossFlowCellMetric>& currentPeak) {
    if (candidate.empty()) {
        return false;
    }
    if (currentPeak.empty()) {
        return true;
    }

    const auto& lhs = candidate.front();
    const auto& rhs = currentPeak.front();
    if (std::fabs(lhs.crossFlowScore - rhs.crossFlowScore) > 1e-9) {
        return lhs.crossFlowScore > rhs.crossFlowScore;
    }
    if (std::fabs(lhs.durationSeconds - rhs.durationSeconds) > 1e-9) {
        return lhs.durationSeconds > rhs.durationSeconds;
    }
    return lhs.movingAgentCount > rhs.movingAgentCount;
}

bool barrierMatchesFloor(const Barrier2D& barrier, const std::string& floorId) {
    return matchesFloor(barrier.floorId, floorId);
}

double barrierCompression(const Barrier2D& barrier, const Point2D& position, double referenceDistance) {
    if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
        return 0.0;
    }

    double force = 0.0;
    const auto& vertices = barrier.geometry.vertices;
    for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
        const auto distance = distancePointToSegment(position, vertices[index], vertices[index + 1]);
        if (distance < referenceDistance) {
            force += (referenceDistance - distance) / referenceDistance;
        }
    }
    if (barrier.geometry.closed) {
        const auto distance = distancePointToSegment(position, vertices.back(), vertices.front());
        if (distance < referenceDistance) {
            force += (referenceDistance - distance) / referenceDistance;
        }
    }
    return force;
}

ScenarioAgentSpatialIndexResource buildDisplayFloorPressureIndex(
    const std::vector<ActiveAgentContext>& activeAgents,
    const FacilityLayout2D& layout) {
    ScenarioAgentSpatialIndexResource index;
    index.cellSize = kPressureReferenceDistanceMeters;
    index.displayCellsByFloor.reserve(4);
    for (const auto& agent : activeAgents) {
        const auto cellKey = spatialKey(spatialCellFor(agent.position, index.cellSize));
        index.displayCellsByFloor[agent.floorId][cellKey].push_back(agent.entity);
    }
    index.barrierIndicesByFloor.reserve(std::max<std::size_t>(1, layout.floors.size()));
    for (std::size_t barrierIndex = 0; barrierIndex < layout.barriers.size(); ++barrierIndex) {
        const auto& barrier = layout.barriers[barrierIndex];
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        auto insertCoverage = [&](const Point2D& minPoint, const Point2D& maxPoint) {
            auto& floorCells = index.barrierIndicesByFloor[barrier.floorId];
            for (const auto& cell : spatialCellsForBounds(minPoint, maxPoint, index.cellSize)) {
                floorCells[spatialKey(cell)].push_back(barrierIndex);
            }
        };

        for (std::size_t vertexIndex = 0; vertexIndex + 1 < vertices.size(); ++vertexIndex) {
            insertCoverage(
                {
                    .x = std::min(vertices[vertexIndex].x, vertices[vertexIndex + 1].x),
                    .y = std::min(vertices[vertexIndex].y, vertices[vertexIndex + 1].y),
                },
                {
                    .x = std::max(vertices[vertexIndex].x, vertices[vertexIndex + 1].x),
                    .y = std::max(vertices[vertexIndex].y, vertices[vertexIndex + 1].y),
                });
        }
        if (barrier.geometry.closed) {
            auto minX = vertices.front().x;
            auto minY = vertices.front().y;
            auto maxX = vertices.front().x;
            auto maxY = vertices.front().y;
            for (const auto& vertex : vertices) {
                minX = std::min(minX, vertex.x);
                minY = std::min(minY, vertex.y);
                maxX = std::max(maxX, vertex.x);
                maxY = std::max(maxY, vertex.y);
            }
            insertCoverage({.x = minX, .y = minY}, {.x = maxX, .y = maxY});
        }
    }
    return index;
}

std::size_t crossFlowDirectionBinForVelocity(Point2D velocity) {
    constexpr double kTau = 6.28318530717958647692;
    auto angle = std::atan2(velocity.y, velocity.x);
    if (angle < 0.0) {
        angle += kTau;
    }
    const auto bin = static_cast<std::size_t>(std::floor(
        angle / (kTau / static_cast<double>(kScenarioCrossFlowDirectionBinCount))));
    return std::min(kScenarioCrossFlowDirectionBinCount - 1, bin);
}

std::array<Point2D, kScenarioCrossFlowDirectionBinCount> makeCrossFlowDirectionVectors() {
    constexpr double kTau = 6.28318530717958647692;
    std::array<Point2D, kScenarioCrossFlowDirectionBinCount> vectors{};
    for (std::size_t index = 0; index < vectors.size(); ++index) {
        const auto angle = ((static_cast<double>(index) + 0.5)
            / static_cast<double>(kScenarioCrossFlowDirectionBinCount)) * kTau;
        vectors[index] = {
            .x = std::cos(angle),
            .y = std::sin(angle),
        };
    }
    return vectors;
}

double crossFlowDirectionCosine(std::size_t lhs, std::size_t rhs) {
    static const auto directions = makeCrossFlowDirectionVectors();
    return dot(directions[lhs], directions[rhs]);
}

double crossFlowSpeedDropRatio(double averageSpeed) {
    return std::clamp(
        1.0 - (averageSpeed / std::max(1e-9, kScenarioCrossFlowReferenceSpeedMetersPerSecond)),
        0.0,
        1.0);
}

double crossFlowScore(double crossFlowRatio, double averageSpeed) {
    const auto speedDropRatio = crossFlowSpeedDropRatio(averageSpeed);
    return std::clamp((crossFlowRatio * 0.65) + (speedDropRatio * 0.35), 0.0, 1.0);
}

std::optional<CrossFlowObservation> detectCrossFlow(
    const std::array<std::size_t, kScenarioCrossFlowDirectionBinCount>& directionCounts,
    std::size_t movingAgentCount,
    double averageSpeed) {
    if (movingAgentCount < kScenarioCrossFlowMinMovingAgents) {
        return std::nullopt;
    }
    if (averageSpeed > kScenarioCrossFlowMinimumExpectedSpeedMetersPerSecond + kCrossFlowDirectionEpsilon) {
        return std::nullopt;
    }

    CrossFlowObservation best;
    best.averageSpeed = averageSpeed;
    best.movingAgentCount = movingAgentCount;
    for (std::size_t anchorBin = 0; anchorBin < kScenarioCrossFlowDirectionBinCount; ++anchorBin) {
        if (directionCounts[anchorBin] == 0) {
            continue;
        }

        CrossFlowObservation candidate;
        candidate.averageSpeed = averageSpeed;
        candidate.movingAgentCount = movingAgentCount;
        for (std::size_t bin = 0; bin < kScenarioCrossFlowDirectionBinCount; ++bin) {
            if (directionCounts[bin] == 0) {
                continue;
            }
            const auto cosine = crossFlowDirectionCosine(anchorBin, bin);
            if (cosine > kScenarioCrossFlowCosineThreshold) {
                candidate.primaryFlowCount += directionCounts[bin];
            } else {
                candidate.crossFlowCount += directionCounts[bin];
            }
        }

        const auto primaryRatio = static_cast<double>(candidate.primaryFlowCount)
            / static_cast<double>(candidate.movingAgentCount);
        const auto crossRatio = static_cast<double>(candidate.crossFlowCount)
            / static_cast<double>(candidate.movingAgentCount);
        if (primaryRatio < kScenarioCrossFlowSideRatioThreshold
            || crossRatio < kScenarioCrossFlowSideRatioThreshold) {
            continue;
        }

        candidate.crossFlowRatio = std::min(primaryRatio, crossRatio);
        candidate.speedDropRatio = crossFlowSpeedDropRatio(averageSpeed);
        candidate.crossFlowScore = crossFlowScore(candidate.crossFlowRatio, averageSpeed);
        if (candidate.crossFlowScore > best.crossFlowScore
            || (std::fabs(candidate.crossFlowScore - best.crossFlowScore) <= 1e-9
                && candidate.primaryFlowCount + candidate.crossFlowCount > best.primaryFlowCount + best.crossFlowCount)) {
            best = candidate;
        }
    }

    return best.crossFlowScore <= 0.0 ? std::nullopt : std::optional<CrossFlowObservation>{best};
}

double pressureFeedbackLevel(double compressionForce, double exposureSeconds, bool critical) {
    if (critical) {
        return 1.0;
    }

    const auto forceRange =
        std::max(1e-9, kScenarioCriticalPressureForceThreshold - kScenarioPressureFeedbackForceThreshold);
    const auto forceLevel = compressionForce <= kScenarioPressureFeedbackForceThreshold
        ? 0.0
        : std::clamp(
            (compressionForce - kScenarioPressureFeedbackForceThreshold) / forceRange,
            0.0,
            1.0);
    const auto exposureLevel = kScenarioCriticalPressureExposureThresholdSeconds <= 1e-9
        ? 0.0
        : std::clamp(
            exposureSeconds / kScenarioCriticalPressureExposureThresholdSeconds,
            0.0,
            1.0);
    return std::clamp(std::max(forceLevel, exposureLevel * 0.7), 0.0, 1.0);
}

double pressureFeedbackProbeRadius(double radius) {
    return std::max(
        kScenarioPressureFeedbackNeighborProbeRadius,
        (radius * 2.0) + kPersonalSpaceBuffer);
}

std::uint64_t pressureFeedbackUpdateDivisor(
    bool previouslyExposed,
    bool previouslyCritical,
    std::size_t nearbyOtherCount) {
    if (previouslyCritical
        || nearbyOtherCount >= kScenarioPressureFeedbackDenseNeighborThreshold) {
        return 1U;
    }
    if (previouslyExposed || nearbyOtherCount > 0) {
        return kScenarioPressureFeedbackCrowdedUpdateDivisor;
    }
    return kScenarioPressureFeedbackQuietUpdateDivisor;
}

ScenarioRiskLevel completionRiskLevel(
    const ScenarioSimulationClockResource& clock,
    std::size_t totalAgentCount,
    std::size_t evacuatedAgentCount,
    std::size_t stalledAgentCount,
    std::size_t hotspotCount,
    std::size_t pressureHotspotCount,
    std::size_t criticalPressureAgentCount,
    std::size_t criticalPressureEventCount,
    std::size_t bottleneckCount,
    std::size_t crossFlowCellCount,
    double peakCrossFlowScore) {
    if (totalAgentCount == 0 || evacuatedAgentCount >= totalAgentCount) {
        return ScenarioRiskLevel::Low;
    }

    const auto elapsedRatio = clock.timeLimitSeconds > 0.0
        ? clock.elapsedSeconds / clock.timeLimitSeconds
        : 0.0;
    const auto activeAgentCount = totalAgentCount - evacuatedAgentCount;
    const auto stalledRatio = activeAgentCount > 0
        ? static_cast<double>(stalledAgentCount) / static_cast<double>(activeAgentCount)
        : 0.0;

    if (elapsedRatio >= 0.8
        || stalledRatio >= 0.35
        || criticalPressureEventCount > 0
        || bottleneckCount >= 2
        || peakCrossFlowScore >= 0.75) {
        return ScenarioRiskLevel::High;
    }
    if (elapsedRatio >= 0.5
        || stalledRatio >= 0.15
        || hotspotCount > 0
        || pressureHotspotCount > 0
        || criticalPressureAgentCount > 0
        || bottleneckCount > 0
        || crossFlowCellCount > 0
        || peakCrossFlowScore >= 0.45) {
        return ScenarioRiskLevel::Medium;
    }
    return ScenarioRiskLevel::Low;
}

SimulationFrame captureSimulationFrame(
    engine::WorldQuery& query,
    const ScenarioSimulationClockResource& clock) {
    SimulationFrame frame;
    frame.elapsedSeconds = clock.elapsedSeconds;
    frame.complete = clock.complete;

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
        const auto* route = query.contains<EvacuationRoute>(entity) ? &query.get<EvacuationRoute>(entity) : nullptr;
        frame.agents.push_back({
            .id = entity.index,
            .position = position.value,
            .velocity = velocity.value,
            .radius = agent.radius,
            .floorId = route != nullptr
                ? (!route->displayFloorId.empty()
                    ? route->displayFloorId
                    : route->currentFloorId)
                : std::string{},
            .stalled = route != nullptr && isStalled(velocity, *route),
        });
    }
    return frame;
}

class ScenarioPressureFeedbackSystem final : public engine::EngineSystem {
public:
    explicit ScenarioPressureFeedbackSystem(FacilityLayout2D layout)
        : layoutCache_(buildScenarioLayoutCache(std::move(layout))) {
    }

    void configure(engine::EngineWorld& world) override {
        if (layoutCache_.has_value() && !world.resources().contains<ScenarioLayoutCacheResource>()) {
            world.resources().set(*layoutCache_);
        }
        if (!world.resources().contains<ScenarioPressureFeedbackResource>()) {
            world.resources().set(ScenarioPressureFeedbackResource{});
        }
    }

    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override {
        (void)step;

        auto& resources = world.resources();
        if (!resources.contains<ScenarioSimulationStepResource>()) {
            return;
        }
        if (!resources.contains<ScenarioLayoutCacheResource>()) {
            return;
        }

        auto& feedback = resources.get<ScenarioPressureFeedbackResource>();
        const auto& clock = resources.get<ScenarioSimulationClockResource>();
        if (clock.complete) {
            feedback = ScenarioPressureFeedbackResource{};
            return;
        }

        const auto deltaSeconds = std::max(0.0, resources.get<ScenarioSimulationStepResource>().deltaSeconds);
        auto& query = world.query();
        const auto entities = query.view<Position, Agent, EvacuationRoute, EvacuationStatus>();
        const auto& activeLayout = resources.get<ScenarioLayoutCacheResource>().layout;
        const auto* spatialIndex = resources.contains<ScenarioAgentSpatialIndexResource>()
            ? &resources.get<ScenarioAgentSpatialIndexResource>()
            : nullptr;

        std::vector<ActivePressureFeedbackContext> activeAgents;
        activeAgents.reserve(entities.size());
        for (const auto entity : entities) {
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            const auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
            const auto& route = query.get<EvacuationRoute>(entity);
            activeAgents.push_back({
                .entity = entity,
                .agentId = entity.index,
                .position = position.value,
                .displayFloorId = agentDisplayFloorId(route),
                .collisionFloorId = agentCollisionFloorId(route),
                .radius = static_cast<double>(agent.radius),
            });
        }

        auto previousStates = std::move(feedback.agentsById);
        feedback.agentsById.clear();
        feedback.exposedAgentCount = 0;
        feedback.criticalAgentCount = 0;

        std::vector<double> forces(activeAgents.size(), 0.0);
        std::vector<bool> recomputed(activeAgents.size(), true);
        if (spatialIndex == nullptr) {
            for (std::size_t lhsIndex = 0; lhsIndex < activeAgents.size(); ++lhsIndex) {
                for (std::size_t rhsIndex = lhsIndex + 1; rhsIndex < activeAgents.size(); ++rhsIndex) {
                    if (activeAgents[lhsIndex].collisionFloorId != activeAgents[rhsIndex].collisionFloorId) {
                        continue;
                    }

                    const auto distance =
                        distanceBetween(activeAgents[lhsIndex].position, activeAgents[rhsIndex].position);
                    const auto combinedRadius = activeAgents[lhsIndex].radius + activeAgents[rhsIndex].radius;
                    if (distance >= combinedRadius) {
                        continue;
                    }

                    const auto overlap = combinedRadius - distance;
                    forces[lhsIndex] += overlap;
                    forces[rhsIndex] += overlap;
                }
            }

            for (std::size_t index = 0; index < activeAgents.size(); ++index) {
                for (const auto& barrier : activeLayout.barriers) {
                    if (!barrierMatchesFloor(barrier, activeAgents[index].collisionFloorId)) {
                        continue;
                    }
                    forces[index] += barrierCompression(
                        barrier,
                        activeAgents[index].position,
                        activeAgents[index].radius);
                }
            }
        } else {
            std::vector<std::vector<engine::Entity>> neighborCandidates(activeAgents.size());
            std::vector<std::size_t> nearbyOtherCounts(activeAgents.size(), 0);
            for (std::size_t index = 0; index < activeAgents.size(); ++index) {
                neighborCandidates[index] = scenarioNearbyAgents(
                    query,
                    *spatialIndex,
                    activeAgents[index].position,
                    activeAgents[index].collisionFloorId,
                    pressureFeedbackProbeRadius(activeAgents[index].radius));
                nearbyOtherCounts[index] = static_cast<std::size_t>(std::count_if(
                    neighborCandidates[index].begin(),
                    neighborCandidates[index].end(),
                    [&](const auto candidate) {
                        return candidate != activeAgents[index].entity;
                    }));
            }

            for (std::size_t index = 0; index < activeAgents.size(); ++index) {
                const auto previousIt = previousStates.find(activeAgents[index].agentId);
                const bool previouslyExposed =
                    previousIt != previousStates.end() && previousIt->second.exposed;
                const bool previouslyCritical =
                    previousIt != previousStates.end() && previousIt->second.critical;
                const auto updateDivisor = pressureFeedbackUpdateDivisor(
                    previouslyExposed,
                    previouslyCritical,
                    nearbyOtherCounts[index]);
                recomputed[index] = updateDivisor <= 1U
                    || ((step.frameIndex + activeAgents[index].agentId) % updateDivisor) == 0U;
                if (!recomputed[index]) {
                    continue;
                }

                for (const auto otherEntity : neighborCandidates[index]) {
                    if (otherEntity == activeAgents[index].entity) {
                        continue;
                    }
                    const auto& otherPosition = query.get<Position>(otherEntity);
                    const auto& otherAgent = query.get<Agent>(otherEntity);
                    const auto distance =
                        distanceBetween(activeAgents[index].position, otherPosition.value);
                    const auto combinedRadius =
                        activeAgents[index].radius + static_cast<double>(otherAgent.radius);
                    if (distance >= combinedRadius) {
                        continue;
                    }

                    forces[index] += combinedRadius - distance;
                }

                for (const auto* barrier : scenarioNearbyBarriers(
                         activeLayout,
                         *spatialIndex,
                         activeAgents[index].position,
                         activeAgents[index].collisionFloorId,
                         activeAgents[index].radius)) {
                    if (barrier == nullptr) {
                        continue;
                    }
                    forces[index] += barrierCompression(
                        *barrier,
                        activeAgents[index].position,
                        activeAgents[index].radius);
                }
            }
        }

        for (std::size_t index = 0; index < activeAgents.size(); ++index) {
            const auto& context = activeAgents[index];
            ScenarioPressureFeedbackAgentState state{
                .agentId = context.agentId,
                .position = context.position,
                .floorId = context.displayFloorId,
            };
            if (const auto previousIt = previousStates.find(context.agentId);
                previousIt != previousStates.end()) {
                state.exposureSeconds = previousIt->second.exposureSeconds;
            }

            bool reusedPreviousForce = false;
            if (recomputed[index]) {
                state.compressionForce = forces[index];
            } else if (const auto previousIt = previousStates.find(context.agentId);
                       previousIt != previousStates.end()) {
                state.compressionForce = previousIt->second.compressionForce;
                reusedPreviousForce = true;
            }

            if ((recomputed[index] || reusedPreviousForce)
                && state.compressionForce > kScenarioPressureFeedbackForceThreshold) {
                state.exposureSeconds += deltaSeconds;
            } else if (state.exposureSeconds > 0.0) {
                state.exposureSeconds = std::max(
                    0.0,
                    state.exposureSeconds - (deltaSeconds * kScenarioPressureFeedbackExposureRecoveryPerSecond));
            }

            state.critical =
                state.compressionForce > kScenarioCriticalPressureForceThreshold
                && state.exposureSeconds >= kScenarioCriticalPressureExposureThresholdSeconds;
            state.exposed =
                state.compressionForce > kScenarioPressureFeedbackForceThreshold
                || state.exposureSeconds > 1e-9;
            if (!state.exposed) {
                continue;
            }

            state.feedbackLevel =
                pressureFeedbackLevel(state.compressionForce, state.exposureSeconds, state.critical);
            const auto slowdown =
                (state.critical
                        ? kScenarioPressureFeedbackMaxCriticalSlowdown
                        : kScenarioPressureFeedbackMaxExposedSlowdown)
                * state.feedbackLevel;
            state.speedFactor = std::clamp(
                1.0 - slowdown,
                1.0 - kScenarioPressureFeedbackMaxCriticalSlowdown,
                1.0);
            state.avoidanceScale =
                1.0 + (state.feedbackLevel * (kScenarioPressureFeedbackMaxAvoidanceScale - 1.0));
            state.barrierScale =
                1.0 + (state.feedbackLevel * (kScenarioPressureFeedbackMaxBarrierScale - 1.0));

            if (state.exposed) {
                ++feedback.exposedAgentCount;
            }
            if (state.critical) {
                ++feedback.criticalAgentCount;
            }
            feedback.agentsById.emplace(context.agentId, std::move(state));
        }
    }

private:
    std::optional<ScenarioLayoutCacheResource> layoutCache_{};
};

class ScenarioRiskMetricsSystem final : public engine::EngineSystem {
public:
    explicit ScenarioRiskMetricsSystem(FacilityLayout2D layout)
        : layout_(std::move(layout)) {
    }

    void configure(engine::EngineWorld& world) override {
        world.resources().set(ScenarioRiskMetricsResource{});
        world.resources().set(ScenarioPressureTrackingResource{});
        world.resources().set(ScenarioCrossFlowResource{});
    }

    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override {
        (void)step;

        auto& query = world.query();
        auto& resources = world.resources();
        const auto& activeLayout = resources.contains<ScenarioLayoutCacheResource>()
            ? resources.get<ScenarioLayoutCacheResource>().layout
            : layout_;
        ScenarioRiskSnapshot snapshot;
        const auto entities = query.view<Position, Agent, Velocity, EvacuationRoute, EvacuationStatus>();

        std::size_t totalAgentCount = 0;
        std::size_t evacuatedAgentCount = 0;
        std::vector<ActiveAgentContext> activeAgents;
        std::unordered_map<long long, RiskCellAccumulator> cells;
        cells.reserve(entities.size());

        for (const auto entity : entities) {
            ++totalAgentCount;
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                ++evacuatedAgentCount;
                continue;
            }

            const auto& position = query.get<Position>(entity);
            const auto& velocity = query.get<Velocity>(entity);
            const auto& route = query.get<EvacuationRoute>(entity);
            const auto stalled = isStalled(velocity, route);
            if (stalled) {
                ++snapshot.stalledAgentCount;
            }

            const auto floorId = agentDisplayFloorId(route);
            activeAgents.push_back({
                .entity = entity,
                .agentId = entity.index,
                .position = position.value,
                .velocity = velocity.value,
                .floorId = floorId,
                .radius = static_cast<double>(query.get<Agent>(entity).radius),
                .stalled = stalled,
            });
            const auto address = riskCellAddress(position.value, floorId);
            auto& cell = cells[riskCellKey(address)];
            if (cell.agentCount == 0) {
                cell.cellMin = riskCellMin(address);
                cell.cellMax = riskCellMax(address);
                cell.floorId = address.floorId;
            }
            cell.positionSum = cell.positionSum + position.value;
            ++cell.agentCount;
            cell.entities.push_back(entity);
        }

        std::optional<ScenarioAgentSpatialIndexResource> fallbackPressureIndex;
        const auto* pressureIndex = resources.contains<ScenarioAgentSpatialIndexResource>()
            ? &resources.get<ScenarioAgentSpatialIndexResource>()
            : nullptr;
        if (pressureIndex == nullptr) {
            fallbackPressureIndex = buildDisplayFloorPressureIndex(activeAgents, activeLayout);
            pressureIndex = &*fallbackPressureIndex;
        }

        ScenarioSimulationClockResource clock;
        if (resources.contains<ScenarioSimulationClockResource>()) {
            clock = resources.get<ScenarioSimulationClockResource>();
        }
        auto& pressureTracking = resources.get<ScenarioPressureTrackingResource>();
        const auto deltaSeconds = updatePressureTracking(
            snapshot,
            activeLayout,
            activeAgents,
            *pressureIndex,
            clock.elapsedSeconds,
            pressureTracking);

        collectHotspots(snapshot, cells);
        collectPressureHotspots(snapshot, query, cells);
        collectCriticalPressureEvents(snapshot, cells, clock.elapsedSeconds, deltaSeconds, pressureTracking);
        collectBottlenecks(snapshot, activeAgents, *pressureIndex, activeLayout);
        collectCrossFlows(
            snapshot,
            activeAgents,
            clock,
            deltaSeconds,
            resources.get<ScenarioCrossFlowResource>());

        snapshot.completionRisk = completionRiskLevel(
            clock,
            totalAgentCount,
            evacuatedAgentCount,
            snapshot.stalledAgentCount,
            snapshot.hotspots.size(),
            snapshot.pressureHotspots.size(),
            snapshot.criticalPressureAgentCount,
            snapshot.criticalPressureEvents.size(),
            snapshot.bottlenecks.size(),
            snapshot.crossFlowCells.size(),
            snapshot.peakCrossFlowScore);
        if (!snapshot.hotspots.empty()
            || !snapshot.pressureHotspots.empty()
            || !snapshot.criticalPressureEvents.empty()
            || !snapshot.bottlenecks.empty()
            || !snapshot.crossFlowCells.empty()) {
            attachDetectionState(snapshot, captureSimulationFrame(query, clock), clock.elapsedSeconds);
        }

        auto peakSnapshot = resources.contains<ScenarioRiskMetricsResource>()
            ? resources.get<ScenarioRiskMetricsResource>().peakSnapshot
            : ScenarioRiskSnapshot{};
        mergePeakSnapshot(peakSnapshot, snapshot);
        resources.set(ScenarioRiskMetricsResource{
            .snapshot = std::move(snapshot),
            .peakSnapshot = std::move(peakSnapshot),
        });
    }

private:
    void mergePeakSnapshot(ScenarioRiskSnapshot& peak, const ScenarioRiskSnapshot& current) const {
        if (riskSeverity(current.completionRisk) > riskSeverity(peak.completionRisk)) {
            peak.completionRisk = current.completionRisk;
        }
        peak.stalledAgentCount = std::max(peak.stalledAgentCount, current.stalledAgentCount);
        peak.pressureExposedAgentCount = std::max(peak.pressureExposedAgentCount, current.pressureExposedAgentCount);
        peak.criticalPressureAgentCount = std::max(peak.criticalPressureAgentCount, current.criticalPressureAgentCount);
        peak.crossFlowAgentCount = std::max(peak.crossFlowAgentCount, current.crossFlowAgentCount);
        peak.peakCrossFlowScore = std::max(peak.peakCrossFlowScore, current.peakCrossFlowScore);
        peak.totalCrossFlowExposureAgentSeconds = std::max(
            peak.totalCrossFlowExposureAgentSeconds,
            current.totalCrossFlowExposureAgentSeconds);
        if (isHotspotSetWorse(current.hotspots, peak.hotspots)) {
            peak.hotspots = current.hotspots;
        }
        if (isPressureHotspotSetWorse(current.pressureHotspots, peak.pressureHotspots)) {
            peak.pressureHotspots = current.pressureHotspots;
        }
        if (isPressureAgentSetWorse(current.pressureAgents, peak.pressureAgents)) {
            peak.pressureAgents = current.pressureAgents;
        }
        if (isCriticalPressureEventSetWorse(current.criticalPressureEvents, peak.criticalPressureEvents)) {
            peak.criticalPressureEvents = current.criticalPressureEvents;
        }
        if (isBottleneckSetWorse(current.bottlenecks, peak.bottlenecks)) {
            peak.bottlenecks = current.bottlenecks;
        }
        if (isCrossFlowCellSetWorse(
                current.crossFlowCells,
                peak.crossFlowCells)) {
            peak.crossFlowCells = current.crossFlowCells;
        }
    }

    void attachDetectionState(
        ScenarioRiskSnapshot& snapshot,
        const SimulationFrame& frame,
        double elapsedSeconds) const {
        for (auto& hotspot : snapshot.hotspots) {
            hotspot.detectedAtSeconds = elapsedSeconds;
            hotspot.detectionFrame = frame;
        }
        for (auto& hotspot : snapshot.pressureHotspots) {
            hotspot.detectedAtSeconds = elapsedSeconds;
            hotspot.detectionFrame = frame;
        }
        for (auto& event : snapshot.criticalPressureEvents) {
            event.detectedAtSeconds = elapsedSeconds;
            event.detectionFrame = frame;
        }
        for (auto& bottleneck : snapshot.bottlenecks) {
            bottleneck.detectedAtSeconds = elapsedSeconds;
            bottleneck.detectionFrame = frame;
        }
        for (auto& cell : snapshot.crossFlowCells) {
            cell.detectedAtSeconds = elapsedSeconds;
            cell.detectionFrame = frame;
        }
    }

    double updatePressureTracking(
        ScenarioRiskSnapshot& snapshot,
        const FacilityLayout2D& layout,
        const std::vector<ActiveAgentContext>& activeAgents,
        const ScenarioAgentSpatialIndexResource& spatialIndex,
        double elapsedSeconds,
        ScenarioPressureTrackingResource& tracking) const {
        const auto deltaSeconds = tracking.hasPreviousElapsedSeconds
            ? std::max(0.0, elapsedSeconds - tracking.previousElapsedSeconds)
            : 0.0;
        tracking.previousElapsedSeconds = elapsedSeconds;
        tracking.hasPreviousElapsedSeconds = true;

        std::unordered_map<std::uint64_t, std::size_t> activeAgentIndices;
        activeAgentIndices.reserve(activeAgents.size());
        for (std::size_t index = 0; index < activeAgents.size(); ++index) {
            activeAgentIndices.emplace(activeAgents[index].agentId, index);
        }

        std::vector<double> interactionForces(activeAgents.size(), 0.0);
        const auto probeRange = std::max(1, static_cast<int>(std::ceil(
            kPressureReferenceDistanceMeters / std::max(0.1, spatialIndex.cellSize))));
        for (std::size_t lhsIndex = 0; lhsIndex < activeAgents.size(); ++lhsIndex) {
            const auto& lhsAgent = activeAgents[lhsIndex];
            const auto floorIt = spatialIndex.displayCellsByFloor.find(lhsAgent.floorId);
            if (floorIt == spatialIndex.displayCellsByFloor.end()) {
                continue;
            }

            const auto centerCell = spatialCellFor(lhsAgent.position, spatialIndex.cellSize);
            for (int dy = -probeRange; dy <= probeRange; ++dy) {
                for (int dx = -probeRange; dx <= probeRange; ++dx) {
                    const auto cellIt = floorIt->second.find(spatialKey({
                        .x = centerCell.x + dx,
                        .y = centerCell.y + dy,
                    }));
                    if (cellIt == floorIt->second.end()) {
                        continue;
                    }

                    for (const auto rhsEntity : cellIt->second) {
                        const auto rhsIndexIt = activeAgentIndices.find(rhsEntity.index);
                        if (rhsIndexIt == activeAgentIndices.end()) {
                            continue;
                        }
                        const auto rhsIndex = rhsIndexIt->second;
                        if (rhsIndex <= lhsIndex) {
                            continue;
                        }

                        const auto& rhsAgent = activeAgents[rhsIndex];
                        const auto distance = distanceBetween(lhsAgent.position, rhsAgent.position);
                        const auto interactionScore = occupantInteractionPressureScore(
                            distance,
                            lhsAgent.radius,
                            rhsAgent.radius);
                        if (interactionScore <= 0.0) {
                            continue;
                        }

                        interactionForces[lhsIndex] += interactionScore;
                        interactionForces[rhsIndex] += interactionScore;
                    }
                }
            }
        }

        std::vector<double> forces(activeAgents.size(), 0.0);
        for (std::size_t index = 0; index < activeAgents.size(); ++index) {
            forces[index] = interactionForces[index];
            for (const auto* barrier : scenarioNearbyBarriers(
                     layout,
                     spatialIndex,
                     activeAgents[index].position,
                     activeAgents[index].floorId,
                     activeAgents[index].radius + kPersonalSpaceBuffer)) {
                if (barrier == nullptr) {
                    continue;
                }
                forces[index] = std::max(
                    forces[index],
                    interactionForces[index] + barrierCompression(
                        *barrier,
                        activeAgents[index].position,
                        activeAgents[index].radius + kPersonalSpaceBuffer));
            }
        }

        std::unordered_set<std::uint64_t> activeAgentIds;
        activeAgentIds.reserve(activeAgents.size());
        snapshot.pressureAgents.reserve(activeAgents.size());
        for (std::size_t index = 0; index < activeAgents.size(); ++index) {
            const auto& context = activeAgents[index];
            activeAgentIds.insert(context.agentId);
            auto& state = tracking.agentStates[context.agentId];
            state.currentForce = forces[index];
            if (state.currentForce >= kScenarioCriticalPressureForceThreshold) {
                state.exposureSeconds += deltaSeconds;
            }

            const bool critical =
                state.currentForce >= kScenarioCriticalPressureForceThreshold
                && state.exposureSeconds >= kScenarioCriticalPressureExposureThresholdSeconds;
            if (state.exposureSeconds > 0.0 || state.currentForce >= kScenarioCriticalPressureForceThreshold) {
                ++snapshot.pressureExposedAgentCount;
                snapshot.pressureAgents.push_back({
                    .agentId = context.agentId,
                    .position = context.position,
                    .floorId = context.floorId,
                    .compressionForce = state.currentForce,
                    .exposureSeconds = state.exposureSeconds,
                    .critical = critical,
                });
            }
            if (critical) {
                ++snapshot.criticalPressureAgentCount;
            }
        }

        for (auto it = tracking.agentStates.begin(); it != tracking.agentStates.end();) {
            if (activeAgentIds.contains(it->first)) {
                ++it;
                continue;
            }
            it = tracking.agentStates.erase(it);
        }

        sortAndTrimTop(snapshot.pressureAgents, kMaxReportedPressureAgents, [](const auto& lhs, const auto& rhs) {
            if (lhs.critical != rhs.critical) {
                return lhs.critical;
            }
            if (std::fabs(lhs.exposureSeconds - rhs.exposureSeconds) > 1e-9) {
                return lhs.exposureSeconds > rhs.exposureSeconds;
            }
            return lhs.compressionForce > rhs.compressionForce;
        });

        return deltaSeconds;
    }

    void collectHotspots(
        ScenarioRiskSnapshot& snapshot,
        const std::unordered_map<long long, RiskCellAccumulator>& cells) const {
        snapshot.hotspots.reserve(cells.size());
        const auto hotspotCellArea = kScenarioHotspotCellSize * kScenarioHotspotCellSize;
        for (const auto& [_, cell] : cells) {
            const auto densityPeoplePerSquareMeter = hotspotCellArea <= 0.0
                ? 0.0
                : static_cast<double>(cell.agentCount) / hotspotCellArea;
            if (densityPeoplePerSquareMeter < kScenarioHotspotDensityThresholdPeoplePerSquareMeter) {
                continue;
            }
            const auto count = static_cast<double>(cell.agentCount);
            snapshot.hotspots.push_back({
                .center = {.x = cell.positionSum.x / count, .y = cell.positionSum.y / count},
                .cellMin = cell.cellMin,
                .cellMax = cell.cellMax,
                .floorId = cell.floorId,
                .agentCount = cell.agentCount,
            });
        }

        sortAndTrimTop(snapshot.hotspots, kMaxReportedHotspots, [](const auto& lhs, const auto& rhs) {
            return lhs.agentCount > rhs.agentCount;
        });
    }

    void collectPressureHotspots(
        ScenarioRiskSnapshot& snapshot,
        engine::WorldQuery& query,
        const std::unordered_map<long long, RiskCellAccumulator>& cells) const {
        snapshot.pressureHotspots.reserve(cells.size());
        const auto cellArea = kScenarioHotspotCellSize * kScenarioHotspotCellSize;

        for (const auto& [_, cell] : cells) {
            const auto densityPeoplePerSquareMeter = cellArea <= 1e-9
                ? 0.0
                : static_cast<double>(cell.agentCount) / cellArea;
            double pressureScore = 0.0;
            std::size_t intrudingPairCount = 0;
            for (std::size_t lhsIndex = 0; lhsIndex < cell.entities.size(); ++lhsIndex) {
                const auto lhsEntity = cell.entities[lhsIndex];
                const auto& lhsPosition = query.get<Position>(lhsEntity);
                const auto& lhsAgent = query.get<Agent>(lhsEntity);
                for (std::size_t rhsIndex = lhsIndex + 1; rhsIndex < cell.entities.size(); ++rhsIndex) {
                    const auto rhsEntity = cell.entities[rhsIndex];
                    const auto& rhsPosition = query.get<Position>(rhsEntity);
                    const auto& rhsAgent = query.get<Agent>(rhsEntity);
                    const auto distance = distanceBetween(lhsPosition.value, rhsPosition.value);
                    const auto interactionScore = occupantInteractionPressureScore(
                        distance,
                        static_cast<double>(lhsAgent.radius),
                        static_cast<double>(rhsAgent.radius));
                    if (interactionScore <= 0.0) {
                        continue;
                    }

                    pressureScore += interactionScore;
                    ++intrudingPairCount;
                }
            }

            if (pressureScore < kScenarioPressureScoreThreshold || intrudingPairCount == 0) {
                continue;
            }

            const auto count = static_cast<double>(cell.agentCount);
            snapshot.pressureHotspots.push_back({
                .center = {.x = cell.positionSum.x / count, .y = cell.positionSum.y / count},
                .cellMin = cell.cellMin,
                .cellMax = cell.cellMax,
                .floorId = cell.floorId,
                .agentCount = cell.agentCount,
                .intrudingPairCount = intrudingPairCount,
                .densityPeoplePerSquareMeter = densityPeoplePerSquareMeter,
                .pressureScore = pressureScore,
            });
        }

        sortAndTrimTop(snapshot.pressureHotspots, kMaxReportedPressureHotspots, [](const auto& lhs, const auto& rhs) {
            if (std::fabs(lhs.pressureScore - rhs.pressureScore) > 1e-9) {
                return lhs.pressureScore > rhs.pressureScore;
            }
            if (lhs.intrudingPairCount != rhs.intrudingPairCount) {
                return lhs.intrudingPairCount > rhs.intrudingPairCount;
            }
            return lhs.agentCount > rhs.agentCount;
        });
    }

    void collectCriticalPressureEvents(
        ScenarioRiskSnapshot& snapshot,
        const std::unordered_map<long long, RiskCellAccumulator>& cells,
        double elapsedSeconds,
        double deltaSeconds,
        ScenarioPressureTrackingResource& tracking) const {
        std::unordered_set<long long> candidateEventKeys;
        candidateEventKeys.reserve(cells.size());

        for (const auto& [cellKey, cell] : cells) {
            std::size_t exposedAgentCount = 0;
            std::size_t criticalAgentCount = 0;
            double pressureScore = 0.0;

            for (const auto entity : cell.entities) {
                const auto stateIt = tracking.agentStates.find(entity.index);
                if (stateIt == tracking.agentStates.end()) {
                    continue;
                }
                const auto& state = stateIt->second;
                const bool exposed =
                    state.exposureSeconds > 0.0
                    || state.currentForce >= kScenarioCriticalPressureForceThreshold;
                const bool critical =
                    state.currentForce >= kScenarioCriticalPressureForceThreshold
                    && state.exposureSeconds >= kScenarioCriticalPressureExposureThresholdSeconds;
                if (exposed) {
                    ++exposedAgentCount;
                }
                if (critical) {
                    ++criticalAgentCount;
                }
                pressureScore += state.currentForce;
            }

            if (criticalAgentCount < kScenarioCriticalPressureEventAgentThreshold) {
                continue;
            }

            candidateEventKeys.insert(cellKey);
            auto [eventIt, inserted] = tracking.activeEvents.try_emplace(
                cellKey,
                ScenarioActiveCriticalPressureEventState{.startedAtSeconds = elapsedSeconds});
            if (inserted) {
                eventIt->second.startedAtSeconds = elapsedSeconds;
            }

            const auto durationSeconds = std::max(
                0.0,
                (elapsedSeconds - eventIt->second.startedAtSeconds) + deltaSeconds);
            if (durationSeconds < kScenarioCriticalPressureEventDurationThresholdSeconds) {
                continue;
            }

            const auto count = static_cast<double>(cell.agentCount);
            snapshot.criticalPressureEvents.push_back({
                .center = count <= 0.0
                    ? Point2D{}
                    : Point2D{.x = cell.positionSum.x / count, .y = cell.positionSum.y / count},
                .cellMin = cell.cellMin,
                .cellMax = cell.cellMax,
                .floorId = cell.floorId,
                .exposedAgentCount = exposedAgentCount,
                .criticalAgentCount = criticalAgentCount,
                .pressureScore = pressureScore,
                .startedAtSeconds = eventIt->second.startedAtSeconds,
                .durationSeconds = durationSeconds,
            });
        }

        for (auto it = tracking.activeEvents.begin(); it != tracking.activeEvents.end();) {
            if (candidateEventKeys.contains(it->first)) {
                ++it;
                continue;
            }
            it = tracking.activeEvents.erase(it);
        }

        sortAndTrimTop(snapshot.criticalPressureEvents, kMaxReportedCriticalPressureEvents, [](const auto& lhs, const auto& rhs) {
            if (lhs.criticalAgentCount != rhs.criticalAgentCount) {
                return lhs.criticalAgentCount > rhs.criticalAgentCount;
            }
            if (std::fabs(lhs.durationSeconds - rhs.durationSeconds) > 1e-9) {
                return lhs.durationSeconds > rhs.durationSeconds;
            }
            return lhs.pressureScore > rhs.pressureScore;
        });
    }

    std::string zoneDisplayName(const FacilityLayout2D& layout, const std::string& zoneId) const {
        const auto* zone = findZone(layout, zoneId);
        if (zone == nullptr) {
            return zoneId;
        }
        return zone->label.empty() ? zone->id : zone->label;
    }

    std::string connectionLabel(const FacilityLayout2D& layout, const Connection2D& connection) const {
        const auto from = zoneDisplayName(layout, connection.fromZoneId);
        const auto to = zoneDisplayName(layout, connection.toZoneId);
        if (!from.empty() && !to.empty()) {
            return from + " -> " + to;
        }
        return connection.id;
    }

    std::string connectionFloorId(const FacilityLayout2D& layout, const Connection2D& connection) const {
        if (!connection.floorId.empty()) {
            return connection.floorId;
        }
        if (const auto* fromZone = findZone(layout, connection.fromZoneId); fromZone != nullptr && !fromZone->floorId.empty()) {
            return fromZone->floorId;
        }
        if (const auto* toZone = findZone(layout, connection.toZoneId); toZone != nullptr) {
            return toZone->floorId;
        }
        return {};
    }

    void collectBottlenecks(
        ScenarioRiskSnapshot& snapshot,
        const std::vector<ActiveAgentContext>& activeAgents,
        const ScenarioAgentSpatialIndexResource& spatialIndex,
        const FacilityLayout2D& layout) const {
        std::unordered_map<std::uint64_t, std::size_t> activeAgentIndices;
        activeAgentIndices.reserve(activeAgents.size());
        for (std::size_t index = 0; index < activeAgents.size(); ++index) {
            activeAgentIndices.emplace(activeAgents[index].agentId, index);
        }

        auto nearbyEntitiesForConnection = [&](const std::string& floorId, const LineSegment2D& passage) {
            std::vector<engine::Entity> candidates;
            std::unordered_set<std::uint64_t> seen;
            const auto appendFloor = [&](const std::string& candidateFloorId) {
                const auto floorIt = spatialIndex.displayCellsByFloor.find(candidateFloorId);
                if (floorIt == spatialIndex.displayCellsByFloor.end()) {
                    return;
                }
                const Point2D minPoint{
                    .x = std::min(passage.start.x, passage.end.x) - kScenarioBottleneckRadius,
                    .y = std::min(passage.start.y, passage.end.y) - kScenarioBottleneckRadius,
                };
                const Point2D maxPoint{
                    .x = std::max(passage.start.x, passage.end.x) + kScenarioBottleneckRadius,
                    .y = std::max(passage.start.y, passage.end.y) + kScenarioBottleneckRadius,
                };
                for (const auto& cell : spatialCellsForBounds(minPoint, maxPoint, spatialIndex.cellSize)) {
                    const auto cellIt = floorIt->second.find(spatialKey(cell));
                    if (cellIt == floorIt->second.end()) {
                        continue;
                    }
                    for (const auto entity : cellIt->second) {
                        const auto packed =
                            (static_cast<std::uint64_t>(entity.generation) << 32U) | entity.index;
                        if (seen.insert(packed).second) {
                            candidates.push_back(entity);
                        }
                    }
                }
            };

            appendFloor(floorId);
            if (!floorId.empty()) {
                appendFloor(std::string{});
            }
            return candidates;
        };

        for (const auto& connection : layout.connections) {
            if (connection.directionality == TravelDirection::Closed) {
                continue;
            }

            ScenarioBottleneckMetric metric;
            metric.connectionId = connection.id;
            metric.label = connectionLabel(layout, connection);
            metric.floorId = connectionFloorId(layout, connection);
            metric.passage = connection.centerSpan;
            double speedSum = 0.0;

            for (const auto entity : nearbyEntitiesForConnection(metric.floorId, connection.centerSpan)) {
                const auto activeIt = activeAgentIndices.find(entity.index);
                if (activeIt == activeAgentIndices.end()) {
                    continue;
                }
                const auto& agent = activeAgents[activeIt->second];
                if (agent.floorId != metric.floorId) {
                    continue;
                }
                const auto distanceToConnection = distanceBetween(
                    agent.position,
                    closestPointOnSegment(agent.position, connection.centerSpan.start, connection.centerSpan.end));
                if (distanceToConnection > kScenarioBottleneckRadius) {
                    continue;
                }

                ++metric.nearbyAgentCount;
                speedSum += lengthOf(agent.velocity);
                if (agent.stalled) {
                    ++metric.stalledAgentCount;
                }
            }

            if (metric.nearbyAgentCount == 0) {
                continue;
            }
            metric.averageSpeed = speedSum / static_cast<double>(metric.nearbyAgentCount);
            if (metric.nearbyAgentCount >= kScenarioBottleneckAgentThreshold
                && (metric.stalledAgentCount > 0 || metric.averageSpeed <= kScenarioStalledSpeedThreshold * 2.0)) {
                snapshot.bottlenecks.push_back(std::move(metric));
            }
        }

        sortAndTrimTop(snapshot.bottlenecks, kMaxReportedBottlenecks, [](const auto& lhs, const auto& rhs) {
            if (lhs.stalledAgentCount != rhs.stalledAgentCount) {
                return lhs.stalledAgentCount > rhs.stalledAgentCount;
            }
            return lhs.nearbyAgentCount > rhs.nearbyAgentCount;
        });
    }

    void collectCrossFlows(
        ScenarioRiskSnapshot& snapshot,
        const std::vector<ActiveAgentContext>& activeAgents,
        const ScenarioSimulationClockResource& clock,
        double deltaSeconds,
        ScenarioCrossFlowResource& crossFlow) const {
        std::unordered_map<long long, CrossFlowCellAccumulator> cells;
        cells.reserve(activeAgents.size());
        for (const auto& agent : activeAgents) {
            const auto speed = lengthOf(agent.velocity);
            if (speed <= 0.05) {
                continue;
            }

            const auto address = crossFlowCellAddress(agent.position, agent.floorId);
            auto& cell = cells[crossFlowCellKey(address)];
            if (cell.movingAgentCount == 0) {
                cell.cellMin = crossFlowCellMin(address);
                cell.cellMax = crossFlowCellMax(address);
                cell.floorId = address.floorId;
            }
            cell.positionSum = cell.positionSum + agent.position;
            ++cell.movingAgentCount;
            cell.speedSum += speed;
            ++cell.directionCounts[crossFlowDirectionBinForVelocity(agent.velocity)];
        }

        std::unordered_set<long long> activeCellKeys;
        activeCellKeys.reserve(cells.size());
        snapshot.crossFlowCells.clear();
        snapshot.crossFlowAgentCount = 0;
        snapshot.peakCrossFlowScore = 0.0;
        snapshot.totalCrossFlowExposureAgentSeconds = 0.0;

        for (const auto& [cellKey, cell] : cells) {
            if (cell.movingAgentCount == 0) {
                continue;
            }

            const auto averageSpeed = cell.speedSum / static_cast<double>(cell.movingAgentCount);
            const auto observation = detectCrossFlow(
                cell.directionCounts,
                cell.movingAgentCount,
                averageSpeed);
            if (!observation.has_value()) {
                continue;
            }

            activeCellKeys.insert(cellKey);
            const auto [stateIt, inserted] = crossFlow.activeCellsByAddress.try_emplace(cellKey);
            auto& state = stateIt->second;
            if (inserted) {
                state.startedAtSeconds = clock.elapsedSeconds;
            }
            const auto exposureDelta =
                static_cast<double>(observation->movingAgentCount) * std::max(0.0, deltaSeconds);
            state.exposureAgentSeconds += exposureDelta;
            crossFlow.totalCrossFlowExposureAgentSeconds += exposureDelta;
            state.peakCrossFlowScore = std::max(state.peakCrossFlowScore, observation->crossFlowScore);
            state.peakAgentCount = std::max(state.peakAgentCount, observation->movingAgentCount);

            const auto count = static_cast<double>(cell.movingAgentCount);
            const Point2D center{
                .x = count <= 0.0 ? 0.0 : cell.positionSum.x / count,
                .y = count <= 0.0 ? 0.0 : cell.positionSum.y / count,
            };

            const auto durationSeconds = std::max(0.0, (clock.elapsedSeconds - state.startedAtSeconds) + deltaSeconds);
            snapshot.crossFlowAgentCount += observation->movingAgentCount;
            snapshot.peakCrossFlowScore = std::max(snapshot.peakCrossFlowScore, observation->crossFlowScore);
            snapshot.crossFlowCells.push_back({
                .center = center,
                .cellMin = cell.cellMin,
                .cellMax = cell.cellMax,
                .floorId = cell.floorId,
                .movingAgentCount = observation->movingAgentCount,
                .peakAgentCount = state.peakAgentCount,
                .primaryFlowCount = observation->primaryFlowCount,
                .crossFlowCount = observation->crossFlowCount,
                .crossFlowRatio = observation->crossFlowRatio,
                .averageSpeed = observation->averageSpeed,
                .speedDropRatio = observation->speedDropRatio,
                .crossFlowScore = state.peakCrossFlowScore,
                .durationSeconds = durationSeconds,
                .exposureAgentSeconds = state.exposureAgentSeconds,
            });
        }

        for (auto it = crossFlow.activeCellsByAddress.begin();
             it != crossFlow.activeCellsByAddress.end();) {
            if (activeCellKeys.contains(it->first)) {
                ++it;
                continue;
            }
            it = crossFlow.activeCellsByAddress.erase(it);
        }

        sortAndTrimTop(snapshot.crossFlowCells, kMaxReportedCrossFlowCells, [](const auto& lhs, const auto& rhs) {
            if (std::fabs(lhs.crossFlowScore - rhs.crossFlowScore) > 1e-9) {
                return lhs.crossFlowScore > rhs.crossFlowScore;
            }
            if (std::fabs(lhs.durationSeconds - rhs.durationSeconds) > 1e-9) {
                return lhs.durationSeconds > rhs.durationSeconds;
            }
            return lhs.movingAgentCount > rhs.movingAgentCount;
        });
        snapshot.totalCrossFlowExposureAgentSeconds = crossFlow.totalCrossFlowExposureAgentSeconds;
    }

    FacilityLayout2D layout_{};
};

}  // namespace

std::unique_ptr<engine::EngineSystem> makeScenarioPressureFeedbackSystem(FacilityLayout2D layout) {
    return std::make_unique<ScenarioPressureFeedbackSystem>(std::move(layout));
}

std::unique_ptr<engine::EngineSystem> makeScenarioRiskMetricsSystem(FacilityLayout2D layout) {
    return std::make_unique<ScenarioRiskMetricsSystem>(std::move(layout));
}

}  // namespace safecrowd::domain
