#include "domain/AlternativeRecommendationService.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "domain/GeometryQueries.h"

namespace safecrowd::domain {
namespace {

constexpr double kExitImbalanceThreshold = 0.25;
constexpr std::size_t kMinimumStagedEvacuationAgentsPerSpawn = 4;
constexpr std::size_t kMaximumStagedEvacuationAgentsPerSpawn = 25;
constexpr double kMinimumStagedEvacuationIntervalSeconds = 3.0;
constexpr double kMaximumStagedEvacuationIntervalSeconds = 12.0;
constexpr double kDefaultGuidanceCompliance = 0.5;
constexpr double kMinimumGuidanceCompliance = 0.45;
constexpr double kMaximumGuidanceCompliance = 0.85;
constexpr double kDefaultGuidanceInfluenceRadiusMeters = 2.5;
constexpr double kMaximumGuidanceInfluenceRadiusMeters = 10.0;
constexpr double kDefaultGuidanceMaxDetourMeters = 20.0;
constexpr double kMaximumGuidanceMaxDetourMeters = 60.0;
constexpr double kMaximumGuidanceDetourEstimateMeters = 35.0;

struct AlternativeRecommendationInput {
    const FacilityLayout2D& layout;
    const ScenarioDraft& sourceScenario;
    const ScenarioDraft* baselineScenario{nullptr};
    const ScenarioRiskSnapshot& risk;
    const ScenarioResultArtifacts& artifacts;
    const SimulationFrame* finalFrame{nullptr};
};

struct SourceAnchor {
    std::string id{};
    std::string zoneId{};
    std::string floorId{};
    Point2D position{};
    std::size_t agentCount{0};
};

struct GuidanceInstallAnchor {
    std::string connectionId{};
    std::string floorId{};
    std::string zoneId{};
    Point2D position{};
    std::string description{};
    std::string evidenceSource{};
};

struct StagedEvacuationProfile {
    std::size_t agentsPerSpawn{0};
    double intervalSeconds{0.0};
};

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

double distanceBetweenPoints(const Point2D& lhs, const Point2D& rhs) {
    return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y);
}

Point2D segmentMidpoint(const LineSegment2D& segment) {
    return {
        .x = (segment.start.x + segment.end.x) * 0.5,
        .y = (segment.start.y + segment.end.y) * 0.5,
    };
}

std::string pointLabel(const Point2D& point) {
    return "(" + fixed(point.x, 1) + ", " + fixed(point.y, 1) + ")";
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

std::string floorIdForZone(const FacilityLayout2D& layout, const std::string& zoneId) {
    const auto* zone = findZone(layout, zoneId);
    return zone == nullptr ? std::string{} : zone->floorId;
}

bool zoneGeometryContainsPoint(const Zone2D& zone, const Point2D& point) {
    return zone.area.outline.size() >= 3 && pointInPolygon(zone.area, point);
}

std::string zoneIdContainingPoint(
    const FacilityLayout2D& layout,
    const Point2D& point,
    const std::string& floorId) {
    for (const auto& zone : layout.zones) {
        if (!matchesFloor(zone.floorId, floorId) || !zoneGeometryContainsPoint(zone, point)) {
            continue;
        }
        return zone.id;
    }
    return {};
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

bool hasCompletedResultArtifactEvidence(const AlternativeRecommendationInput& request) {
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
        || request.finalFrame != nullptr;
}

bool containsString(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

void appendUniqueString(std::vector<std::string>& values, const std::string& value) {
    if (!value.empty() && !containsString(values, value)) {
        values.push_back(value);
    }
}

bool exitUsageContainsZone(const ScenarioResultArtifacts& artifacts, const std::string& zoneId) {
    return std::any_of(artifacts.exitUsage.begin(), artifacts.exitUsage.end(), [&](const auto& usage) {
        return usage.exitZoneId == zoneId;
    });
}

std::vector<ExitUsageMetric> exitUsageCandidates(const AlternativeRecommendationInput& request) {
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

std::optional<ExitUsageMetric> mostUsedExit(const AlternativeRecommendationInput& request) {
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

bool hasRouteGuidanceForExit(const ScenarioDraft& scenario,
                             const std::string& guidedExitZoneId) {
    return std::any_of(scenario.control.routeGuidances.begin(), scenario.control.routeGuidances.end(), [&](const auto& guidance) {
        return guidance.guidedExitZoneId == guidedExitZoneId;
    });
}

bool exitHasBottleneck(
    const AlternativeRecommendationInput& request,
    const std::string& exitZoneId) {
    return std::any_of(request.risk.bottlenecks.begin(), request.risk.bottlenecks.end(), [&](const auto& bottleneck) {
        const auto* connection = findConnection(request.layout, bottleneck.connectionId);
        return connection != nullptr
            && connectionTouchesExit(request.layout, *connection)
            && connectionTouchesZone(*connection, exitZoneId);
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

std::size_t totalInitialPlacementAgents(const ScenarioDraft& scenario) {
    std::size_t total = 0;
    for (const auto& placement : scenario.population.initialPlacements) {
        total += placementAgentCount(placement);
    }
    return total;
}

StagedEvacuationProfile stagedEvacuationProfileFor(
    const AlternativeRecommendationInput& request,
    int severity) {
    const auto totalAgents = totalInitialPlacementAgents(request.sourceScenario);
    if (totalAgents == 0) {
        return {};
    }

    const auto normalizedSeverity = std::clamp(static_cast<double>(std::max(0, severity)) / 160.0, 0.0, 1.0);
    const auto desiredBatchCount = 2.0 + (normalizedSeverity * 8.0);
    const auto agentsPerSpawn = static_cast<std::size_t>(
        std::ceil(static_cast<double>(totalAgents) / desiredBatchCount));
    return {
        .agentsPerSpawn = std::clamp(
            agentsPerSpawn,
            kMinimumStagedEvacuationAgentsPerSpawn,
            kMaximumStagedEvacuationAgentsPerSpawn),
        .intervalSeconds = std::clamp(
            kMinimumStagedEvacuationIntervalSeconds + (normalizedSeverity * 7.0),
            kMinimumStagedEvacuationIntervalSeconds,
            kMaximumStagedEvacuationIntervalSeconds),
    };
}

std::size_t stagedEvacuationTickCount(std::size_t targetAgentCount, std::size_t agentsPerSpawn) {
    if (targetAgentCount == 0 || agentsPerSpawn == 0) {
        return 0;
    }
    return (targetAgentCount + agentsPerSpawn - 1) / agentsPerSpawn;
}

OccupantSource2D makeStagedEvacuationSource(
    const AlternativeRecommendationInput& request,
    const InitialPlacement2D& placement,
    std::size_t targetAgentCount,
    Point2D sourcePosition,
    const StagedEvacuationProfile& profile,
    double startSeconds) {
    const auto tickCount = stagedEvacuationTickCount(targetAgentCount, profile.agentsPerSpawn);
    OccupantSource2D source;
    source.id = "recommendation-source-" + sanitizeId(placement.id.empty() ? placement.zoneId : placement.id);
    source.zoneId = placement.zoneId;
    source.floorId = placement.floorId;
    source.position = sourcePosition;
    source.targetAgentCount = targetAgentCount;
    source.agentsPerSpawn = std::min(profile.agentsPerSpawn, targetAgentCount);
    source.startSeconds = startSeconds;
    source.endSeconds = startSeconds + (profile.intervalSeconds * static_cast<double>(tickCount));
    source.spawnIntervalSeconds = profile.intervalSeconds;
    source.initialVelocity = placement.initialVelocity;
    if (source.floorId.empty()) {
        if (const auto* zone = findZone(request.layout, placement.zoneId); zone != nullptr) {
            source.floorId = zone->floorId;
        }
    }
    return source;
}

std::vector<OccupantSource2D> makeSequentialStagedEvacuationSources(
    const AlternativeRecommendationInput& request,
    const StagedEvacuationProfile& profile) {
    if (profile.agentsPerSpawn == 0 || profile.intervalSeconds <= 0.0) {
        return {};
    }

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
        auto source = makeStagedEvacuationSource(
            request,
            placement,
            targetAgentCount,
            *sourcePosition,
            profile,
            nextStartSeconds);
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
        total += stagedEvacuationTickCount(source.targetAgentCount, source.agentsPerSpawn);
    }
    return total;
}

std::vector<std::string> adjacentExitZoneIdsForConnection(
    const AlternativeRecommendationInput& request,
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
                                double baseComplianceRate = kDefaultGuidanceCompliance) {
    RouteGuidanceDraft guidance;
    guidance.id = id;
    guidance.guidedExitZoneId = guidedExitZoneId;
    guidance.baseComplianceRate = std::clamp(baseComplianceRate, 0.0, 1.0);
    guidance.influenceRadiusMeters = kDefaultGuidanceInfluenceRadiusMeters;
    guidance.maxDetourMeters = kDefaultGuidanceMaxDetourMeters;
    return guidance;
}

std::string recommendedScenarioId(const ScenarioDraft& source, AlternativeRecommendationKind kind) {
    const auto sourceId = source.scenarioId.empty() ? "scenario" : source.scenarioId;
    return sourceId + "-recommended-" + alternativeRecommendationKindId(kind);
}

ScenarioDraft makeRecommendedDraft(const AlternativeRecommendationInput& request,
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

void finalizeDiffKeys(const AlternativeRecommendationInput& request, ScenarioDraft& draft) {
    if (request.baselineScenario != nullptr) {
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

bool sourceHasActiveConnectionBlockAt(
    const ScenarioDraft& scenario,
    const std::string& connectionId,
    const std::optional<double>& elapsedSeconds) {
    return std::any_of(scenario.control.connectionBlocks.begin(), scenario.control.connectionBlocks.end(), [&](const auto& block) {
        if (block.connectionId != connectionId) {
            return false;
        }
        return !elapsedSeconds.has_value() || connectionBlockActiveAt(block, *elapsedSeconds);
    });
}

bool recommendedDraftChangesSource(
    const AlternativeRecommendationInput& request,
    const ScenarioDraft& draft) {
    return !computeScenarioDiffKeys(request.sourceScenario, draft).empty();
}

std::optional<Point2D> zoneReferencePoint(const FacilityLayout2D& layout, const std::string& zoneId) {
    const auto* zone = findZone(layout, zoneId);
    if (zone == nullptr || zone->area.outline.empty()) {
        return std::nullopt;
    }
    if (zone->area.outline.size() == 1) {
        return zone->area.outline.front();
    }
    return representativePointInPolygon(zone->area).value_or(averagePoint(zone->area.outline));
}

std::vector<SourceAnchor> sourceAnchorsForRequest(const AlternativeRecommendationInput& request) {
    std::vector<SourceAnchor> anchors;
    anchors.reserve(
        request.sourceScenario.population.initialPlacements.size()
        + request.sourceScenario.population.occupantSources.size());

    for (const auto& placement : request.sourceScenario.population.initialPlacements) {
        const auto targetAgentCount = placementAgentCount(placement);
        if (targetAgentCount == 0) {
            continue;
        }
        const auto position = sourcePointForPlacement(request.layout, placement);
        if (!position.has_value()) {
            continue;
        }
        auto floorId = placement.floorId;
        if (floorId.empty()) {
            floorId = floorIdForZone(request.layout, placement.zoneId);
        }
        anchors.push_back({
            .id = placement.id,
            .zoneId = placement.zoneId,
            .floorId = std::move(floorId),
            .position = *position,
            .agentCount = targetAgentCount,
        });
    }

    for (const auto& source : request.sourceScenario.population.occupantSources) {
        if (source.targetAgentCount == 0) {
            continue;
        }
        auto floorId = source.floorId;
        if (floorId.empty()) {
            floorId = floorIdForZone(request.layout, source.zoneId);
        }
        anchors.push_back({
            .id = source.id,
            .zoneId = source.zoneId,
            .floorId = std::move(floorId),
            .position = source.position,
            .agentCount = source.targetAgentCount,
        });
    }

    return anchors;
}

std::optional<SourceAnchor> bestSourceAnchor(
    const AlternativeRecommendationInput& request,
    const std::optional<Point2D>& preferredPoint = std::nullopt,
    const std::string& preferredFloorId = {}) {
    const auto anchors = sourceAnchorsForRequest(request);
    if (anchors.empty()) {
        return std::nullopt;
    }

    const SourceAnchor* best = nullptr;
    double bestDistance = 0.0;
    for (const auto& anchor : anchors) {
        if (!preferredFloorId.empty() && !matchesFloor(anchor.floorId, preferredFloorId)) {
            continue;
        }
        const auto distance = preferredPoint.has_value()
            ? distanceBetweenPoints(anchor.position, *preferredPoint)
            : 0.0;
        if (best == nullptr
            || (preferredPoint.has_value() && distance < bestDistance)
            || (!preferredPoint.has_value() && anchor.agentCount > best->agentCount)
            || (preferredPoint.has_value() && distance == bestDistance && anchor.agentCount > best->agentCount)) {
            best = &anchor;
            bestDistance = distance;
        }
    }

    if (best == nullptr && !preferredFloorId.empty()) {
        return bestSourceAnchor(request, preferredPoint);
    }
    return best == nullptr ? std::nullopt : std::optional<SourceAnchor>{*best};
}

std::optional<GuidanceInstallAnchor> sourceGuidanceAnchor(
    const AlternativeRecommendationInput& request,
    const std::optional<Point2D>& preferredPoint = std::nullopt,
    const std::string& preferredFloorId = {},
    const std::string& evidenceSource = "ScenarioDraft.population") {
    const auto anchor = bestSourceAnchor(request, preferredPoint, preferredFloorId);
    if (!anchor.has_value()) {
        return std::nullopt;
    }

    auto installFloorId = anchor->floorId;
    auto installZoneId = zoneIdContainingPoint(request.layout, anchor->position, installFloorId);
    if (installFloorId.empty() && !installZoneId.empty()) {
        installFloorId = floorIdForZone(request.layout, installZoneId);
    }

    std::string description = "near source";
    if (!anchor->zoneId.empty()) {
        description += " " + zoneName(request.layout, anchor->zoneId);
    }
    description += " at " + pointLabel(anchor->position);
    if (!installFloorId.empty()) {
        description += " on " + installFloorId;
    }

    return GuidanceInstallAnchor{
        .floorId = std::move(installFloorId),
        .zoneId = std::move(installZoneId),
        .position = anchor->position,
        .description = std::move(description),
        .evidenceSource = evidenceSource,
    };
}

GuidanceInstallAnchor pointGuidanceAnchor(
    const AlternativeRecommendationInput& request,
    const Point2D& point,
    std::string floorId,
    std::string description,
    std::string evidenceSource) {
    auto installZoneId = zoneIdContainingPoint(request.layout, point, floorId);
    if (floorId.empty() && !installZoneId.empty()) {
        floorId = floorIdForZone(request.layout, installZoneId);
    }
    if (floorId.empty()) {
        if (const auto source = bestSourceAnchor(request, point); source.has_value()) {
            floorId = source->floorId;
        }
    }

    if (!installZoneId.empty()) {
        description += " in " + zoneName(request.layout, installZoneId);
    }
    description += " at " + pointLabel(point);
    if (!floorId.empty()) {
        description += " on " + floorId;
    }

    return GuidanceInstallAnchor{
        .floorId = std::move(floorId),
        .zoneId = std::move(installZoneId),
        .position = point,
        .description = std::move(description),
        .evidenceSource = std::move(evidenceSource),
    };
}

std::optional<GuidanceInstallAnchor> connectionGuidanceAnchor(
    const AlternativeRecommendationInput& request,
    const std::string& connectionId,
    std::string evidenceSource) {
    const auto* connection = findConnection(request.layout, connectionId);
    if (connection == nullptr) {
        return std::nullopt;
    }

    auto floorId = connection->floorId;
    if (floorId.empty()) {
        floorId = floorIdForZone(request.layout, connection->fromZoneId);
    }
    if (floorId.empty()) {
        floorId = floorIdForZone(request.layout, connection->toZoneId);
    }

    auto description = connectionName(request.layout, connectionId);
    if (!floorId.empty()) {
        description += " on " + floorId;
    }

    return GuidanceInstallAnchor{
        .connectionId = connectionId,
        .floorId = std::move(floorId),
        .position = segmentMidpoint(connection->centerSpan),
        .description = std::move(description),
        .evidenceSource = std::move(evidenceSource),
    };
}

std::optional<GuidanceInstallAnchor> pressureGuidanceAnchor(const AlternativeRecommendationInput& request) {
    if (request.artifacts.pressureSummary.peakCell.has_value()) {
        const auto& cell = *request.artifacts.pressureSummary.peakCell;
        return pointGuidanceAnchor(
            request,
            cell.center,
            cell.floorId,
            "pressure peak cell",
            "ScenarioResultArtifacts.pressureSummary.peakCell");
    }

    if (!request.artifacts.pressureSummary.peakHotspots.empty()) {
        const auto it = std::max_element(
            request.artifacts.pressureSummary.peakHotspots.begin(),
            request.artifacts.pressureSummary.peakHotspots.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.pressureScore < rhs.pressureScore;
            });
        return pointGuidanceAnchor(
            request,
            it->center,
            it->floorId,
            "pressure hotspot",
            "ScenarioResultArtifacts.pressureSummary.peakHotspots");
    }

    if (!request.risk.pressureHotspots.empty()) {
        const auto it = std::max_element(
            request.risk.pressureHotspots.begin(),
            request.risk.pressureHotspots.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.pressureScore < rhs.pressureScore;
            });
        return pointGuidanceAnchor(
            request,
            it->center,
            it->floorId,
            "risk pressure hotspot",
            "ScenarioRiskSnapshot.pressureHotspots");
    }

    if (!request.risk.criticalPressureEvents.empty()) {
        const auto it = std::max_element(
            request.risk.criticalPressureEvents.begin(),
            request.risk.criticalPressureEvents.end(),
            [](const auto& lhs, const auto& rhs) {
                if (lhs.criticalAgentCount == rhs.criticalAgentCount) {
                    return lhs.pressureScore < rhs.pressureScore;
                }
                return lhs.criticalAgentCount < rhs.criticalAgentCount;
            });
        return pointGuidanceAnchor(
            request,
            it->center,
            it->floorId,
            "critical pressure event",
            "ScenarioRiskSnapshot.criticalPressureEvents");
    }

    return sourceGuidanceAnchor(request, std::nullopt, {}, "ScenarioDraft.population fallback");
}

std::optional<GuidanceInstallAnchor> operationalConflictGuidanceAnchor(const AlternativeRecommendationInput& request) {
    if (!request.risk.operationalConflictCells.empty()) {
        const auto it = std::max_element(
            request.risk.operationalConflictCells.begin(),
            request.risk.operationalConflictCells.end(),
            [](const auto& lhs, const auto& rhs) {
                if (lhs.conflictScore == rhs.conflictScore) {
                    return lhs.exposureAgentSeconds < rhs.exposureAgentSeconds;
                }
                return lhs.conflictScore < rhs.conflictScore;
            });
        return pointGuidanceAnchor(
            request,
            it->center,
            it->floorId,
            "operational-conflict hotspot",
            "ScenarioRiskSnapshot.operationalConflictCells");
    }
    return sourceGuidanceAnchor(request, std::nullopt, {}, "ScenarioDraft.population fallback");
}

std::optional<double> anchorDistanceToZone(
    const AlternativeRecommendationInput& request,
    const GuidanceInstallAnchor& anchor,
    const std::string& targetZoneId) {
    const auto targetPoint = zoneReferencePoint(request.layout, targetZoneId);
    if (!targetPoint.has_value()) {
        return std::nullopt;
    }
    return distanceBetweenPoints(anchor.position, *targetPoint);
}

double guidanceInfluenceRadiusForSeverity(int severity) {
    return std::clamp(
        kDefaultGuidanceInfluenceRadiusMeters + (static_cast<double>(std::max(0, severity)) / 35.0),
        kDefaultGuidanceInfluenceRadiusMeters,
        kMaximumGuidanceInfluenceRadiusMeters);
}

double guidanceMaxDetourForSeverity(
    int severity,
    const std::optional<double>& anchorDistanceToTarget) {
    auto detourMeters = kDefaultGuidanceMaxDetourMeters + (static_cast<double>(std::max(0, severity)) / 5.0);
    if (anchorDistanceToTarget.has_value()) {
        detourMeters = std::max(detourMeters, *anchorDistanceToTarget * 0.75);
    }
    return std::clamp(
        detourMeters,
        kDefaultGuidanceMaxDetourMeters,
        kMaximumGuidanceMaxDetourMeters);
}

void configureGuidanceForRecommendation(
    RouteGuidanceDraft& guidance,
    const AlternativeRecommendationInput& request,
    const GuidanceInstallAnchor& anchor,
    int severity) {
    guidance.installConnectionId = anchor.connectionId;
    guidance.installFloorId = anchor.floorId;
    guidance.installZoneId = anchor.zoneId;
    guidance.installPosition = anchor.position;
    guidance.influenceRadiusMeters = guidanceInfluenceRadiusForSeverity(severity);
    guidance.maxDetourMeters = guidanceMaxDetourForSeverity(
        severity,
        anchorDistanceToZone(request, anchor, guidance.guidedExitZoneId));
}

AlternativeRecommendationEvidence guidanceInstallEvidence(const GuidanceInstallAnchor& anchor) {
    return evidence("Guidance install", anchor.description, anchor.evidenceSource);
}

AlternativeRecommendationEvidence guidanceTuningEvidence(const RouteGuidanceDraft& guidance) {
    return evidence(
        "Guidance tuning",
        percent(guidance.baseComplianceRate) + " compliance / "
            + fixed(guidance.influenceRadiusMeters, 1) + " m radius / "
            + fixed(guidance.maxDetourMeters, 0) + " m max detour",
        "recommended RouteGuidanceDraft");
}

std::vector<std::string> sourceZoneIds(const ScenarioDraft& scenario) {
    std::vector<std::string> zones;
    for (const auto& placement : scenario.population.initialPlacements) {
        appendUniqueString(zones, placement.zoneId);
    }
    for (const auto& source : scenario.population.occupantSources) {
        appendUniqueString(zones, source.zoneId);
    }
    return zones;
}

bool connectionTraversableFromZone(
    const ScenarioDraft& scenario,
    const Connection2D& connection,
    const std::string& zoneId,
    std::string& nextZoneId) {
    if (connection.directionality == TravelDirection::Closed
        || sourceHasConnectionBlock(scenario, connection.id)) {
        return false;
    }
    if (connection.fromZoneId == zoneId && connection.directionality != TravelDirection::ReverseOnly) {
        nextZoneId = connection.toZoneId;
        return !nextZoneId.empty();
    }
    if (connection.toZoneId == zoneId && connection.directionality != TravelDirection::ForwardOnly) {
        nextZoneId = connection.fromZoneId;
        return !nextZoneId.empty();
    }
    return false;
}

bool routeExistsBetweenZones(
    const FacilityLayout2D& layout,
    const ScenarioDraft& scenario,
    const std::string& startZoneId,
    const std::string& targetZoneId) {
    if (startZoneId.empty() || targetZoneId.empty()) {
        return false;
    }
    if (startZoneId == targetZoneId) {
        return true;
    }

    std::vector<std::string> pending{startZoneId};
    std::unordered_set<std::string> visited{startZoneId};
    for (std::size_t index = 0; index < pending.size(); ++index) {
        const auto zoneId = pending[index];
        for (const auto& connection : layout.connections) {
            std::string nextZoneId;
            if (!connectionTraversableFromZone(scenario, connection, zoneId, nextZoneId)
                || visited.find(nextZoneId) != visited.end()) {
                continue;
            }
            if (nextZoneId == targetZoneId) {
                return true;
            }
            visited.insert(nextZoneId);
            pending.push_back(std::move(nextZoneId));
        }
    }
    return false;
}

bool exitReachableFromScenarioSources(
    const AlternativeRecommendationInput& request,
    const std::string& exitZoneId) {
    if (exitZoneId.empty()) {
        return false;
    }
    const auto* exitZone = findZone(request.layout, exitZoneId);
    if (exitZone == nullptr || exitZone->kind != ZoneKind::Exit) {
        return false;
    }
    const auto zones = sourceZoneIds(request.sourceScenario);
    if (zones.empty()) {
        return false;
    }
    return std::any_of(zones.begin(), zones.end(), [&](const auto& zoneId) {
        return routeExistsBetweenZones(request.layout, request.sourceScenario, zoneId, exitZoneId);
    });
}

std::optional<double> nearestSourceDistanceToZone(
    const AlternativeRecommendationInput& request,
    const std::string& targetZoneId) {
    const auto targetPoint = zoneReferencePoint(request.layout, targetZoneId);
    if (!targetPoint.has_value()) {
        return std::nullopt;
    }

    std::optional<double> best;
    for (const auto& zoneId : sourceZoneIds(request.sourceScenario)) {
        const auto sourcePoint = zoneReferencePoint(request.layout, zoneId);
        if (!sourcePoint.has_value()) {
            continue;
        }
        const auto distance = distanceBetweenPoints(*sourcePoint, *targetPoint);
        if (!best.has_value() || distance < *best) {
            best = distance;
        }
    }
    return best;
}

bool guidanceDetourAcceptable(
    const AlternativeRecommendationInput& request,
    const std::string& currentExitZoneId,
    const std::string& targetExitZoneId) {
    const auto currentDistance = nearestSourceDistanceToZone(request, currentExitZoneId);
    const auto targetDistance = nearestSourceDistanceToZone(request, targetExitZoneId);
    if (!currentDistance.has_value() || !targetDistance.has_value()) {
        return true;
    }
    return *targetDistance - *currentDistance <= kMaximumGuidanceDetourEstimateMeters;
}

std::optional<ExitUsageMetric> leastUsedReachableExit(
    const AlternativeRecommendationInput& request,
    const std::vector<std::string>& excludedExitZoneIds = {}) {
    const auto candidates = exitUsageCandidates(request);
    std::optional<ExitUsageMetric> best;
    for (const auto& usage : candidates) {
        if (containsString(excludedExitZoneIds, usage.exitZoneId)
            || !exitReachableFromScenarioSources(request, usage.exitZoneId)) {
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

bool bottleneckLessSevere(const ScenarioBottleneckMetric& lhs, const ScenarioBottleneckMetric& rhs) {
    if (lhs.stalledAgentCount == rhs.stalledAgentCount) {
        return lhs.nearbyAgentCount < rhs.nearbyAgentCount;
    }
    return lhs.stalledAgentCount < rhs.stalledAgentCount;
}

std::optional<std::string> blockedConnectionToRelieve(const AlternativeRecommendationInput& request) {
    if (request.sourceScenario.control.connectionBlocks.empty()) {
        return std::nullopt;
    }

    std::optional<ScenarioBottleneckMetric> best;
    for (const auto& bottleneck : request.risk.bottlenecks) {
        if (!bottleneck.connectionId.empty()
            && sourceHasActiveConnectionBlockAt(
                request.sourceScenario,
                bottleneck.connectionId,
                bottleneck.detectedAtSeconds)) {
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
    const AlternativeRecommendationInput& request,
    const ScenarioBottleneckMetric& bottleneck) {
    const auto* connection = findConnection(request.layout, bottleneck.connectionId);
    if (connection != nullptr && connectionTouchesExit(request.layout, *connection)) {
        return AlternativeRecommendationRiskKind::ExitBottleneck;
    }
    return AlternativeRecommendationRiskKind::CorridorBottleneck;
}

std::optional<ScenarioBottleneckMetric> worstBottleneckForKind(
    const AlternativeRecommendationInput& request,
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
    const AlternativeRecommendationInput& request,
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

bool hasPressureSignal(const AlternativeRecommendationInput& request) {
    return (request.artifacts.pressureSummary.hotspotScoreThreshold > 0.0
            && request.artifacts.pressureSummary.peakPressureScore >= request.artifacts.pressureSummary.hotspotScoreThreshold)
        || !request.artifacts.pressureSummary.peakHotspots.empty()
        || !request.artifacts.pressureSummary.criticalEvents.empty()
        || !request.risk.pressureHotspots.empty()
        || !request.risk.criticalPressureEvents.empty()
        || request.risk.criticalPressureAgentCount > 0;
}

std::optional<AlternativeRecommendationRiskSignal> makePressureRiskSignal(
    const AlternativeRecommendationInput& request) {
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

const SimulationFrame* finalFrameForRequest(const AlternativeRecommendationInput& request) {
    if (request.finalFrame != nullptr) {
        return request.finalFrame;
    }
    if (!request.artifacts.replayFrames.empty()) {
        return &request.artifacts.replayFrames.back();
    }
    return nullptr;
}

std::optional<AlternativeRecommendationRiskSignal> makeOperationalConflictRiskSignal(
    const AlternativeRecommendationInput& request) {
    if (request.risk.operationalConflictCells.empty()
        && request.risk.operationalConflictConnections.empty()
        && request.artifacts.operationalConflictSummary.peakConflictScore <= 0.0) {
        return std::nullopt;
    }

    AlternativeRecommendationRiskSignal signal;
    signal.kind = AlternativeRecommendationRiskKind::OperationalConflict;
    signal.summary = "Operational conflict detected from opposing movement intent at shared passages.";

    double severity = request.artifacts.operationalConflictSummary.peakConflictScore * 100.0;
    severity += request.artifacts.operationalConflictSummary.longestConflictDurationSeconds * 4.0;
    severity += request.artifacts.operationalConflictSummary.totalConflictExposureAgentSeconds * 0.2;
    severity += static_cast<double>(request.artifacts.operationalConflictSummary.conflictConnectionCount * 10U);

    if (!request.risk.operationalConflictConnections.empty()) {
        const auto& connection = request.risk.operationalConflictConnections.front();
        severity += static_cast<double>(connection.forwardCount + connection.reverseCount);
        signal.evidence.push_back(evidence(
            "Conflict connection",
            connection.label.empty() ? connection.connectionId : connection.label,
            "ScenarioRiskSnapshot.operationalConflictConnections"));
        signal.evidence.push_back(evidence(
            "Opposing intent",
            std::to_string(connection.forwardCount) + " forward / "
                + std::to_string(connection.reverseCount) + " reverse",
            "ScenarioRiskSnapshot.operationalConflictConnections"));
        signal.evidence.push_back(evidence(
            "Opposition score",
            fixed(connection.oppositionScore, 2),
            "ScenarioRiskSnapshot.operationalConflictConnections"));
        signal.evidence.push_back(evidence(
            "Queued agents",
            std::to_string(connection.queueAgentCount),
            "ScenarioRiskSnapshot.operationalConflictConnections"));
    } else if (!request.risk.operationalConflictCells.empty()) {
        const auto& cell = request.risk.operationalConflictCells.front();
        severity += static_cast<double>(cell.forwardCount + cell.reverseCount);
        signal.evidence.push_back(evidence(
            "Conflict cell",
            std::to_string(cell.forwardCount) + " forward / "
                + std::to_string(cell.reverseCount) + " reverse movers",
            "ScenarioRiskSnapshot.operationalConflictCells"));
        signal.evidence.push_back(evidence(
            "Conflict duration",
            fixed(cell.durationSeconds, 1) + " sec",
            "ScenarioRiskSnapshot.operationalConflictCells"));
        signal.evidence.push_back(evidence(
            "Opposition score",
            fixed(cell.oppositionScore, 2),
            "ScenarioRiskSnapshot.operationalConflictCells"));
        signal.evidence.push_back(evidence(
            "Average speed",
            fixed(cell.averageSpeed, 2) + " m/s",
            "ScenarioRiskSnapshot.operationalConflictCells"));
    }

    signal.evidence.push_back(evidence(
        "Peak conflict score",
        fixed(request.artifacts.operationalConflictSummary.peakConflictScore, 2),
        "ScenarioResultArtifacts.operationalConflictSummary"));
    signal.evidence.push_back(evidence(
        "Conflict exposure",
        fixed(request.artifacts.operationalConflictSummary.totalConflictExposureAgentSeconds, 1) + " agent-sec",
        "ScenarioResultArtifacts.operationalConflictSummary"));
    signal.evidence.push_back(evidence(
        "Conflict connections",
        std::to_string(request.artifacts.operationalConflictSummary.conflictConnectionCount),
        "ScenarioResultArtifacts.operationalConflictSummary"));

    signal.severity = static_cast<int>(std::round(severity));
    return signal;
}

std::optional<AlternativeRecommendationRiskSignal> makeTimeLimitRiskSignal(
    const AlternativeRecommendationInput& request) {
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
    if (finalFrame != nullptr && finalFrame->totalAgentCount > finalFrame->evacuatedAgentCount
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
            request.finalFrame != nullptr ? "AlternativeRecommendationRequest.finalFrame" : "ScenarioResultArtifacts.replayFrames"));
    }
    return signal;
}

std::vector<AlternativeRecommendationRiskSignal> detectRiskSignals(
    const AlternativeRecommendationInput& request) {
    std::vector<AlternativeRecommendationRiskSignal> signals;
    if (const auto bottleneck = worstBottleneckForKind(request, AlternativeRecommendationRiskKind::ExitBottleneck);
        bottleneck.has_value()) {
        signals.push_back(makeBottleneckRiskSignal(request, *bottleneck, AlternativeRecommendationRiskKind::ExitBottleneck));
    }
    if (const auto bottleneck = worstBottleneckForKind(request, AlternativeRecommendationRiskKind::CorridorBottleneck);
        bottleneck.has_value()) {
        signals.push_back(makeBottleneckRiskSignal(request, *bottleneck, AlternativeRecommendationRiskKind::CorridorBottleneck));
    }
    if (const auto operationalConflict = makeOperationalConflictRiskSignal(request); operationalConflict.has_value()) {
        signals.push_back(*operationalConflict);
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

struct RecommendationContext {
    std::vector<AlternativeRecommendationRiskSignal> riskSignals{};
    std::optional<ScenarioBottleneckMetric> worstBottleneck{};
    std::optional<ScenarioBottleneckMetric> worstExitBottleneck{};
    std::optional<ScenarioBottleneckMetric> worstCorridorBottleneck{};
    std::optional<ExitUsageMetric> mostUsedExit{};
    std::optional<ExitUsageMetric> leastUsedReachableExit{};
    double exitUsageSpread{0.0};
    bool exitUsageImbalanced{false};
};

RecommendationContext buildRecommendationContext(const AlternativeRecommendationInput& request) {
    RecommendationContext context;
    context.riskSignals = detectRiskSignals(request);
    context.worstBottleneck = worstBottleneck(request.risk);
    context.worstExitBottleneck = worstBottleneckForKind(request, AlternativeRecommendationRiskKind::ExitBottleneck);
    context.worstCorridorBottleneck = worstBottleneckForKind(request, AlternativeRecommendationRiskKind::CorridorBottleneck);
    context.mostUsedExit = mostUsedExit(request);
    context.leastUsedReachableExit = leastUsedReachableExit(request);
    if (context.mostUsedExit.has_value() && context.leastUsedReachableExit.has_value()) {
        context.exitUsageSpread =
            context.mostUsedExit->usageRatio - context.leastUsedReachableExit->usageRatio;
        context.exitUsageImbalanced = context.mostUsedExit->exitZoneId != context.leastUsedReachableExit->exitZoneId
            && context.exitUsageSpread >= kExitImbalanceThreshold;
    }
    return context;
}

const AlternativeRecommendationRiskSignal* findRiskSignal(
    const RecommendationContext& context,
    AlternativeRecommendationRiskKind kind) {
    return findRiskSignal(context.riskSignals, kind);
}

int riskSeverity(const RecommendationContext& context, AlternativeRecommendationRiskKind kind) {
    const auto* signal = findRiskSignal(context, kind);
    return signal == nullptr ? 0 : signal->severity;
}

int priorityForCandidate(int basePriority, int severity, bool simulationAffecting) {
    const auto severityBonus = std::clamp(severity / 8, 0, 35);
    return basePriority - severityBonus + (simulationAffecting ? 0 : 100);
}

int exitUsageSeverity(const RecommendationContext& context) {
    if (!context.mostUsedExit.has_value() || !context.leastUsedReachableExit.has_value()) {
        return 0;
    }
    return static_cast<int>(std::round(std::max(0.0, context.exitUsageSpread) * 100.0))
        + static_cast<int>(context.mostUsedExit->evacuatedCount / 2U);
}

double guidanceComplianceForSeverity(int severity) {
    return std::clamp(
        kMinimumGuidanceCompliance + (static_cast<double>(std::max(0, severity)) / 200.0),
        kMinimumGuidanceCompliance,
        kMaximumGuidanceCompliance);
}

double guidanceComplianceForExitSpread(double spread) {
    return std::clamp(
        kMinimumGuidanceCompliance + (std::max(0.0, spread) * 0.45),
        kMinimumGuidanceCompliance,
        0.80);
}

std::optional<AlternativeRecommendationCandidate> makeBlockedConnectionCandidate(
    const AlternativeRecommendationInput& request,
    const RecommendationContext& context) {
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
    if (!recommendedDraftChangesSource(request, draft)) {
        return std::nullopt;
    }

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::BlockedConnectionRelief;
    candidate.id = "open-" + sanitizeId(*connectionId);
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
    candidate.priority = priorityForCandidate(
        100,
        candidate.riskKind.has_value() ? riskSeverity(context, *candidate.riskKind) : 0,
        true);
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
    const AlternativeRecommendationInput& request,
    const RecommendationContext& context) {
    const auto bottleneck = context.worstBottleneck;
    if (!bottleneck.has_value() || bottleneck->connectionId.empty()) {
        return std::nullopt;
    }
    const auto adjacentExitZoneIds = adjacentExitZoneIdsForConnection(request, bottleneck->connectionId);
    const auto targetExit = leastUsedReachableExit(
        request,
        adjacentExitZoneIds);
    if (!targetExit.has_value() || targetExit->exitZoneId.empty()) {
        return std::nullopt;
    }
    if (hasRouteGuidanceForExit(request.sourceScenario, targetExit->exitZoneId)
        || exitHasBottleneck(request, targetExit->exitZoneId)) {
        return std::nullopt;
    }
    if (context.mostUsedExit.has_value()
        && !guidanceDetourAcceptable(request, context.mostUsedExit->exitZoneId, targetExit->exitZoneId)) {
        return std::nullopt;
    }

    const auto severity = riskSeverity(context, bottleneckRiskKind(request, *bottleneck));
    const auto installAnchor = connectionGuidanceAnchor(
        request,
        bottleneck->connectionId,
        "ScenarioRiskSnapshot.bottlenecks");
    if (!installAnchor.has_value()) {
        return std::nullopt;
    }
    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::BottleneckBypassGuidance,
        "Recommended: guide to " + zoneName(request.layout, targetExit->exitZoneId));
    auto guidance = makeGuidance(
        "recommendation-guidance-" + sanitizeId(targetExit->exitZoneId),
        targetExit->exitZoneId,
        guidanceComplianceForSeverity(severity));
    configureGuidanceForRecommendation(guidance, request, *installAnchor, severity);
    draft.control.routeGuidances.push_back(guidance);
    finalizeDiffKeys(request, draft);
    if (!recommendedDraftChangesSource(request, draft)) {
        return std::nullopt;
    }

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::BottleneckBypassGuidance;
    candidate.id = "guide-around-" + sanitizeId(bottleneck->connectionId);
    candidate.priority = priorityForCandidate(120, severity, true);
    candidate.title = "Guide occupants to another exit";
    candidate.summary = "Install guidance near "
        + connectionName(request.layout, bottleneck->connectionId)
        + " and route a share of occupants toward "
        + zoneName(request.layout, targetExit->exitZoneId) + ".";
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
    candidate.evidence.push_back(guidanceInstallEvidence(*installAnchor));
    candidate.evidence.push_back(guidanceTuningEvidence(guidance));
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makeExitBalancingCandidate(
    const AlternativeRecommendationInput& request,
    const RecommendationContext& context) {
    if (exitUsageCandidates(request).size() < 2) {
        return std::nullopt;
    }
    const auto low = context.leastUsedReachableExit;
    const auto high = context.mostUsedExit;
    if (!low.has_value() || !high.has_value() || low->exitZoneId.empty() || high->exitZoneId.empty()) {
        return std::nullopt;
    }
    if (low->exitZoneId == high->exitZoneId
        || high->usageRatio - low->usageRatio < kExitImbalanceThreshold
        || hasRouteGuidanceForExit(request.sourceScenario, low->exitZoneId)
        || exitHasBottleneck(request, low->exitZoneId)
        || !guidanceDetourAcceptable(request, high->exitZoneId, low->exitZoneId)) {
        return std::nullopt;
    }

    const auto severity = exitUsageSeverity(context);
    const auto highExitPoint = zoneReferencePoint(request.layout, high->exitZoneId);
    const auto highExitFloorId = high->floorId.empty()
        ? floorIdForZone(request.layout, high->exitZoneId)
        : high->floorId;
    const auto installAnchor = sourceGuidanceAnchor(
        request,
        highExitPoint,
        highExitFloorId,
        "ScenarioResultArtifacts.exitUsage + ScenarioDraft.population");
    if (!installAnchor.has_value()) {
        return std::nullopt;
    }
    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::ExitUsageBalancing,
        "Recommended: balance exit usage toward " + zoneName(request.layout, low->exitZoneId));
    auto guidance = makeGuidance(
        "recommendation-guidance-exit-" + sanitizeId(low->exitZoneId),
        low->exitZoneId,
        guidanceComplianceForExitSpread(high->usageRatio - low->usageRatio));
    configureGuidanceForRecommendation(guidance, request, *installAnchor, severity);
    draft.control.routeGuidances.push_back(guidance);
    finalizeDiffKeys(request, draft);
    if (!recommendedDraftChangesSource(request, draft)) {
        return std::nullopt;
    }

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::ExitUsageBalancing;
    candidate.id = "balance-exit-" + sanitizeId(low->exitZoneId);
    candidate.priority = priorityForCandidate(140, severity, true);
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
    candidate.evidence.push_back(guidanceInstallEvidence(*installAnchor));
    candidate.evidence.push_back(guidanceTuningEvidence(guidance));
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makePressureHotspotCandidate(
    const AlternativeRecommendationInput& request,
    const RecommendationContext& context) {
    if (!hasPressureSignal(request)) {
        return std::nullopt;
    }
    if (exitUsageCandidates(request).size() < 2) {
        return std::nullopt;
    }

    const auto targetExit = context.leastUsedReachableExit;
    if (!targetExit.has_value() || targetExit->exitZoneId.empty()
        || hasRouteGuidanceForExit(request.sourceScenario, targetExit->exitZoneId)
        || exitHasBottleneck(request, targetExit->exitZoneId)) {
        return std::nullopt;
    }
    if (context.exitUsageImbalanced) {
        return std::nullopt;
    }
    if (context.mostUsedExit.has_value()
        && !guidanceDetourAcceptable(request, context.mostUsedExit->exitZoneId, targetExit->exitZoneId)) {
        return std::nullopt;
    }

    const auto severity = riskSeverity(context, AlternativeRecommendationRiskKind::PressureHotspot);
    const auto installAnchor = pressureGuidanceAnchor(request);
    if (!installAnchor.has_value()) {
        return std::nullopt;
    }
    auto draft = makeRecommendedDraft(
        request,
        AlternativeRecommendationKind::PressureHotspotRelief,
        "Recommended: relieve pressure toward " + zoneName(request.layout, targetExit->exitZoneId));
    auto guidance = makeGuidance(
        "recommendation-guidance-pressure-" + sanitizeId(targetExit->exitZoneId),
        targetExit->exitZoneId,
        guidanceComplianceForSeverity(severity));
    configureGuidanceForRecommendation(guidance, request, *installAnchor, severity);
    draft.control.routeGuidances.push_back(guidance);
    finalizeDiffKeys(request, draft);
    if (!recommendedDraftChangesSource(request, draft)) {
        return std::nullopt;
    }

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::PressureHotspotRelief;
    candidate.id = "relieve-pressure-" + sanitizeId(targetExit->exitZoneId);
    candidate.priority = priorityForCandidate(150, severity, true);
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
    candidate.evidence.push_back(guidanceInstallEvidence(*installAnchor));
    candidate.evidence.push_back(guidanceTuningEvidence(guidance));
    candidate.recommendedScenario = std::move(draft);
    return candidate;
}

std::optional<AlternativeRecommendationCandidate> makeOperationalConflictCandidate(
    const AlternativeRecommendationInput& request,
    const RecommendationContext& context) {
    const auto* signal = findRiskSignal(context, AlternativeRecommendationRiskKind::OperationalConflict);
    if (signal == nullptr) {
        return std::nullopt;
    }

    const auto severity = signal->severity;
    if (!context.exitUsageImbalanced && exitUsageCandidates(request).size() >= 2) {
        const auto targetExit = context.leastUsedReachableExit;
        if (targetExit.has_value()
            && context.mostUsedExit.has_value()
            && targetExit->exitZoneId != context.mostUsedExit->exitZoneId
            && !hasRouteGuidanceForExit(request.sourceScenario, targetExit->exitZoneId)
            && !exitHasBottleneck(request, targetExit->exitZoneId)
            && guidanceDetourAcceptable(request, context.mostUsedExit->exitZoneId, targetExit->exitZoneId)) {
            const auto installAnchor = operationalConflictGuidanceAnchor(request);
            if (!installAnchor.has_value()) {
                return std::nullopt;
            }
            auto draft = makeRecommendedDraft(
                request,
                AlternativeRecommendationKind::OperationalConflictSeparation,
                "Recommended: guide operational conflict away from shared movement");
            auto guidance = makeGuidance(
                "recommendation-guidance-operational-conflict-" + sanitizeId(targetExit->exitZoneId),
                targetExit->exitZoneId,
                guidanceComplianceForSeverity(severity));
            configureGuidanceForRecommendation(guidance, request, *installAnchor, severity);
            draft.control.routeGuidances.push_back(guidance);
            finalizeDiffKeys(request, draft);
            if (recommendedDraftChangesSource(request, draft)) {
                AlternativeRecommendationCandidate candidate;
                candidate.kind = AlternativeRecommendationKind::OperationalConflictSeparation;
                candidate.riskKind = AlternativeRecommendationRiskKind::OperationalConflict;
                candidate.id = "guide-operational-conflict-" + sanitizeId(targetExit->exitZoneId);
                candidate.priority = priorityForCandidate(130, severity, true);
                candidate.title = "Guide operational conflict to another exit";
                candidate.summary = "Guide part of the crossing stream toward "
                    + zoneName(request.layout, targetExit->exitZoneId)
                    + " so the shared movement area carries less conflicting flow.";
                candidate.expectedImprovement = "Reduces crossing movement interference with a rerunnable route-guidance draft.";
                candidate.artifactSource = "AlternativeRecommendationRiskSignal + ScenarioResultArtifacts.exitUsage";
                candidate.evidence = signal->evidence;
                candidate.evidence.push_back(evidence(
                    "Guided exit",
                    zoneName(request.layout, targetExit->exitZoneId),
                    "least-used reachable exit from FacilityLayout2D.zones + ScenarioResultArtifacts.exitUsage"));
                candidate.evidence.push_back(guidanceInstallEvidence(*installAnchor));
                candidate.evidence.push_back(guidanceTuningEvidence(guidance));
                candidate.recommendedScenario = std::move(draft);
                return candidate;
            }
        }
    }

    if (findRiskSignal(context, AlternativeRecommendationRiskKind::PressureHotspot) == nullptr
        && findRiskSignal(context, AlternativeRecommendationRiskKind::TimeLimitMissed) == nullptr) {
        const auto profile = stagedEvacuationProfileFor(request, severity);
        const auto stagedSources = makeSequentialStagedEvacuationSources(request, profile);
        if (stagedSources.size() >= 2
            && std::none_of(stagedSources.begin(), stagedSources.end(), [&](const auto& source) {
                   return hasOccupantSource(request.sourceScenario, source.id);
               })) {
            auto draft = makeRecommendedDraft(
                request,
                AlternativeRecommendationKind::OperationalConflictSeparation,
                "Recommended: time-separate operational-conflict groups");
            draft.population.initialPlacements.clear();
            draft.population.occupantSources.insert(
                draft.population.occupantSources.end(),
                stagedSources.begin(),
                stagedSources.end());
            finalizeDiffKeys(request, draft);
            if (recommendedDraftChangesSource(request, draft)) {
                AlternativeRecommendationCandidate candidate;
                candidate.kind = AlternativeRecommendationKind::OperationalConflictSeparation;
                candidate.riskKind = AlternativeRecommendationRiskKind::OperationalConflict;
                candidate.id = "stage-operational-conflict-groups";
                candidate.priority = priorityForCandidate(135, severity, true);
                candidate.title = "Time-separate operational-conflict groups";
                candidate.summary = "Release source groups sequentially to reduce simultaneous opposing streams.";
                candidate.expectedImprovement = "Turns the operational-conflict rule into a rerunnable staged-release draft.";
                candidate.artifactSource = "AlternativeRecommendationRiskSignal + ScenarioDraft.population.initialPlacements";
                candidate.evidence = signal->evidence;
                candidate.evidence.push_back(evidence(
                    "Staged groups",
                    std::to_string(stagedSources.size()) + " source groups",
                    "ScenarioDraft.population.initialPlacements"));
                candidate.evidence.push_back(evidence(
                    "Release schedule",
                    "sequential groups, up to " + std::to_string(profile.agentsPerSpawn) + " agents every "
                        + fixed(profile.intervalSeconds, 1) + " sec",
                    "recommended OccupantSource2D"));
                candidate.recommendedScenario = std::move(draft);
                return candidate;
            }
        }
    }

    return std::nullopt;
}

std::optional<AlternativeRecommendationCandidate> makeStagedEvacuationCandidate(
    const AlternativeRecommendationInput& request,
    const RecommendationContext& context) {
    const auto* signal = findRiskSignal(context, AlternativeRecommendationRiskKind::PressureHotspot);
    if (signal == nullptr) {
        signal = findRiskSignal(context, AlternativeRecommendationRiskKind::TimeLimitMissed);
    }
    if (signal == nullptr) {
        return std::nullopt;
    }
    if (!request.sourceScenario.population.occupantSources.empty()) {
        return std::nullopt;
    }

    const auto profile = stagedEvacuationProfileFor(request, signal->severity);
    const auto stagedSources = makeSequentialStagedEvacuationSources(request, profile);
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
    const auto finalSpawnSeconds = profile.intervalSeconds * static_cast<double>(tickCount - 1);

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
    if (!recommendedDraftChangesSource(request, draft)) {
        return std::nullopt;
    }

    AlternativeRecommendationCandidate candidate;
    candidate.kind = AlternativeRecommendationKind::StagedEvacuation;
    candidate.riskKind = signal->kind;
    candidate.id = "staged-evacuation-batches";
    candidate.priority = priorityForCandidate(160, signal->severity, true);
    candidate.title = "Stage groups sequentially";
    candidate.summary = "Release each source group in order: up to " + std::to_string(profile.agentsPerSpawn)
        + " agents every " + fixed(profile.intervalSeconds, 1)
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
        "sequential groups, up to " + std::to_string(profile.agentsPerSpawn) + " agents every "
            + fixed(profile.intervalSeconds, 1) + " sec x " + std::to_string(tickCount) + " batches",
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
    case AlternativeRecommendationKind::OperationalConflictSeparation:
        return "operational-conflict-separation";
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
    case AlternativeRecommendationRiskKind::OperationalConflict:
        return "operational-conflict";
    case AlternativeRecommendationRiskKind::TimeLimitMissed:
        return "time-limit-missed";
    case AlternativeRecommendationRiskKind::PressureHotspot:
        return "pressure-hotspot";
    }
    return "risk";
}

namespace {

AlternativeRecommendationResult recommendFromInput(const AlternativeRecommendationInput& request) {
    AlternativeRecommendationResult result;
    if (!hasCompletedResultArtifactEvidence(request)) {
        result.blockingReasons.push_back(
            "Completed scenario result artifacts are required before SafeCrowd can recommend an operational draft.");
        return result;
    }

    const auto context = buildRecommendationContext(request);
    result.riskSignals = context.riskSignals;

    if (const auto candidate = makeBlockedConnectionCandidate(request, context); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeBottleneckGuidanceCandidate(request, context); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeExitBalancingCandidate(request, context); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeOperationalConflictCandidate(request, context); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makePressureHotspotCandidate(request, context); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }
    if (const auto candidate = makeStagedEvacuationCandidate(request, context); candidate.has_value()) {
        result.candidates.push_back(*candidate);
    }

    std::sort(result.candidates.begin(), result.candidates.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.priority != rhs.priority) {
            return lhs.priority < rhs.priority;
        }
        return lhs.id < rhs.id;
    });

    if (result.candidates.empty()) {
        result.blockingReasons.push_back(result.riskSignals.empty()
            ? "No detected risk signal, blocked connection, exit imbalance, or pressure hotspot produced an actionable v1 recommendation."
            : "Detected risk signals did not produce a simulation-changing recommendation draft with the current layout and scenario data.");
    }

    return result;
}

}  // namespace

AlternativeRecommendationResult AlternativeRecommendationService::recommend(
    const AlternativeRecommendationRequest& request) const {
    return recommend(
        request.layout,
        request.sourceScenario,
        request.risk,
        request.artifacts,
        request.baselineScenario.has_value() ? &*request.baselineScenario : nullptr,
        request.finalFrame.has_value() ? &*request.finalFrame : nullptr);
}

AlternativeRecommendationResult AlternativeRecommendationService::recommend(
    const FacilityLayout2D& layout,
    const ScenarioDraft& sourceScenario,
    const ScenarioRiskSnapshot& risk,
    const ScenarioResultArtifacts& artifacts,
    const ScenarioDraft* baselineScenario,
    const SimulationFrame* finalFrame) const {
    return recommendFromInput({
        .layout = layout,
        .sourceScenario = sourceScenario,
        .baselineScenario = baselineScenario,
        .risk = risk,
        .artifacts = artifacts,
        .finalFrame = finalFrame,
    });
}

}  // namespace safecrowd::domain
