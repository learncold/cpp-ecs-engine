#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "domain/Geometry2D.h"
#include "domain/ScenarioSimulationFrame.h"

namespace safecrowd::domain {

inline constexpr double kScenarioStalledSpeedThreshold = 0.12;
inline constexpr double kScenarioStalledSecondsThreshold = 0.75;
inline constexpr double kScenarioHotspotCellSize = 1.5;
inline constexpr std::size_t kScenarioHotspotAgentThreshold = 5;
inline constexpr double kScenarioBottleneckRadius = 1.25;
inline constexpr std::size_t kScenarioBottleneckAgentThreshold = 3;

enum class ScenarioRiskLevel {
    Low,
    Medium,
    High,
};

struct ScenarioCongestionHotspot {
    Point2D center{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::size_t agentCount{0};
    std::optional<double> detectedAtSeconds{};
    std::optional<SimulationFrame> detectionFrame{};
};

struct ScenarioBottleneckMetric {
    std::string connectionId{};
    std::string label{};
    LineSegment2D passage{};
    std::size_t nearbyAgentCount{0};
    std::size_t stalledAgentCount{0};
    double averageSpeed{0.0};
    std::optional<double> detectedAtSeconds{};
    std::optional<SimulationFrame> detectionFrame{};
};

struct ScenarioRiskSnapshot {
    ScenarioRiskLevel completionRisk{ScenarioRiskLevel::Low};
    std::size_t stalledAgentCount{0};
    std::vector<ScenarioCongestionHotspot> hotspots{};
    std::vector<ScenarioBottleneckMetric> bottlenecks{};
};

const char* scenarioRiskLevelLabel(ScenarioRiskLevel level) noexcept;
const char* scenarioRiskDefinition() noexcept;
const char* scenarioStalledDefinition() noexcept;
const char* scenarioHotspotDefinition() noexcept;
const char* scenarioBottleneckDefinition() noexcept;

}  // namespace safecrowd::domain
