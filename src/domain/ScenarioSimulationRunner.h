#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioSimulationFrame.h"
#include "engine/EcsCore.h"

namespace safecrowd::domain {

class ScenarioSimulationRunner {
public:
    ScenarioSimulationRunner() = default;
    ScenarioSimulationRunner(FacilityLayout2D layout, ScenarioDraft scenario);

    void reset(FacilityLayout2D layout, ScenarioDraft scenario);
    void step(double deltaSeconds);

    const SimulationFrame& frame() const noexcept;
    double timeLimitSeconds() const noexcept;
    bool complete() const noexcept;

private:
    struct RoutePlan {
        std::vector<Point2D> waypoints{};
        std::vector<LineSegment2D> waypointPassages{};
        std::vector<std::string> waypointFromZoneIds{};
        std::vector<std::string> waypointZoneIds{};
        std::string destinationZoneId{};
    };

    void advanceRouteWaypoint(EvacuationRoute& route, const Point2D& reachedPoint) const;
    void advanceRoutesForWaypointProgress(double deltaSeconds, const std::vector<engine::Entity>& entities);
    void advanceRoutesForCurrentZones(const std::vector<engine::Entity>& entities);
    void initializeAgents();
    void rebuildFrame(const std::vector<engine::Entity>& entities);
    void replanBlockedRouteSegments(const std::vector<engine::Entity>& entities);
    void resolveAgentOverlaps(const std::vector<engine::Entity>& entities);
    RoutePlan routePlan(const Point2D& start, const std::string& startZoneId) const;
    std::optional<std::vector<std::string>> zoneRouteToExit(const std::string& startZoneId) const;
    std::string zoneAt(const Point2D& point) const;
    Point2D placementPoint(const InitialPlacement2D& placement, std::size_t index) const;

    FacilityLayout2D layout_{};
    ScenarioDraft scenario_{};
    engine::EcsCore core_{4096};
    SimulationFrame frame_{};
    double timeLimitSeconds_{60.0};
};

}  // namespace safecrowd::domain
