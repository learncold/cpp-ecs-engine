#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "domain/AgentComponents.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"
#include "domain/ScenarioSimulationSystems.h"
#include "engine/EngineRuntime.h"

namespace safecrowd::domain {

class ScenarioSimulationRunner {
public:
    ScenarioSimulationRunner() = default;
    ScenarioSimulationRunner(FacilityLayout2D layout, ScenarioDraft scenario);

    void reset(FacilityLayout2D layout, ScenarioDraft scenario);
    void step(double deltaSeconds);

    const SimulationFrame& frame() const noexcept;
    const ScenarioRiskSnapshot& riskSnapshot() const noexcept;
    const ScenarioRiskSnapshot& resultRiskSnapshot() const noexcept;
    const ScenarioResultArtifacts& resultArtifacts() const noexcept;
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

    std::vector<ScenarioAgentSeed> createAgentSeeds() const;
    void initializeRuntime();
    void syncFrameFromRuntime();
    RoutePlan routePlan(const Point2D& start, const std::string& startZoneId) const;
    std::optional<std::vector<std::string>> zoneRouteToExit(const std::string& startZoneId) const;
    std::string zoneAt(const Point2D& point) const;
    Point2D placementPoint(const InitialPlacement2D& placement, std::size_t index) const;

    FacilityLayout2D layout_{};
    ScenarioDraft scenario_{};
    std::unique_ptr<engine::EngineRuntime> runtime_{};
    SimulationFrame frame_{};
    ScenarioRiskSnapshot riskSnapshot_{};
    ScenarioRiskSnapshot resultRiskSnapshot_{};
    ScenarioResultArtifacts resultArtifacts_{};
    double timeLimitSeconds_{60.0};
};

}  // namespace safecrowd::domain
