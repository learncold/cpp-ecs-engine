#include "domain/ScenarioRiskMetrics.h"

namespace safecrowd::domain {

const char* scenarioStalledDefinition() noexcept {
    return "An active agent is stalled when speed is at or below 0.12 m/s, "
           "or route progress has been stalled for at least 0.75 seconds.";
}

const char* scenarioHotspotDefinition() noexcept {
    return "A hotspot is a 1.5 m by 1.5 m cell whose density reaches 3.55 occupants per square meter.";
}

const char* scenarioPressureHotspotDefinition() noexcept {
    return "A pressure hotspot is a 1.5 m by 1.5 m cell whose occupant interaction pressure score reaches 1.0. "
           "Pair pressure starts when occupants intrude into each other's radius plus personal-space buffer.";
}

const char* scenarioBottleneckDefinition() noexcept {
    return "A bottleneck is reported around an open connection when at least 3 active agents "
           "are within 1.25 m and at least one is stalled or average speed is low.";
}

const char* scenarioOperationalConflictDefinition() noexcept {
    return "Operational conflict highlights opposing route intent and queueing around shared passages. "
           "Cells use a 2.0 m grid derived from Pathfinder's 4 m^2 measurement-region influence area, "
           "while connection metrics require forward and reverse movement intent before scoring speed drop. "
           "This separates bidirectional conflict from one-way bottleneck concentration.";
}

bool scenarioAgentStalled(double speedMetersPerSecond, double routeStalledSeconds) noexcept {
    return speedMetersPerSecond <= kScenarioStalledSpeedThreshold
        || routeStalledSeconds >= kScenarioStalledSecondsThreshold;
}

}  // namespace safecrowd::domain
