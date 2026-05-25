#include "domain/ScenarioRiskMetrics.h"

namespace safecrowd::domain {

const char* scenarioRiskLevelLabel(ScenarioRiskLevel level) noexcept {
    switch (level) {
    case ScenarioRiskLevel::Low:
        return "Low";
    case ScenarioRiskLevel::Medium:
        return "Medium";
    case ScenarioRiskLevel::High:
        return "High";
    }
    return "Low";
}

const char* scenarioRiskDefinition() noexcept {
    return "Low when evacuation is complete or no active risk is detected. "
           "Medium when elapsed time reaches 50% of the limit, stalled active agents reach 15%, "
           "any hotspot/pressure hotspot/bottleneck/cross-flow hotspot is detected, or any active agent reaches "
           "critical pressure exposure. "
           "High when elapsed time reaches 80% of the limit, stalled active agents reach 35%, "
           "a critical pressure event is sustained, two or more bottlenecks are detected, "
           "or peak cross-flow score reaches 0.75.";
}

const char* scenarioStalledDefinition() noexcept {
    return "An active agent is stalled when speed is at or below 0.12 m/s, "
           "or route progress has been stalled for at least 0.75 seconds.";
}

const char* scenarioHotspotDefinition() noexcept {
    return "A hotspot is a 1.5 m by 1.5 m cell whose density reaches 3.55 occupants per square meter.";
}

const char* scenarioPressureHotspotDefinition() noexcept {
    return "A pressure hotspot is a 1.5 m by 1.5 m cell whose density reaches 3.55 occupants "
           "per square meter and whose 1.0 m interpersonal-pressure score reaches 1.0.";
}

const char* scenarioBottleneckDefinition() noexcept {
    return "A bottleneck is reported around an open connection when at least 3 active agents "
           "are within 1.25 m and at least one is stalled or average speed is low.";
}

const char* scenarioCrossFlowDefinition() noexcept {
    return "Cross flow highlights non-aligned movement streams sharing the same space while slowed down. "
           "Cross-flow cells use a 2.0 m grid derived from Pathfinder's 4 m^2 measurement-region "
           "influence area and compare observed speed against "
           "Pathfinder's 1.30 m/s mean and 0.97 m/s minimum walking speeds.";
}

bool scenarioAgentStalled(double speedMetersPerSecond, double routeStalledSeconds) noexcept {
    return speedMetersPerSecond <= kScenarioStalledSpeedThreshold
        || routeStalledSeconds >= kScenarioStalledSecondsThreshold;
}

}  // namespace safecrowd::domain
