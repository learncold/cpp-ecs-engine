#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

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
};

struct ScenarioBottleneckMetric {
    std::string connectionId{};
    std::string label{};
    LineSegment2D passage{};
    std::size_t nearbyAgentCount{0};
    std::size_t stalledAgentCount{0};
    double averageSpeed{0.0};
};

struct ScenarioRiskSnapshot {
    ScenarioRiskLevel completionRisk{ScenarioRiskLevel::Low};
    std::size_t stalledAgentCount{0};
    std::vector<ScenarioCongestionHotspot> hotspots{};
    std::vector<ScenarioBottleneckMetric> bottlenecks{};
};

const char* scenarioRiskLevelLabel(ScenarioRiskLevel level) noexcept;

}  // namespace safecrowd::domain
