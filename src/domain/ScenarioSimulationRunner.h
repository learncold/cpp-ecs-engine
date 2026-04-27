#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "engine/EcsCore.h"

namespace safecrowd::domain {

struct SimulationAgentFrame {
    std::uint64_t id{0};
    Point2D position{};
    Point2D velocity{};
    double radius{0.25};
};

struct SimulationFrame {
    double elapsedSeconds{0.0};
    bool complete{false};
    std::size_t totalAgentCount{0};
    std::size_t evacuatedAgentCount{0};
    std::vector<SimulationAgentFrame> agents{};
};

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
        std::vector<std::string> waypointZoneIds{};
        std::string destinationZoneId{};
    };

    void advanceRouteWaypoint(EvacuationRoute& route, const Point2D& reachedPoint) const;
    void advanceRoutesForWaypointProgress(double deltaSeconds);
    void advanceRoutesForCurrentZones();
    void initializeAgents();
    void rebuildFrame();
    void replanBlockedRouteSegments();
    void resolveAgentOverlaps();
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
