#include "domain/ScenarioSimulationRouteGuidance.h"

#include "domain/GeometryQueries.h"
#include "domain/ScenarioSimulationInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace safecrowd::domain::simulation_internal {

class ScenarioRouteGuidanceController::Impl final {
public:
    Impl() = default;

    explicit Impl(std::vector<RouteGuidanceDraft> routeGuidances)
        : routeGuidances_(std::move(routeGuidances)) {
    }

    static long long quantizedPlanningCoordinate(double value) {
        return static_cast<long long>(std::llround(value * 1000.0));
    }

    static std::string pointCachePart(const Point2D& point) {
        return std::to_string(quantizedPlanningCoordinate(point.x))
            + ","
            + std::to_string(quantizedPlanningCoordinate(point.y));
    }

    static std::string zoneRouteCacheKey(
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& exitZoneId) {
        return startZoneId + '\x1f' + pointCachePart(start) + '\x1f' + exitZoneId;
    }

    static std::string pathCacheKey(
        const std::string& floorId,
        const Point2D& start,
        const Point2D& goal,
        double clearance) {
        return floorId
            + '\x1f'
            + pointCachePart(start)
            + '\x1f'
            + pointCachePart(goal)
            + '\x1f'
            + std::to_string(quantizedPlanningCoordinate(clearance));
    }

    void refreshPlanningCache(std::uint64_t layoutRevision) const {
        if (planningCacheRevision_ == layoutRevision) {
            return;
        }
        planningCacheRevision_ = layoutRevision;
        nearestExitRouteCache_.clear();
        exitRouteCache_.clear();
        pathCache_.clear();
        hazardAwareExitPlanCache_.clear();
        routeHazardPenaltyCache_.clear();
    }

    std::optional<ZoneRouteToExit> cachedZoneRouteToNearestExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId) const {
        const auto key = zoneRouteCacheKey(start, startZoneId, std::string{"nearest"});
        const auto it = nearestExitRouteCache_.find(key);
        if (it != nearestExitRouteCache_.end()) {
            return it->second;
        }
        auto route = simulation_internal::zoneRouteToNearestExit(layoutCache, start, startZoneId);
        nearestExitRouteCache_.emplace(key, route);
        return route;
    }

    std::optional<ZoneRouteResult> cachedZoneRouteToExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& exitZoneId) const {
        const auto key = zoneRouteCacheKey(start, startZoneId, exitZoneId);
        const auto it = exitRouteCache_.find(key);
        if (it != exitRouteCache_.end()) {
            return it->second;
        }
        auto route = simulation_internal::zoneRouteToExit(layoutCache, start, startZoneId, exitZoneId);
        exitRouteCache_.emplace(key, route);
        return route;
    }

    std::optional<ZoneRouteToExit> zoneRouteToZone(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& targetZoneId) const {
        if (startZoneId.empty() || targetZoneId.empty()) {
            return std::nullopt;
        }
        if (startZoneId == targetZoneId) {
            return ZoneRouteToExit{.zoneIds = {startZoneId}};
        }

        auto stateKey = [](const std::string& zoneId, std::size_t entryConnectionIndex) {
            return zoneId + '\x1f' + std::to_string(entryConnectionIndex);
        };

        constexpr std::size_t kStartConnectionIndex = static_cast<std::size_t>(-1);
        struct QueueItem {
            double distance{0.0};
            std::string key{};
            std::string zoneId{};
            Point2D point{};

            bool operator>(const QueueItem& other) const noexcept {
                return distance > other.distance;
            }
        };

        std::unordered_map<std::string, double> distances;
        std::unordered_map<std::string, std::string> previous;
        std::unordered_map<std::string, std::string> stateZones;
        std::unordered_map<std::string, std::size_t> stateConnectionIndices;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;

        const auto startKey = stateKey(startZoneId, kStartConnectionIndex);
        distances[startKey] = 0.0;
        stateZones[startKey] = startZoneId;
        stateConnectionIndices[startKey] = kStartConnectionIndex;
        queue.push({
            .distance = 0.0,
            .key = startKey,
            .zoneId = startZoneId,
            .point = start,
        });

        std::string bestTargetKey;
        while (!queue.empty()) {
            const auto current = queue.top();
            queue.pop();

            const auto bestIt = distances.find(current.key);
            if (bestIt == distances.end() || current.distance > bestIt->second + 1e-12) {
                continue;
            }
            if (current.zoneId == targetZoneId) {
                bestTargetKey = current.key;
                break;
            }

            for (const auto& traversal : cachedTraversalsForZone(layoutCache, current.zoneId)) {
                if (traversal.nextZoneId.empty() || traversal.connectionIndex >= layoutCache.layout.connections.size()) {
                    continue;
                }
                const auto& connection = layoutCache.layout.connections[traversal.connectionIndex];
                const auto portal = midpoint(connection.centerSpan);
                const auto nextDistance = current.distance + distanceBetween(current.point, portal);
                const auto nextKey = stateKey(traversal.nextZoneId, traversal.connectionIndex);
                const auto distanceIt = distances.find(nextKey);
                if (distanceIt != distances.end() && distanceIt->second <= nextDistance + 1e-12) {
                    continue;
                }

                distances[nextKey] = nextDistance;
                previous[nextKey] = current.key;
                stateZones[nextKey] = traversal.nextZoneId;
                stateConnectionIndices[nextKey] = traversal.connectionIndex;
                queue.push({
                    .distance = nextDistance,
                    .key = nextKey,
                    .zoneId = traversal.nextZoneId,
                    .point = portal,
                });
            }
        }

        if (bestTargetKey.empty()) {
            return std::nullopt;
        }

        ZoneRouteToExit route;
        for (auto key = bestTargetKey; !key.empty();) {
            const auto zoneIt = stateZones.find(key);
            if (zoneIt != stateZones.end()) {
                route.zoneIds.push_back(zoneIt->second);
            }
            const auto connectionIt = stateConnectionIndices.find(key);
            if (connectionIt != stateConnectionIndices.end() && connectionIt->second != kStartConnectionIndex) {
                route.connectionIndices.push_back(connectionIt->second);
            }
            const auto previousIt = previous.find(key);
            key = previousIt == previous.end() ? std::string{} : previousIt->second;
        }
        std::reverse(route.zoneIds.begin(), route.zoneIds.end());
        std::reverse(route.connectionIndices.begin(), route.connectionIndices.end());
        if (route.zoneIds.empty() || route.connectionIndices.size() + 1 != route.zoneIds.size()) {
            return std::nullopt;
        }
        return route;
    }

    const std::vector<Point2D>& cachedPath(
        const std::string& floorId,
        const FacilityLayout2D& layout,
        const Point2D& start,
        const Point2D& goal,
        double clearance) const {
        const auto key = pathCacheKey(floorId, start, goal, clearance);
        const auto it = pathCache_.find(key);
        if (it != pathCache_.end()) {
            return it->second;
        }
        auto [insertedIt, _] = pathCache_.emplace(key, buildPath(layout, start, goal, clearance));
        return insertedIt->second;
    }

    const Connection2D* findConnectionById(
        const ScenarioLayoutCacheResource& layoutCache,
        const std::string& connectionId) const {
        if (connectionId.empty()) {
            return nullptr;
        }
        const auto it = layoutCache.connectionIndices.find(connectionId);
        if (it == layoutCache.connectionIndices.end() || it->second >= layoutCache.layout.connections.size()) {
            return nullptr;
        }
        return &layoutCache.layout.connections[it->second];
    }

    bool connectionTouchesFloor(
        const ScenarioLayoutCacheResource& layoutCache,
        const Connection2D& connection,
        const std::string& floorId) const {
        if (matchesFloor(connection.floorId, floorId)) {
            return true;
        }
        const auto fromFloorId = cachedFloorIdForZone(layoutCache, connection.fromZoneId);
        const auto toFloorId = cachedFloorIdForZone(layoutCache, connection.toZoneId);
        return matchesFloor(fromFloorId, floorId) || matchesFloor(toFloorId, floorId);
    }

    bool guidanceHasInstallPosition(const RouteGuidanceDraft& guidance) const {
        return !guidance.installFloorId.empty() || !guidance.installZoneId.empty();
    }

    bool agentCanSeeGuidanceAtInstallConnection(
        const ScenarioLayoutCacheResource& layoutCache,
        const RouteGuidanceDraft& guidance,
        const Position& position,
        const EvacuationRoute& route) const {
        if (guidance.installConnectionId.empty()) {
            return true;
        }

        const auto* connection = findConnectionById(layoutCache, guidance.installConnectionId);
        if (connection == nullptr || !connectionTouchesFloor(layoutCache, *connection, route.currentFloorId)) {
            return false;
        }

        const auto currentZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
        if (!currentZoneId.empty()
            && currentZoneId != connection->fromZoneId
            && currentZoneId != connection->toZoneId) {
            return false;
        }

        const auto closestOnInstall = closestPointOnSegment(
            position.value,
            connection->centerSpan.start,
            connection->centerSpan.end);
        const auto visibilityDistance = std::max(0.0, guidance.influenceRadiusMeters);
        return distanceBetween(position.value, closestOnInstall) <= visibilityDistance;
    }

    bool agentCanSeeGuidanceAtInstallPosition(
        const ScenarioLayoutCacheResource& layoutCache,
        const RouteGuidanceDraft& guidance,
        const Position& position,
        const Agent& agent,
        const EvacuationRoute& route) const {
        if (!guidanceHasInstallPosition(guidance) || !matchesFloor(guidance.installFloorId, route.currentFloorId)) {
            return false;
        }

        const auto currentZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
        if (!guidance.installZoneId.empty() && currentZoneId != guidance.installZoneId) {
            return false;
        }

        const auto visibilityDistance = std::max(0.0, guidance.influenceRadiusMeters);
        if (distanceBetween(position.value, guidance.installPosition) > visibilityDistance) {
            return false;
        }

        const auto& floorLayout = cachedLayoutForFloor(layoutCache, route.currentFloorId);
        const auto clearance = std::max(0.08, static_cast<double>(agent.radius) + 0.05);
        return lineOfSightClear(floorLayout, position.value, guidance.installPosition, clearance);
    }

    static std::uint64_t fnv1a64(const std::string& value) {
        std::uint64_t hash = 1469598103934665603ULL;
        for (const unsigned char ch : value) {
            hash ^= static_cast<std::uint64_t>(ch);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static std::uint64_t mix64(std::uint64_t value) {
        value += 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31U);
    }

    static double uniform01(std::uint64_t value) {
        const auto mixed = mix64(value);
        const auto mantissa = mixed >> 11U;
        return static_cast<double>(mantissa) * (1.0 / 9007199254740992.0);
    }

    static double clamp01(double value) {
        return std::clamp(value, 0.0, 1.0);
    }

    static double logit(double p) {
        const auto clamped = std::clamp(p, 1e-6, 1.0 - 1e-6);
        return std::log(clamped / (1.0 - clamped));
    }

    static double sigmoid(double x) {
        if (x >= 0.0) {
            const auto z = std::exp(-x);
            return 1.0 / (1.0 + z);
        }
        const auto z = std::exp(x);
        return z / (1.0 + z);
    }

    struct ActiveRouteGuidance {
        const RouteGuidanceDraft* guidance{nullptr};
        std::size_t guidanceIndex{0};
        std::size_t periodIndex{0};
        double startSeconds{0.0};
        double endSeconds{0.0};
    };

    std::vector<ActiveRouteGuidance> activeRouteGuidances(double elapsedSeconds) const {
        std::vector<ActiveRouteGuidance> active;
        for (std::size_t guidanceIndex = 0; guidanceIndex < routeGuidances_.size(); ++guidanceIndex) {
            const auto& guidance = routeGuidances_[guidanceIndex];
            if (guidance.periods.empty()) {
                const double start = 0.0;
                const double end = 1e18;
                if (elapsedSeconds + 1e-9 < start || elapsedSeconds > end + 1e-9) {
                    continue;
                }
                active.push_back(ActiveRouteGuidance{
                    .guidance = &guidance,
                    .guidanceIndex = guidanceIndex,
                    .periodIndex = 0,
                    .startSeconds = start,
                    .endSeconds = end,
                });
                continue;
            }

            for (std::size_t index = 0; index < guidance.periods.size(); ++index) {
                const auto& period = guidance.periods[index];
                const auto start = std::max(0.0, period.startSeconds);
                const auto end = std::max(start, std::max(0.0, period.endSeconds));
                if (elapsedSeconds + 1e-9 < start) {
                    continue;
                }
                if (elapsedSeconds > end + 1e-9) {
                    continue;
                }
                active.push_back(ActiveRouteGuidance{
                    .guidance = &guidance,
                    .guidanceIndex = guidanceIndex,
                    .periodIndex = index,
                    .startSeconds = start,
                    .endSeconds = end,
                });
            }
        }
        return active;
    }

    std::string activeRouteGuidanceKey(const ActiveRouteGuidance& active) const {
        if (active.guidance == nullptr) {
            return {};
        }
        auto id = active.guidance->id;
        if (id.empty()) {
            id = "route-guidance:";
            id.append(std::to_string(active.guidanceIndex));
        }
        if (!active.guidance->periods.empty()) {
            id.append(":p");
            id.append(std::to_string(active.periodIndex));
        }
        return id;
    }

    std::string activeRouteGuidanceSignature(const std::vector<ActiveRouteGuidance>& activeGuidances) const {
        std::string signature;
        for (const auto& active : activeGuidances) {
            if (!signature.empty()) {
                signature.push_back('\x1f');
            }
            signature.append(activeRouteGuidanceKey(active));
        }
        return signature;
    }

    static std::string activeHazardSignature(const ScenarioActiveEnvironmentHazardsResource* activeHazards) {
        if (activeHazards == nullptr || activeHazards->hazards.empty()) {
            return {};
        }
        if (!activeHazards->signature.empty()) {
            return activeHazards->signature;
        }

        std::string signature;
        for (const auto& hazard : activeHazards->hazards) {
            if (!signature.empty()) {
                signature.push_back('\x1f');
            }
            signature.append(hazard.key);
            signature.push_back('\x1e');
            signature.append(hazard.floorId);
            signature.push_back('\x1e');
            signature.append(pointCachePart(hazard.draft.position));
            signature.push_back('\x1e');
            signature.append(std::to_string(quantizedPlanningCoordinate(hazard.radiusMeters)));
            signature.push_back('\x1e');
            signature.append(std::to_string(quantizedPlanningCoordinate(hazard.routePenaltyMeters)));
        }
        return signature;
    }

    static constexpr double kHazardSafeRoutePenaltyMeters = 1.0;

    void refreshRouteSafetyCache(const std::string& activeGuidanceSignature,
                                 const std::string& activeHazardSignature) const {
        if (routeSafetyGuidanceSignature_ == activeGuidanceSignature
            && routeSafetyHazardSignature_ == activeHazardSignature) {
            return;
        }
        routeSafetyGuidanceSignature_ = activeGuidanceSignature;
        routeSafetyHazardSignature_ = activeHazardSignature;
        hazardAwareExitPlanCache_.clear();
        routeHazardPenaltyCache_.clear();
    }

    static bool hasActiveHazards(const ScenarioActiveEnvironmentHazardsResource* activeHazards) {
        return activeHazards != nullptr && !activeHazards->hazards.empty();
    }

    static bool agentHazardAware(
        const ScenarioEnvironmentReactionResource* reactions,
        engine::Entity entity) {
        if (reactions == nullptr) {
            return false;
        }
        const auto it = reactions->agentsById.find(entity.index);
        return it != reactions->agentsById.end() && it->second.hazardAware;
    }

    static ScenarioRoutePlan remainingRoutePlan(const EvacuationRoute& route) {
        ScenarioRoutePlan plan;
        plan.destinationZoneId = route.destinationZoneId;
        if (route.nextWaypointIndex >= route.waypoints.size()) {
            return plan;
        }

        for (std::size_t index = route.nextWaypointIndex; index < route.waypoints.size(); ++index) {
            plan.waypoints.push_back(route.waypoints[index]);
            plan.waypointPassages.push_back(index < route.waypointPassages.size()
                ? route.waypointPassages[index]
                : pointPassage(route.waypoints[index]));
            plan.waypointFromZoneIds.push_back(index < route.waypointFromZoneIds.size()
                ? route.waypointFromZoneIds[index]
                : std::string{});
            plan.waypointZoneIds.push_back(index < route.waypointZoneIds.size()
                ? route.waypointZoneIds[index]
                : std::string{});
            plan.waypointFloorIds.push_back(index < route.waypointFloorIds.size()
                ? route.waypointFloorIds[index]
                : std::string{});
            plan.waypointConnectionIds.push_back(index < route.waypointConnectionIds.size()
                ? route.waypointConnectionIds[index]
                : std::string{});
            plan.waypointVerticalTransitions.push_back(index < route.waypointVerticalTransitions.size()
                ? route.waypointVerticalTransitions[index]
                : false);
        }
        return plan;
    }

    static std::string routePlanSafetyKey(
        const std::string& activeHazardSignature,
        const Point2D& start,
        const std::string& startFloorId,
        const ScenarioRoutePlan& plan) {
        std::string key = activeHazardSignature;
        key.push_back('\x1d');
        key.append(startFloorId);
        key.push_back('\x1d');
        key.append(pointCachePart(start));
        key.push_back('\x1d');
        key.append(plan.destinationZoneId);
        for (std::size_t index = 0; index < plan.waypoints.size(); ++index) {
            key.push_back('\x1d');
            key.append(pointCachePart(plan.waypoints[index]));
            if (index < plan.waypointFloorIds.size()) {
                key.push_back('@');
                key.append(plan.waypointFloorIds[index]);
            }
            if (index < plan.waypointConnectionIds.size()) {
                key.push_back('#');
                key.append(plan.waypointConnectionIds[index]);
            }
        }
        return key;
    }

    std::string hazardAwareExitPlanCacheKey(
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& agentFloorId,
        const std::string& activeHazardSignature) const {
        return std::to_string(planningCacheRevision_)
            + '\x1f'
            + activeHazardSignature
            + '\x1f'
            + startZoneId
            + '\x1f'
            + agentFloorId
            + '\x1f'
            + pointCachePart(start);
    }

    double cachedHazardRoutePenalty(
        const ScenarioLayoutCacheResource& layoutCache,
        const ScenarioRoutePlan& plan,
        const Point2D& start,
        const std::string& startFloorId,
        const ScenarioActiveEnvironmentHazardsResource& activeHazards,
        const std::string& activeHazardSignature) const {
        if (activeHazardSignature.empty() || plan.destinationZoneId.empty()) {
            return 0.0;
        }

        const auto key = routePlanSafetyKey(activeHazardSignature, start, startFloorId, plan);
        const auto it = routeHazardPenaltyCache_.find(key);
        if (it != routeHazardPenaltyCache_.end()) {
            return it->second;
        }

        const auto penalty = hazardRoutePenalty(layoutCache, plan, start, startFloorId, activeHazards);
        routeHazardPenaltyCache_.emplace(key, penalty);
        return penalty;
    }

    bool remainingRouteHazardSafe(
        const ScenarioLayoutCacheResource& layoutCache,
        const EvacuationRoute& route,
        const Point2D& position,
        const std::string& startFloorId,
        const ScenarioActiveEnvironmentHazardsResource* activeHazards,
        const std::string& activeHazardSignature) const {
        if (!hasActiveHazards(activeHazards) || activeHazardSignature.empty()) {
            return true;
        }
        const auto plan = remainingRoutePlan(route);
        return cachedHazardRoutePenalty(
            layoutCache,
            plan,
            position,
            startFloorId,
            *activeHazards,
            activeHazardSignature) <= kHazardSafeRoutePenaltyMeters;
    }

    ScenarioRoutePlan safetyConstrainedGuidancePlan(
        const ScenarioLayoutCacheResource& layoutCache,
        const ScenarioRoutePlan& preferredPlan,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& agentFloorId,
        const ScenarioActiveEnvironmentHazardsResource* activeHazards,
        const std::string& activeHazardSignature) const {
        if (!hasActiveHazards(activeHazards) || activeHazardSignature.empty()) {
            return preferredPlan;
        }
        if (!preferredPlan.destinationZoneId.empty()
            && cachedHazardRoutePenalty(
                layoutCache,
                preferredPlan,
                start,
                agentFloorId,
                *activeHazards,
                activeHazardSignature) <= kHazardSafeRoutePenaltyMeters) {
            return preferredPlan;
        }

        auto fallback = routePlanToHazardAwareNearestExit(
            layoutCache,
            start,
            startZoneId,
            agentFloorId,
            *activeHazards);
        if (!fallback.destinationZoneId.empty()) {
            return fallback;
        }
        return preferredPlan;
    }

    const ActiveRouteGuidance* matchingActiveGuidance(
        const std::vector<ActiveRouteGuidance>& activeGuidances,
        const std::string& guidanceEventId) const {
        if (guidanceEventId.empty()) {
            return nullptr;
        }
        for (const auto& active : activeGuidances) {
            if (activeRouteGuidanceKey(active) == guidanceEventId) {
                return &active;
            }
        }
        return nullptr;
    }

    const ActiveRouteGuidance* applicableActiveGuidance(
        const std::vector<ActiveRouteGuidance>& activeGuidances,
        const ScenarioLayoutCacheResource& layoutCache,
        const Position& position,
        const Agent& agent,
        const EvacuationRoute& route) const {
        const ActiveRouteGuidance* best = nullptr;
        double bestStart = -1.0;
        for (const auto& active : activeGuidances) {
            if (active.guidance == nullptr) {
                continue;
            }
            const auto& guidance = *active.guidance;
            bool applicable = true;
            if (!guidance.installConnectionId.empty()) {
                applicable = agentCanSeeGuidanceAtInstallConnection(layoutCache, guidance, position, route);
            } else if (guidanceHasInstallPosition(guidance)) {
                applicable = agentCanSeeGuidanceAtInstallPosition(layoutCache, guidance, position, agent, route);
            }
            if (!applicable) {
                continue;
            }
            if (best == nullptr || active.startSeconds >= bestStart) {
                bestStart = active.startSeconds;
                best = &active;
            }
        }
        return best;
    }

    double guidancePriorityRadiusMeters(
        const ScenarioLayoutCacheResource& layoutCache,
        const RouteGuidanceDraft& guidance) const {
        (void)layoutCache;
        if (!guidance.installConnectionId.empty() || guidanceHasInstallPosition(guidance)) {
            return std::max(0.0, guidance.influenceRadiusMeters);
        }
        return 0.0;
    }

    std::vector<engine::Entity> prioritizedGuidanceEntities(
        engine::WorldQuery& query,
        const std::vector<ActiveRouteGuidance>& activeGuidances,
        const ScenarioLayoutCacheResource& layoutCache,
        const ScenarioAgentSpatialIndexResource* sharedSpatialIndex,
        const AgentSpatialIndex* localSpatialIndex) const {
        std::vector<engine::Entity> prioritized;

        auto appendNearby = [&](const Point2D& anchor, const std::string& floorId, double radius) {
            if (radius <= 0.0) {
                return;
            }

            std::vector<engine::Entity> nearby;
            if (sharedSpatialIndex != nullptr) {
                if (floorId.empty()) {
                    for (const auto& [candidateFloorId, _] : sharedSpatialIndex->cellsByFloor) {
                        const auto floorNearby = scenarioNearbyAgents(query, *sharedSpatialIndex, anchor, candidateFloorId, radius);
                        nearby.insert(nearby.end(), floorNearby.begin(), floorNearby.end());
                    }
                } else {
                    nearby = scenarioNearbyAgents(query, *sharedSpatialIndex, anchor, floorId, radius);
                }
            } else if (localSpatialIndex != nullptr) {
                if (floorId.empty()) {
                    nearby = nearbyAgents(query, *localSpatialIndex, anchor, radius);
                } else {
                    nearby = nearbyAgents(query, *localSpatialIndex, anchor, floorId, radius);
                }
            }
            prioritized.insert(prioritized.end(), nearby.begin(), nearby.end());
        };

        for (const auto& active : activeGuidances) {
            if (active.guidance == nullptr) {
                continue;
            }

            const auto& guidance = *active.guidance;
            const auto radius = guidancePriorityRadiusMeters(layoutCache, guidance);
            if (!guidance.installConnectionId.empty()) {
                const auto* connection = findConnectionById(layoutCache, guidance.installConnectionId);
                if (connection == nullptr) {
                    continue;
                }
                const auto floorId = connection->floorId.empty() ? guidance.installFloorId : connection->floorId;
                appendNearby(midpoint(connection->centerSpan), floorId, radius);
                continue;
            }

            if (guidanceHasInstallPosition(guidance)) {
                appendNearby(guidance.installPosition, guidance.installFloorId, radius);
            }
        }

        std::sort(prioritized.begin(), prioritized.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.index != rhs.index) {
                return lhs.index < rhs.index;
            }
            return lhs.generation < rhs.generation;
        });
        prioritized.erase(std::unique(prioritized.begin(), prioritized.end()), prioritized.end());
        return prioritized;
    }

    double complianceProbability(
        const RouteGuidanceDraft& guidance,
        const Agent& agent,
        double detourMeters) const {
        constexpr double kDetourWeight = 2.0;
        constexpr double kPropensityWeight = 1.0;

        const auto base = logit(clamp01(guidance.baseComplianceRate));
        const auto detourRatio = std::max(0.0, detourMeters) / std::max(1e-6, guidance.maxDetourMeters);
        const auto propensity = clamp01(agent.guidancePropensity);
        const auto score =
            base
            - (kDetourWeight * detourRatio)
            + (kPropensityWeight * logit(propensity));
        return clamp01(sigmoid(score));
    }

    static bool routeUsesConnection(const EvacuationRoute& route, const std::string& connectionId) {
        if (connectionId.empty()) {
            return false;
        }
        return std::any_of(route.waypointConnectionIds.begin(), route.waypointConnectionIds.end(), [&](const auto& id) {
            return id == connectionId;
        });
    }

    bool guidanceTargetsExitConnection(
        const ScenarioLayoutCacheResource& layoutCache,
        const RouteGuidanceDraft& guidance) const {
        if (guidance.installConnectionId.empty() || guidance.guidedExitZoneId.empty()) {
            return false;
        }
        const auto* connection = findConnectionById(layoutCache, guidance.installConnectionId);
        if (connection == nullptr) {
            return false;
        }
        return connection->fromZoneId == guidance.guidedExitZoneId || connection->toZoneId == guidance.guidedExitZoneId;
    }

    ScenarioRoutePlan routePlanForGuidance(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const RouteGuidanceDraft& guidance) const {
        ScenarioRoutePlan plan;
        if (guidanceTargetsExitConnection(layoutCache, guidance)) {
            plan = routePlanToExitThroughConnection(
                layoutCache,
                start,
                startZoneId,
                guidance.guidedExitZoneId,
                guidance.installConnectionId);
        }
        if (plan.destinationZoneId.empty()) {
            plan = routePlanToExit(layoutCache, start, startZoneId, guidance.guidedExitZoneId);
        }
        return plan;
    }

    void applyRouteGuidanceToEntity(
        engine::WorldQuery& query,
        engine::Entity entity,
        const ScenarioLayoutCacheResource& layoutCache,
        const std::vector<ActiveRouteGuidance>& activeGuidances,
        double elapsedSeconds,
        std::uint64_t stableSeed,
        const ScenarioEnvironmentReactionResource* reactions,
        const ScenarioActiveEnvironmentHazardsResource* activeHazards,
        const std::string& activeHazardSignature) {
        const auto& status = query.get<EvacuationStatus>(entity);
        if (status.evacuated) {
            return;
        }

        const auto& position = query.get<Position>(entity);
        const auto& agent = query.get<Agent>(entity);
        auto& route = query.get<EvacuationRoute>(entity);
        if (route.originalDestinationZoneId.empty()) {
            route.originalDestinationZoneId = route.destinationZoneId;
        }

        const auto startZoneId = zoneAt(layoutCache, position.value, route.currentFloorId);
        if (startZoneId.empty()) {
            return;
        }
        const auto agentFloorId = !route.displayFloorId.empty() ? route.displayFloorId : route.currentFloorId;
        const auto useHazardAwareSafety = agentHazardAware(reactions, entity)
            && hasActiveHazards(activeHazards)
            && !activeHazardSignature.empty();

        if (activeGuidances.empty()) {
            route.guidanceEventId.clear();
            route.followsGuidance = false;

            std::string desiredExit = route.originalDestinationZoneId;
            if (desiredExit.empty()) {
                return;
            }

            if (route.destinationZoneId == desiredExit && !route.waypoints.empty()
                && (!useHazardAwareSafety
                    || remainingRouteHazardSafe(
                        layoutCache,
                        route,
                        position.value,
                        agentFloorId,
                        activeHazards,
                        activeHazardSignature))) {
                return;
            }

            ScenarioRoutePlan plan = routePlanToExit(layoutCache, position.value, startZoneId, desiredExit);
            if (plan.destinationZoneId.empty()) {
                plan = routePlanToNearestExit(layoutCache, position.value, startZoneId);
            }
            if (useHazardAwareSafety) {
                plan = safetyConstrainedGuidancePlan(
                    layoutCache,
                    plan,
                    position.value,
                    startZoneId,
                    agentFloorId,
                    activeHazards,
                    activeHazardSignature);
            }
            if (plan.destinationZoneId.empty()) {
                return;
            }
            if (route.destinationZoneId == plan.destinationZoneId && !route.waypoints.empty()
                && (!useHazardAwareSafety
                    || remainingRouteHazardSafe(
                        layoutCache,
                        route,
                        position.value,
                        agentFloorId,
                        activeHazards,
                        activeHazardSignature))) {
                return;
            }
            replaceRouteWithPlan(route, plan, position.value);
            route.nextExitReplanSeconds = elapsedSeconds + 0.25;
            return;
        }

        const auto* visibleGuidance = applicableActiveGuidance(activeGuidances, layoutCache, position, agent, route);
        if (visibleGuidance != nullptr && visibleGuidance->guidance != nullptr
            && (!visibleGuidance->guidance->installConnectionId.empty()
                || guidanceHasInstallPosition(*visibleGuidance->guidance))) {
            if (route.guidanceEventId == activeRouteGuidanceKey(*visibleGuidance)) {
                const bool routeNeedsConnectionCorrection =
                    guidanceTargetsExitConnection(layoutCache, *visibleGuidance->guidance)
                    && !routeUsesConnection(route, visibleGuidance->guidance->installConnectionId);
                if (!routeNeedsConnectionCorrection
                    && (!useHazardAwareSafety
                        || remainingRouteHazardSafe(
                            layoutCache,
                            route,
                            position.value,
                            agentFloorId,
                            activeHazards,
                            activeHazardSignature))) {
                    return;
                }
            }
        }

        const auto* activeGuidance = visibleGuidance;
        if (activeGuidance == nullptr) {
            if (const auto* retained = matchingActiveGuidance(activeGuidances, route.guidanceEventId);
                retained != nullptr
                && retained->guidance != nullptr
                && (!retained->guidance->installConnectionId.empty() || guidanceHasInstallPosition(*retained->guidance))) {
                const bool routeNeedsConnectionCorrection =
                    guidanceTargetsExitConnection(layoutCache, *retained->guidance)
                    && !routeUsesConnection(route, retained->guidance->installConnectionId);
                if (!routeNeedsConnectionCorrection
                    && (!useHazardAwareSafety
                        || remainingRouteHazardSafe(
                            layoutCache,
                            route,
                            position.value,
                            agentFloorId,
                            activeHazards,
                            activeHazardSignature))) {
                    return;
                }
                activeGuidance = retained;
            }
        }

        if (activeGuidance == nullptr || activeGuidance->guidance == nullptr) {
            if (!route.guidanceEventId.empty() || route.followsGuidance) {
                route.guidanceEventId.clear();
                route.followsGuidance = false;
                const auto& desiredExit = route.originalDestinationZoneId;
                if (!desiredExit.empty()) {
                    const bool currentRouteSafe = !useHazardAwareSafety
                        || remainingRouteHazardSafe(
                            layoutCache,
                            route,
                            position.value,
                            agentFloorId,
                            activeHazards,
                            activeHazardSignature);
                    const bool currentNeedsPlan =
                        route.destinationZoneId != desiredExit || route.waypoints.empty() || !currentRouteSafe;
                    if (!currentNeedsPlan) {
                        return;
                    }

                    auto plan = routePlanToExit(layoutCache, position.value, startZoneId, desiredExit);
                    if (plan.destinationZoneId.empty()) {
                        plan = routePlanToNearestExit(layoutCache, position.value, startZoneId);
                    }
                    if (useHazardAwareSafety) {
                        plan = safetyConstrainedGuidancePlan(
                            layoutCache,
                            plan,
                            position.value,
                            startZoneId,
                            agentFloorId,
                            activeHazards,
                            activeHazardSignature);
                    }
                    if (!plan.destinationZoneId.empty()
                        && (plan.destinationZoneId != route.destinationZoneId
                            || route.waypoints.empty()
                            || !currentRouteSafe)) {
                        replaceRouteWithPlan(route, plan, position.value);
                        route.nextExitReplanSeconds = elapsedSeconds + 0.25;
                    }
                }
            }
            return;
        }

        const auto& selectedGuidance = *activeGuidance->guidance;
        const auto activeId = activeRouteGuidanceKey(*activeGuidance);
        const auto activeIdHash = fnv1a64(activeId);

        bool guidedExitValid = false;
        if (!selectedGuidance.guidedExitZoneId.empty()) {
            if (const auto* exitZone = findCachedZone(layoutCache, selectedGuidance.guidedExitZoneId);
                exitZone != nullptr && exitZone->kind == ZoneKind::Exit) {
                guidedExitValid = true;
            }
        }

        double detourMeters = 0.0;
        if (guidedExitValid && !route.originalDestinationZoneId.empty()) {
            const auto* originalExit = findCachedZone(layoutCache, route.originalDestinationZoneId);
            const auto* guidedExit = findCachedZone(layoutCache, selectedGuidance.guidedExitZoneId);
            if (originalExit != nullptr && guidedExit != nullptr && originalExit->kind == ZoneKind::Exit
                && guidedExit->kind == ZoneKind::Exit) {
                const auto originalDistance = distanceBetween(position.value, polygonCenter(originalExit->area));
                const auto guidedDistance = distanceBetween(position.value, polygonCenter(guidedExit->area));
                detourMeters = std::max(0.0, guidedDistance - originalDistance);
            }
        }

        const auto pFollow = complianceProbability(selectedGuidance, agent, detourMeters);
        const auto u = uniform01(
            stableSeed
            ^ activeIdHash
            ^ (static_cast<std::uint64_t>(entity.index) << 1U)
            ^ static_cast<std::uint64_t>(entity.generation));
        const bool follows = u < pFollow;

        route.guidanceEventId = activeId;
        route.followsGuidance = follows;

        std::string desiredExit;
        if (follows && guidedExitValid) {
            desiredExit = selectedGuidance.guidedExitZoneId;
        } else if (!follows) {
            desiredExit = route.originalDestinationZoneId;
        }

        const bool shouldUseGuidanceConnection = follows
            && guidedExitValid
            && guidanceTargetsExitConnection(layoutCache, selectedGuidance);
        if (!desiredExit.empty() && route.destinationZoneId == desiredExit && !route.waypoints.empty()
            && !shouldUseGuidanceConnection) {
            if (!useHazardAwareSafety
                || remainingRouteHazardSafe(
                    layoutCache,
                    route,
                    position.value,
                    agentFloorId,
                    activeHazards,
                    activeHazardSignature)) {
                return;
            }
        }

        ScenarioRoutePlan plan;
        if (!desiredExit.empty()) {
            if (shouldUseGuidanceConnection) {
                plan = routePlanForGuidance(layoutCache, position.value, startZoneId, selectedGuidance);
            } else {
                plan = routePlanToExit(layoutCache, position.value, startZoneId, desiredExit);
            }
        }
        if (useHazardAwareSafety) {
            plan = safetyConstrainedGuidancePlan(
                layoutCache,
                plan,
                position.value,
                startZoneId,
                agentFloorId,
                activeHazards,
                activeHazardSignature);
        }
        if (plan.destinationZoneId.empty()) {
            if (useHazardAwareSafety && activeHazards != nullptr) {
                plan = routePlanToHazardAwareNearestExit(
                    layoutCache,
                    position.value,
                    startZoneId,
                    agentFloorId,
                    *activeHazards);
            }
            if (plan.destinationZoneId.empty()) {
                plan = routePlanToNearestExit(layoutCache, position.value, startZoneId);
            }
        }
        if (plan.destinationZoneId.empty()) {
            return;
        }
        replaceRouteWithPlan(route, plan, position.value);
        route.nextExitReplanSeconds = elapsedSeconds + 0.25;
    }

    void applyRouteGuidance(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache,
        double elapsedSeconds,
        std::uint64_t derivedSeed,
        const ScenarioEnvironmentReactionResource* reactions,
        const ScenarioActiveEnvironmentHazardsResource* activeHazards,
        const ScenarioAgentSpatialIndexResource* sharedSpatialIndex) {
        // Keep this small to avoid frame spikes when guidance toggles.
        // Higher values converge faster but may cause noticeable hitching with many agents.
        constexpr std::size_t kGuidanceReplanBudgetPerFrame = 50;

        const auto activeGuidances = activeRouteGuidances(elapsedSeconds);
        const auto activeSignature = activeRouteGuidanceSignature(activeGuidances);
        const auto hazardSignature = activeHazardSignature(activeHazards);
        refreshRouteSafetyCache(activeSignature, hazardSignature);
        const auto hasVisibilityAnchoredGuidance = std::any_of(activeGuidances.begin(), activeGuidances.end(), [&](const auto& active) {
            return active.guidance != nullptr
                && (!active.guidance->installConnectionId.empty() || guidanceHasInstallPosition(*active.guidance));
        });
        const auto hasGlobalGuidance = std::any_of(activeGuidances.begin(), activeGuidances.end(), [&](const auto& active) {
            return active.guidance != nullptr
                && active.guidance->installConnectionId.empty()
                && !guidanceHasInstallPosition(*active.guidance);
        });

        if (activeSignature != activeRouteGuidanceId_) {
            activeRouteGuidanceId_ = activeSignature;
            guidanceReplanCursor_ = 0;
            guidanceReplanSeed_ = derivedSeed;
            guidanceReplanPending_ = true;
        } else if (hasGlobalGuidance && !guidanceReplanPending_) {
            guidanceReplanCursor_ = 0;
            guidanceReplanSeed_ = derivedSeed;
            guidanceReplanPending_ = true;
        }

        std::optional<AgentSpatialIndex> localGuidanceIndex;
        if (hasVisibilityAnchoredGuidance && sharedSpatialIndex == nullptr) {
            localGuidanceIndex = buildAgentSpatialIndex(query, entities, 1.0);
        }

        if (hasVisibilityAnchoredGuidance) {
            const auto prioritizedEntities = prioritizedGuidanceEntities(
                query,
                activeGuidances,
                layoutCache,
                sharedSpatialIndex,
                localGuidanceIndex.has_value() ? &(*localGuidanceIndex) : nullptr);
            for (const auto entity : prioritizedEntities) {
                applyRouteGuidanceToEntity(
                    query,
                    entity,
                    layoutCache,
                    activeGuidances,
                    elapsedSeconds,
                    guidanceReplanSeed_,
                    reactions,
                    activeHazards,
                    hazardSignature);
            }
        }

        if (!guidanceReplanPending_ || guidanceReplanCursor_ >= entities.size()) {
            return;
        }

        const auto endIndex = std::min<std::size_t>(entities.size(), guidanceReplanCursor_ + kGuidanceReplanBudgetPerFrame);
        const auto stableSeed = guidanceReplanSeed_;

        for (std::size_t i = guidanceReplanCursor_; i < endIndex; ++i) {
            applyRouteGuidanceToEntity(
                query,
                entities[i],
                layoutCache,
                activeGuidances,
                elapsedSeconds,
                stableSeed,
                reactions,
                activeHazards,
                hazardSignature);
        }

        guidanceReplanCursor_ = endIndex;
        if (guidanceReplanCursor_ >= entities.size()) {
            guidanceReplanPending_ = false;
        }
    }

    ScenarioRoutePlan routePlanToNearestExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId) const {
        ScenarioRoutePlan plan;
        auto zoneRoute = cachedZoneRouteToNearestExit(layoutCache, start, startZoneId);
        if (!zoneRoute.has_value() || zoneRoute->empty()) {
            return plan;
        }

        plan.destinationZoneId = zoneRoute->zoneIds.back();

        Point2D segmentStart = start;
        auto appendSegment = [&](const std::vector<Point2D>& segment,
                                 const LineSegment2D& finalPassage,
                                 const std::string& finalFromZoneId,
                                 const std::string& finalZoneId,
                                 const std::string& finalFloorId,
                                 const std::string& finalConnectionId,
                                 bool finalVerticalTransition) {
            for (std::size_t waypointIndex = 0; waypointIndex < segment.size(); ++waypointIndex) {
                const bool isFinalWaypoint = waypointIndex + 1 == segment.size();
                plan.waypoints.push_back(segment[waypointIndex]);
                plan.waypointPassages.push_back(isFinalWaypoint ? finalPassage : pointPassage(segment[waypointIndex]));
                plan.waypointFromZoneIds.push_back(isFinalWaypoint ? finalFromZoneId : std::string{});
                plan.waypointZoneIds.push_back(isFinalWaypoint ? finalZoneId : std::string{});
                plan.waypointFloorIds.push_back(isFinalWaypoint ? finalFloorId : std::string{});
                plan.waypointConnectionIds.push_back(isFinalWaypoint ? finalConnectionId : std::string{});
                plan.waypointVerticalTransitions.push_back(isFinalWaypoint && finalVerticalTransition);
            }
        };

        for (std::size_t index = 1; index < zoneRoute->zoneIds.size(); ++index) {
            const auto& fromZoneId = zoneRoute->zoneIds[index - 1];
            const auto& toZoneId = zoneRoute->zoneIds[index];
            const auto connectionIndex = zoneRoute->connectionIndices[index - 1];
            if (connectionIndex < layoutCache.layout.connections.size()) {
                const auto* connection = &layoutCache.layout.connections[connectionIndex];
                const auto passage = passageWithClearance(*connection, kCandidateClearance);
                const auto fromFloorId = cachedFloorIdForZone(layoutCache, fromZoneId);
                const auto toFloorId = cachedFloorIdForZone(layoutCache, toZoneId);
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, fromFloorId);
                const auto target = closestPointOnSegment(segmentStart, passage.start, passage.end);
                const auto& segment = cachedPath(fromFloorId, segmentLayout, segmentStart, target, kCandidateClearance);
                appendSegment(
                    segment,
                    passage,
                    fromZoneId,
                    toZoneId,
                    toFloorId.empty() ? fromFloorId : toFloorId,
                    connection->id,
                    isVerticalConnection(*connection));
                segmentStart = target;
            }
        }

        if (const auto* exitZone = findCachedZone(layoutCache, zoneRoute->zoneIds.back())) {
            const auto exitCenter = polygonCenter(exitZone->area);
            if (distanceBetween(segmentStart, exitCenter) > kArrivalEpsilon) {
                const auto exitFloorId = exitZone->floorId;
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, exitFloorId);
                const auto& segment = cachedPath(exitFloorId, segmentLayout, segmentStart, exitCenter, kCandidateClearance);
                appendSegment(segment, pointPassage(exitCenter), std::string{}, exitZone->id, exitFloorId, std::string{}, false);
            }
        }

        if (!plan.waypoints.empty() && distanceBetween(start, plan.waypoints.front()) <= kArrivalEpsilon) {
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

    ScenarioRoutePlan routePlanToExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& exitZoneId) const {
        ScenarioRoutePlan plan;
        const auto zoneRouteResult = cachedZoneRouteToExit(layoutCache, start, startZoneId, exitZoneId);
        if (!zoneRouteResult.has_value() || zoneRouteResult->route.empty()) {
            return plan;
        }

        const auto& zoneRoute = zoneRouteResult->route;
        plan.destinationZoneId = zoneRoute.zoneIds.back();

        Point2D segmentStart = start;
        auto appendSegment = [&](const std::vector<Point2D>& segment,
                                 const LineSegment2D& finalPassage,
                                 const std::string& finalFromZoneId,
                                 const std::string& finalZoneId,
                                 const std::string& finalFloorId,
                                 const std::string& finalConnectionId,
                                 bool finalVerticalTransition) {
            for (std::size_t waypointIndex = 0; waypointIndex < segment.size(); ++waypointIndex) {
                const bool isFinalWaypoint = waypointIndex + 1 == segment.size();
                plan.waypoints.push_back(segment[waypointIndex]);
                plan.waypointPassages.push_back(isFinalWaypoint ? finalPassage : pointPassage(segment[waypointIndex]));
                plan.waypointFromZoneIds.push_back(isFinalWaypoint ? finalFromZoneId : std::string{});
                plan.waypointZoneIds.push_back(isFinalWaypoint ? finalZoneId : std::string{});
                plan.waypointFloorIds.push_back(isFinalWaypoint ? finalFloorId : std::string{});
                plan.waypointConnectionIds.push_back(isFinalWaypoint ? finalConnectionId : std::string{});
                plan.waypointVerticalTransitions.push_back(isFinalWaypoint && finalVerticalTransition);
            }
        };

        for (std::size_t index = 1; index < zoneRoute.zoneIds.size(); ++index) {
            const auto& fromZoneId = zoneRoute.zoneIds[index - 1];
            const auto& toZoneId = zoneRoute.zoneIds[index];
            const auto connectionIndex = zoneRoute.connectionIndices[index - 1];
            if (connectionIndex < layoutCache.layout.connections.size()) {
                const auto* connection = &layoutCache.layout.connections[connectionIndex];
                const auto passage = passageWithClearance(*connection, kCandidateClearance);
                const auto fromFloorId = cachedFloorIdForZone(layoutCache, fromZoneId);
                const auto toFloorId = cachedFloorIdForZone(layoutCache, toZoneId);
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, fromFloorId);
                const auto target = closestPointOnSegment(segmentStart, passage.start, passage.end);
                const auto& segment = cachedPath(fromFloorId, segmentLayout, segmentStart, target, kCandidateClearance);
                appendSegment(
                    segment,
                    passage,
                    fromZoneId,
                    toZoneId,
                    toFloorId.empty() ? fromFloorId : toFloorId,
                    connection->id,
                    isVerticalConnection(*connection));
                segmentStart = target;
            }
        }

        if (const auto* exitZone = findCachedZone(layoutCache, zoneRoute.zoneIds.back())) {
            const auto exitCenter = polygonCenter(exitZone->area);
            if (distanceBetween(segmentStart, exitCenter) > kArrivalEpsilon) {
                const auto exitFloorId = exitZone->floorId;
                const auto& segmentLayout = cachedLayoutForFloor(layoutCache, exitFloorId);
                const auto& segment = cachedPath(exitFloorId, segmentLayout, segmentStart, exitCenter, kCandidateClearance);
                appendSegment(segment, pointPassage(exitCenter), std::string{}, exitZone->id, exitFloorId, std::string{}, false);
            }
        }

        if (!plan.waypoints.empty() && distanceBetween(start, plan.waypoints.front()) <= kArrivalEpsilon) {
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

    ScenarioRoutePlan routePlanToExitThroughConnection(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& exitZoneId,
        const std::string& connectionId) const {
        ScenarioRoutePlan plan;
        const auto* targetConnection = findConnectionById(layoutCache, connectionId);
        if (targetConnection == nullptr || exitZoneId.empty()) {
            return plan;
        }

        std::string approachZoneId;
        if (targetConnection->toZoneId == exitZoneId) {
            approachZoneId = targetConnection->fromZoneId;
        } else if (targetConnection->fromZoneId == exitZoneId) {
            approachZoneId = targetConnection->toZoneId;
        } else {
            return plan;
        }
        if (approachZoneId.empty()) {
            return plan;
        }

        const auto& approachTraversals = cachedTraversalsForZone(layoutCache, approachZoneId);
        const auto canTraverseTargetConnection = std::any_of(
            approachTraversals.begin(),
            approachTraversals.end(),
            [&](const auto& traversal) {
                if (traversal.nextZoneId != exitZoneId || traversal.connectionIndex >= layoutCache.layout.connections.size()) {
                    return false;
                }
                return layoutCache.layout.connections[traversal.connectionIndex].id == connectionId;
            });
        if (!canTraverseTargetConnection) {
            return plan;
        }

        const auto approachRoute = zoneRouteToZone(layoutCache, start, startZoneId, approachZoneId);
        if (!approachRoute.has_value() || approachRoute->empty()) {
            return plan;
        }

        plan.destinationZoneId = exitZoneId;

        Point2D segmentStart = start;
        auto appendSegment = [&](const std::vector<Point2D>& segment,
                                 const LineSegment2D& finalPassage,
                                 const std::string& finalFromZoneId,
                                 const std::string& finalZoneId,
                                 const std::string& finalFloorId,
                                 const std::string& finalConnectionId,
                                 bool finalVerticalTransition) {
            for (std::size_t waypointIndex = 0; waypointIndex < segment.size(); ++waypointIndex) {
                const bool isFinalWaypoint = waypointIndex + 1 == segment.size();
                plan.waypoints.push_back(segment[waypointIndex]);
                plan.waypointPassages.push_back(isFinalWaypoint ? finalPassage : pointPassage(segment[waypointIndex]));
                plan.waypointFromZoneIds.push_back(isFinalWaypoint ? finalFromZoneId : std::string{});
                plan.waypointZoneIds.push_back(isFinalWaypoint ? finalZoneId : std::string{});
                plan.waypointFloorIds.push_back(isFinalWaypoint ? finalFloorId : std::string{});
                plan.waypointConnectionIds.push_back(isFinalWaypoint ? finalConnectionId : std::string{});
                plan.waypointVerticalTransitions.push_back(isFinalWaypoint && finalVerticalTransition);
            }
        };

        for (std::size_t index = 1; index < approachRoute->zoneIds.size(); ++index) {
            const auto& fromZoneId = approachRoute->zoneIds[index - 1];
            const auto& toZoneId = approachRoute->zoneIds[index];
            const auto connectionIndex = approachRoute->connectionIndices[index - 1];
            if (connectionIndex >= layoutCache.layout.connections.size()) {
                continue;
            }
            const auto* connection = &layoutCache.layout.connections[connectionIndex];
            const auto passage = passageWithClearance(*connection, kCandidateClearance);
            const auto fromFloorId = cachedFloorIdForZone(layoutCache, fromZoneId);
            const auto toFloorId = cachedFloorIdForZone(layoutCache, toZoneId);
            const auto& segmentLayout = cachedLayoutForFloor(layoutCache, fromFloorId);
            const auto target = closestPointOnSegment(segmentStart, passage.start, passage.end);
            const auto& segment = cachedPath(fromFloorId, segmentLayout, segmentStart, target, kCandidateClearance);
            appendSegment(
                segment,
                passage,
                fromZoneId,
                toZoneId,
                toFloorId.empty() ? fromFloorId : toFloorId,
                connection->id,
                isVerticalConnection(*connection));
            segmentStart = target;
        }

        const auto passage = passageWithClearance(*targetConnection, kCandidateClearance);
        const auto fromFloorId = cachedFloorIdForZone(layoutCache, approachZoneId);
        const auto exitFloorId = cachedFloorIdForZone(layoutCache, exitZoneId);
        const auto& segmentLayout = cachedLayoutForFloor(layoutCache, fromFloorId);
        const auto target = closestPointOnSegment(segmentStart, passage.start, passage.end);
        const auto& segment = cachedPath(fromFloorId, segmentLayout, segmentStart, target, kCandidateClearance);
        appendSegment(
            segment,
            passage,
            approachZoneId,
            exitZoneId,
            exitFloorId.empty() ? fromFloorId : exitFloorId,
            targetConnection->id,
            isVerticalConnection(*targetConnection));
        segmentStart = target;

        if (const auto* exitZone = findCachedZone(layoutCache, exitZoneId)) {
            const auto exitCenter = polygonCenter(exitZone->area);
            if (distanceBetween(segmentStart, exitCenter) > kArrivalEpsilon) {
                const auto& exitLayout = cachedLayoutForFloor(layoutCache, exitFloorId);
                const auto& exitSegment = cachedPath(exitFloorId, exitLayout, segmentStart, exitCenter, kCandidateClearance);
                appendSegment(exitSegment, pointPassage(exitCenter), std::string{}, exitZone->id, exitFloorId, std::string{}, false);
            }
        }

        if (!plan.waypoints.empty() && distanceBetween(start, plan.waypoints.front()) <= kArrivalEpsilon) {
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

    static Point2D interpolateSegmentPoint(const Point2D& start, const Point2D& end, double t) {
        return {
            .x = start.x + ((end.x - start.x) * t),
            .y = start.y + ((end.y - start.y) * t),
        };
    }

    static double dotDelta(double ax, double ay, double bx, double by) {
        return (ax * bx) + (ay * by);
    }

    static double hazardSegmentExposureMeters(
        const ScenarioActiveEnvironmentHazard& hazard,
        const Point2D& segmentStart,
        const Point2D& segmentEnd) {
        const auto radius = std::max(0.0, hazard.radiusMeters);
        if (radius <= 1e-9) {
            return 0.0;
        }

        const auto dx = segmentEnd.x - segmentStart.x;
        const auto dy = segmentEnd.y - segmentStart.y;
        const auto lengthSquared = (dx * dx) + (dy * dy);
        if (lengthSquared <= 1e-12) {
            return 0.0;
        }

        const auto fx = segmentStart.x - hazard.draft.position.x;
        const auto fy = segmentStart.y - hazard.draft.position.y;
        const auto a = lengthSquared;
        const auto b = 2.0 * dotDelta(fx, fy, dx, dy);
        const auto c = dotDelta(fx, fy, fx, fy) - (radius * radius);
        const auto discriminant = (b * b) - (4.0 * a * c);
        if (discriminant < -1e-12) {
            return 0.0;
        }

        double enter = 0.0;
        double exit = 1.0;
        if (discriminant >= 0.0) {
            const auto root = std::sqrt(std::max(0.0, discriminant));
            enter = (-b - root) / (2.0 * a);
            exit = (-b + root) / (2.0 * a);
        }

        enter = std::clamp(enter, 0.0, 1.0);
        exit = std::clamp(exit, 0.0, 1.0);
        if (exit <= enter + 1e-12) {
            const auto startInside = distanceBetween(segmentStart, hazard.draft.position) <= radius + 1e-9;
            const auto endInside = distanceBetween(segmentEnd, hazard.draft.position) <= radius + 1e-9;
            if (!startInside && !endInside) {
                return 0.0;
            }
            enter = 0.0;
            exit = 1.0;
        }

        const auto overlapLength = std::sqrt(lengthSquared) * (exit - enter);
        if (overlapLength <= 1e-9) {
            return 0.0;
        }

        const auto midpoint = interpolateSegmentPoint(segmentStart, segmentEnd, (enter + exit) * 0.5);
        const auto influence = environmentHazardInfluenceAt(
            hazard.draft,
            distanceBetween(midpoint, hazard.draft.position));
        return overlapLength * influence;
    }

    double hazardRoutePenalty(
        const ScenarioLayoutCacheResource& layoutCache,
        const ScenarioRoutePlan& plan,
        const Point2D& start,
        const std::string& startFloorId,
        const ScenarioActiveEnvironmentHazardsResource& activeHazards) const {
        if (activeHazards.hazards.empty()) {
            return 0.0;
        }

        std::vector<double> exposureMeters(activeHazards.hazards.size(), 0.0);

        auto addSegmentExposure = [&](const Point2D& segmentStart,
                                      const Point2D& segmentEnd,
                                      const std::string& segmentFloorId) {
            const auto candidateHazards = scenarioHazardIndicesNearSegment(
                activeHazards,
                segmentStart,
                segmentEnd,
                segmentFloorId,
                activeHazards.maxRadiusMeters);
            for (const auto hazardIndex : candidateHazards) {
                if (hazardIndex >= activeHazards.hazards.size()) {
                    continue;
                }
                const auto& hazard = activeHazards.hazards[hazardIndex];
                if (!matchesFloor(segmentFloorId, hazard.floorId)) {
                    continue;
                }
                exposureMeters[hazardIndex] += hazardSegmentExposureMeters(hazard, segmentStart, segmentEnd);
            }
        };

        Point2D segmentStart = start;
        std::string segmentFloorId = startFloorId;
        for (std::size_t waypointIndex = 0; waypointIndex < plan.waypoints.size(); ++waypointIndex) {
            const auto& segmentEnd = plan.waypoints[waypointIndex];
            addSegmentExposure(segmentStart, segmentEnd, segmentFloorId);

            if (waypointIndex < plan.waypointConnectionIds.size()) {
                const auto* connection = findConnectionById(layoutCache, plan.waypointConnectionIds[waypointIndex]);
                if (connection != nullptr) {
                    const auto connectionFloorId = connection->floorId.empty()
                        ? cachedFloorIdForZone(layoutCache, connection->fromZoneId)
                        : connection->floorId;
                    addSegmentExposure(connection->centerSpan.start, connection->centerSpan.end, connectionFloorId);
                }
            }

            segmentStart = segmentEnd;
            if (waypointIndex < plan.waypointFloorIds.size()
                && !plan.waypointFloorIds[waypointIndex].empty()) {
                segmentFloorId = plan.waypointFloorIds[waypointIndex];
            }
        }

        double penalty = 0.0;
        for (std::size_t hazardIndex = 0; hazardIndex < activeHazards.hazards.size(); ++hazardIndex) {
            const auto& hazard = activeHazards.hazards[hazardIndex];
            const auto radius = std::max(0.0, hazard.radiusMeters);
            if (radius <= 1e-9 || exposureMeters[hazardIndex] <= 1e-9) {
                continue;
            }
            penalty += hazard.routePenaltyMeters * std::clamp(exposureMeters[hazardIndex] / radius, 0.0, 1.0);
        }
        return penalty;
    }

    ScenarioRoutePlan routePlanToHazardAwareNearestExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& agentFloorId,
        const ScenarioActiveEnvironmentHazardsResource& activeHazards) const {
        const auto hazardSignature = activeHazardSignature(&activeHazards);
        const auto cacheKey = hazardAwareExitPlanCacheKey(start, startZoneId, agentFloorId, hazardSignature);
        const auto cachedIt = hazardAwareExitPlanCache_.find(cacheKey);
        if (cachedIt != hazardAwareExitPlanCache_.end()) {
            return cachedIt->second;
        }

        ScenarioRoutePlan bestPlan;
        double bestPenalty = std::numeric_limits<double>::max();
        double bestDistance = std::numeric_limits<double>::max();

        for (const auto& zone : layoutCache.layout.zones) {
            if (zone.kind != ZoneKind::Exit) {
                continue;
            }

            const auto result = cachedZoneRouteToExit(layoutCache, start, startZoneId, zone.id);
            if (!result.has_value() || result->route.empty()) {
                continue;
            }

            const auto plan = routePlanToExit(layoutCache, start, startZoneId, zone.id);
            if (plan.destinationZoneId.empty()) {
                continue;
            }

            const auto penalty = cachedHazardRoutePenalty(
                layoutCache,
                plan,
                start,
                agentFloorId,
                activeHazards,
                hazardSignature);
            if (penalty + 1e-9 < bestPenalty
                || (std::fabs(penalty - bestPenalty) <= 1e-9 && result->distance < bestDistance)) {
                bestPenalty = penalty;
                bestDistance = result->distance;
                bestPlan = plan;
            }
        }

        hazardAwareExitPlanCache_.emplace(cacheKey, bestPlan);
        return bestPlan;
    }

    void replaceRouteWithPlan(EvacuationRoute& route, const ScenarioRoutePlan& plan, const Point2D& start) const {
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
        route.displayFloorId = route.currentFloorId;
        route.previousDistanceToWaypoint = route.waypoints.empty()
            ? 0.0
            : distanceToRouteWaypoint(route, start);
        route.stalledSeconds = 0.0;
        route.noExitAvailable = false;
        route.holdingForClosure = false;
        route.closureHoldTarget = {};
        route.nextSegmentReplanSeconds = 0.0;
    }


private:
    std::vector<RouteGuidanceDraft> routeGuidances_{};
    std::string activeRouteGuidanceId_{};
    bool guidanceReplanPending_{false};
    std::size_t guidanceReplanCursor_{0};
    std::uint64_t guidanceReplanSeed_{0U};
    mutable std::uint64_t planningCacheRevision_{std::numeric_limits<std::uint64_t>::max()};
    mutable std::unordered_map<std::string, std::optional<ZoneRouteToExit>> nearestExitRouteCache_{};
    mutable std::unordered_map<std::string, std::optional<ZoneRouteResult>> exitRouteCache_{};
    mutable std::unordered_map<std::string, std::vector<Point2D>> pathCache_{};
    mutable std::string routeSafetyGuidanceSignature_{};
    mutable std::string routeSafetyHazardSignature_{};
    mutable std::unordered_map<std::string, ScenarioRoutePlan> hazardAwareExitPlanCache_{};
    mutable std::unordered_map<std::string, double> routeHazardPenaltyCache_{};
};

ScenarioRouteGuidanceController::ScenarioRouteGuidanceController()
    : impl_(std::make_unique<Impl>()) {
}

ScenarioRouteGuidanceController::ScenarioRouteGuidanceController(std::vector<RouteGuidanceDraft> routeGuidances)
    : impl_(std::make_unique<Impl>(std::move(routeGuidances))) {
}

ScenarioRouteGuidanceController::ScenarioRouteGuidanceController(ScenarioRouteGuidanceController&&) noexcept = default;

ScenarioRouteGuidanceController& ScenarioRouteGuidanceController::operator=(ScenarioRouteGuidanceController&&) noexcept = default;

ScenarioRouteGuidanceController::~ScenarioRouteGuidanceController() = default;

void ScenarioRouteGuidanceController::refreshPlanningCache(std::uint64_t layoutRevision) const {
    impl_->refreshPlanningCache(layoutRevision);
}

const std::vector<Point2D>& ScenarioRouteGuidanceController::cachedPath(
    const std::string& floorId,
    const FacilityLayout2D& layout,
    const Point2D& start,
    const Point2D& goal,
    double clearance) const {
    return impl_->cachedPath(floorId, layout, start, goal, clearance);
}

ScenarioRoutePlan ScenarioRouteGuidanceController::routePlanToNearestExit(
    const ScenarioLayoutCacheResource& layoutCache,
    const Point2D& start,
    const std::string& startZoneId) const {
    return impl_->routePlanToNearestExit(layoutCache, start, startZoneId);
}

ScenarioRoutePlan ScenarioRouteGuidanceController::routePlanToExit(
    const ScenarioLayoutCacheResource& layoutCache,
    const Point2D& start,
    const std::string& startZoneId,
    const std::string& exitZoneId) const {
    return impl_->routePlanToExit(layoutCache, start, startZoneId, exitZoneId);
}

ScenarioRoutePlan ScenarioRouteGuidanceController::routePlanToHazardAwareNearestExit(
    const ScenarioLayoutCacheResource& layoutCache,
    const Point2D& start,
    const std::string& startZoneId,
    const std::string& agentFloorId,
    const ScenarioActiveEnvironmentHazardsResource& activeHazards) const {
    return impl_->routePlanToHazardAwareNearestExit(layoutCache, start, startZoneId, agentFloorId, activeHazards);
}

void ScenarioRouteGuidanceController::replaceRouteWithPlan(
    EvacuationRoute& route,
    const ScenarioRoutePlan& plan,
    const Point2D& start) const {
    impl_->replaceRouteWithPlan(route, plan, start);
}

void ScenarioRouteGuidanceController::apply(
    engine::WorldQuery& query,
    const std::vector<engine::Entity>& entities,
    const ScenarioLayoutCacheResource& layoutCache,
    double elapsedSeconds,
    std::uint64_t derivedSeed,
    const ScenarioEnvironmentReactionResource* reactions,
    const ScenarioActiveEnvironmentHazardsResource* activeHazards,
    const ScenarioAgentSpatialIndexResource* sharedSpatialIndex) {
    impl_->applyRouteGuidance(
        query,
        entities,
        layoutCache,
        elapsedSeconds,
        derivedSeed,
        reactions,
        activeHazards,
        sharedSpatialIndex);
}

bool routePlanUsesConnection(const ScenarioRoutePlan& plan, const std::string& connectionId) {
    if (connectionId.empty()) {
        return false;
    }
    return std::any_of(plan.waypointConnectionIds.begin(), plan.waypointConnectionIds.end(), [&](const auto& id) {
        return id == connectionId;
    });
}

}  // namespace safecrowd::domain::simulation_internal
