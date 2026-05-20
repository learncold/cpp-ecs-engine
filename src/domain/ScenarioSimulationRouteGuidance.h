#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioSimulationSystems.h"
#include "engine/Entity.h"
#include "engine/WorldQuery.h"

namespace safecrowd::domain::simulation_internal {

struct ScenarioRoutePlan {
    std::vector<Point2D> waypoints{};
    std::vector<LineSegment2D> waypointPassages{};
    std::vector<std::string> waypointFromZoneIds{};
    std::vector<std::string> waypointZoneIds{};
    std::vector<std::string> waypointFloorIds{};
    std::vector<std::string> waypointConnectionIds{};
    std::vector<bool> waypointVerticalTransitions{};
    std::string destinationZoneId{};
};

class ScenarioRouteGuidanceController {
public:
    ScenarioRouteGuidanceController();
    explicit ScenarioRouteGuidanceController(std::vector<RouteGuidanceDraft> routeGuidances);
    ScenarioRouteGuidanceController(ScenarioRouteGuidanceController&&) noexcept;
    ScenarioRouteGuidanceController& operator=(ScenarioRouteGuidanceController&&) noexcept;
    ~ScenarioRouteGuidanceController();

    void refreshPlanningCache(std::uint64_t layoutRevision) const;

    const std::vector<Point2D>& cachedPath(
        const std::string& floorId,
        const FacilityLayout2D& layout,
        const Point2D& start,
        const Point2D& goal,
        double clearance) const;

    ScenarioRoutePlan routePlanToNearestExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId) const;

    ScenarioRoutePlan routePlanToExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& exitZoneId) const;

    ScenarioRoutePlan routePlanToHazardAwareNearestExit(
        const ScenarioLayoutCacheResource& layoutCache,
        const Point2D& start,
        const std::string& startZoneId,
        const std::string& agentFloorId,
        const ScenarioActiveEnvironmentHazardsResource& activeHazards) const;

    void replaceRouteWithPlan(EvacuationRoute& route, const ScenarioRoutePlan& plan, const Point2D& start) const;

    void apply(
        engine::WorldQuery& query,
        const std::vector<engine::Entity>& entities,
        const ScenarioLayoutCacheResource& layoutCache,
        double elapsedSeconds,
        std::uint64_t derivedSeed,
        const ScenarioEnvironmentReactionResource* reactions,
        const ScenarioActiveEnvironmentHazardsResource* activeHazards,
        const ScenarioAgentSpatialIndexResource* sharedSpatialIndex);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

bool routePlanUsesConnection(const ScenarioRoutePlan& plan, const std::string& connectionId);

}  // namespace safecrowd::domain::simulation_internal
