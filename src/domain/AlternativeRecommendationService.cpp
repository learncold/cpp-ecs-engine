#include "domain/AlternativeRecommendationService.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace safecrowd::domain {
namespace {

constexpr double kExitImbalanceThreshold = 0.25;
constexpr double kDefaultGuidanceCompliance = 0.5;
constexpr double kDefaultGuidanceStrength = 0.55;
constexpr double kDefaultGuidanceMaxDetourMeters = 20.0;

std::string sanitizeId(std::string value) {
    for (auto& ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        if (!std::isalnum(c)) {
            ch = '-';
        }
    }
    value.erase(std::unique(value.begin(), value.end(), [](char lhs, char rhs) {
        return lhs == '-' && rhs == '-';
    }), value.end());
    while (!value.empty() && value.front() == '-') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == '-') {
        value.pop_back();
    }
    return value.empty() ? "item" : value;
}

std::string fixed(double value, int precision = 1) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

std::string percent(double ratio) {
    return fixed(std::clamp(ratio, 0.0, 1.0) * 100.0, 0) + "%";
}

const Zone2D* findZone(const FacilityLayout2D& layout, const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

const Connection2D* findConnection(const FacilityLayout2D& layout, const std::string& connectionId) {
    const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        return connection.id == connectionId;
    });
    return it == layout.connections.end() ? nullptr : &(*it);
}

std::string zoneName(const FacilityLayout2D& layout, const std::string& zoneId) {
    const auto* zone = findZone(layout, zoneId);
    if (zone == nullptr) {
        return zoneId.empty() ? "unknown zone" : zoneId;
    }
    return zone->label.empty() ? zone->id : zone->label + " (" + zone->id + ")";
}

std::string connectionName(const FacilityLayout2D& layout, const std::string& connectionId) {
    const auto* connection = findConnection(layout, connectionId);
    if (connection == nullptr) {
        return connectionId.empty() ? "unknown connection" : connectionId;
    }
    return connection->id + " (" + zoneName(layout, connection->fromZoneId) + " -> "
        + zoneName(layout, connection->toZoneId) + ")";
}

std::string zoneNameList(const FacilityLayout2D& layout, const std::vector<std::string>& zoneIds) {
    std::string value;
    for (const auto& zoneId : zoneIds) {
        if (!value.empty()) {
            value += ", ";
        }
        value += zoneName(layout, zoneId);
    }
    return value.empty() ? "none" : value;
}

bool hasCompletedResultArtifactEvidence(const AlternativeRecommendationRequest& request) {
    const auto& artifacts = request.artifacts;
    return artifacts.timingSummary.finalEvacuationTimeSeconds.has_value()
        || !artifacts.evacuationProgress.empty()
        || !artifacts.exitUsage.empty()
        || !artifacts.zoneCompletion.empty()
        || !artifacts.placementCompletion.empty()
        || artifacts.densitySummary.peakDensityPeoplePerSquareMeter > 0.0
        || artifacts.pressureSummary.peakPressureScore > 0.0
        || !artifacts.pressureSummary.peakHotspots.empty()
        || !artifacts.pressureSummary.criticalEvents.empty()
        || !artifacts.hazardExposureSummary.hazards.empty();
}

bool containsString(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool exitUsageContainsZone(const ScenarioResultArtifacts& artifacts, const std::string& zoneId) {
    return std::any_of(artifacts.exitUsage.begin(), artifacts.exitUsage.end(), [&](const auto& usage) {
        return usage.exitZoneId == zoneId;
    });
}

std::optional<ExitUsageMetric> leastUsedExit(
    const AlternativeRecommendationRequest& request,
    const std::vector<std::string>& excludedExitZoneIds = {}) {
    if (request.artifacts.exitUsage.empty()) {
        return std::nullopt;
    }

    std::optional<ExitUsageMetric> best;
    for (const auto& usage : request.artifacts.exitUsage) {
        if (containsString(excludedExitZoneIds, usage.exitZoneId)) {
            continue;
        }
        if (!best.has_value()
            || usage.usageRatio < best->usageRatio
            || (usage.usageRatio == best->usageRatio && usage.evacuatedCount < best->evacuatedCount)) {
            best = usage;
        }
    }
    return best;
}

std::optional<ExitUsageMetric> mostUsedExit(const ScenarioResultArtifacts& artifacts) {
    const auto it = std::max_element(
        artifacts.exitUsage.begin(),
        artifacts.exitUsage.end(),
        [](const auto& lhs, const auto& rhs) {
            if (lhs.usageRatio == rhs.usageRatio) {
                return lhs.evacuatedCount < rhs.evacuatedCount;
            }
            return lhs.usageRatio < rhs.usageRatio;
        });
    return it == artifacts.exitUsage.end() ? std::nullopt : std::optional<ExitUsageMetric>{*it};
}

bool hasRouteGuidance(const ScenarioDraft& scenario,
                      const std::string& guidedExitZoneId,
                      const std::string& installConnectionId) {
    return std::any_of(scenario.control.routeGuidances.begin(), scenario.control.routeGuidances.end(), [&](const auto& guidance) {
        return guidance.guidedExitZoneId == guidedExitZoneId
            && guidance.installConnectionId == installConnectionId;
    });
}

std::vector<std::string> adjacentExitZoneIdsForConnection(
    const AlternativeRecommendationRequest& request,
    const std::string& connectionId) {
    const auto* connection = findConnection(request.layout, connectionId);
    if (connection == nullptr) {
        return {};
    }

    std::vector<std::string> exitZoneIds;
    const auto addIfExit = [&](const std::string& zoneId) {
        if (zoneId.empty() || containsString(exitZoneIds, zoneId)) {
            return;
        }
        const auto* zone = findZone(request.layout, zoneId);
        if ((zone != nullptr && zone->kind == ZoneKind::Exit)
            || exitUsageContainsZone(request.artifacts, zoneId)) {
            exitZoneIds.push_back(zoneId);
        }
    };
    addIfExit(connection->fromZoneId);
    addIfExit(connection->toZoneId);
    return exitZoneIds;
}

RouteGuidanceDraft makeGuidance(const std::string& id,
                                const std::string& guidedExitZoneId,
                                const std::string& installConnectionId = {}) {
    RouteGuidanceDraft guidance;
    guidance.id = id;
    guidance.guidedExitZoneId = guidedExitZoneId;
    guidance.installConnectionId = installConnectionId;
    guidance.baseComplianceRate = kDefaultGuidanceCompliance;
    guidance.guidanceStrength = kDefaultGuidanceStrength;
    guidance.maxDetourMeters = kDefaultGuidanceMaxDetourMeters;
    return guidance;
}

std::string recommendedScenarioId(const ScenarioDraft& source, AlternativeRecommendationKind kind) {
    const auto sourceId = source.scenarioId.empty() ? "scenario" : source.scenarioId;
    return sourceId + "-recommended-" + alternativeRecommendationKindId(kind);
}

ScenarioDraft makeRecommendedDraft(const AlternativeRecommendationRequest& request,
                                   AlternativeRecommendationKind kind,
                                   const std::string& name) {
    ScenarioDraft draft = request.sourceScenario;
    draft.scenarioId = recommendedScenarioId(request.sourceScenario, kind);
    draft.name = name;
    draft.role = ScenarioRole::Recommended;
    draft.sourceTemplateId = "recommendation:" + std::string(alternativeRecommendationKindId(kind))
        + ":" + (request.sourceScenario.scenarioId.empty() ? "source" : request.sourceScenario.scenarioId);
    draft.blockingIssues.clear();
    return draft;
}

void finalizeDiffKeys(const AlternativeRecommendationRequest& request, ScenarioDraft& draft) {
    if (request.baselineScenario.has_value()) {
        draft.variationDiffKeys = computeScenarioDiffKeys(*request.baselineScenario, draft);
    } else {
        draft.variationDiffKeys = computeScenarioDiffKeys(request.sourceScenario, draft);
    }
}

bool sourceHasConnectionBlock(const ScenarioDraft& scenario, const std::string& connectionId) {
    return std::any_of(scenario.control.connectionBlocks.begin(), scenario.control.connectionBlocks.end(), [&](const auto& block) {
        return block.connectionId == connectionId;
    });
}

bool bottleneckLessSevere(const ScenarioBottleneckMetric& lhs, const ScenarioBottleneckMetric& rhs) {
    if (lhs.stalledAgentCount == rhs.stalledAgentCount) {
        return lhs.nearbyAgentCount < rhs.nearbyAgentCount;
    }
    return lhs.stalledAgentCount < rhs.stalledAgentCount;
}

std::optional<std::string> blockedConnectionToRelieve(const AlternativeRecommendationRequest& request) {
    if (request.sourceScenario.control.connectionBlocks.empty()) {
        return std::nullopt;
    }

    std::optional<ScenarioBottleneckMetric> best;
    for (const auto& bottleneck : request.risk.bottlenecks) {
        if (!bottleneck.connectionId.empty()
            && sourceHasConnectionBlock(request.sourceScenario, bottleneck.connectionId)) {
            if (!best.has_value() || bottleneckLessSevere(*best, bottleneck)) {
                best = bottleneck;
            }
        }
    }
    return best.has_value() ? std::optional<std::string>{best->connectionId} : std::nullopt;
}

std::optional<ScenarioBottleneckMetric> worstBottleneck(const ScenarioRiskSnapshot& risk) {
    if (risk.bottlenecks.empty()) {
        return std::nullopt;
    }
    const auto it = std::max_element(risk.bottlenecks.begin(), risk.bottlenecks.end(), bottleneckLessSevere);
    return it == risk.bottlenecks.end() ? std::nullopt : std::optional<ScenarioBottleneckMetric>{*it};
}

AlternativeRecommendationEvidence evidence(std::string label, std::string value, std::string source) {
    return {
        .label = std::move(label),
        .value = std::move(value),
        .source = std::move(source),
    };
}

std::optional<AlternativeRecommendationCandidate> makeBlockedConnectionCandidate(
    const AlternativeRecommendationRequest& request) {
    const auto connectionId = blockedConnectionToRelieve(request);
    if (!connectionId.has_value()) {
        return std::nullopt;
    }

    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::BlockedConnectionRelief,
        "Recommended: reopen " + connectionName(request.layout, *connectionId));
    draft.control.connectionBlocks.erase(
        std::remove_if(draft.control.connectionBlocks.begin(), draft.control.connectionBlocks.end(), [&](const auto& block) {
            return block.connectionId == *connectionId;
        }),
        draft.control.connectionBlocks.end());
    finalizeDiffKeys(request, draft);

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::BlockedConnectionRelief;
    candidate.id = "open-" + sanitizeId(*connectionId);
    candidate.priority = 10;
    candidate.title = "Reopen blocked connection";
    candidate.summary = "Create a review draft that removes the block on " + connectionName(request.layout, *connectionId) + ".";
    candidate.expectedImprovement = "Restores a constrained exit path and can reduce queueing near blocked connectors.";
    candidate.artifactSource = "ScenarioDraft.control.connectionBlocks + completed result artifacts";
    candidate.evidence.push_back(evidence("Blocked connection", connectionName(request.layout, *connectionId), "ScenarioDraft.control.connectionBlocks"));
    if (const auto bottleneck = worstBottleneck(request.risk);
        bottleneck.has_value() && bottleneck->connectionId == *connectionId) {
        candidate.evidence.push_back(evidence(
            "Bottleneck signal",
            std::to_string(bottleneck->stalledAgentCount) + " stalled / "
                + std::to_string(bottleneck->nearbyAgentCount) + " nearby",
            "ScenarioRiskSnapshot.bottlenecks"));
    }
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makeBottleneckGuidanceCandidate(
    const AlternativeRecommendationRequest& request) {
    const auto bottleneck = worstBottleneck(request.risk);
    if (!bottleneck.has_value() || bottleneck->connectionId.empty()) {
        return std::nullopt;
    }
    const auto adjacentExitZoneIds = adjacentExitZoneIdsForConnection(request, bottleneck->connectionId);
    const auto targetExit = leastUsedExit(
        request,
        adjacentExitZoneIds);
    if (!targetExit.has_value() || targetExit->exitZoneId.empty()) {
        return std::nullopt;
    }
    if (hasRouteGuidance(request.sourceScenario, targetExit->exitZoneId, bottleneck->connectionId)) {
        return std::nullopt;
    }

    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::BottleneckBypassGuidance,
        "Recommended: guide around " + connectionName(request.layout, bottleneck->connectionId));
    draft.control.routeGuidances.push_back(makeGuidance(
        "recommendation-guidance-" + sanitizeId(bottleneck->connectionId) + "-" + sanitizeId(targetExit->exitZoneId),
        targetExit->exitZoneId,
        bottleneck->connectionId));
    finalizeDiffKeys(request, draft);

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::BottleneckBypassGuidance;
    candidate.id = "guide-around-" + sanitizeId(bottleneck->connectionId);
    candidate.priority = 20;
    candidate.title = "Guide occupants around bottleneck";
    candidate.summary = "Create a review draft that guides occupants near "
        + connectionName(request.layout, bottleneck->connectionId) + " toward "
        + zoneName(request.layout, targetExit->exitZoneId) + ".";
    candidate.expectedImprovement = "Shifts part of the crowd away from a stalled connector before rerunning the scenario.";
    candidate.artifactSource = "ScenarioRiskSnapshot.bottlenecks + ScenarioResultArtifacts.exitUsage";
    candidate.evidence.push_back(evidence(
        "Bottleneck signal",
        std::to_string(bottleneck->stalledAgentCount) + " stalled / "
            + std::to_string(bottleneck->nearbyAgentCount) + " nearby",
        "ScenarioRiskSnapshot.bottlenecks"));
    if (!adjacentExitZoneIds.empty()) {
        candidate.evidence.push_back(evidence(
            "Excluded adjacent exits",
            zoneNameList(request.layout, adjacentExitZoneIds),
            "FacilityLayout2D connection endpoints + ScenarioResultArtifacts.exitUsage"));
    }
    candidate.evidence.push_back(evidence(
        "Guided exit",
        zoneName(request.layout, targetExit->exitZoneId),
        adjacentExitZoneIds.empty()
            ? "least-used exit from ScenarioResultArtifacts.exitUsage"
            : "least-used non-adjacent exit from ScenarioResultArtifacts.exitUsage"));
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makeExitBalancingCandidate(
    const AlternativeRecommendationRequest& request) {
    if (request.artifacts.exitUsage.size() < 2) {
        return std::nullopt;
    }
    const auto low = leastUsedExit(request);
    const auto high = mostUsedExit(request.artifacts);
    if (!low.has_value() || !high.has_value() || low->exitZoneId.empty() || high->exitZoneId.empty()) {
        return std::nullopt;
    }
    if (low->exitZoneId == high->exitZoneId
        || high->usageRatio - low->usageRatio < kExitImbalanceThreshold
        || hasRouteGuidance(request.sourceScenario, low->exitZoneId, {})) {
        return std::nullopt;
    }

    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::ExitUsageBalancing,
        "Recommended: balance exit usage toward " + zoneName(request.layout, low->exitZoneId));
    draft.control.routeGuidances.push_back(makeGuidance(
        "recommendation-guidance-exit-" + sanitizeId(low->exitZoneId),
        low->exitZoneId));
    finalizeDiffKeys(request, draft);

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::ExitUsageBalancing;
    candidate.id = "balance-exit-" + sanitizeId(low->exitZoneId);
    candidate.priority = 30;
    candidate.title = "Balance exit usage";
    candidate.summary = "Create a review draft that guides a share of occupants toward the underused exit "
        + zoneName(request.layout, low->exitZoneId) + ".";
    candidate.expectedImprovement = "Reduces dependence on " + zoneName(request.layout, high->exitZoneId)
        + " and may lower final evacuation time.";
    candidate.artifactSource = "ScenarioResultArtifacts.exitUsage";
    candidate.evidence.push_back(evidence(
        "Most used exit",
        zoneName(request.layout, high->exitZoneId) + " at " + percent(high->usageRatio),
        "ScenarioResultArtifacts.exitUsage"));
    candidate.evidence.push_back(evidence(
        "Underused exit",
        zoneName(request.layout, low->exitZoneId) + " at " + percent(low->usageRatio),
        "ScenarioResultArtifacts.exitUsage"));
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makePressureHotspotCandidate(
    const AlternativeRecommendationRequest& request) {
    const bool hasPressureSignal =
        (request.artifacts.pressureSummary.hotspotScoreThreshold > 0.0
            && request.artifacts.pressureSummary.peakPressureScore >= request.artifacts.pressureSummary.hotspotScoreThreshold)
        || !request.artifacts.pressureSummary.peakHotspots.empty()
        || !request.artifacts.pressureSummary.criticalEvents.empty()
        || !request.risk.pressureHotspots.empty()
        || !request.risk.criticalPressureEvents.empty()
        || request.risk.criticalPressureAgentCount > 0;
    if (!hasPressureSignal) {
        return std::nullopt;
    }

    const auto targetExit = leastUsedExit(request);
    if (!targetExit.has_value() || targetExit->exitZoneId.empty()
        || hasRouteGuidance(request.sourceScenario, targetExit->exitZoneId, {})) {
        return std::nullopt;
    }

    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::PressureHotspotRelief,
        "Recommended: relieve pressure toward " + zoneName(request.layout, targetExit->exitZoneId));
    draft.control.routeGuidances.push_back(makeGuidance(
        "recommendation-guidance-pressure-" + sanitizeId(targetExit->exitZoneId),
        targetExit->exitZoneId));
    finalizeDiffKeys(request, draft);

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::PressureHotspotRelief;
    candidate.id = "relieve-pressure-" + sanitizeId(targetExit->exitZoneId);
    candidate.priority = 40;
    candidate.title = "Relieve pressure hotspot";
    candidate.summary = "Create a review draft that guides part of the crowd toward "
        + zoneName(request.layout, targetExit->exitZoneId) + " to reduce local pressure.";
    candidate.expectedImprovement = "Moves some agents away from high pressure cells before validating by rerun.";
    candidate.artifactSource = "ScenarioResultArtifacts.pressureSummary + ScenarioRiskSnapshot pressure signals + ScenarioResultArtifacts.exitUsage";
    if (request.artifacts.pressureSummary.peakPressureScore > 0.0) {
        candidate.evidence.push_back(evidence(
            "Peak pressure",
            fixed(request.artifacts.pressureSummary.peakPressureScore, 1),
            "ScenarioResultArtifacts.pressureSummary"));
    }
    if (request.artifacts.pressureSummary.peakCell.has_value()) {
        candidate.evidence.push_back(evidence(
            "Peak cell floor",
            request.artifacts.pressureSummary.peakCell->floorId,
            "ScenarioResultArtifacts.pressureSummary.peakCell"));
    }
    if (!request.artifacts.pressureSummary.peakHotspots.empty()) {
        candidate.evidence.push_back(evidence(
            "Pressure hotspots",
            std::to_string(request.artifacts.pressureSummary.peakHotspots.size()),
            "ScenarioResultArtifacts.pressureSummary.peakHotspots"));
    }
    if (!request.artifacts.pressureSummary.criticalEvents.empty()) {
        candidate.evidence.push_back(evidence(
            "Critical pressure events",
            std::to_string(request.artifacts.pressureSummary.criticalEvents.size()),
            "ScenarioResultArtifacts.pressureSummary.criticalEvents"));
    }
    if (!request.risk.pressureHotspots.empty()) {
        candidate.evidence.push_back(evidence(
            "Risk pressure hotspots",
            std::to_string(request.risk.pressureHotspots.size()),
            "ScenarioRiskSnapshot.pressureHotspots"));
    }
    if (!request.risk.criticalPressureEvents.empty()) {
        candidate.evidence.push_back(evidence(
            "Risk critical pressure events",
            std::to_string(request.risk.criticalPressureEvents.size()),
            "ScenarioRiskSnapshot.criticalPressureEvents"));
    }
    if (request.risk.criticalPressureAgentCount > 0) {
        candidate.evidence.push_back(evidence(
            "Critical pressure agents",
            std::to_string(request.risk.criticalPressureAgentCount),
            "ScenarioRiskSnapshot.criticalPressureAgentCount"));
    }
    candidate.evidence.push_back(evidence(
        "Guided exit",
        zoneName(request.layout, targetExit->exitZoneId),
        "least-used exit from ScenarioResultArtifacts.exitUsage"));
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

}  // namespace

const char* alternativeRecommendationKindId(AlternativeRecommendationKind kind) noexcept {
    switch (kind) {
    case AlternativeRecommendationKind::BlockedConnectionRelief:
        return "blocked-connection-relief";
    case AlternativeRecommendationKind::BottleneckBypassGuidance:
        return "bottleneck-bypass-guidance";
    case AlternativeRecommendationKind::ExitUsageBalancing:
        return "exit-usage-balancing";
    case AlternativeRecommendationKind::PressureHotspotRelief:
        return "pressure-hotspot-relief";
    }
    return "recommendation";
}

AlternativeRecommendationResult AlternativeRecommendationService::recommend(
    const AlternativeRecommendationRequest& request) const {
    AlternativeRecommendationResult result;
    if (!hasCompletedResultArtifactEvidence(request)) {
        result.blockingReasons.push_back(
            "Completed scenario result artifacts are required before SafeCrowd can recommend an operational draft.");
        return result;
    }

    if (const auto candidate = makeBlockedConnectionCandidate(request); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeBottleneckGuidanceCandidate(request); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeExitBalancingCandidate(request); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makePressureHotspotCandidate(request); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }

    std::sort(result.candidates.begin(), result.candidates.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.priority < rhs.priority;
    });

    if (result.candidates.empty()) {
        result.blockingReasons.push_back(
            "No blocked connection, bottleneck, exit imbalance, or pressure hotspot produced an actionable v1 recommendation.");
    }

    return result;
}

}  // namespace safecrowd::domain
