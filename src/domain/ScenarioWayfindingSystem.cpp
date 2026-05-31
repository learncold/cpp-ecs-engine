#include "domain/ScenarioSimulationSystems.h"

#include "domain/GeometryQueries.h"
#include "domain/ScenarioSimulationInternal.h"
#include "domain/ScenarioSimulationRouteGuidance.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine/EngineWorld.h"

namespace safecrowd::domain {
namespace {

using namespace simulation_internal;

constexpr double kWayfindingDecisionCooldownSeconds = 0.10;
constexpr double kSignCommitmentSeconds = 3.0;
constexpr double kSignSightClearanceMeters = 0.08;
constexpr double kCandidateCongestionRadiusMeters = 1.0;
constexpr double kStalledConnectionAvoidSeconds = 5.0;

bool signActiveAt(const EvacuationSignDraft& sign, double elapsedSeconds) {
    if (sign.periods.empty()) {
        return true;
    }
    return std::any_of(sign.periods.begin(), sign.periods.end(), [&](const auto& period) {
        const auto start = std::max(0.0, period.startSeconds);
        const auto end = std::max(start, period.endSeconds);
        return elapsedSeconds + 1e-9 >= start && elapsedSeconds <= end + 1e-9;
    });
}

bool containsString(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

void appendUnique(std::vector<std::string>& values, const std::string& value) {
    if (!value.empty() && !containsString(values, value)) {
        values.push_back(value);
    }
}

bool isExitZone(const ScenarioLayoutCacheResource& layoutCache, const std::string& zoneId) {
    const auto* zone = findCachedZone(layoutCache, zoneId);
    return zone != nullptr && zone->kind == ZoneKind::Exit;
}

bool connectionIsVerticalOrStair(const FacilityLayout2D& layout, const Connection2D& connection) {
    return isVerticalConnection(connection)
        || connection.kind == ConnectionKind::Stair
        || connection.kind == ConnectionKind::Ramp
        || connection.isStair
        || connection.isRamp
        || [&]() {
               const auto* fromZone = findZone(layout, connection.fromZoneId);
               const auto* toZone = findZone(layout, connection.toZoneId);
               return (fromZone != nullptr && (fromZone->kind == ZoneKind::Stair || fromZone->isStair || fromZone->isRamp))
                   || (toZone != nullptr && (toZone->kind == ZoneKind::Stair || toZone->isStair || toZone->isRamp));
           }();
}

bool zoneIsStairLike(const Zone2D* zone) {
    return zone != nullptr && (zone->kind == ZoneKind::Stair || zone->isStair || zone->isRamp);
}

std::optional<Point2D> normalized(const Point2D& point) {
    const auto length = lengthOf(point);
    if (length <= 1e-9) {
        return std::nullopt;
    }
    return Point2D{.x = point.x / length, .y = point.y / length};
}

const Connection2D* findConnectionById(
    const ScenarioLayoutCacheResource& layoutCache,
    const std::string& connectionId) {
    if (connectionId.empty()) {
        return nullptr;
    }
    const auto it = layoutCache.connectionIndices.find(connectionId);
    if (it == layoutCache.connectionIndices.end() || it->second >= layoutCache.layout.connections.size()) {
        return nullptr;
    }
    return &layoutCache.layout.connections[it->second];
}

bool routeTargetBlocked(
    const ScenarioLayoutCacheResource& layoutCache,
    const EvacuationRoute& route,
    const std::string& currentZoneId) {
    if (route.nextWaypointIndex >= route.waypointConnectionIds.size()) {
        return false;
    }

    const auto& connectionId = route.waypointConnectionIds[route.nextWaypointIndex];
    if (connectionId.empty()) {
        return false;
    }
    const auto* connection = findConnectionById(layoutCache, connectionId);
    if (connection == nullptr || connection->directionality == TravelDirection::Closed) {
        return true;
    }

    const auto& traversals = cachedTraversalsForZone(layoutCache, currentZoneId);
    return std::none_of(traversals.begin(), traversals.end(), [&](const auto& traversal) {
        return traversal.connectionIndex < layoutCache.layout.connections.size()
            && layoutCache.layout.connections[traversal.connectionIndex].id == connectionId;
    });
}

bool routeTargetStalled(const EvacuationRoute& route) {
    if (route.stalledSeconds < kWaypointStallSeconds) {
        return false;
    }
    if (route.nextWaypointIndex >= route.waypointConnectionIds.size()) {
        return false;
    }
    return !route.waypointConnectionIds[route.nextWaypointIndex].empty();
}

const Connection2D* routeTargetConnection(
    const ScenarioLayoutCacheResource& layoutCache,
    const EvacuationRoute& route) {
    if (route.nextWaypointIndex >= route.waypointConnectionIds.size()) {
        return nullptr;
    }
    return findConnectionById(layoutCache, route.waypointConnectionIds[route.nextWaypointIndex]);
}

bool routeTargetTouchesStairZone(
    const ScenarioLayoutCacheResource& layoutCache,
    const Connection2D* connection) {
    if (connection == nullptr) {
        return false;
    }

    return zoneIsStairLike(findCachedZone(layoutCache, connection->fromZoneId))
        || zoneIsStairLike(findCachedZone(layoutCache, connection->toZoneId));
}

void clearRoute(EvacuationRoute& route, const Point2D& position) {
    route.waypoints.clear();
    route.waypointPassages.clear();
    route.waypointFromZoneIds.clear();
    route.waypointZoneIds.clear();
    route.waypointFloorIds.clear();
    route.waypointConnectionIds.clear();
    route.waypointVerticalTransitions.clear();
    route.nextWaypointIndex = 0;
    route.currentSegmentStart = position;
    route.previousDistanceToWaypoint = 0.0;
    route.stalledSeconds = 0.0;
    route.nextSegmentReplanSeconds = 0.0;
    route.destinationZoneId.clear();
    route.noExitAvailable = false;
}

struct VisibleSign {
    const EvacuationSignDraft* sign{nullptr};
    double weight{0.0};
};

std::vector<VisibleSign> visibleSigns(
    const ScenarioLayoutCacheResource& layoutCache,
    const std::vector<EvacuationSignDraft>& signs,
    const Position& position,
    const Agent& agent,
    const EvacuationRoute& route,
    const std::string& currentZoneId,
    double elapsedSeconds) {
    std::vector<VisibleSign> visible;
    const auto floorId = !route.displayFloorId.empty() ? route.displayFloorId : route.currentFloorId;
    const auto& floorLayout = cachedLayoutForFloor(layoutCache, floorId);
    const auto sightClearance = std::max(kSignSightClearanceMeters, static_cast<double>(agent.radius) + 0.05);
    for (const auto& sign : signs) {
        if (!signActiveAt(sign, elapsedSeconds)) {
            continue;
        }
        if (!matchesFloor(sign.floorId, floorId)) {
            continue;
        }
        if (!sign.installZoneId.empty() && sign.installZoneId != currentZoneId) {
            continue;
        }
        const auto radius = std::max(0.0, sign.visibilityRadiusMeters);
        if (radius <= 1e-9 || distanceBetween(position.value, sign.position) > radius + 1e-9) {
            continue;
        }
        if (!lineOfSightClear(floorLayout, position.value, sign.position, sightClearance)) {
            continue;
        }
        const auto weight = std::clamp(sign.complianceRate, 0.0, 1.0)
            * std::clamp(agent.guidancePropensity, 0.0, 1.0);
        if (weight <= 1e-9) {
            continue;
        }
        visible.push_back(VisibleSign{.sign = &sign, .weight = weight});
    }
    return visible;
}

struct WayfindingCandidate {
    std::size_t connectionIndex{0};
    std::string connectionId{};
    std::string nextZoneId{};
    double score{-std::numeric_limits<double>::infinity()};
    std::string signId{};
    WayfindingIntent intent{WayfindingIntent::Exploring};
};

bool connectionVisibleFrom(
    const ScenarioLayoutCacheResource& layoutCache,
    const Position& position,
    const Agent& agent,
    const std::string& currentZoneId,
    const Connection2D& connection) {
    const auto currentFloorId = cachedFloorIdForZone(layoutCache, currentZoneId);
    const auto connectionFloorId = connection.floorId.empty() ? currentFloorId : connection.floorId;
    if (!matchesFloor(connectionFloorId, currentFloorId)) {
        return false;
    }
    const auto target = midpoint(connection.centerSpan);
    const auto clearance = std::max(kSignSightClearanceMeters, static_cast<double>(agent.radius) + 0.05);
    return lineOfSightClear(cachedLayoutForFloor(layoutCache, currentFloorId), position.value, target, clearance);
}

bool zoneLooksLikeDeadEnd(
    const ScenarioLayoutCacheResource& layoutCache,
    const std::string& currentZoneId,
    const std::string& nextZoneId) {
    const auto* nextZone = findCachedZone(layoutCache, nextZoneId);
    if (nextZone == nullptr || nextZone->kind == ZoneKind::Exit || zoneIsStairLike(nextZone)) {
        return false;
    }
    const auto& onwardTraversals = cachedTraversalsForZone(layoutCache, nextZoneId);
    return std::none_of(onwardTraversals.begin(), onwardTraversals.end(), [&](const auto& traversal) {
        return !traversal.nextZoneId.empty() && traversal.nextZoneId != currentZoneId;
    });
}

double congestionPenalty(
    engine::WorldQuery& query,
    const ScenarioAgentSpatialIndexResource* spatialIndex,
    const Point2D& point,
    const std::string& floorId) {
    if (spatialIndex == nullptr) {
        return 0.0;
    }
    const auto nearby = scenarioNearbyAgents(query, *spatialIndex, point, floorId, kCandidateCongestionRadiusMeters);
    return static_cast<double>(nearby.size()) * 0.35;
}

WayfindingCandidate scoreCandidate(
    engine::WorldQuery& query,
    const ScenarioLayoutCacheResource& layoutCache,
    const ScenarioAgentSpatialIndexResource* spatialIndex,
    const std::vector<VisibleSign>& signs,
    const WayfindingState& state,
    const Position& position,
    const Agent& agent,
    const std::string& currentZoneId,
    const ScenarioConnectionTraversal& traversal,
    double elapsedSeconds) {
    const auto& connection = layoutCache.layout.connections[traversal.connectionIndex];
    const auto* currentZone = findCachedZone(layoutCache, currentZoneId);
    const auto* nextZone = findCachedZone(layoutCache, traversal.nextZoneId);
    const auto midpointPoint = midpoint(connection.centerSpan);
    const auto connectionFloorId = connection.floorId.empty()
        ? cachedFloorIdForZone(layoutCache, currentZoneId)
        : connection.floorId;
    const bool currentZoneIsStairLike = zoneIsStairLike(currentZone);
    const bool nextZoneIsStairLike = zoneIsStairLike(nextZone);
    const bool candidateIsVertical = isVerticalConnection(connection);
    const bool candidateIsStairLike = connectionIsVerticalOrStair(layoutCache.layout, connection);
    const bool targetZoneVisited = containsString(state.visitedZoneIds, traversal.nextZoneId);

    WayfindingCandidate candidate{
        .connectionIndex = traversal.connectionIndex,
        .connectionId = connection.id,
        .nextZoneId = traversal.nextZoneId,
        .score = 0.0,
        .intent = WayfindingIntent::Exploring,
    };

    if (connection.kind == ConnectionKind::Exit || (nextZone != nullptr && nextZone->kind == ZoneKind::Exit)) {
        candidate.score += 100.0;
        candidate.intent = WayfindingIntent::MovingToVisibleExit;
    }
    if (candidateIsVertical && currentZoneIsStairLike) {
        if (!targetZoneVisited) {
            candidate.score += 90.0;
        }
    } else if (candidateIsStairLike) {
        if (!targetZoneVisited && nextZoneIsStairLike) {
            candidate.score += 75.0;
        } else if (connectionVisibleFrom(layoutCache, position, agent, currentZoneId, connection)) {
            candidate.score += 70.0;
        }
    }
    if (zoneLooksLikeDeadEnd(layoutCache, currentZoneId, traversal.nextZoneId)) {
        candidate.score -= 18.0;
    }
    if (!state.avoidedConnectionId.empty()
        && state.avoidedConnectionId == connection.id
        && elapsedSeconds + 1e-9 < state.avoidConnectionUntilSeconds) {
        candidate.score -= 120.0;
    }

    candidate.score += targetZoneVisited ? -6.0 : 5.0;
    if (state.currentTargetZoneId == traversal.nextZoneId) {
        candidate.score -= 2.0;
    }
    candidate.score -= distanceBetween(position.value, midpointPoint) * 0.10;
    candidate.score -= congestionPenalty(query, spatialIndex, midpointPoint, connectionFloorId);

    double bestSignBonus = 0.0;
    std::string bestSignId;
    WayfindingIntent signIntent = WayfindingIntent::FollowingSign;
    for (const auto& visible : signs) {
        if (visible.sign == nullptr) {
            continue;
        }
        const auto& sign = *visible.sign;
        const auto candidateDirection = normalized(midpointPoint - sign.position);
        if (!candidateDirection.has_value()) {
            continue;
        }
        const auto signDirection = Point2D{
            .x = std::cos(sign.orientationRadians),
            .y = std::sin(sign.orientationRadians),
        };
        const auto alignment = dot(signDirection, *candidateDirection);
        const auto positiveAlignment = std::max(0.0, alignment);

        double signBonus = 0.0;
        switch (sign.kind) {
        case EvacuationSignKind::DirectionArrow:
            signBonus = 45.0 * positiveAlignment;
            break;
        case EvacuationSignKind::ExitMarker:
            signBonus = 50.0 * positiveAlignment;
            if (connection.kind == ConnectionKind::Exit || (nextZone != nullptr && nextZone->kind == ZoneKind::Exit)) {
                signBonus += 80.0;
                signIntent = WayfindingIntent::MovingToVisibleExit;
            }
            break;
        case EvacuationSignKind::StairExitMarker:
            signBonus = 35.0 * positiveAlignment;
            if (connectionIsVerticalOrStair(layoutCache.layout, connection)) {
                signBonus += 95.0;
            }
            break;
        }

        signBonus *= visible.weight;
        if (signBonus > bestSignBonus) {
            bestSignBonus = signBonus;
            bestSignId = sign.id;
        }
    }

    candidate.score += bestSignBonus;
    if (!bestSignId.empty()) {
        candidate.signId = bestSignId;
        candidate.intent = signIntent;
    }
    return candidate;
}

std::optional<WayfindingCandidate> chooseCandidate(
    engine::WorldQuery& query,
    const ScenarioLayoutCacheResource& layoutCache,
    const ScenarioAgentSpatialIndexResource* spatialIndex,
    const std::vector<VisibleSign>& signs,
    const WayfindingState& state,
    const Position& position,
    const Agent& agent,
    const std::string& currentZoneId,
    double elapsedSeconds) {
    const auto& traversals = cachedTraversalsForZone(layoutCache, currentZoneId);
    std::optional<WayfindingCandidate> best;
    for (const auto& traversal : traversals) {
        if (traversal.nextZoneId.empty() || traversal.connectionIndex >= layoutCache.layout.connections.size()) {
            continue;
        }
        const auto candidate = scoreCandidate(
            query,
            layoutCache,
            spatialIndex,
            signs,
            state,
            position,
            agent,
            currentZoneId,
            traversal,
            elapsedSeconds);
        if (!best.has_value()
            || candidate.score > best->score + 1e-9
            || (std::fabs(candidate.score - best->score) <= 1e-9
                && candidate.connectionId < best->connectionId)) {
            best = candidate;
        }
    }
    return best;
}

ScenarioRoutePlan routePlanToConnection(
    const ScenarioLayoutCacheResource& layoutCache,
    const Point2D& start,
    const std::string& startZoneId,
    const WayfindingCandidate& candidate) {
    ScenarioRoutePlan plan;
    if (candidate.connectionIndex >= layoutCache.layout.connections.size()) {
        return plan;
    }

    const auto& connection = layoutCache.layout.connections[candidate.connectionIndex];
    const auto passage = passageWithClearance(connection, kCandidateClearance);
    const auto startFloorId = cachedFloorIdForZone(layoutCache, startZoneId);
    const auto targetFloorId = cachedFloorIdForZone(layoutCache, candidate.nextZoneId);
    const auto target = closestPointOnSegment(start, passage.start, passage.end);
    const auto& floorLayout = cachedLayoutForFloor(layoutCache, startFloorId);
    const auto segment = buildPath(floorLayout, start, target, kCandidateClearance);

    plan.destinationZoneId = candidate.nextZoneId;
    for (std::size_t waypointIndex = 0; waypointIndex < segment.size(); ++waypointIndex) {
        const bool finalWaypoint = waypointIndex + 1 == segment.size();
        plan.waypoints.push_back(segment[waypointIndex]);
        plan.waypointPassages.push_back(finalWaypoint ? passage : pointPassage(segment[waypointIndex]));
        plan.waypointFromZoneIds.push_back(finalWaypoint ? startZoneId : std::string{});
        plan.waypointZoneIds.push_back(finalWaypoint ? candidate.nextZoneId : std::string{});
        plan.waypointFloorIds.push_back(finalWaypoint && !targetFloorId.empty() ? targetFloorId : std::string{});
        plan.waypointConnectionIds.push_back(finalWaypoint ? connection.id : std::string{});
        plan.waypointVerticalTransitions.push_back(finalWaypoint && isVerticalConnection(connection));
    }

    const bool firstWaypointCarriesConnection = !plan.waypointConnectionIds.empty()
        && !plan.waypointConnectionIds.front().empty();
    const bool firstWaypointCarriesZoneTransition = !plan.waypointFromZoneIds.empty()
        && !plan.waypointFromZoneIds.front().empty()
        && !plan.waypointZoneIds.empty()
        && !plan.waypointZoneIds.front().empty();
    const bool firstWaypointCarriesVerticalTransition = !plan.waypointVerticalTransitions.empty()
        && plan.waypointVerticalTransitions.front();
    if (!plan.waypoints.empty()
        && distanceBetween(start, plan.waypoints.front()) <= kArrivalEpsilon
        && !firstWaypointCarriesConnection
        && !firstWaypointCarriesZoneTransition
        && !firstWaypointCarriesVerticalTransition) {
        plan.waypoints.erase(plan.waypoints.begin());
        plan.waypointPassages.erase(plan.waypointPassages.begin());
        plan.waypointFromZoneIds.erase(plan.waypointFromZoneIds.begin());
        plan.waypointZoneIds.erase(plan.waypointZoneIds.begin());
        plan.waypointFloorIds.erase(plan.waypointFloorIds.begin());
        plan.waypointConnectionIds.erase(plan.waypointConnectionIds.begin());
        plan.waypointVerticalTransitions.erase(plan.waypointVerticalTransitions.begin());
    }
    return plan;
}

void replaceRouteWithPlan(EvacuationRoute& route, const ScenarioRoutePlan& plan, const Point2D& start) {
    route.destinationZoneId = plan.destinationZoneId;
    route.waypoints = plan.waypoints;
    route.waypointPassages = plan.waypointPassages;
    route.waypointFromZoneIds = plan.waypointFromZoneIds;
    route.waypointZoneIds = plan.waypointZoneIds;
    route.waypointFloorIds = plan.waypointFloorIds;
    route.waypointConnectionIds = plan.waypointConnectionIds;
    route.waypointVerticalTransitions = plan.waypointVerticalTransitions;
    route.nextWaypointIndex = 0;
    route.currentSegmentStart = start;
    route.previousDistanceToWaypoint = route.waypoints.empty()
        ? 0.0
        : distanceToRouteWaypoint(route, start);
    route.stalledSeconds = 0.0;
    route.replanCooldownSeconds = 0.0;
    route.nextExitReplanSeconds = 0.0;
    route.nextSegmentReplanSeconds = 0.0;
    route.noExitAvailable = false;
    route.holdingForClosure = false;
    route.closureHoldTarget = {};
    route.followsGuidance = false;
    route.guidanceEventId.clear();
}

class ScenarioWayfindingSystem final : public engine::EngineSystem {
public:
    ScenarioWayfindingSystem(FacilityLayout2D layout, std::vector<EvacuationSignDraft> signs)
        : layoutCache_(buildScenarioLayoutCache(std::move(layout))),
          signs_(std::move(signs)) {
    }

    void configure(engine::EngineWorld& world) override {
        if (!world.resources().contains<ScenarioLayoutCacheResource>()) {
            world.resources().set(layoutCache_);
        }
    }

    void update(engine::EngineWorld& world, const engine::EngineStepContext& step) override {
        (void)step;

        auto& resources = world.resources();
        if (!resources.contains<ScenarioSimulationClockResource>()
            || !resources.contains<ScenarioSimulationStepResource>()
            || !resources.contains<ScenarioLayoutCacheResource>()) {
            return;
        }

        const auto& clock = resources.get<ScenarioSimulationClockResource>();
        if (clock.complete) {
            return;
        }

        auto& query = world.query();
        const auto entities = query.view<Position, Agent, Velocity, AvoidanceState, EvacuationRoute, WayfindingState, EvacuationStatus>();
        const auto& layoutCache = resources.get<ScenarioLayoutCacheResource>();
        const auto* spatialIndex = resources.contains<ScenarioAgentSpatialIndexResource>()
            ? &resources.get<ScenarioAgentSpatialIndexResource>()
            : nullptr;

        for (const auto entity : entities) {
            const auto& status = query.get<EvacuationStatus>(entity);
            if (status.evacuated) {
                continue;
            }

            const auto& position = query.get<Position>(entity);
            const auto& agent = query.get<Agent>(entity);
            auto& route = query.get<EvacuationRoute>(entity);
            auto& state = query.get<WayfindingState>(entity);

            const auto currentZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
            if (currentZoneId.empty() || isExitZone(layoutCache, currentZoneId)) {
                continue;
            }
            appendUnique(state.visitedZoneIds, currentZoneId);
            if (clock.elapsedSeconds + 1e-9 >= state.avoidConnectionUntilSeconds) {
                state.avoidedConnectionId.clear();
                state.avoidConnectionUntilSeconds = 0.0;
            }

            const bool hasActiveRoute = route.nextWaypointIndex < route.waypoints.size();
            const bool targetBlocked = hasActiveRoute && routeTargetBlocked(layoutCache, route, currentZoneId);
            const bool targetStalled = hasActiveRoute && routeTargetStalled(route);
            const auto* targetConnection = hasActiveRoute ? routeTargetConnection(layoutCache, route) : nullptr;
            const bool targetIsVerticalTransition = targetConnection != nullptr && isVerticalConnection(*targetConnection);
            const bool targetTouchesStairZone = routeTargetTouchesStairZone(layoutCache, targetConnection);
            if (hasActiveRoute && !targetBlocked && (!targetStalled || targetIsVerticalTransition)) {
                continue;
            }
            if (hasActiveRoute) {
                if (targetStalled
                    && route.nextWaypointIndex < route.waypointConnectionIds.size()
                    && !route.waypointConnectionIds[route.nextWaypointIndex].empty()
                    && !targetTouchesStairZone) {
                    state.avoidedConnectionId = route.waypointConnectionIds[route.nextWaypointIndex];
                    state.avoidConnectionUntilSeconds = clock.elapsedSeconds + kStalledConnectionAvoidSeconds;
                }
                clearRoute(route, position.value);
                state.currentTargetConnectionId.clear();
                state.currentTargetZoneId.clear();
                state.nextDecisionSeconds = 0.0;
            }

            if (clock.elapsedSeconds + 1e-9 < state.nextDecisionSeconds) {
                continue;
            }

            const auto signs = visibleSigns(layoutCache, signs_, position, agent, route, currentZoneId, clock.elapsedSeconds);
            auto candidate = chooseCandidate(
                query,
                layoutCache,
                spatialIndex,
                signs,
                state,
                position,
                agent,
                currentZoneId,
                clock.elapsedSeconds);
            if (!candidate.has_value()) {
                route.noExitAvailable = true;
                state.nextDecisionSeconds = clock.elapsedSeconds + kWayfindingDecisionCooldownSeconds;
                continue;
            }

            auto plan = routePlanToConnection(layoutCache, position.value, currentZoneId, *candidate);
            if (plan.destinationZoneId.empty()) {
                route.noExitAvailable = true;
                state.nextDecisionSeconds = clock.elapsedSeconds + kWayfindingDecisionCooldownSeconds;
                continue;
            }

            replaceRouteWithPlan(route, plan, position.value);
            state.intent = candidate->intent;
            state.currentTargetConnectionId = candidate->connectionId;
            state.currentTargetZoneId = candidate->nextZoneId;
            state.lastSeenSignId = candidate->signId;
            state.signCommitmentUntilSeconds = candidate->signId.empty()
                ? 0.0
                : clock.elapsedSeconds + kSignCommitmentSeconds;
            state.nextDecisionSeconds = clock.elapsedSeconds + kWayfindingDecisionCooldownSeconds;
        }
    }

private:
    ScenarioLayoutCacheResource layoutCache_{};
    std::vector<EvacuationSignDraft> signs_{};
};

}  // namespace

std::unique_ptr<engine::EngineSystem> makeScenarioWayfindingSystem(
    FacilityLayout2D layout,
    std::vector<EvacuationSignDraft> signs) {
    return std::make_unique<ScenarioWayfindingSystem>(std::move(layout), std::move(signs));
}

}  // namespace safecrowd::domain
