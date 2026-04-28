#include "domain/ScenarioSimulationSystems.h"

#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace safecrowd::domain {
namespace {

using namespace simulation_internal;

constexpr double kStalledSpeedThreshold = 0.12;
constexpr double kStalledSecondsThreshold = 0.75;
constexpr double kHotspotCellSize = 1.5;
constexpr std::size_t kHotspotAgentThreshold = 5;
constexpr double kBottleneckRadius = 1.25;
constexpr std::size_t kBottleneckAgentThreshold = 3;
constexpr std::size_t kMaxReportedHotspots = 3;
constexpr std::size_t kMaxReportedBottlenecks = 3;

struct RiskCellAccumulator {
    Point2D positionSum{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::size_t agentCount{0};
};

struct RiskCellAddress {
    int x{0};
    int y{0};
};

RiskCellAddress riskCellAddress(const Point2D& point) {
    return {
        .x = static_cast<int>(std::floor(point.x / kHotspotCellSize)),
        .y = static_cast<int>(std::floor(point.y / kHotspotCellSize)),
    };
}

long long riskCellKey(const RiskCellAddress& cell) {
    const auto x = cell.x;
    const auto y = cell.y;
    return (static_cast<long long>(x) << 32) ^ static_cast<unsigned int>(y);
}

Point2D riskCellMin(const RiskCellAddress& cell) {
    return {
        .x = static_cast<double>(cell.x) * kHotspotCellSize,
        .y = static_cast<double>(cell.y) * kHotspotCellSize,
    };
}

Point2D riskCellMax(const RiskCellAddress& cell) {
    const auto min = riskCellMin(cell);
    return {
        .x = min.x + kHotspotCellSize,
        .y = min.y + kHotspotCellSize,
    };
}

bool isStalled(const Velocity& velocity, const EvacuationRoute& route) {
    return lengthOf(velocity.value) <= kStalledSpeedThreshold
        || route.stalledSeconds >= kStalledSecondsThreshold;
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

ScenarioRiskLevel completionRiskLevel(
    const ScenarioSimulationClockResource& clock,
    std::size_t totalAgentCount,
    std::size_t evacuatedAgentCount,
    std::size_t stalledAgentCount,
    std::size_t hotspotCount,
    std::size_t bottleneckCount) {
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

    if (elapsedRatio >= 0.8 || stalledRatio >= 0.35 || bottleneckCount >= 2) {
        return ScenarioRiskLevel::High;
    }
    if (elapsedRatio >= 0.5 || stalledRatio >= 0.15 || hotspotCount > 0 || bottleneckCount > 0) {
        return ScenarioRiskLevel::Medium;
    }
    return ScenarioRiskLevel::Low;
}

class ScenarioRiskMetricsSystem final : public engine::EngineSystem {
public:
    explicit ScenarioRiskMetricsSystem(FacilityLayout2D layout)
        : layout_(std::move(layout)) {
    }

    void configure(engine::EngineWorld& world) override {
        world.resources().set(ScenarioRiskMetricsResource{});
    }

    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override {
        (void)step;

        auto& query = world.query();
        auto& resources = world.resources();
        ScenarioRiskSnapshot snapshot;
        const auto entities = query.view<Position, Agent, Velocity, EvacuationRoute, EvacuationStatus>();

        std::size_t totalAgentCount = 0;
        std::size_t evacuatedAgentCount = 0;
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
            if (isStalled(velocity, route)) {
                ++snapshot.stalledAgentCount;
            }

            const auto address = riskCellAddress(position.value);
            auto& cell = cells[riskCellKey(address)];
            if (cell.agentCount == 0) {
                cell.cellMin = riskCellMin(address);
                cell.cellMax = riskCellMax(address);
            }
            cell.positionSum = cell.positionSum + position.value;
            ++cell.agentCount;
        }

        collectHotspots(snapshot, cells);
        collectBottlenecks(snapshot, query, entities);

        ScenarioSimulationClockResource clock;
        if (resources.contains<ScenarioSimulationClockResource>()) {
            clock = resources.get<ScenarioSimulationClockResource>();
        }
        snapshot.completionRisk = completionRiskLevel(
            clock,
            totalAgentCount,
            evacuatedAgentCount,
            snapshot.stalledAgentCount,
            snapshot.hotspots.size(),
            snapshot.bottlenecks.size());

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
        if (isHotspotSetWorse(current.hotspots, peak.hotspots)) {
            peak.hotspots = current.hotspots;
        }
        if (isBottleneckSetWorse(current.bottlenecks, peak.bottlenecks)) {
            peak.bottlenecks = current.bottlenecks;
        }
    }

    void collectHotspots(
        ScenarioRiskSnapshot& snapshot,
        const std::unordered_map<long long, RiskCellAccumulator>& cells) const {
        snapshot.hotspots.reserve(cells.size());
        for (const auto& [_, cell] : cells) {
            if (cell.agentCount < kHotspotAgentThreshold) {
                continue;
            }
            const auto count = static_cast<double>(cell.agentCount);
            snapshot.hotspots.push_back({
                .center = {.x = cell.positionSum.x / count, .y = cell.positionSum.y / count},
                .cellMin = cell.cellMin,
                .cellMax = cell.cellMax,
                .agentCount = cell.agentCount,
            });
        }

        std::sort(snapshot.hotspots.begin(), snapshot.hotspots.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.agentCount > rhs.agentCount;
        });
        if (snapshot.hotspots.size() > kMaxReportedHotspots) {
            snapshot.hotspots.resize(kMaxReportedHotspots);
        }
    }

    void collectBottlenecks(
        ScenarioRiskSnapshot& snapshot,
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities) const {
        for (const auto& connection : layout_.connections) {
            if (connection.directionality == TravelDirection::Closed) {
                continue;
            }

            ScenarioBottleneckMetric metric;
            metric.connectionId = connection.id;
            metric.label = connection.id;
            metric.passage = connection.centerSpan;
            double speedSum = 0.0;

            for (const auto entity : entities) {
                const auto& status = query.get<EvacuationStatus>(entity);
                if (status.evacuated) {
                    continue;
                }
                const auto& position = query.get<Position>(entity);
                const auto& velocity = query.get<Velocity>(entity);
                const auto& route = query.get<EvacuationRoute>(entity);
                const auto distanceToConnection = distanceBetween(
                    position.value,
                    closestPointOnSegment(position.value, connection.centerSpan.start, connection.centerSpan.end));
                if (distanceToConnection > kBottleneckRadius) {
                    continue;
                }

                ++metric.nearbyAgentCount;
                speedSum += lengthOf(velocity.value);
                if (isStalled(velocity, route)) {
                    ++metric.stalledAgentCount;
                }
            }

            if (metric.nearbyAgentCount == 0) {
                continue;
            }
            metric.averageSpeed = speedSum / static_cast<double>(metric.nearbyAgentCount);
            if (metric.nearbyAgentCount >= kBottleneckAgentThreshold
                && (metric.stalledAgentCount > 0 || metric.averageSpeed <= kStalledSpeedThreshold * 2.0)) {
                snapshot.bottlenecks.push_back(std::move(metric));
            }
        }

        std::sort(snapshot.bottlenecks.begin(), snapshot.bottlenecks.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.stalledAgentCount != rhs.stalledAgentCount) {
                return lhs.stalledAgentCount > rhs.stalledAgentCount;
            }
            return lhs.nearbyAgentCount > rhs.nearbyAgentCount;
        });
        if (snapshot.bottlenecks.size() > kMaxReportedBottlenecks) {
            snapshot.bottlenecks.resize(kMaxReportedBottlenecks);
        }
    }

    FacilityLayout2D layout_{};
};

}  // namespace

std::unique_ptr<engine::EngineSystem> makeScenarioRiskMetricsSystem(FacilityLayout2D layout) {
    return std::make_unique<ScenarioRiskMetricsSystem>(std::move(layout));
}

}  // namespace safecrowd::domain
