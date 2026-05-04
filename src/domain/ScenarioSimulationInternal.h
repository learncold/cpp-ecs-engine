#pragma once

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
#include "engine/Entity.h"
#include "engine/WorldQuery.h"

namespace safecrowd::domain {
struct ScenarioConnectionTraversal;
struct ScenarioLayoutCacheResource;
}

namespace safecrowd::domain::simulation_internal {

inline constexpr double kDefaultTimeLimitSeconds = 60.0;
inline constexpr double kDefaultAgentRadius = 0.25;
inline constexpr double kDefaultAgentSpeed = 1.5;
inline constexpr double kStairSpeedMultiplier = 0.55;
inline constexpr double kArrivalEpsilon = 0.05;
inline constexpr double kGeometryEpsilon = 1e-9;
inline constexpr double kPathClearance = 0.08;
inline constexpr double kPathPlanningWallBuffer = 0.18;
inline constexpr double kCandidateClearance = kDefaultAgentRadius + kPathPlanningWallBuffer;

// Social Force model parameters.
// Reference: Helbing & Molnár, "Social force model for pedestrian dynamics",
//            Physical Review E 51 (1995); anisotropy from Helbing, Farkas & Vicsek,
//            "Simulating dynamical features of escape panic", Nature 407 (2000).
// Citation trail and parameter rationale:
//   docs/references/Helbing Social Force 모델.md
//   docs/product/고급 위험 모델.md §3.5
//
// Driving term: a_drv = (v_desired - v_current) / tau
inline constexpr double kSocialForceRelaxationTime = 0.5;            // tau, s
// Agent-agent repulsion: f_ij = A * exp((r_ij - d_ij) / B) * n_ij * w(phi)
inline constexpr double kSocialForceAgentStrength = 2.1;             // A_i / m_i, m/s^2
inline constexpr double kSocialForceAgentRange = 0.30;               // B_i, m
inline constexpr double kSocialForceAgentAnisotropy = 0.5;           // lambda; w(phi) = lambda + (1-lambda)*(1+cos(phi))/2
inline constexpr double kSocialForceAgentInteractionRadius = 2.0;    // m, neighbor cutoff
// Wall repulsion: f_iW = A_W * exp((r_i - d_iW) / B_W) * n_iW
inline constexpr double kSocialForceWallStrength = 5.0;              // A_W / m_i, m/s^2
inline constexpr double kSocialForceWallRange = 0.20;                // B_W, m
inline constexpr double kSocialForceWallInteractionRadius = 1.0;     // m, wall cutoff
// Body acceleration limit (typical comfortable human: 3-5 m/s^2)
inline constexpr double kSocialForceMaxAcceleration = 5.0;           // m/s^2
// Tangential nudge that breaks symmetry on a perfectly head-on encounter.
// Without it two mirror-image agents along a shared axis would receive a purely
// longitudinal repulsion and stall against each other. Each agent steps to its
// own left so that a mirror-image pair separates onto opposite world sides.
// The threshold keeps the bias narrowly scoped to nearly co-linear pairs so it
// does not perturb generic oblique encounters handled by the base repulsion.
inline constexpr double kSocialForceHeadOnTangentBias = 0.4;
inline constexpr double kSocialForceHeadOnAlignmentThreshold = 0.985;
inline constexpr double kWaypointCrossingEpsilon = 0.08;
inline constexpr double kWaypointProgressEpsilon = 0.02;
inline constexpr double kWaypointBypassLongitudinalTolerance = 0.5;
inline constexpr double kWaypointBypassLateralTolerance = 0.65;
inline constexpr double kWaypointStallSeconds = 0.75;
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

struct SpatialCell {
    int x{0};
    int y{0};
};

struct AgentSpatialIndex {
    double cellSize{1.0};
    std::unordered_map<std::string, std::unordered_map<long long, std::vector<engine::Entity>>> cellsByFloor{};
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

long long spatialKey(const SpatialCell& cell);
SpatialCell spatialCellFor(const Point2D& point, double cellSize);
Bounds boundsOf(const Polygon2D& polygon);
LayoutBounds boundsOf(const FacilityLayout2D& layout);
double distanceBetween(const Point2D& lhs, const Point2D& rhs);
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
Point2D closestPointOnSegment(const Point2D& point, const Point2D& start, const Point2D& end);
LineSegment2D passageWithClearance(const Connection2D& connection, double clearance);
Point2D routeWaypointTarget(const EvacuationRoute& route, const Point2D& position);
double distanceToRouteWaypoint(const EvacuationRoute& route, const Point2D& position);
bool pointInRing(const std::vector<Point2D>& ring, const Point2D& point);
Point2D polygonCenter(const Polygon2D& polygon);
const Zone2D* findZone(const FacilityLayout2D& layout, const std::string& zoneId);
const Connection2D* findConnectionBetween(const FacilityLayout2D& layout, const std::string& from, const std::string& to);
std::optional<std::vector<std::string>> zoneRouteToNearestExit(
    const ScenarioLayoutCacheResource& cache,
    const std::string& startZoneId);
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
std::string agentCollisionFloorId(const EvacuationRoute& route);
std::string zoneAt(const ScenarioLayoutCacheResource& cache, const Point2D& point, const std::string& floorId);
bool routePassageCrossed(const FacilityLayout2D& layout, const EvacuationRoute& route, const Point2D& position, double agentRadius);
double speedOf(const Point2D& velocity);
std::vector<engine::Entity> simulationEntities(engine::WorldQuery& query);
AgentSpatialIndex buildAgentSpatialIndex(engine::WorldQuery& query, const std::vector<engine::Entity>& entities, double cellSize);
std::vector<engine::Entity> nearbyAgents(engine::WorldQuery& query, const AgentSpatialIndex& index, const Point2D& point, double radius);
std::vector<engine::Entity> nearbyAgents(
    engine::WorldQuery& query,
    const AgentSpatialIndex& index,
    const Point2D& point,
    const std::string& floorId,
    double radius);
Point2D deterministicFallbackDirection(engine::Entity entity);
Point2D socialForceDriving(const Point2D& desiredVelocity, const Point2D& currentVelocity);
Point2D socialForceAgentRepulsion(
    engine::WorldQuery& query,
    engine::Entity entity,
    const std::vector<engine::Entity>& candidates,
    const Point2D& currentVelocity);
Point2D socialForceWallRepulsion(
    const FacilityLayout2D& layout,
    const Position& position,
    const Agent& agent);
bool movementCrossesBarrier(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to);
bool lineOfSightClear(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to, double clearance);
std::vector<Point2D> buildPath(const FacilityLayout2D& layout, const Point2D& start, const Point2D& goal, double clearance);
Point2D constrainedMove(const FacilityLayout2D& layout, const Point2D& from, const Point2D& to);

}  // namespace safecrowd::domain::simulation_internal
