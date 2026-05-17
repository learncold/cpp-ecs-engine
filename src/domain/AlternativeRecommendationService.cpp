#include "domain/AlternativeRecommendationService.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "domain/GeometryQueries.h"

namespace safecrowd::domain {
namespace {

constexpr double kExitImbalanceThreshold = 0.25;
constexpr double kCounterflowCosineThreshold = -0.5;
constexpr double kCounterflowSideRatioThreshold = 0.30;
constexpr double kCounterflowAverageSpeedThreshold = 0.7;
constexpr double kCounterflowSustainedSecondsThreshold = 10.0;
constexpr std::size_t kStagedEvacuationAgentsPerSpawn = 10;
constexpr double kStagedEvacuationIntervalSeconds = 5.0;
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

AlternativeRecommendationEvidence evidence(std::string label, std::string value, std::string source) {
    return {
        .label = std::move(label),
        .value = std::move(value),
        .source = std::move(source),
    };
}

const Zone2D* findZone(const FacilityLayout2D& layout, const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

bool zoneIsExit(const FacilityLayout2D& layout, const std::string& zoneId) {
    const auto* zone = findZone(layout, zoneId);
    return zone != nullptr && zone->kind == ZoneKind::Exit;
}

const Connection2D* findConnection(const FacilityLayout2D& layout, const std::string& connectionId) {
    const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
        return connection.id == connectionId;
    });
    return it == layout.connections.end() ? nullptr : &(*it);
}

bool connectionTouchesExit(const FacilityLayout2D& layout, const Connection2D& connection) {
    return connection.kind == ConnectionKind::Exit
        || zoneIsExit(layout, connection.fromZoneId)
        || zoneIsExit(layout, connection.toZoneId);
}

bool connectionTouchesZone(const Connection2D& connection, const std::string& zoneId) {
    return !zoneId.empty() && (connection.fromZoneId == zoneId || connection.toZoneId == zoneId);
}

Point2D averagePoint(const std::vector<Point2D>& points) {
    if (points.empty()) {
        return {};
    }
    Point2D result;
    for (const auto& point : points) {
        result.x += point.x;
        result.y += point.y;
    }
    const auto count = static_cast<double>(points.size());
    result.x /= count;
    result.y /= count;
    return result;
}

std::optional<Point2D> sourcePointForPlacement(
    const FacilityLayout2D& layout,
    const InitialPlacement2D& placement) {
    if (!placement.explicitPositions.empty()) {
        return averagePoint(placement.explicitPositions);
    }
    if (placement.area.outline.size() == 1) {
        return placement.area.outline.front();
    }
    if (!placement.area.outline.empty()) {
        return representativePointInPolygon(placement.area).value_or(averagePoint(placement.area.outline));
    }
    const auto* zone = findZone(layout, placement.zoneId);
    if (zone != nullptr && zone->area.outline.size() == 1) {
        return zone->area.outline.front();
    }
    if (zone != nullptr && !zone->area.outline.empty()) {
        return representativePointInPolygon(zone->area).value_or(averagePoint(zone->area.outline));
    }
    return std::nullopt;
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
        || !artifacts.replayFrames.empty()
        || !artifacts.exitUsage.empty()
        || !artifacts.zoneCompletion.empty()
        || !artifacts.placementCompletion.empty()
        || artifacts.densitySummary.peakDensityPeoplePerSquareMeter > 0.0
        || artifacts.pressureSummary.peakPressureScore > 0.0
        || !artifacts.pressureSummary.peakHotspots.empty()
        || !artifacts.pressureSummary.criticalEvents.empty()
        || !artifacts.hazardExposureSummary.hazards.empty()
        || request.finalFrame.has_value();
}

bool containsString(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool exitUsageContainsZone(const ScenarioResultArtifacts& artifacts, const std::string& zoneId) {
    return std::any_of(artifacts.exitUsage.begin(), artifacts.exitUsage.end(), [&](const auto& usage) {
        return usage.exitZoneId == zoneId;
    });
}

std::vector<ExitUsageMetric> exitUsageCandidates(const AlternativeRecommendationRequest& request) {
    if (request.artifacts.exitUsage.empty()) {
        return {};
    }

    auto candidates = request.artifacts.exitUsage;
    for (const auto& zone : request.layout.zones) {
        if (zone.kind != ZoneKind::Exit || exitUsageContainsZone(request.artifacts, zone.id)) {
            continue;
        }
        candidates.push_back({
            .exitZoneId = zone.id,
            .exitLabel = zone.label.empty() ? zone.id : zone.label,
            .floorId = zone.floorId,
            .evacuatedCount = 0,
            .usageRatio = 0.0,
        });
    }
    return candidates;
}

std::optional<ExitUsageMetric> leastUsedExit(
    const AlternativeRecommendationRequest& request,
    const std::vector<std::string>& excludedExitZoneIds = {}) {
    const auto candidates = exitUsageCandidates(request);
    if (candidates.empty()) {
        return std::nullopt;
    }

    std::optional<ExitUsageMetric> best;
    for (const auto& usage : candidates) {
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

std::optional<ExitUsageMetric> mostUsedExit(const AlternativeRecommendationRequest& request) {
    const auto candidates = exitUsageCandidates(request);
    const auto it = std::max_element(
        candidates.begin(),
        candidates.end(),
        [](const auto& lhs, const auto& rhs) {
            if (lhs.usageRatio == rhs.usageRatio) {
                return lhs.evacuatedCount < rhs.evacuatedCount;
            }
            return lhs.usageRatio < rhs.usageRatio;
        });
    return it == candidates.end() ? std::nullopt : std::optional<ExitUsageMetric>{*it};
}

bool hasRouteGuidance(const ScenarioDraft& scenario,
                      const std::string& guidedExitZoneId,
                      const std::string& installConnectionId) {
    return std::any_of(scenario.control.routeGuidances.begin(), scenario.control.routeGuidances.end(), [&](const auto& guidance) {
        return guidance.guidedExitZoneId == guidedExitZoneId
            && guidance.installConnectionId == installConnectionId;
    });
}

bool exitHasBottleneck(
    const AlternativeRecommendationRequest& request,
    const std::string& exitZoneId) {
    return std::any_of(request.risk.bottlenecks.begin(), request.risk.bottlenecks.end(), [&](const auto& bottleneck) {
        const auto* connection = findConnection(request.layout, bottleneck.connectionId);
        return connection != nullptr
            && connectionTouchesExit(request.layout, *connection)
            && connectionTouchesZone(*connection, exitZoneId);
    });
}

bool hasOperationalEvent(const ScenarioDraft& scenario, const std::string& id) {
    return std::any_of(scenario.control.events.begin(), scenario.control.events.end(), [&](const auto& event) {
        return event.id == id;
    });
}

bool hasOccupantSource(const ScenarioDraft& scenario, const std::string& id) {
    return std::any_of(scenario.population.occupantSources.begin(), scenario.population.occupantSources.end(), [&](const auto& source) {
        return source.id == id;
    });
}

std::size_t placementAgentCount(const InitialPlacement2D& placement) {
    return placement.explicitPositions.empty()
        ? placement.targetAgentCount
        : placement.explicitPositions.size();
}

std::size_t stagedEvacuationTickCount(std::size_t targetAgentCount) {
    if (targetAgentCount == 0) {
        return 0;
    }
    return (targetAgentCount + kStagedEvacuationAgentsPerSpawn - 1) / kStagedEvacuationAgentsPerSpawn;
}

OccupantSource2D makeStagedEvacuationSource(
    const AlternativeRecommendationRequest& request,
    const InitialPlacement2D& placement,
    std::size_t targetAgentCount,
    Point2D sourcePosition,
    double startSeconds) {
    const auto tickCount = stagedEvacuationTickCount(targetAgentCount);
    OccupantSource2D source;
    source.id = "recommendation-source-" + sanitizeId(placement.id.empty() ? placement.zoneId : placement.id);
    source.zoneId = placement.zoneId;
    source.floorId = placement.floorId;
    source.position = sourcePosition;
    source.targetAgentCount = targetAgentCount;
    source.agentsPerSpawn = std::min(kStagedEvacuationAgentsPerSpawn, targetAgentCount);
    source.startSeconds = startSeconds;
    source.endSeconds = startSeconds + (kStagedEvacuationIntervalSeconds * static_cast<double>(tickCount));
    source.spawnIntervalSeconds = kStagedEvacuationIntervalSeconds;
    source.initialVelocity = placement.initialVelocity;
    if (source.floorId.empty()) {
        if (const auto* zone = findZone(request.layout, placement.zoneId); zone != nullptr) {
            source.floorId = zone->floorId;
        }
    }
    return source;
}

std::vector<OccupantSource2D> makeSequentialStagedEvacuationSources(
    const AlternativeRecommendationRequest& request) {
    std::vector<OccupantSource2D> sources;
    sources.reserve(request.sourceScenario.population.initialPlacements.size());

    double nextStartSeconds = 0.0;
    for (const auto& placement : request.sourceScenario.population.initialPlacements) {
        const auto targetAgentCount = placementAgentCount(placement);
        if (targetAgentCount == 0) {
            continue;
        }
        const auto sourcePosition = sourcePointForPlacement(request.layout, placement);
        if (!sourcePosition.has_value()) {
            return {};
        }
        auto source = makeStagedEvacuationSource(request, placement, targetAgentCount, *sourcePosition, nextStartSeconds);
        nextStartSeconds = source.endSeconds;
        sources.push_back(std::move(source));
    }
    return sources;
}

std::size_t totalSourceAgents(const std::vector<OccupantSource2D>& sources) {
    std::size_t total = 0;
    for (const auto& source : sources) {
        total += source.targetAgentCount;
    }
    return total;
}

std::size_t totalSourceTicks(const std::vector<OccupantSource2D>& sources) {
    std::size_t total = 0;
    for (const auto& source : sources) {
        total += stagedEvacuationTickCount(source.targetAgentCount);
    }
    return total;
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

OperationalEventDraft makeOperationalEvent(const std::string& id,
                                           const std::string& name,
                                           const std::string& triggerSummary,
                                           const std::string& targetSummary) {
    OperationalEventDraft event;
    event.id = id;
    event.name = name;
    event.triggerSummary = triggerSummary;
    event.targetSummary = targetSummary;
    return event;
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

AlternativeRecommendationRiskKind bottleneckRiskKind(
    const AlternativeRecommendationRequest& request,
    const ScenarioBottleneckMetric& bottleneck) {
    const auto* connection = findConnection(request.layout, bottleneck.connectionId);
    if (connection != nullptr && connectionTouchesExit(request.layout, *connection)) {
        return AlternativeRecommendationRiskKind::ExitBottleneck;
    }
    return AlternativeRecommendationRiskKind::CorridorBottleneck;
}

std::optional<ScenarioBottleneckMetric> worstBottleneckForKind(
    const AlternativeRecommendationRequest& request,
    AlternativeRecommendationRiskKind kind) {
    std::optional<ScenarioBottleneckMetric> best;
    for (const auto& bottleneck : request.risk.bottlenecks) {
        if (bottleneck.connectionId.empty() || bottleneckRiskKind(request, bottleneck) != kind) {
            continue;
        }
        if (!best.has_value() || bottleneckLessSevere(*best, bottleneck)) {
            best = bottleneck;
        }
    }
    return best;
}

AlternativeRecommendationRiskSignal makeBottleneckRiskSignal(
    const AlternativeRecommendationRequest& request,
    const ScenarioBottleneckMetric& bottleneck,
    AlternativeRecommendationRiskKind kind) {
    AlternativeRecommendationRiskSignal signal;
    signal.kind = kind;
    signal.severity = static_cast<int>(bottleneck.stalledAgentCount * 10 + bottleneck.nearbyAgentCount);
    signal.summary = kind == AlternativeRecommendationRiskKind::ExitBottleneck
        ? "Exit bottleneck detected from persisted bottleneck metrics."
        : "Corridor bottleneck detected from persisted bottleneck metrics.";
    signal.evidence.push_back(evidence(
        "Connection",
        connectionName(request.layout, bottleneck.connectionId),
        "ScenarioRiskSnapshot.bottlenecks"));
    signal.evidence.push_back(evidence(
        "Stalled / nearby",
        std::to_string(bottleneck.stalledAgentCount) + " / " + std::to_string(bottleneck.nearbyAgentCount),
        "ScenarioRiskSnapshot.bottlenecks"));
    if (bottleneck.averageSpeed > 0.0) {
        signal.evidence.push_back(evidence(
            "Average speed",
            fixed(bottleneck.averageSpeed, 2) + " m/s",
            "ScenarioRiskSnapshot.bottlenecks"));
    }
    if (bottleneck.detectedAtSeconds.has_value()) {
        signal.evidence.push_back(evidence(
            "Detected at",
            fixed(*bottleneck.detectedAtSeconds, 1) + " sec",
            "ScenarioRiskSnapshot.bottlenecks"));
    }
    return signal;
}

bool hasPressureSignal(const AlternativeRecommendationRequest& request) {
    return (request.artifacts.pressureSummary.hotspotScoreThreshold > 0.0
            && request.artifacts.pressureSummary.peakPressureScore >= request.artifacts.pressureSummary.hotspotScoreThreshold)
        || !request.artifacts.pressureSummary.peakHotspots.empty()
        || !request.artifacts.pressureSummary.criticalEvents.empty()
        || !request.risk.pressureHotspots.empty()
        || !request.risk.criticalPressureEvents.empty()
        || request.risk.criticalPressureAgentCount > 0;
}

std::optional<AlternativeRecommendationRiskSignal> makePressureRiskSignal(
    const AlternativeRecommendationRequest& request) {
    if (!hasPressureSignal(request)) {
        return std::nullopt;
    }

    AlternativeRecommendationRiskSignal signal;
    signal.kind = AlternativeRecommendationRiskKind::PressureHotspot;
    signal.severity = static_cast<int>(request.artifacts.pressureSummary.peakPressureScore * 10.0)
        + static_cast<int>(request.risk.criticalPressureAgentCount * 5);
    signal.summary = "Pressure hotspot or critical pressure signal detected.";
    if (request.artifacts.pressureSummary.peakPressureScore > 0.0) {
        signal.evidence.push_back(evidence(
            "Peak pressure",
            fixed(request.artifacts.pressureSummary.peakPressureScore, 1),
            "ScenarioResultArtifacts.pressureSummary"));
    }
    if (!request.artifacts.pressureSummary.peakHotspots.empty()) {
        signal.evidence.push_back(evidence(
            "Pressure hotspots",
            std::to_string(request.artifacts.pressureSummary.peakHotspots.size()),
            "ScenarioResultArtifacts.pressureSummary.peakHotspots"));
    }
    if (!request.artifacts.pressureSummary.criticalEvents.empty()) {
        signal.evidence.push_back(evidence(
            "Critical pressure events",
            std::to_string(request.artifacts.pressureSummary.criticalEvents.size()),
            "ScenarioResultArtifacts.pressureSummary.criticalEvents"));
    }
    if (request.risk.criticalPressureAgentCount > 0) {
        signal.evidence.push_back(evidence(
            "Critical pressure agents",
            std::to_string(request.risk.criticalPressureAgentCount),
            "ScenarioRiskSnapshot.criticalPressureAgentCount"));
    }
    return signal;
}

std::optional<SimulationFrame> finalFrameForRequest(const AlternativeRecommendationRequest& request) {
    if (request.finalFrame.has_value()) {
        return request.finalFrame;
    }
    if (!request.artifacts.replayFrames.empty()) {
        return request.artifacts.replayFrames.back();
    }
    return std::nullopt;
}

double speedOf(Point2D velocity) {
    return std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
}

double dot(Point2D lhs, Point2D rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

struct FrameCounterflowObservation {
    std::size_t forwardCount{0};
    std::size_t oppositeCount{0};
    std::size_t movingCount{0};
    double averageSpeed{0.0};
};

std::optional<FrameCounterflowObservation> counterflowAtFrame(const SimulationFrame& frame) {
    std::vector<SimulationAgentFrame> movingAgents;
    movingAgents.reserve(frame.agents.size());
    double speedSum = 0.0;
    for (const auto& agent : frame.agents) {
        const auto speed = speedOf(agent.velocity);
        if (speed <= 0.05) {
            continue;
        }
        movingAgents.push_back(agent);
        speedSum += speed;
    }
    if (movingAgents.size() < 2) {
        return std::nullopt;
    }

    FrameCounterflowObservation best;
    for (const auto& anchor : movingAgents) {
        const auto anchorSpeed = speedOf(anchor.velocity);
        if (anchorSpeed <= 0.0) {
            continue;
        }
        FrameCounterflowObservation candidate;
        candidate.movingCount = movingAgents.size();
        candidate.averageSpeed = speedSum / static_cast<double>(movingAgents.size());
        for (const auto& agent : movingAgents) {
            const auto agentSpeed = speedOf(agent.velocity);
            const auto cosine = dot(anchor.velocity, agent.velocity) / (anchorSpeed * agentSpeed);
            if (cosine >= 0.5) {
                ++candidate.forwardCount;
            } else if (cosine <= kCounterflowCosineThreshold) {
                ++candidate.oppositeCount;
            }
        }
        const auto forwardRatio = static_cast<double>(candidate.forwardCount) / static_cast<double>(candidate.movingCount);
        const auto oppositeRatio = static_cast<double>(candidate.oppositeCount) / static_cast<double>(candidate.movingCount);
        if (forwardRatio >= kCounterflowSideRatioThreshold
            && oppositeRatio >= kCounterflowSideRatioThreshold
            && candidate.averageSpeed <= kCounterflowAverageSpeedThreshold
            && candidate.forwardCount + candidate.oppositeCount > best.forwardCount + best.oppositeCount) {
            best = candidate;
        }
    }

    return best.movingCount == 0 ? std::nullopt : std::optional<FrameCounterflowObservation>{best};
}

struct SustainedCounterflowObservation {
    FrameCounterflowObservation frame{};
    double durationSeconds{0.0};
    double startedAtSeconds{0.0};
    double endedAtSeconds{0.0};
};

std::optional<SustainedCounterflowObservation> sustainedCounterflowConflict(
    const std::vector<SimulationFrame>& frames) {
    if (frames.size() < 2) {
        return std::nullopt;
    }

    std::optional<SustainedCounterflowObservation> current;
    std::optional<SustainedCounterflowObservation> best;
    double previousTimeSeconds = frames.front().elapsedSeconds;
    bool previousWasConflict = false;
    for (const auto& frame : frames) {
        const auto observation = counterflowAtFrame(frame);
        if (observation.has_value()) {
            if (!current.has_value() || !previousWasConflict) {
                current = SustainedCounterflowObservation{
                    .frame = *observation,
                    .durationSeconds = 0.0,
                    .startedAtSeconds = frame.elapsedSeconds,
                    .endedAtSeconds = frame.elapsedSeconds,
                };
            } else {
                current->durationSeconds += std::max(0.0, frame.elapsedSeconds - previousTimeSeconds);
                current->endedAtSeconds = frame.elapsedSeconds;
                if (observation->forwardCount + observation->oppositeCount
                    > current->frame.forwardCount + current->frame.oppositeCount) {
                    current->frame = *observation;
                }
            }
            if (!best.has_value() || current->durationSeconds > best->durationSeconds) {
                best = current;
            }
            previousWasConflict = true;
        } else {
            previousWasConflict = false;
            current.reset();
        }
        previousTimeSeconds = frame.elapsedSeconds;
    }

    if (best.has_value() && best->durationSeconds >= kCounterflowSustainedSecondsThreshold) {
        return best;
    }
    return std::nullopt;
}

std::optional<AlternativeRecommendationRiskSignal> makeCounterflowRiskSignal(
    const AlternativeRecommendationRequest& request) {
    const auto observation = sustainedCounterflowConflict(request.artifacts.replayFrames);
    if (!observation.has_value()) {
        return std::nullopt;
    }

    AlternativeRecommendationRiskSignal signal;
    signal.kind = AlternativeRecommendationRiskKind::CounterflowConflict;
    signal.severity = static_cast<int>(observation->durationSeconds * 10.0)
        + static_cast<int>(observation->frame.forwardCount + observation->frame.oppositeCount);
    signal.summary = "Sustained counterflow conflict detected from replay frames.";
    signal.evidence.push_back(evidence(
        "Opposing flow",
        std::to_string(observation->frame.forwardCount) + " vs "
            + std::to_string(observation->frame.oppositeCount) + " agents",
        "ScenarioResultArtifacts.replayFrames"));
    signal.evidence.push_back(evidence(
        "Average speed",
        fixed(observation->frame.averageSpeed, 2) + " m/s",
        "ScenarioResultArtifacts.replayFrames"));
    signal.evidence.push_back(evidence(
        "Sustained duration",
        fixed(observation->durationSeconds, 1) + " sec",
        "ScenarioResultArtifacts.replayFrames"));
    return signal;
}

std::optional<AlternativeRecommendationRiskSignal> makeTimeLimitRiskSignal(
    const AlternativeRecommendationRequest& request) {
    const auto targetSeconds = request.artifacts.timingSummary.targetTimeSeconds > 0.0
        ? request.artifacts.timingSummary.targetTimeSeconds
        : request.sourceScenario.execution.timeLimitSeconds;
    if (targetSeconds <= 0.0) {
        return std::nullopt;
    }

    const auto finalFrame = finalFrameForRequest(request);
    bool missed = false;
    double overrunSeconds = 0.0;
    std::size_t remainingAgents = 0;
    if (request.artifacts.timingSummary.marginSeconds.has_value() && *request.artifacts.timingSummary.marginSeconds < 0.0) {
        missed = true;
        overrunSeconds = std::max(overrunSeconds, -*request.artifacts.timingSummary.marginSeconds);
    }
    if (request.artifacts.timingSummary.finalEvacuationTimeSeconds.has_value()
        && *request.artifacts.timingSummary.finalEvacuationTimeSeconds > targetSeconds) {
        missed = true;
        overrunSeconds = std::max(overrunSeconds, *request.artifacts.timingSummary.finalEvacuationTimeSeconds - targetSeconds);
    }
    if (finalFrame.has_value() && finalFrame->totalAgentCount > finalFrame->evacuatedAgentCount
        && finalFrame->elapsedSeconds + 1e-9 >= targetSeconds) {
        missed = true;
        remainingAgents = finalFrame->totalAgentCount - finalFrame->evacuatedAgentCount;
    }
    if (!missed) {
        return std::nullopt;
    }

    AlternativeRecommendationRiskSignal signal;
    signal.kind = AlternativeRecommendationRiskKind::TimeLimitMissed;
    signal.severity = static_cast<int>(overrunSeconds) + static_cast<int>(remainingAgents * 10);
    signal.summary = "Time limit missed or unevacuated agents remain at the target time.";
    signal.evidence.push_back(evidence(
        "Target time",
        fixed(targetSeconds, 1) + " sec",
        request.artifacts.timingSummary.targetTimeSeconds > 0.0
            ? "ScenarioResultArtifacts.timingSummary.targetTimeSeconds"
            : "ScenarioDraft.execution.timeLimitSeconds"));
    if (overrunSeconds > 0.0) {
        signal.evidence.push_back(evidence(
            "Overrun",
            fixed(overrunSeconds, 1) + " sec",
            "ScenarioResultArtifacts.timingSummary"));
    }
    if (remainingAgents > 0) {
        signal.evidence.push_back(evidence(
            "Remaining agents",
            std::to_string(remainingAgents),
            request.finalFrame.has_value() ? "AlternativeRecommendationRequest.finalFrame" : "ScenarioResultArtifacts.replayFrames"));
    }
    return signal;
}

std::vector<AlternativeRecommendationRiskSignal> detectRiskSignals(
    const AlternativeRecommendationRequest& request) {
    std::vector<AlternativeRecommendationRiskSignal> signals;
    if (const auto bottleneck = worstBottleneckForKind(request, AlternativeRecommendationRiskKind::ExitBottleneck);
        bottleneck.has_value()) {
        signals.push_back(makeBottleneckRiskSignal(request, *bottleneck, AlternativeRecommendationRiskKind::ExitBottleneck));
    }
    if (const auto bottleneck = worstBottleneckForKind(request, AlternativeRecommendationRiskKind::CorridorBottleneck);
        bottleneck.has_value()) {
        signals.push_back(makeBottleneckRiskSignal(request, *bottleneck, AlternativeRecommendationRiskKind::CorridorBottleneck));
    }
    if (const auto counterflow = makeCounterflowRiskSignal(request); counterflow.has_value()) {
        signals.push_back(*counterflow);
    }
    if (const auto timeLimit = makeTimeLimitRiskSignal(request); timeLimit.has_value()) {
        signals.push_back(*timeLimit);
    }
    if (const auto pressure = makePressureRiskSignal(request); pressure.has_value()) {
        signals.push_back(*pressure);
    }
    return signals;
}

const AlternativeRecommendationRiskSignal* findRiskSignal(
    const std::vector<AlternativeRecommendationRiskSignal>& signals,
    AlternativeRecommendationRiskKind kind) {
    const auto it = std::find_if(signals.begin(), signals.end(), [&](const auto& signal) {
        return signal.kind == kind;
    });
    return it == signals.end() ? nullptr : &(*it);
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
    candidate.summary = "Remove the block on " + connectionName(request.layout, *connectionId) + ".";
    candidate.expectedImprovement = "Restores a constrained exit path and can reduce queueing near blocked connectors.";
    candidate.artifactSource = "ScenarioDraft.control.connectionBlocks + completed result artifacts";
    candidate.evidence.push_back(evidence("Blocked connection", connectionName(request.layout, *connectionId), "ScenarioDraft.control.connectionBlocks"));
    if (const auto* connection = findConnection(request.layout, *connectionId); connection != nullptr) {
        candidate.riskKind = connectionTouchesExit(request.layout, *connection)
            ? AlternativeRecommendationRiskKind::ExitBottleneck
            : AlternativeRecommendationRiskKind::CorridorBottleneck;
    }
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
    if (hasRouteGuidance(request.sourceScenario, targetExit->exitZoneId, {})) {
        return std::nullopt;
    }

    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::BottleneckBypassGuidance,
        "Recommended: guide to " + zoneName(request.layout, targetExit->exitZoneId));
    draft.control.routeGuidances.push_back(makeGuidance(
        "recommendation-guidance-" + sanitizeId(targetExit->exitZoneId),
        targetExit->exitZoneId));
    finalizeDiffKeys(request, draft);

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::BottleneckBypassGuidance;
    candidate.id = "guide-around-" + sanitizeId(bottleneck->connectionId);
    candidate.priority = 20;
    candidate.title = "Guide occupants to another exit";
    candidate.summary = "Install guidance at "
        + zoneName(request.layout, targetExit->exitZoneId)
        + " instead of placing a guidance marker on "
        + connectionName(request.layout, bottleneck->connectionId) + ".";
    candidate.expectedImprovement = "Shifts part of the crowd toward an alternate exit before rerunning the scenario.";
    candidate.artifactSource = "ScenarioRiskSnapshot.bottlenecks + FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage";
    candidate.riskKind = bottleneckRiskKind(request, *bottleneck);
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
            ? "least-used exit from FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage"
            : "least-used non-adjacent exit from FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage"));
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makeExitBalancingCandidate(
    const AlternativeRecommendationRequest& request) {
    if (exitUsageCandidates(request).size() < 2) {
        return std::nullopt;
    }
    const auto low = leastUsedExit(request);
    const auto high = mostUsedExit(request);
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
    candidate.summary = "Guide a share of occupants toward the underused exit "
        + zoneName(request.layout, low->exitZoneId) + ".";
    candidate.expectedImprovement = "Reduces dependence on " + zoneName(request.layout, high->exitZoneId)
        + " and may lower final evacuation time.";
    candidate.artifactSource = "FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage";
    candidate.evidence.push_back(evidence(
        "Most used exit",
        zoneName(request.layout, high->exitZoneId) + " at " + percent(high->usageRatio),
        "ScenarioResultArtifacts.exitUsage"));
    candidate.evidence.push_back(evidence(
        "Underused exit",
        zoneName(request.layout, low->exitZoneId) + " at " + percent(low->usageRatio),
        "FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage"));
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makePressureHotspotCandidate(
    const AlternativeRecommendationRequest& request) {
    if (!hasPressureSignal(request)) {
        return std::nullopt;
    }
    if (exitUsageCandidates(request).size() < 2) {
        return std::nullopt;
    }

    const auto targetExit = leastUsedExit(request);
    if (!targetExit.has_value() || targetExit->exitZoneId.empty()
        || hasRouteGuidance(request.sourceScenario, targetExit->exitZoneId, {})
        || exitHasBottleneck(request, targetExit->exitZoneId)) {
        return std::nullopt;
    }
    if (const auto high = mostUsedExit(request);
        high.has_value()
        && high->exitZoneId != targetExit->exitZoneId
        && high->usageRatio - targetExit->usageRatio >= kExitImbalanceThreshold) {
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
    candidate.summary = "Guide part of the crowd toward "
        + zoneName(request.layout, targetExit->exitZoneId) + " to reduce local pressure.";
    candidate.expectedImprovement = "Moves some agents away from high pressure cells before validating by rerun.";
    candidate.artifactSource = "ScenarioResultArtifacts.pressureSummary + ScenarioRiskSnapshot pressure signals + FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage";
    candidate.riskKind = AlternativeRecommendationRiskKind::PressureHotspot;
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
        "least-used exit from FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage"));
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makeCounterflowCandidate(
    const AlternativeRecommendationRequest& request,
    const std::vector<AlternativeRecommendationRiskSignal>& riskSignals) {
    const auto* signal = findRiskSignal(riskSignals, AlternativeRecommendationRiskKind::CounterflowConflict);
    if (signal == nullptr) {
        return std::nullopt;
    }

    const std::string eventId = "recommendation-counterflow-separation";
    if (hasOperationalEvent(request.sourceScenario, eventId)) {
        return std::nullopt;
    }

    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::CounterflowSeparation,
        "Recommended: separate counterflow movements");
    draft.control.events.push_back(makeOperationalEvent(
        eventId,
        "Separate counterflow movements",
        "Sustained opposing movement detected in replay frames",
        "Use lane separation, time-separated entry, or exit-before-entry operation."));
    finalizeDiffKeys(request, draft);

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::CounterflowSeparation;
    candidate.riskKind = AlternativeRecommendationRiskKind::CounterflowConflict;
    candidate.id = "separate-counterflow";
    candidate.priority = 35;
    candidate.title = "Separate counterflow movements";
    candidate.summary = "Record a lane separation or time-separated entry operation.";
    candidate.expectedImprovement = "Reduces head-on movement conflict and lets the revised operation be compared by rerun.";
    candidate.artifactSource = "AlternativeRecommendationRiskSignal + ScenarioResultArtifacts.replayFrames";
    candidate.evidence = signal->evidence;
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makeStagedEvacuationCandidate(
    const AlternativeRecommendationRequest& request,
    const std::vector<AlternativeRecommendationRiskSignal>& riskSignals) {
    const auto* signal = findRiskSignal(riskSignals, AlternativeRecommendationRiskKind::PressureHotspot);
    if (signal == nullptr) {
        signal = findRiskSignal(riskSignals, AlternativeRecommendationRiskKind::TimeLimitMissed);
    }
    if (signal == nullptr) {
        return std::nullopt;
    }

    const auto stagedSources = makeSequentialStagedEvacuationSources(request);
    if (stagedSources.empty()) {
        return std::nullopt;
    }
    for (const auto& source : stagedSources) {
        if (hasOccupantSource(request.sourceScenario, source.id)) {
            return std::nullopt;
        }
    }
    const auto stagedAgentCount = totalSourceAgents(stagedSources);
    const auto tickCount = totalSourceTicks(stagedSources);
    if (stagedAgentCount == 0 || tickCount == 0) {
        return std::nullopt;
    }
    const auto finalSpawnSeconds = kStagedEvacuationIntervalSeconds * static_cast<double>(tickCount - 1);

    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::StagedEvacuation,
        "Recommended: stage departure batches");
    draft.population.initialPlacements.clear();
    draft.population.occupantSources.insert(
        draft.population.occupantSources.end(),
        stagedSources.begin(),
        stagedSources.end());
    finalizeDiffKeys(request, draft);

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::StagedEvacuation;
    candidate.riskKind = signal->kind;
    candidate.id = "staged-evacuation-batches";
    candidate.priority = 45;
    candidate.title = "Stage groups sequentially";
    candidate.summary = "Release each source group in order: up to " + std::to_string(kStagedEvacuationAgentsPerSpawn)
        + " agents every " + fixed(kStagedEvacuationIntervalSeconds, 0)
        + " sec, then start the next group instead of releasing " + std::to_string(stagedAgentCount) + " at once.";
    candidate.expectedImprovement = "Reduces simultaneous demand at exits or bottlenecks before validating by rerun.";
    candidate.artifactSource = "AlternativeRecommendationRiskSignal + ScenarioDraft.population.initialPlacements";
    candidate.evidence = signal->evidence;
    candidate.evidence.push_back(evidence(
        "Staged groups",
        std::to_string(stagedSources.size()) + " groups / " + std::to_string(stagedAgentCount) + " agents",
        "ScenarioDraft.population.initialPlacements"));
    candidate.evidence.push_back(evidence(
        "Release schedule",
        "sequential groups, up to " + std::to_string(kStagedEvacuationAgentsPerSpawn) + " agents every "
            + fixed(kStagedEvacuationIntervalSeconds, 0) + " sec x " + std::to_string(tickCount) + " batches",
        "recommended OccupantSource2D"));
    candidate.evidence.push_back(evidence(
        "Release window",
        "0-" + fixed(finalSpawnSeconds, 0) + " sec",
        "recommended OccupantSource2D"));
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
    case AlternativeRecommendationKind::CorridorOneWayFlow:
        return "corridor-one-way-flow";
    case AlternativeRecommendationKind::CounterflowSeparation:
        return "counterflow-separation";
    case AlternativeRecommendationKind::StagedEvacuation:
        return "staged-evacuation";
    }
    return "recommendation";
}

const char* alternativeRecommendationRiskKindId(AlternativeRecommendationRiskKind kind) noexcept {
    switch (kind) {
    case AlternativeRecommendationRiskKind::ExitBottleneck:
        return "exit-bottleneck";
    case AlternativeRecommendationRiskKind::CorridorBottleneck:
        return "corridor-bottleneck";
    case AlternativeRecommendationRiskKind::CounterflowConflict:
        return "counterflow-conflict";
    case AlternativeRecommendationRiskKind::TimeLimitMissed:
        return "time-limit-missed";
    case AlternativeRecommendationRiskKind::PressureHotspot:
        return "pressure-hotspot";
    }
    return "risk";
}

AlternativeRecommendationResult AlternativeRecommendationService::recommend(
    const AlternativeRecommendationRequest& request) const {
    AlternativeRecommendationResult result;
    if (!hasCompletedResultArtifactEvidence(request)) {
        result.blockingReasons.push_back(
            "Completed scenario result artifacts are required before SafeCrowd can recommend an operational draft.");
        return result;
    }

    result.riskSignals = detectRiskSignals(request);

    if (const auto candidate = makeBlockedConnectionCandidate(request); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeBottleneckGuidanceCandidate(request); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeExitBalancingCandidate(request); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeCounterflowCandidate(request, result.riskSignals); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makePressureHotspotCandidate(request); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeStagedEvacuationCandidate(request, result.riskSignals); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }

    std::sort(result.candidates.begin(), result.candidates.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.priority < rhs.priority;
    });

    if (result.candidates.empty()) {
        result.blockingReasons.push_back(
            "No detected risk signal, blocked connection, exit imbalance, or pressure hotspot produced an actionable v1 recommendation.");
    }

    return result;
}

}  // namespace safecrowd::domain
