#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
#include "domain/GeometryQueries.h"
#include "engine/Entity.h"
#include "engine/WorldQuery.h"

namespace safecrowd::domain {
struct ScenarioConnectionTraversal;
struct ScenarioLayoutCacheResource;
struct SimulationFrame;
}

namespace safecrowd::domain::simulation_internal {

inline constexpr double kDefaultTimeLimitSeconds = 60.0;
inline constexpr double kDefaultAgentRadius = kDefaultAgentRadiusMeters;
inline constexpr double kDefaultAgentSpeed = 1.3;
inline constexpr double kStairAgentSpeed = 0.8;
inline constexpr double kArrivalEpsilon = 0.05;
inline constexpr double kPersonalSpaceBuffer = 0.08;
inline constexpr double kAvoidanceLateralStrength = 0.65;
inline constexpr double kAvoidanceSlowdownStrength = 0.7;
inline constexpr double kAvoidanceSideLockSeconds = 0.55;
inline constexpr double kHeadOnLookAheadDistance = 1.2;
inline constexpr double kHeadOnDirectionDotThreshold = -0.6;
inline constexpr double kBarrierAvoidanceBuffer = 0.18;
inline constexpr double kBarrierAvoidanceStrength = 1.1;
inline constexpr int kOverlapRelaxationIterations = 4;
inline constexpr double kGeometryEpsilon = 1e-9;
inline constexpr double kPathClearance = 0.08;
inline constexpr double kCandidateClearance = kDefaultAgentRadius + kBarrierAvoidanceBuffer;
inline constexpr double kWaypointCrossingEpsilon = 0.08;
inline constexpr double kWaypointProgressEpsilon = 0.02;
inline constexpr double kWaypointBypassLongitudinalTolerance = 0.5;
inline constexpr double kWaypointBypassLateralTolerance = 0.65;
inline constexpr double kWaypointStallSeconds = 0.75;
inline constexpr double kStalledQueuePropagationExtraReach = 0.35;
inline constexpr double kStalledQueuePropagationLateralBuffer = 0.15;
inline constexpr double kStalledQueuePropagationMinimumLongitudinal = 0.05;
inline constexpr double kPortalCrossingEpsilon = 0.02;
inline constexpr double kRouteReplanCooldownSeconds = 0.35;

struct Bounds {
    double minX{0.0};
    double minY{0.0};
    double maxX{0.0};
    double maxY{0.0};
};

struct MovementPlan {
    engine::Entity entity{};
    Point2D velocity{};
};

struct AgentSpatialIndex {
    double cellSize{1.0};
    std::unordered_map<std::string, std::unordered_map<long long, std::vector<engine::Entity>>> cellsByFloor{};
    std::unordered_map<std::string, std::unordered_map<long long, std::vector<engine::Entity>>> displayCellsByFloor{};
};

struct LayoutBounds {
    double minX{std::numeric_limits<double>::max()};
    double minY{std::numeric_limits<double>::max()};
    double maxX{std::numeric_limits<double>::lowest()};
    double maxY{std::numeric_limits<double>::lowest()};

    bool valid() const noexcept {
        return minX <= maxX && minY <= maxY;
    }
};

struct VisibilityNode {
    Point2D point{};
};

struct PathQueueNode {
    std::size_t index{0};
    double priority{0.0};

    bool operator>(const PathQueueNode& other) const noexcept {
        return priority > other.priority;
    }
};

struct ZoneRouteToExit {
    std::vector<std::string> zoneIds{};
    std::vector<std::size_t> connectionIndices{};

    bool empty() const noexcept {
        return zoneIds.empty();
    }
};

struct ZoneRouteResult {
    ZoneRouteToExit route{};
    double distance{0.0};
};

Bounds boundsOf(const Polygon2D& polygon);
LayoutBounds boundsOf(const FacilityLayout2D& layout);
double distanceBetween(const Point2D& lhs, const Point2D& rhs);
double occupantInteractionPressureScore(double distance, double lhsRadius, double rhsRadius) noexcept;
Point2D operator+(const Point2D& lhs, const Point2D& rhs);
Point2D operator-(const Point2D& lhs, const Point2D& rhs);
Point2D operator*(const Point2D& point, double scalar);
double lengthOf(const Point2D& point);
double dot(const Point2D& lhs, const Point2D& rhs);
Point2D perpendicularLeft(const Point2D& point);
Point2D normalizedOr(const Point2D& point, Point2D fallback);
Point2D clampedToLength(const Point2D& point, double maxLength);
Point2D midpoint(const LineSegment2D& line);
double lengthSquaredOf(const LineSegment2D& line);
LineSegment2D pointPassage(const Point2D& point);
LineSegment2D passageWithClearance(const Connection2D& connection, double clearance);
Point2D routeWaypointTarget(const EvacuationRoute& route, const Point2D& position);
double distanceToRouteWaypoint(const EvacuationRoute& route, const Point2D& position);
const Zone2D* findZone(const FacilityLayout2D& layout, const std::string& zoneId);
const Connection2D* findConnectionBetween(const FacilityLayout2D& layout, const std::string& from, const std::string& to);
std::optional<ZoneRouteToExit> zoneRouteToNearestExit(
    const ScenarioLayoutCacheResource& cache,
    const Point2D& startPosition,
    const std::string& startZoneId);
std::optional<ZoneRouteResult> zoneRouteToExit(
    const ScenarioLayoutCacheResource& cache,
    const Point2D& startPosition,
    const std::string& startZoneId,
    const std::string& exitZoneId);
std::string floorIdForZone(const FacilityLayout2D& layout, const std::string& zoneId);
bool isVerticalConnection(const Connection2D& connection);
bool canTraverseConnection(const FacilityLayout2D& layout, const Connection2D& connection);
StairEntryDirection stairEntryDirectionForFloor(
    const FacilityLayout2D& layout,
    const Connection2D& connection,
    const std::string& floorId);
FacilityLayout2D layoutForFloor(const FacilityLayout2D& layout, const std::string& floorId);
ScenarioLayoutCacheResource buildScenarioLayoutCache(FacilityLayout2D layout);
const FacilityLayout2D& cachedLayoutForFloor(const ScenarioLayoutCacheResource& cache, const std::string& floorId);
const Zone2D* findCachedZone(const ScenarioLayoutCacheResource& cache, const std::string& zoneId);
const Connection2D* findCachedConnectionBetween(
    const ScenarioLayoutCacheResource& cache,
    const std::string& from,
    const std::string& to);
std::string cachedFloorIdForZone(const ScenarioLayoutCacheResource& cache, const std::string& zoneId);
const std::vector<ScenarioConnectionTraversal>& cachedTraversalsForZone(
    const ScenarioLayoutCacheResource& cache,
    const std::string& zoneId);
std::string agentDisplayFloorId(const EvacuationRoute& route);
std::string agentCollisionFloorId(const EvacuationRoute& route);
bool agentCollisionScopesOverlap(const EvacuationRoute& lhs, const EvacuationRoute& rhs);
void updateAgentPhysicsFloorIds(
    engine::WorldQuery& query,
    const ScenarioLayoutCacheResource& cache,
    const std::vector<engine::Entity>& entities);
std::string zoneAt(const ScenarioLayoutCacheResource& cache, const Point2D& point, const std::string& floorId);
bool routePassageCrossed(const FacilityLayout2D& layout, const EvacuationRoute& route, const Point2D& position, double agentRadius);
double speedOf(const Point2D& velocity);
bool pointHasBarrierClearance(const FacilityLayout2D& layout, const Point2D& point, double clearance);
std::vector<engine::Entity> simulationEntities(engine::WorldQuery& query);
void propagateStalledStateThroughQueues(SimulationFrame& frame);
AgentSpatialIndex buildAgentSpatialIndex(engine::WorldQuery& query, const std::vector<engine::Entity>& entities, double cellSize);
std::vector<engine::Entity> nearbyAgents(engine::WorldQuery& query, const AgentSpatialIndex& index, const Point2D& point, double radius);
std::vector<engine::Entity> nearbyAgents(
    engine::WorldQuery& query,
    const AgentSpatialIndex& index,
    const Point2D& point,
    const std::string& floorId,
    double radius);
std::vector<engine::Entity> nearbyDisplayAgents(
    engine::WorldQuery& query,
    const AgentSpatialIndex& index,
    const Point2D& point,
    const std::string& floorId,
    double radius);
Point2D deterministicFallbackDirection(engine::Entity entity);
Point2D forwardPreservingAgentAvoidanceVelocity(
    engine::WorldQuery& query,
    engine::Entity entity,
    const std::vector<engine::Entity>& candidates,
    const Point2D& desiredVelocity,
    double deltaSeconds,
    double& speedScale);
Point2D barrierSeparationVelocity(
    const FacilityLayout2D& layout,
    const Position& position,
    const Agent& agent,
    double referenceSpeed);
Point2D barrierSeparationVelocity(
    const std::vector<const Barrier2D*>& barriers,
    const Position& position,
    const Agent& agent,
    double referenceSpeed);
bool movementCrossesBarrier(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to);
bool lineOfSightClear(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to, double clearance);
std::vector<Point2D> buildPath(const FacilityLayout2D& layout, const Point2D& start, const Point2D& goal, double clearance);
Point2D constrainedMove(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to, double clearance = 0.0);
Point2D constrainedMoveWithBarrierClearance(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to, double clearance);

}  // namespace safecrowd::domain::simulation_internal
