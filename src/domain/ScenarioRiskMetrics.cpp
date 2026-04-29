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
           "or any hotspot/bottleneck is detected. "
           "High when elapsed time reaches 80% of the limit, stalled active agents reach 35%, "
           "or two or more bottlenecks are detected.";
}

const char* scenarioStalledDefinition() noexcept {
    return "An active agent is stalled when speed is at or below 0.12 m/s, "
           "or route progress has been stalled for at least 0.75 seconds.";
}

const char* scenarioHotspotDefinition() noexcept {
    return "A hotspot is a 1.5 m by 1.5 m cell containing at least 5 active agents.";
}

const char* scenarioBottleneckDefinition() noexcept {
    return "A bottleneck is reported around an open connection when at least 3 active agents "
           "are within 1.25 m and at least one is stalled or average speed is low.";
}

}  // namespace safecrowd::domain
