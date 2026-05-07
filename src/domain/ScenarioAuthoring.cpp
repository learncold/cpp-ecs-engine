#include "domain/ScenarioAuthoring.h"

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

}  // namespace

ScenarioDraft duplicateScenarioDraft(const ScenarioDraft& source,
                                     std::string newScenarioId,
                                     std::string newName) {
    ScenarioDraft copy = source;
    copy.scenarioId = std::move(newScenarioId);
    copy.name = std::move(newName);
    copy.role = ScenarioRole::Alternative;
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
    if (!eventsEqual(baseline.control.events, variant.control.events)) {
        keys.emplace_back("control.events");
    }
    if (!connectionBlocksEqual(baseline.control.connectionBlocks, variant.control.connectionBlocks)) {
        keys.emplace_back("control.connectionBlocks");
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

}  // namespace safecrowd::domain
