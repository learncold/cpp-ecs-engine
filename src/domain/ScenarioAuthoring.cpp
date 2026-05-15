#include "domain/ScenarioAuthoring.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace safecrowd::domain {

namespace {

bool pointsEqual(const Point2D& lhs, const Point2D& rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool pointVectorsEqual(const std::vector<Point2D>& lhs, const std::vector<Point2D>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (!pointsEqual(lhs[i], rhs[i])) {
            return false;
        }
    }
    return true;
}

bool polygonsEqual(const Polygon2D& lhs, const Polygon2D& rhs) {
    if (!pointVectorsEqual(lhs.outline, rhs.outline) || lhs.holes.size() != rhs.holes.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.holes.size(); ++i) {
        if (!pointVectorsEqual(lhs.holes[i], rhs.holes[i])) {
            return false;
        }
    }
    return true;
}

bool placementsEqual(const InitialPlacement2D& lhs, const InitialPlacement2D& rhs) {
    if (lhs.id != rhs.id || lhs.zoneId != rhs.zoneId || lhs.floorId != rhs.floorId
        || lhs.targetAgentCount != rhs.targetAgentCount
        || !pointsEqual(lhs.initialVelocity, rhs.initialVelocity)
        || lhs.distribution != rhs.distribution
        || !polygonsEqual(lhs.area, rhs.area)
        || !pointVectorsEqual(lhs.explicitPositions, rhs.explicitPositions)) {
        return false;
    }
    return true;
}

bool populationsEqual(const PopulationSpec& lhs, const PopulationSpec& rhs) {
    if (lhs.initialPlacements.size() != rhs.initialPlacements.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.initialPlacements.size(); ++i) {
        if (!placementsEqual(lhs.initialPlacements[i], rhs.initialPlacements[i])) {
            return false;
        }
    }
    return true;
}

bool hazardsEqual(const std::vector<EnvironmentHazardDraft>& lhs,
                  const std::vector<EnvironmentHazardDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].id != rhs[i].id
            || lhs[i].kind != rhs[i].kind
            || lhs[i].name != rhs[i].name
            || lhs[i].affectedZoneId != rhs[i].affectedZoneId
            || lhs[i].floorId != rhs[i].floorId
            || !pointsEqual(lhs[i].position, rhs[i].position)
            || lhs[i].startSeconds != rhs[i].startSeconds
            || lhs[i].endSeconds != rhs[i].endSeconds
            || lhs[i].severity != rhs[i].severity
            || lhs[i].note != rhs[i].note) {
            return false;
        }
    }
    return true;
}

bool eventsEqual(const std::vector<OperationalEventDraft>& lhs,
                 const std::vector<OperationalEventDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].id != rhs[i].id || lhs[i].name != rhs[i].name
            || lhs[i].triggerSummary != rhs[i].triggerSummary
            || lhs[i].targetSummary != rhs[i].targetSummary) {
            return false;
        }
    }
    return true;
}

bool connectionBlocksEqual(const std::vector<ConnectionBlockDraft>& lhs,
                            const std::vector<ConnectionBlockDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].id != rhs[i].id || lhs[i].connectionId != rhs[i].connectionId) {
            return false;
        }
        if (lhs[i].intervals.size() != rhs[i].intervals.size()) {
            return false;
        }
        for (std::size_t j = 0; j < lhs[i].intervals.size(); ++j) {
            if (lhs[i].intervals[j].startSeconds != rhs[i].intervals[j].startSeconds
                || lhs[i].intervals[j].endSeconds != rhs[i].intervals[j].endSeconds) {
                return false;
            }
        }
    }
    return true;
}

bool routeGuidancePeriodsEqual(const std::vector<RouteGuidancePeriodDraft>& lhs,
                               const std::vector<RouteGuidancePeriodDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].startSeconds != rhs[i].startSeconds
            || lhs[i].endSeconds != rhs[i].endSeconds) {
            return false;
        }
    }
    return true;
}

bool routeGuidancesEqual(const std::vector<RouteGuidanceDraft>& lhs,
                         const std::vector<RouteGuidanceDraft>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i].id != rhs[i].id
            || lhs[i].startSeconds != rhs[i].startSeconds
            || lhs[i].endSeconds != rhs[i].endSeconds
            || lhs[i].guidedExitZoneId != rhs[i].guidedExitZoneId
            || lhs[i].installConnectionId != rhs[i].installConnectionId
            || lhs[i].baseComplianceRate != rhs[i].baseComplianceRate
            || lhs[i].guidanceStrength != rhs[i].guidanceStrength
            || lhs[i].maxDetourMeters != rhs[i].maxDetourMeters
            || !routeGuidancePeriodsEqual(lhs[i].periods, rhs[i].periods)) {
            return false;
        }
    }
    return true;
}

}  // namespace

ScenarioDraft duplicateScenarioDraft(const ScenarioDraft& source,
                                     std::string newScenarioId,
                                     std::string newName) {
    ScenarioDraft copy = source;
    copy.scenarioId = std::move(newScenarioId);
    copy.name = std::move(newName);
    copy.role = ScenarioRole::Alternative;
    copy.sourceTemplateId.clear();
    copy.variationDiffKeys.clear();
    copy.blockingIssues.clear();
    return copy;
}

std::vector<std::string> computeScenarioDiffKeys(const ScenarioDraft& baseline,
                                                 const ScenarioDraft& variant) {
    std::vector<std::string> keys;

    if (!populationsEqual(baseline.population, variant.population)) {
        keys.emplace_back("population.placements");
    }
    if (baseline.environment.reducedVisibility != variant.environment.reducedVisibility) {
        keys.emplace_back("environment.reducedVisibility");
    }
    if (baseline.environment.familiarityProfile != variant.environment.familiarityProfile) {
        keys.emplace_back("environment.familiarityProfile");
    }
    if (baseline.environment.guidanceProfile != variant.environment.guidanceProfile) {
        keys.emplace_back("environment.guidanceProfile");
    }
    if (!hazardsEqual(baseline.environment.hazards, variant.environment.hazards)) {
        keys.emplace_back("environment.hazards");
    }
    if (!eventsEqual(baseline.control.events, variant.control.events)) {
        keys.emplace_back("control.events");
    }
    if (!connectionBlocksEqual(baseline.control.connectionBlocks, variant.control.connectionBlocks)) {
        keys.emplace_back("control.connectionBlocks");
    }
    if (!routeGuidancesEqual(baseline.control.routeGuidances, variant.control.routeGuidances)) {
        keys.emplace_back("control.routeGuidances");
    }
    if (baseline.execution.timeLimitSeconds != variant.execution.timeLimitSeconds) {
        keys.emplace_back("execution.timeLimit");
    }
    if (baseline.execution.sampleIntervalSeconds != variant.execution.sampleIntervalSeconds) {
        keys.emplace_back("execution.sampleInterval");
    }
    if (baseline.execution.repeatCount != variant.execution.repeatCount) {
        keys.emplace_back("execution.repeatCount");
    }
    if (baseline.execution.baseSeed != variant.execution.baseSeed) {
        keys.emplace_back("execution.baseSeed");
    }
    if (baseline.execution.recordOccupantHistory != variant.execution.recordOccupantHistory) {
        keys.emplace_back("execution.recordOccupantHistory");
    }

    return keys;
}

double environmentHazardRadiusMeters(ScenarioElementSeverity severity) {
    switch (severity) {
    case ScenarioElementSeverity::Low:
        return 2.0;
    case ScenarioElementSeverity::High:
        return 5.0;
    case ScenarioElementSeverity::Medium:
    default:
        return 3.5;
    }
}

double environmentHazardRoutePenaltyMeters(ScenarioElementSeverity severity) {
    switch (severity) {
    case ScenarioElementSeverity::Low:
        return 25.0;
    case ScenarioElementSeverity::High:
        return 150.0;
    case ScenarioElementSeverity::Medium:
    default:
        return 75.0;
    }
}

double environmentHazardSeverityWeight(ScenarioElementSeverity severity) {
    switch (severity) {
    case ScenarioElementSeverity::Low:
        return 1.0;
    case ScenarioElementSeverity::High:
        return 3.0;
    case ScenarioElementSeverity::Medium:
    default:
        return 2.0;
    }
}

double environmentHazardSpeedFactor(EnvironmentHazardKind kind, ScenarioElementSeverity severity) {
    if (kind == EnvironmentHazardKind::Smoke) {
        EnvironmentHazardDraft hazard;
        hazard.kind = EnvironmentHazardKind::Smoke;
        hazard.severity = severity;
        return environmentHazardSpeedFactorAt(hazard, 0.0, 1.5);
    }

    switch (severity) {
    case ScenarioElementSeverity::Low:
        return 0.90;
    case ScenarioElementSeverity::High:
        return 0.60;
    case ScenarioElementSeverity::Medium:
    default:
        return 0.75;
    }
}

double environmentHazardSmokeVisibilityMetersAt(const EnvironmentHazardDraft& hazard, double distanceMeters) {
    if (hazard.kind != EnvironmentHazardKind::Smoke) {
        return std::numeric_limits<double>::infinity();
    }

    const auto radius = environmentHazardRadiusMeters(hazard.severity);
    if (radius <= 1e-9) {
        return std::numeric_limits<double>::infinity();
    }

    double sourceVisibility = 1.5;
    switch (hazard.severity) {
    case ScenarioElementSeverity::Low:
        sourceVisibility = 2.5;
        break;
    case ScenarioElementSeverity::High:
        sourceVisibility = 0.5;
        break;
    case ScenarioElementSeverity::Medium:
    default:
        sourceVisibility = 1.5;
        break;
    }

    const auto distance = std::max(0.0, distanceMeters);
    if (distance >= radius) {
        return 3.0;
    }

    const auto t = std::clamp(distance / radius, 0.0, 1.0);
    return sourceVisibility + ((3.0 - sourceVisibility) * t);
}

double environmentHazardSmokeSpeedMetersPerSecond(double smokeFreeSpeedMetersPerSecond, double visibilityMeters) {
    const auto smokeFreeSpeed = std::max(0.0, smokeFreeSpeedMetersPerSecond);
    if (smokeFreeSpeed <= 1e-9) {
        return 0.0;
    }

    if (visibilityMeters >= 3.0) {
        return smokeFreeSpeed;
    }

    // Pathfinder applies the Fridolf et al. visibility relation and floors smoke walking speed at 0.2 m/s.
    const auto smokeSpeed = std::max(0.2, smokeFreeSpeed - (0.34 * (3.0 - std::max(0.0, visibilityMeters))));
    return std::min(smokeFreeSpeed, smokeSpeed);
}

double environmentHazardSpeedFactorAt(
    const EnvironmentHazardDraft& hazard,
    double distanceMeters,
    double smokeFreeSpeedMetersPerSecond) {
    const auto smokeFreeSpeed = std::max(0.0, smokeFreeSpeedMetersPerSecond);
    if (smokeFreeSpeed <= 1e-9) {
        return 1.0;
    }

    const auto radius = environmentHazardRadiusMeters(hazard.severity);
    if (radius <= 1e-9 || distanceMeters >= radius) {
        return 1.0;
    }

    if (hazard.kind == EnvironmentHazardKind::Smoke) {
        const auto visibility = environmentHazardSmokeVisibilityMetersAt(hazard, distanceMeters);
        return environmentHazardSmokeSpeedMetersPerSecond(smokeFreeSpeed, visibility) / smokeFreeSpeed;
    }

    const auto centerFactor = environmentHazardSpeedFactor(hazard.kind, hazard.severity);
    const auto proximity = 1.0 - std::clamp(std::max(0.0, distanceMeters) / radius, 0.0, 1.0);
    return 1.0 - ((1.0 - centerFactor) * proximity);
}

EnvironmentHazardRuntimeProfile environmentHazardRuntimeProfile(const EnvironmentHazardDraft& hazard) {
    return {
        .radiusMeters = environmentHazardRadiusMeters(hazard.severity),
        .speedFactor = std::max(0.35, environmentHazardSpeedFactorAt(hazard, 0.0, 1.5)),
        .routePenaltyMeters = environmentHazardRoutePenaltyMeters(hazard.severity),
        .severityWeight = environmentHazardSeverityWeight(hazard.severity),
    };
}

bool environmentHazardHasOpenEndedSchedule(const EnvironmentHazardDraft& hazard) {
    return hazard.endSeconds <= hazard.startSeconds;
}

bool environmentHazardActiveAt(const EnvironmentHazardDraft& hazard, double elapsedSeconds) {
    const auto start = std::max(0.0, hazard.startSeconds);
    if (elapsedSeconds + 1e-9 < start) {
        return false;
    }
    if (environmentHazardHasOpenEndedSchedule(hazard)) {
        return true;
    }
    return elapsedSeconds <= std::max(start, hazard.endSeconds) + 1e-9;
}

std::string environmentHazardFloorId(const FacilityLayout2D& layout, const EnvironmentHazardDraft& hazard) {
    if (!hazard.floorId.empty()) {
        return hazard.floorId;
    }
    if (hazard.affectedZoneId.empty()) {
        return {};
    }
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == hazard.affectedZoneId;
    });
    return it == layout.zones.end() ? std::string{} : it->floorId;
}

bool connectionBlockIntervalActiveAt(const ConnectionBlockIntervalDraft& interval, double elapsedSeconds) {
    const auto start = std::max(0.0, interval.startSeconds);
    if (elapsedSeconds + 1e-9 < start) {
        return false;
    }
    if (interval.endSeconds <= interval.startSeconds) {
        return true;
    }
    return elapsedSeconds <= std::max(start, interval.endSeconds) + 1e-9;
}

bool connectionBlockActiveAt(const ConnectionBlockDraft& block, double elapsedSeconds) {
    if (block.connectionId.empty()) {
        return false;
    }
    if (block.intervals.empty()) {
        return true;
    }
    return std::any_of(block.intervals.begin(), block.intervals.end(), [&](const auto& interval) {
        return connectionBlockIntervalActiveAt(interval, elapsedSeconds);
    });
}

}  // namespace safecrowd::domain
