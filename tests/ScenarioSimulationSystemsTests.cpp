#include "TestSupport.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "domain/AgentComponents.h"
#include "domain/DemoLayouts.h"
#include "domain/GeometryQueries.h"
#include "domain/ScenarioSimulationInternal.h"
#include "domain/ScenarioSimulationSystems.h"
#include "engine/EngineRuntime.h"

namespace {

class ConfigureScenarioAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.5,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 1.0, .y = 1.0}},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{.value = {.x = 0.5, .y = 0.0}},
            safecrowd::domain::EvacuationStatus{});
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 1.3, .y = 1.0}},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{.value = {.x = 0.0, .y = 0.0}},
            safecrowd::domain::EvacuationStatus{.evacuated = true});
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureOverlappingFloorAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 0.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 1.0, .y = 1.0}},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{.value = {.x = 0.0, .y = 0.0}},
            safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
            safecrowd::domain::EvacuationStatus{});
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 1.0, .y = 1.0}},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{.value = {.x = 0.0, .y = 0.0}},
            safecrowd::domain::EvacuationRoute{.currentFloorId = "L2", .displayFloorId = "L2"},
            safecrowd::domain::EvacuationStatus{});
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureConnectedStairEndpointAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    ConfigureConnectedStairEndpointAgentsSystem(
        safecrowd::domain::FacilityLayout2D layout,
        safecrowd::domain::Point2D position,
        bool activeVerticalTransition = false)
        : layout_(std::move(layout))
        , position_(position)
        , activeVerticalTransition_(activeVerticalTransition) {
    }

    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 0.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        world.resources().set(safecrowd::domain::simulation_internal::buildScenarioLayoutCache(layout_));
        auto routeForFloor = [&](const std::string& floorId) {
            safecrowd::domain::EvacuationRoute route{
                .currentFloorId = floorId,
                .displayFloorId = floorId,
            };
            if (!activeVerticalTransition_ || (floorId != "L1" && floorId != "L2")) {
                return route;
            }

            route.waypoints = {{.x = 1.0, .y = 1.0}};
            route.waypointPassages = {{{.x = 0.6, .y = 1.0}, {.x = 1.4, .y = 1.0}}};
            route.waypointFromZoneIds = {floorId == "L1" ? "stair-l1" : "stair-l2"};
            route.waypointZoneIds = {floorId == "L1" ? "stair-l2" : "stair-l1"};
            route.waypointFloorIds = {floorId == "L1" ? "L2" : "L1"};
            route.waypointConnectionIds = {"stair-vertical"};
            route.waypointVerticalTransitions = {true};
            route.currentSegmentStart = position_;
            route.previousDistanceToWaypoint = safecrowd::domain::simulation_internal::distanceBetween(
                position_,
                route.waypoints.front());
            route.destinationZoneId = route.waypointZoneIds.front();
            return route;
        };
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = position_},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{},
            routeForFloor("L1"),
            safecrowd::domain::EvacuationStatus{});
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = position_},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{},
            routeForFloor("L2"),
            safecrowd::domain::EvacuationStatus{});
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = position_},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{},
            routeForFloor("L3"),
            safecrowd::domain::EvacuationStatus{});
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }

private:
    safecrowd::domain::FacilityLayout2D layout_{};
    safecrowd::domain::Point2D position_{};
    bool activeVerticalTransition_{false};
};

class ConfigureBarrierIndexedLayoutSystem final : public safecrowd::engine::EngineSystem {
public:
    explicit ConfigureBarrierIndexedLayoutSystem(safecrowd::domain::FacilityLayout2D layout)
        : layout_(std::move(layout)) {
    }

    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 0.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        world.resources().set(safecrowd::domain::simulation_internal::buildScenarioLayoutCache(layout_));
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }

private:
    safecrowd::domain::FacilityLayout2D layout_{};
};

class ConfigureEvacuatedAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 7.0,
            .timeLimitSeconds = 10.0,
            .complete = true,
        });
        spawnEvacuatedAgent(world, 2.0);
        spawnEvacuatedAgent(world, 5.0);
        spawnEvacuatedAgent(world, 7.0);
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }

private:
    static void spawnEvacuatedAgent(safecrowd::engine::EngineWorld& world, double completionTimeSeconds) {
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = completionTimeSeconds, .y = 0.0}},
            safecrowd::domain::Agent{
                .radius = 0.25f,
                .maxSpeed = 1.5f,
                .sourcePlacementId = "group-a",
                .sourceZoneId = "room-a",
            },
            safecrowd::domain::Velocity{.value = {}},
            safecrowd::domain::EvacuationRoute{.destinationZoneId = "exit-a"},
            safecrowd::domain::EvacuationStatus{
                .evacuated = true,
                .completionTimeSeconds = completionTimeSeconds,
            });
    }
};

class ConfigureEvacuatedAgentWithActualExitSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 4.0,
            .timeLimitSeconds = 10.0,
            .complete = true,
        });
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 0.0, .y = 0.0}},
            safecrowd::domain::Agent{
                .radius = 0.25f,
                .maxSpeed = 1.5f,
                .sourcePlacementId = "group-a",
                .sourceZoneId = "room-a",
            },
            safecrowd::domain::Velocity{.value = {}},
            safecrowd::domain::EvacuationRoute{.destinationZoneId = "intermediate-zone"},
            safecrowd::domain::EvacuationStatus{
                .evacuated = true,
                .completionTimeSeconds = 4.0,
                .exitZoneId = "actual-exit",
            });
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureDenseActiveAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        for (int index = 0; index < 10; ++index) {
            world.commands().spawnEntity(
                safecrowd::domain::Position{.value = {.x = 0.1 + (0.04 * static_cast<double>(index)), .y = 0.1}},
                safecrowd::domain::Agent{
                    .radius = 0.25f,
                    .maxSpeed = 1.5f,
                    .sourcePlacementId = "dense-group",
                    .sourceZoneId = "room-a",
                },
                safecrowd::domain::Velocity{.value = {}},
                safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
                safecrowd::domain::EvacuationStatus{});
        }
        for (int index = 0; index < 6; ++index) {
            world.commands().spawnEntity(
                safecrowd::domain::Position{.value = {.x = 3.0 + (2.0 * static_cast<double>(index)), .y = 0.1}},
                safecrowd::domain::Agent{
                    .radius = 0.25f,
                    .maxSpeed = 1.5f,
                    .sourcePlacementId = "spread-group",
                    .sourceZoneId = "room-a",
                },
                safecrowd::domain::Velocity{.value = {}},
                safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
                safecrowd::domain::EvacuationStatus{});
        }
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureDensePersonalSpaceAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                const safecrowd::domain::Point2D position{
                    .x = 0.10 + (0.60 * static_cast<double>(column)),
                    .y = 0.10 + (0.60 * static_cast<double>(row)),
                };
                world.commands().spawnEntity(
                    safecrowd::domain::Position{.value = position},
                    safecrowd::domain::Agent{
                        .radius = 0.25f,
                        .maxSpeed = 1.5f,
                        .sourcePlacementId = "personal-space-grid",
                        .sourceZoneId = "room-a",
                    },
                    safecrowd::domain::Velocity{.value = {}},
                    safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
                    safecrowd::domain::EvacuationStatus{});
            }
        }
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureCrossFlowArtifactsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 12.0,
            .timeLimitSeconds = 30.0,
            .complete = false,
        });

        safecrowd::domain::ScenarioRiskMetricsResource metrics;
        metrics.snapshot.peakCrossFlowScore = 0.95;
        metrics.snapshot.totalCrossFlowExposureAgentSeconds = 18.0;
        metrics.snapshot.crossFlowCells.push_back({
            .center = {.x = 1.0, .y = 1.0},
            .cellMin = {.x = 0.0, .y = 0.0},
            .cellMax = {.x = 2.0, .y = 2.0},
            .floorId = "L1",
            .movingAgentCount = 6,
            .peakAgentCount = 6,
            .primaryFlowCount = 3,
            .crossFlowCount = 3,
            .crossFlowRatio = 0.5,
            .averageSpeed = 0.6,
            .speedDropRatio = 0.54,
            .crossFlowScore = 0.95,
            .durationSeconds = 11.0,
            .exposureAgentSeconds = 18.0,
            .detectedAtSeconds = 12.0,
        });
        metrics.peakSnapshot = metrics.snapshot;
        world.resources().set(std::move(metrics));

        safecrowd::domain::ScenarioCrossFlowResource crossFlow;
        crossFlow.totalCrossFlowExposureAgentSeconds = 18.0;
        world.resources().set(std::move(crossFlow));
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureQuietAgentWithPriorPressureFeedbackSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 0.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        world.resources().set(safecrowd::domain::ScenarioPressureFeedbackResource{
            .agentsById = {
                {0U,
                 safecrowd::domain::ScenarioPressureFeedbackAgentState{
                     .agentId = 0U,
                     .position = {.x = 0.5, .y = 0.5},
                     .floorId = "L1",
                     .compressionForce = 0.22,
                     .exposureSeconds = 0.6,
                     .feedbackLevel = 0.4,
                     .speedFactor = 0.96,
                     .avoidanceScale = 1.12,
                     .barrierScale = 1.06,
                     .exposed = true,
                     .critical = false,
                 }},
            },
            .exposedAgentCount = 1,
            .criticalAgentCount = 0,
        });
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = {.x = 0.5, .y = 0.5}},
            safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
            safecrowd::domain::Velocity{.value = {0.0, 0.0}},
            safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
            safecrowd::domain::EvacuationStatus{});
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureMovingFloorDensityAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        for (int index = 0; index < 6; ++index) {
            world.commands().spawnEntity(
                safecrowd::domain::Position{.value = {.x = 0.1 + (0.04 * static_cast<double>(index)), .y = 0.1}},
                safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
                safecrowd::domain::Velocity{.value = {}},
                safecrowd::domain::EvacuationRoute{.currentFloorId = "L2", .displayFloorId = "L2"},
                safecrowd::domain::EvacuationStatus{});
        }
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureStagedHotspotAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        for (int index = 0; index < 8; ++index) {
            spawnHotspotAgent(
                world,
                "hotspot-initial",
                {.x = 0.1 + (0.04 * static_cast<double>(index)), .y = 0.1},
                false);
        }
        for (int index = 0; index < 9; ++index) {
            spawnHotspotAgent(
                world,
                "hotspot-delayed",
                {.x = 3.1 + (0.04 * static_cast<double>(index)), .y = 0.1},
                true);
        }
    }

    void update(safecrowd::engine::EngineWorld& world, const safecrowd::engine::EngineStepContext&) override {
        if (!world.resources().contains<safecrowd::domain::ScenarioSimulationClockResource>()) {
            return;
        }
        const auto& clock = world.resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
        if (clock.elapsedSeconds < 2.0) {
            return;
        }

        auto& query = world.query();
        for (const auto entity : query.view<safecrowd::domain::Agent, safecrowd::domain::EvacuationStatus>()) {
            const auto& agent = query.get<safecrowd::domain::Agent>(entity);
            if (agent.sourcePlacementId != "hotspot-delayed") {
                continue;
            }
            query.get<safecrowd::domain::EvacuationStatus>(entity).evacuated = false;
        }
    }

private:
    static void spawnHotspotAgent(
        safecrowd::engine::EngineWorld& world,
        std::string placementId,
        safecrowd::domain::Point2D position,
        bool initiallyEvacuated) {
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = position},
            safecrowd::domain::Agent{
                .radius = 0.25f,
                .maxSpeed = 1.5f,
                .sourcePlacementId = std::move(placementId),
                .sourceZoneId = "room",
            },
            safecrowd::domain::Velocity{.value = {}},
            safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
            safecrowd::domain::EvacuationStatus{.evacuated = initiallyEvacuated});
    }
};

class ConfigureStagedCrossFlowAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        for (int index = 0; index < 4; ++index) {
            spawnCrossFlowAgent(
                world,
                "cross-flow-initial",
                {.x = 0.1 + (0.04 * static_cast<double>(index)), .y = 0.1},
                index < 2 ? safecrowd::domain::Point2D{.x = 0.5, .y = 0.0}
                          : safecrowd::domain::Point2D{.x = -0.5, .y = 0.0},
                false);
        }
        for (int index = 0; index < 4; ++index) {
            spawnCrossFlowAgent(
                world,
                "cross-flow-delayed",
                {.x = 3.1 + (0.04 * static_cast<double>(index)), .y = 0.1},
                index < 2 ? safecrowd::domain::Point2D{.x = 0.5, .y = 0.0}
                          : safecrowd::domain::Point2D{.x = -0.5, .y = 0.0},
                true);
        }
    }

    void update(safecrowd::engine::EngineWorld& world, const safecrowd::engine::EngineStepContext&) override {
        if (!world.resources().contains<safecrowd::domain::ScenarioSimulationClockResource>()) {
            return;
        }
        const auto& clock = world.resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
        if (clock.elapsedSeconds < 2.0) {
            return;
        }

        auto& query = world.query();
        for (const auto entity : query.view<safecrowd::domain::Agent, safecrowd::domain::EvacuationStatus>()) {
            const auto& agent = query.get<safecrowd::domain::Agent>(entity);
            auto& status = query.get<safecrowd::domain::EvacuationStatus>(entity);
            if (agent.sourcePlacementId == "cross-flow-initial") {
                status.evacuated = true;
            } else if (agent.sourcePlacementId == "cross-flow-delayed") {
                status.evacuated = false;
            }
        }
    }

private:
    static void spawnCrossFlowAgent(
        safecrowd::engine::EngineWorld& world,
        std::string placementId,
        safecrowd::domain::Point2D position,
        safecrowd::domain::Point2D velocity,
        bool initiallyEvacuated) {
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = position},
            safecrowd::domain::Agent{
                .radius = 0.25f,
                .maxSpeed = 1.5f,
                .sourcePlacementId = std::move(placementId),
                .sourceZoneId = "room",
            },
            safecrowd::domain::Velocity{.value = velocity},
            safecrowd::domain::EvacuationRoute{.currentFloorId = "L1", .displayFloorId = "L1"},
            safecrowd::domain::EvacuationStatus{.evacuated = initiallyEvacuated});
    }
};

class ConfigureSplitFloorHotspotAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        for (int index = 0; index < 6; ++index) {
            const auto floorId = index < 3 ? std::string{"L1"} : std::string{"L2"};
            world.commands().spawnEntity(
                safecrowd::domain::Position{.value = {.x = 0.2 + (0.03 * static_cast<double>(index % 3)), .y = 0.2}},
                safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
                safecrowd::domain::Velocity{.value = {}},
                safecrowd::domain::EvacuationRoute{
                    .stalledSeconds = 1.0,
                    .currentFloorId = floorId,
                    .displayFloorId = floorId,
                },
                safecrowd::domain::EvacuationStatus{});
        }
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureSecondFloorBottleneckAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        for (int index = 0; index < 5; ++index) {
            world.commands().spawnEntity(
                safecrowd::domain::Position{.value = {.x = 0.75 + (0.03 * static_cast<double>(index)), .y = 0.0}},
                safecrowd::domain::Agent{.radius = 0.25f, .maxSpeed = 1.5f},
                safecrowd::domain::Velocity{.value = {}},
                safecrowd::domain::EvacuationRoute{.stalledSeconds = 1.0, .currentFloorId = "L2", .displayFloorId = "L2"},
                safecrowd::domain::EvacuationStatus{});
        }
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureStagedBottleneckAgentsSystem final : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(safecrowd::domain::ScenarioSimulationClockResource{
            .elapsedSeconds = 1.0,
            .timeLimitSeconds = 10.0,
            .complete = false,
        });
        for (int index = 0; index < 7; ++index) {
            spawnBottleneckAgent(
                world,
                "lower-door-initial",
                {.x = 1.75 + (0.03 * static_cast<double>(index)), .y = 0.5},
                false);
        }
        for (int index = 0; index < 8; ++index) {
            spawnBottleneckAgent(
                world,
                "upper-door-delayed",
                {.x = 1.75 + (0.03 * static_cast<double>(index)), .y = 3.5},
                true);
        }
    }

    void update(safecrowd::engine::EngineWorld& world, const safecrowd::engine::EngineStepContext&) override {
        if (!world.resources().contains<safecrowd::domain::ScenarioSimulationClockResource>()) {
            return;
        }
        const auto& clock = world.resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
        if (clock.elapsedSeconds < 2.0) {
            return;
        }

        auto& query = world.query();
        for (const auto entity : query.view<safecrowd::domain::Agent, safecrowd::domain::EvacuationStatus>()) {
            const auto& agent = query.get<safecrowd::domain::Agent>(entity);
            if (agent.sourcePlacementId != "upper-door-delayed") {
                continue;
            }
            query.get<safecrowd::domain::EvacuationStatus>(entity).evacuated = false;
        }
    }

private:
    static void spawnBottleneckAgent(
        safecrowd::engine::EngineWorld& world,
        std::string placementId,
        safecrowd::domain::Point2D position,
        bool initiallyEvacuated) {
        world.commands().spawnEntity(
            safecrowd::domain::Position{.value = position},
            safecrowd::domain::Agent{
                .radius = 0.25f,
                .maxSpeed = 1.5f,
                .sourcePlacementId = std::move(placementId),
                .sourceZoneId = "room",
            },
            safecrowd::domain::Velocity{.value = {}},
            safecrowd::domain::EvacuationRoute{
                .stalledSeconds = 1.0,
                .destinationZoneId = "shared-exit",
            },
            safecrowd::domain::EvacuationStatus{.evacuated = initiallyEvacuated});
    }
};

safecrowd::domain::FacilityLayout2D straightExitLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{-1.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {-1.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{1.0, -1.0}, {2.0, -1.0}, {2.0, 1.0}, {1.0, 1.0}}},
    });
    layout.connections.push_back({
        .id = "room-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "exit",
        .effectiveWidth = 0.8,
        .centerSpan = {{1.0, -0.4}, {1.0, 0.4}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D twoExitGuidanceDetourLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "near-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Near Exit",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 1.0}, {2.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "far-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Far Exit",
        .area = {.outline = {{2.0, 3.0}, {4.0, 3.0}, {4.0, 4.0}, {2.0, 4.0}}},
    });
    layout.connections.push_back({
        .id = "room-near-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "near-exit",
        .effectiveWidth = 0.8,
        .centerSpan = {{2.0, 0.3}, {2.0, 0.7}},
    });
    layout.connections.push_back({
        .id = "room-far-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "far-exit",
        .effectiveWidth = 0.8,
        .centerSpan = {{2.0, 3.3}, {2.0, 3.7}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D sameExitTwoDoorGuidanceLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "shared-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Shared Exit",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {2.0, 4.0}}},
    });
    layout.connections.push_back({
        .id = "room-exit-lower",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "shared-exit",
        .effectiveWidth = 0.8,
        .centerSpan = {{2.0, 0.3}, {2.0, 0.7}},
    });
    layout.connections.push_back({
        .id = "room-exit-upper",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "shared-exit",
        .effectiveWidth = 0.8,
        .centerSpan = {{2.0, 3.3}, {2.0, 3.7}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D branchingWayfindingLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.zones.push_back({
        .id = "room",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "left-corridor",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Left Corridor",
        .area = {.outline = {{-3.0, 2.0}, {0.0, 2.0}, {0.0, 4.0}, {-3.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "right-corridor",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Right Corridor",
        .area = {.outline = {{4.0, 0.0}, {7.0, 0.0}, {7.0, 2.0}, {4.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "room-left",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "room",
        .toZoneId = "left-corridor",
        .effectiveWidth = 1.0,
        .centerSpan = {{0.0, 2.4}, {0.0, 3.6}},
    });
    layout.connections.push_back({
        .id = "room-right",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "room",
        .toZoneId = "right-corridor",
        .effectiveWidth = 1.0,
        .centerSpan = {{4.0, 0.4}, {4.0, 1.6}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D signedBacktrackWayfindingLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.zones.push_back({
        .id = "entry",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Entry",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "stair",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Stair",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "hall",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Hall",
        .area = {.outline = {{4.0, 0.0}, {6.0, 0.0}, {6.0, 2.0}, {4.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "entry-stair",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "entry",
        .toZoneId = "stair",
        .effectiveWidth = 1.0,
        .centerSpan = {{2.0, 0.5}, {2.0, 1.5}},
    });
    layout.connections.push_back({
        .id = "stair-hall",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "stair",
        .toZoneId = "hall",
        .effectiveWidth = 1.0,
        .centerSpan = {{4.0, 0.5}, {4.0, 1.5}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D widePassageAnchorLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.zones.push_back({
        .id = "room",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "hall",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Hall",
        .area = {.outline = {{2.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {2.0, 4.0}}},
    });
    layout.connections.push_back({
        .id = "room-hall-wide",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "room",
        .toZoneId = "hall",
        .effectiveWidth = 4.0,
        .centerSpan = {{2.0, 0.0}, {2.0, 4.0}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D directExitPriorityWayfindingLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.zones.push_back({
        .id = "room",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "side-room",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Side Room",
        .area = {.outline = {{4.0, 2.0}, {6.0, 2.0}, {6.0, 4.0}, {4.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{4.0, 0.0}, {6.0, 0.0}, {6.0, 2.0}, {4.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "room-side-room",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "room",
        .toZoneId = "side-room",
        .effectiveWidth = 1.0,
        .centerSpan = {{4.0, 2.4}, {4.0, 3.6}},
    });
    layout.connections.push_back({
        .id = "room-exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "exit",
        .effectiveWidth = 1.0,
        .centerSpan = {{4.0, 0.4}, {4.0, 1.6}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D sameFloorExitRouteWayfindingLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.zones.push_back({
        .id = "lobby",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Lobby",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "side-room",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Side Room",
        .area = {.outline = {{4.0, 2.0}, {6.0, 2.0}, {6.0, 4.0}, {4.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "exit-corridor",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Exit Corridor",
        .area = {.outline = {{4.0, 0.0}, {6.0, 0.0}, {6.0, 2.0}, {4.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit",
        .area = {.outline = {{6.0, 0.0}, {8.0, 0.0}, {8.0, 2.0}, {6.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "lobby-side-room",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "lobby",
        .toZoneId = "side-room",
        .effectiveWidth = 1.0,
        .centerSpan = {{4.0, 2.4}, {4.0, 3.6}},
    });
    layout.connections.push_back({
        .id = "lobby-exit-corridor",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "lobby",
        .toZoneId = "exit-corridor",
        .effectiveWidth = 1.0,
        .centerSpan = {{4.0, 0.4}, {4.0, 1.6}},
    });
    layout.connections.push_back({
        .id = "corridor-exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "exit-corridor",
        .toZoneId = "exit",
        .effectiveWidth = 1.0,
        .centerSpan = {{6.0, 0.4}, {6.0, 1.6}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D stairWayfindingLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "upper-room",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Upper Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "upper-stair",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Upper Stair",
        .area = {.outline = {{4.0, 2.0}, {6.0, 2.0}, {6.0, 4.0}, {4.0, 4.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "lower-stair",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Lower Stair",
        .area = {.outline = {{4.0, 2.0}, {6.0, 2.0}, {6.0, 4.0}, {4.0, 4.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "ground-room",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Ground Room",
        .area = {.outline = {{0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "ground-exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Ground Exit",
        .area = {.outline = {{-2.0, 0.0}, {0.0, 0.0}, {0.0, 2.0}, {-2.0, 2.0}}},
    });
    layout.connections.push_back({
        .id = "upper-room-stair",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "upper-room",
        .toZoneId = "upper-stair",
        .effectiveWidth = 1.0,
        .centerSpan = {{4.0, 2.4}, {4.0, 3.6}},
    });
    layout.connections.push_back({
        .id = "stair-vertical",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "upper-stair",
        .toZoneId = "lower-stair",
        .effectiveWidth = 1.0,
        .isStair = true,
        .centerSpan = {{4.5, 2.5}, {5.5, 2.5}},
    });
    layout.connections.push_back({
        .id = "lower-stair-ground-room",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "lower-stair",
        .toZoneId = "ground-room",
        .effectiveWidth = 1.0,
        .centerSpan = {{4.0, 2.4}, {4.0, 3.6}},
    });
    layout.connections.push_back({
        .id = "ground-room-exit",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "ground-room",
        .toZoneId = "ground-exit",
        .effectiveWidth = 1.0,
        .centerSpan = {{0.0, 0.4}, {0.0, 1.6}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D stairWithDeadEndDoorWayfindingLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.zones.push_back({
        .id = "room",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {6.0, 0.0}, {6.0, 4.0}, {0.0, 4.0}}},
    });
    layout.zones.push_back({
        .id = "dead-end-room",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Dead End Room",
        .area = {.outline = {{0.7, -1.5}, {2.3, -1.5}, {2.3, 0.0}, {0.7, 0.0}}},
    });
    layout.zones.push_back({
        .id = "stair",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .label = "Exit Stair",
        .area = {.outline = {{6.0, 1.8}, {8.0, 1.8}, {8.0, 3.4}, {6.0, 3.4}}},
        .isStair = true,
    });
    layout.connections.push_back({
        .id = "room-dead-end-door",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "room",
        .toZoneId = "dead-end-room",
        .effectiveWidth = 1.0,
        .centerSpan = {{1.0, 0.0}, {2.0, 0.0}},
    });
    layout.connections.push_back({
        .id = "room-stair",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Doorway,
        .fromZoneId = "room",
        .toZoneId = "stair",
        .effectiveWidth = 1.0,
        .centerSpan = {{6.0, 2.0}, {6.0, 3.2}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D wideTwoExitHazardRouteLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}}},
    });
    layout.zones.push_back({
        .id = "near-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Near Exit",
        .area = {.outline = {{10.0, 0.0}, {12.0, 0.0}, {12.0, 2.0}, {10.0, 2.0}}},
    });
    layout.zones.push_back({
        .id = "far-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Far Exit",
        .area = {.outline = {{10.0, 8.0}, {12.0, 8.0}, {12.0, 10.0}, {10.0, 10.0}}},
    });
    layout.connections.push_back({
        .id = "room-near-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "near-exit",
        .effectiveWidth = 0.8,
        .centerSpan = {{10.0, 0.7}, {10.0, 1.3}},
    });
    layout.connections.push_back({
        .id = "room-far-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "far-exit",
        .effectiveWidth = 0.8,
        .centerSpan = {{10.0, 8.7}, {10.0, 9.3}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D rightExitHazardDetourLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}}},
    });
    layout.zones.push_back({
        .id = "right-exit",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Right Exit",
        .area = {.outline = {{10.0, 4.0}, {12.0, 4.0}, {12.0, 6.0}, {10.0, 6.0}}},
    });
    layout.connections.push_back({
        .id = "room-right-exit",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room",
        .toZoneId = "right-exit",
        .effectiveWidth = 0.8,
        .centerSpan = {{10.0, 4.7}, {10.0, 5.3}},
    });
    return layout;
}

safecrowd::domain::ScenarioAgentSeed straightRouteSeed(
    safecrowd::domain::Point2D start,
    double maxSpeed = 1.0,
    double reactionDelaySeconds = 0.0,
    std::string destinationZoneId = "exit") {
    return {
        .position = {.value = start},
        .agent = {
            .radius = 0.25f,
            .maxSpeed = static_cast<float>(maxSpeed),
            .reactionDelaySeconds = reactionDelaySeconds,
        },
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 0.0}},
            .waypointPassages = {{{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {destinationZoneId},
            .nextWaypointIndex = 0,
            .currentSegmentStart = start,
            .previousDistanceToWaypoint = 1.0,
            .destinationZoneId = destinationZoneId,
        },
        .status = {},
    };
}

safecrowd::domain::ScenarioAgentSeed localWayfindingSeed(
    safecrowd::domain::Point2D start,
    std::string zoneId = "room",
    std::string floorId = "L1") {
    return {
        .position = {.value = start},
        .agent = {
            .radius = 0.25f,
            .maxSpeed = 1.0f,
            .sourceZoneId = zoneId,
            .guidancePropensity = 1.0,
        },
        .velocity = {.value = {}},
        .route = {
            .currentSegmentStart = start,
            .currentFloorId = floorId,
            .displayFloorId = floorId,
        },
        .wayfinding = {},
        .status = {},
    };
}

safecrowd::domain::ScenarioAgentSeed localWayfindingVerticalStairSeed(
    safecrowd::domain::Point2D start) {
    auto seed = localWayfindingSeed(start, "upper-stair", "L2");
    seed.route.waypoints = {{.x = 1.0, .y = 1.0}};
    seed.route.waypointPassages = {{{.x = 0.6, .y = 1.0}, {.x = 1.4, .y = 1.0}}};
    seed.route.waypointFromZoneIds = {"upper-stair"};
    seed.route.waypointZoneIds = {"lower-stair"};
    seed.route.waypointFloorIds = {"L1"};
    seed.route.waypointConnectionIds = {"vertical-stair"};
    seed.route.waypointVerticalTransitions = {true};
    seed.route.nextWaypointIndex = 0;
    seed.route.currentSegmentStart = {.x = 1.0, .y = 1.8};
    seed.route.previousDistanceToWaypoint =
        safecrowd::domain::simulation_internal::distanceBetween(start, seed.route.waypoints.front());
    seed.route.destinationZoneId = "lower-stair";
    seed.wayfinding.currentTargetConnectionId = "vertical-stair";
    seed.wayfinding.currentTargetZoneId = "lower-stair";
    return seed;
}

safecrowd::domain::ScenarioAgentSeed doorRouteSeed(
    safecrowd::domain::Point2D start,
    std::string destinationZoneId,
    std::string connectionId,
    safecrowd::domain::LineSegment2D passage,
    double maxSpeed = 1.0,
    double closurePatienceSeconds = 0.0) {
    const auto target = safecrowd::domain::closestPointOnSegment(
        start,
        passage.start,
        passage.end);
    return {
        .position = {.value = start},
        .agent = {
            .radius = 0.25f,
            .maxSpeed = static_cast<float>(maxSpeed),
            .closurePatienceSeconds = closurePatienceSeconds,
        },
        .velocity = {.value = {}},
        .route = {
            .waypoints = {target},
            .waypointPassages = {passage},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {destinationZoneId},
            .waypointFloorIds = {""},
            .waypointConnectionIds = {connectionId},
            .waypointVerticalTransitions = {false},
            .nextWaypointIndex = 0,
            .currentSegmentStart = start,
            .previousDistanceToWaypoint = 1.0,
            .destinationZoneId = destinationZoneId,
        },
        .status = {},
    };
}

safecrowd::domain::EnvironmentHazardDraft hazardDraft(
    std::string id,
    safecrowd::domain::EnvironmentHazardKind kind,
    safecrowd::domain::ScenarioElementSeverity severity,
    safecrowd::domain::Point2D position,
    std::string affectedZoneId = "room",
    std::string floorId = {}) {
    return {
        .id = std::move(id),
        .kind = kind,
        .name = "Test hazard",
        .affectedZoneId = std::move(affectedZoneId),
        .floorId = std::move(floorId),
        .position = position,
        .startSeconds = 0.0,
        .endSeconds = 0.0,
        .severity = severity,
    };
}

void addHazardMotionSystems(
    safecrowd::engine::EngineRuntime& runtime,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<safecrowd::domain::EnvironmentHazardDraft> hazards) {
    runtime.addSystem(
        safecrowd::domain::makeScenarioEnvironmentHazardSystem(layout, std::move(hazards)),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -20,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
}

void addClosureMotionSystems(
    safecrowd::engine::EngineRuntime& runtime,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<safecrowd::domain::ConnectionBlockDraft> blocks) {
    runtime.addSystem(
        safecrowd::domain::makeScenarioControlSystem(layout, std::move(blocks)),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
}

void addLocalWayfindingMotionSystems(
    safecrowd::engine::EngineRuntime& runtime,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<safecrowd::domain::EvacuationSignDraft> signs = {}) {
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, std::move(signs)),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(
            layout,
            {},
            safecrowd::domain::ScenarioWayfindingMode::LocalWayfinding),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
}

void addClosureGuidanceMotionSystems(
    safecrowd::engine::EngineRuntime& runtime,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<safecrowd::domain::ConnectionBlockDraft> blocks,
    std::vector<safecrowd::domain::RouteGuidanceDraft> guidances) {
    runtime.addSystem(
        safecrowd::domain::makeScenarioControlSystem(layout, std::move(blocks)),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout, std::move(guidances)),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
}

void addHazardGuidanceMotionSystems(
    safecrowd::engine::EngineRuntime& runtime,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<safecrowd::domain::EnvironmentHazardDraft> hazards,
    std::vector<safecrowd::domain::RouteGuidanceDraft> guidances) {
    runtime.addSystem(
        safecrowd::domain::makeScenarioEnvironmentHazardSystem(layout, std::move(hazards)),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -20,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout, std::move(guidances)),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
}

void addHazardClosureMotionSystems(
    safecrowd::engine::EngineRuntime& runtime,
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<safecrowd::domain::EnvironmentHazardDraft> hazards,
    std::vector<safecrowd::domain::ConnectionBlockDraft> blocks) {
    runtime.addSystem(
        safecrowd::domain::makeScenarioControlSystem(layout, std::move(blocks)),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioEnvironmentHazardSystem(layout, std::move(hazards)),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -20,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
}

void stepScenarioRuntime(safecrowd::engine::EngineRuntime& runtime, double deltaSeconds) {
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = deltaSeconds});
    runtime.stepFrame(0.0);
}

safecrowd::domain::FacilityLayout2D overlappingFloorBottleneckLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "room-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L1",
        .area = {.outline = {{0.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {0.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "exit-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L1",
        .area = {.outline = {{1.0, -1.0}, {2.0, -1.0}, {2.0, 1.0}, {1.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "room-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room L2",
        .area = {.outline = {{0.0, -1.0}, {1.0, -1.0}, {1.0, 1.0}, {0.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "exit-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Exit,
        .label = "Exit L2",
        .area = {.outline = {{1.0, -1.0}, {2.0, -1.0}, {2.0, 1.0}, {1.0, 1.0}}},
    });
    layout.connections.push_back({
        .id = "door-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room-l1",
        .toZoneId = "exit-l1",
        .effectiveWidth = 1.0,
        .centerSpan = {{1.0, -0.4}, {1.0, 0.4}},
    });
    layout.connections.push_back({
        .id = "door-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ConnectionKind::Exit,
        .fromZoneId = "room-l2",
        .toZoneId = "exit-l2",
        .effectiveWidth = 1.0,
        .centerSpan = {{1.0, -0.4}, {1.0, 0.4}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D connectedStairPhysicsBucketLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.floors.push_back({.id = "L3", .label = "Floor 3", .elevationMeters = 7.0});
    layout.zones.push_back({
        .id = "stair-l1",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 4.0}, {0.0, 4.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "stair-l2",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 4.0}, {0.0, 4.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "room-l3",
        .floorId = "L3",
        .kind = safecrowd::domain::ZoneKind::Room,
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 4.0}, {0.0, 4.0}}},
    });
    layout.connections.push_back({
        .id = "stair-vertical",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "stair-l1",
        .toZoneId = "stair-l2",
        .effectiveWidth = 1.0,
        .isStair = true,
        .centerSpan = {{0.6, 1.0}, {1.4, 1.0}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D verticalPortalTransitionLayout() {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors.push_back({.id = "L1", .label = "Floor 1"});
    layout.floors.push_back({.id = "L2", .label = "Floor 2", .elevationMeters = 3.5});
    layout.zones.push_back({
        .id = "lower-stair",
        .floorId = "L1",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .area = {.outline = {{0.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {0.0, 1.0}}},
        .isStair = true,
    });
    layout.zones.push_back({
        .id = "upper-stair",
        .floorId = "L2",
        .kind = safecrowd::domain::ZoneKind::Stair,
        .area = {.outline = {{0.0, 1.0}, {2.0, 1.0}, {2.0, 2.0}, {0.0, 2.0}}},
        .isStair = true,
    });
    layout.connections.push_back({
        .id = "vertical-stair",
        .floorId = "L1",
        .kind = safecrowd::domain::ConnectionKind::Stair,
        .fromZoneId = "upper-stair",
        .toZoneId = "lower-stair",
        .effectiveWidth = 0.8,
        .isStair = true,
        .centerSpan = {{0.6, 1.0}, {1.4, 1.0}},
    });
    return layout;
}

safecrowd::domain::FacilityLayout2D verticalPortalTransitionLayoutWithBlockedLandingClearance() {
    auto layout = verticalPortalTransitionLayout();
    layout.barriers.push_back({
        .id = "target-landing-clearance-barrier",
        .floorId = "L1",
        .geometry = {.vertices = {{0.0, 0.95}, {2.0, 0.95}}},
        .blocksMovement = true,
    });
    return layout;
}

std::vector<safecrowd::domain::ScenarioAgentSeed> pressureFeedbackMotionSeeds() {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (int index = 0; index < 3; ++index) {
        const auto x = 0.08 + (0.12 * static_cast<double>(index));
        seeds.push_back({
            .position = {.value = {.x = x, .y = 0.0}},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 1.0, .y = 0.0}},
                .waypointPassages = {{{.x = 1.0, .y = -0.3}, {.x = 1.0, .y = 0.3}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"exit"},
                .waypointConnectionIds = {"room-exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = {.x = x, .y = 0.0},
                .previousDistanceToWaypoint = 1.0 - x,
                .stalledSeconds = 0.0,
                .destinationZoneId = "exit",
                .currentFloorId = "L1",
                .displayFloorId = "L1",
            },
            .status = {},
        });
    }
    return seeds;
}

}  // namespace

SC_TEST(Agent_DefaultsIncludeEnvironmentReactionTraits) {
    const safecrowd::domain::Agent agent{};

    SC_EXPECT_NEAR(agent.hazardSensitivity, 1.0, 1e-9);
    SC_EXPECT_NEAR(agent.smokeSensitivity, 1.0, 1e-9);
    SC_EXPECT_NEAR(agent.reactionDelaySeconds, 0.0, 1e-9);
    SC_EXPECT_NEAR(agent.closurePatienceSeconds, 0.0, 1e-9);
}

SC_TEST(ScenarioEnvironmentReactionResource_DefaultsEmptyAndStoresAgentState) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 2,
    });

    auto& resources = runtime.world().resources();
    resources.set(safecrowd::domain::ScenarioEnvironmentReactionResource{});

    SC_EXPECT_TRUE(resources.contains<safecrowd::domain::ScenarioEnvironmentReactionResource>());
    auto& reactions = resources.get<safecrowd::domain::ScenarioEnvironmentReactionResource>();
    SC_EXPECT_TRUE(reactions.agentsById.empty());

    reactions.agentsById.emplace(
        7,
        safecrowd::domain::ScenarioEnvironmentReactionAgentState{
            .hazardDetected = true,
            .hazardAware = false,
            .hazardKey = "fire-a",
            .hazardDetectedAtSeconds = 1.25,
            .hazardReactionReadySeconds = 2.0,
            .closureDetected = true,
            .closureAware = false,
            .blockedConnectionId = "door-a",
            .closureDetectedAtSeconds = 1.5,
            .closureReactionReadySeconds = 3.0,
        });

    const auto& state = reactions.agentsById.at(7);
    SC_EXPECT_TRUE(state.hazardDetected);
    SC_EXPECT_EQ(state.hazardKey, std::string{"fire-a"});
    SC_EXPECT_TRUE(state.closureDetected);
    SC_EXPECT_EQ(state.blockedConnectionId, std::string{"door-a"});
}

SC_TEST(ScenarioAgentSpawnSystem_ConfiguresClockAndSpawnsAgentSeeds) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 2.0, .y = 3.0}},
        .agent = {.radius = 0.3f, .maxSpeed = 1.2f},
        .velocity = {.value = {.x = 0.2, .y = 0.1}},
        .route = {.stalledSeconds = 1.0},
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 2,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 15.0));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& agent = runtime.world().query().get<safecrowd::domain::Agent>(entities.front());
    SC_EXPECT_NEAR(agent.hazardSensitivity, 1.0, 1e-9);
    SC_EXPECT_NEAR(agent.smokeSensitivity, 1.0, 1e-9);
    SC_EXPECT_NEAR(agent.reactionDelaySeconds, 0.0, 1e-9);
    SC_EXPECT_NEAR(agent.closurePatienceSeconds, 0.0, 1e-9);
    const auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    SC_EXPECT_NEAR(clock.timeLimitSeconds, 15.0, 1e-9);
    const auto& frame = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
    SC_EXPECT_EQ(frame.totalAgentCount, std::size_t{1});
    SC_EXPECT_EQ(frame.agents.size(), std::size_t{1});
    SC_EXPECT_NEAR(frame.agents.front().position.x, 2.0, 1e-9);
    SC_EXPECT_TRUE(frame.agents.front().stalled);
}

SC_TEST(ScenarioWayfindingSystem_DirectionArrowSelectsAlignedConnection) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed({.x = 1.0, .y = 3.0}));

    safecrowd::domain::EvacuationSignDraft sign;
    sign.id = "right-arrow";
    sign.floorId = "L1";
    sign.installZoneId = "room";
    sign.position = {.x = 1.0, .y = 3.0};
    sign.orientationRadians = 0.0;
    sign.visibilityRadiusMeters = 10.0;
    sign.complianceRate = 1.0;
    sign.kind = safecrowd::domain::EvacuationSignKind::DirectionArrow;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 41,
    });
    const auto layout = branchingWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    addLocalWayfindingMotionSystems(runtime, layout, {sign});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_TRUE(!route.destinationZoneId.empty());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"right-corridor"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"room-right"});
    SC_EXPECT_EQ(state.lastSeenSignId, std::string{"right-arrow"});
    SC_EXPECT_EQ(state.intent, safecrowd::domain::WayfindingIntent::FollowingSign);
}

SC_TEST(ScenarioWayfindingSystem_IgnoresSignOnOtherFloor) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed({.x = 0.8, .y = 3.0}));

    safecrowd::domain::EvacuationSignDraft sign;
    sign.id = "other-floor-arrow";
    sign.floorId = "L2";
    sign.installZoneId = "room";
    sign.position = {.x = 1.0, .y = 3.0};
    sign.orientationRadians = 0.0;
    sign.visibilityRadiusMeters = 10.0;
    sign.complianceRate = 1.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 42,
    });
    const auto layout = branchingWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    addLocalWayfindingMotionSystems(runtime, layout, {sign});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"left-corridor"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"room-left"});
    SC_EXPECT_TRUE(state.lastSeenSignId.empty());
}

SC_TEST(ScenarioWayfindingSystem_StairExitMarkerPrioritizesStairConnection) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed({.x = 1.0, .y = 3.0}, "upper-room", "L2"));

    safecrowd::domain::EvacuationSignDraft sign;
    sign.id = "stair-exit-sign";
    sign.floorId = "L2";
    sign.installZoneId = "upper-room";
    sign.position = {.x = 1.0, .y = 3.0};
    sign.orientationRadians = 0.0;
    sign.visibilityRadiusMeters = 10.0;
    sign.complianceRate = 1.0;
    sign.kind = safecrowd::domain::EvacuationSignKind::StairExitMarker;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 43,
    });
    const auto layout = stairWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    addLocalWayfindingMotionSystems(runtime, layout, {sign});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"upper-stair"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"upper-room-stair"});
    SC_EXPECT_EQ(state.lastSeenSignId, std::string{"stair-exit-sign"});
}

SC_TEST(ScenarioWayfindingSystem_VisibleStairBeatsCloserDeadEndDoor) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed({.x = 1.5, .y = 1.0}));

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 44,
    });
    const auto layout = stairWithDeadEndDoorWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    addLocalWayfindingMotionSystems(runtime, layout);

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"stair"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"room-stair"});
}

SC_TEST(ScenarioWayfindingSystem_ResistsSignedBacktrackToPreviousZone) {
    auto seed = localWayfindingSeed({.x = 2.5, .y = 1.0}, "stair", "L1");
    seed.wayfinding.visitedZoneIds = {"entry", "stair"};

    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(seed);

    safecrowd::domain::EvacuationSignDraft sign;
    sign.id = "backtrack-arrow";
    sign.floorId = "L1";
    sign.installZoneId = "stair";
    sign.position = {.x = 2.5, .y = 1.0};
    sign.orientationRadians = 3.14159265358979323846;
    sign.visibilityRadiusMeters = 10.0;
    sign.complianceRate = 1.0;
    sign.kind = safecrowd::domain::EvacuationSignKind::DirectionArrow;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 52,
    });
    const auto layout = signedBacktrackWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {sign}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"hall"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"stair-hall"});
    SC_EXPECT_EQ(state.currentTargetConnectionId, std::string{"stair-hall"});
}

SC_TEST(ScenarioWayfindingSystem_DirectExitConnectionBeatsSignedRoomDetour) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed({.x = 2.0, .y = 2.0}));

    safecrowd::domain::EvacuationSignDraft sign;
    sign.id = "side-room-arrow";
    sign.floorId = "L1";
    sign.installZoneId = "room";
    sign.position = {.x = 2.0, .y = 2.0};
    sign.orientationRadians = 0.7;
    sign.visibilityRadiusMeters = 10.0;
    sign.complianceRate = 1.0;
    sign.kind = safecrowd::domain::EvacuationSignKind::DirectionArrow;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 55,
    });
    const auto layout = directExitPriorityWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {sign}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"exit"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"room-exit"});
    SC_EXPECT_EQ(state.currentTargetConnectionId, std::string{"room-exit"});
}

SC_TEST(ScenarioWayfindingSystem_SelectsSameFloorExitRouteBeforeRoomDetour) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed({.x = 2.0, .y = 2.0}, "lobby", "L1"));

    safecrowd::domain::EvacuationSignDraft sign;
    sign.id = "side-room-arrow";
    sign.floorId = "L1";
    sign.installZoneId = "lobby";
    sign.position = {.x = 2.0, .y = 2.0};
    sign.orientationRadians = 0.55;
    sign.visibilityRadiusMeters = 10.0;
    sign.complianceRate = 1.0;
    sign.kind = safecrowd::domain::EvacuationSignKind::DirectionArrow;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 56,
    });
    const auto layout = sameFloorExitRouteWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {sign}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"exit-corridor"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"lobby-exit-corridor"});
    SC_EXPECT_EQ(state.currentTargetConnectionId, std::string{"lobby-exit-corridor"});
}

SC_TEST(ScenarioWayfindingSystem_ReconsidersStalledDoorwayTarget) {
    auto seed = localWayfindingSeed({.x = 1.5, .y = 0.2});
    seed.route.waypoints = {{.x = 1.5, .y = 0.0}};
    seed.route.waypointPassages = {{{.x = 1.0, .y = 0.0}, {.x = 2.0, .y = 0.0}}};
    seed.route.waypointFromZoneIds = {"room"};
    seed.route.waypointZoneIds = {"dead-end-room"};
    seed.route.waypointFloorIds = {"L1"};
    seed.route.waypointConnectionIds = {"room-dead-end-door"};
    seed.route.waypointVerticalTransitions = {false};
    seed.route.nextWaypointIndex = 0;
    seed.route.previousDistanceToWaypoint = 0.2;
    seed.route.stalledSeconds = 1.0;
    seed.route.destinationZoneId = "dead-end-room";
    seed.wayfinding.currentTargetConnectionId = "room-dead-end-door";
    seed.wayfinding.currentTargetZoneId = "dead-end-room";

    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(seed);

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 45,
    });
    const auto layout = stairWithDeadEndDoorWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    addLocalWayfindingMotionSystems(runtime, layout);

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"stair"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"room-stair"});
    SC_EXPECT_EQ(state.avoidedConnectionId, std::string{"room-dead-end-door"});
}

SC_TEST(ScenarioWayfindingSystem_KeepsStalledStairTargetCommitted) {
    auto seed = localWayfindingVerticalStairSeed({.x = 1.0, .y = 1.01});
    seed.route.stalledSeconds = 1.0;

    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(seed);

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 46,
    });
    const auto layout = verticalPortalTransitionLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"lower-stair"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"vertical-stair"});
    SC_EXPECT_EQ(route.nextWaypointIndex, std::size_t{0});
    SC_EXPECT_TRUE(state.avoidedConnectionId.empty());
    SC_EXPECT_EQ(state.currentTargetConnectionId, std::string{"vertical-stair"});
}

SC_TEST(ScenarioWayfindingSystem_ReplansStalledStairAdjacentOpeningTarget) {
    auto seed = localWayfindingSeed({.x = 4.2, .y = 3.0}, "lower-stair", "L1");
    seed.route.waypoints = {{.x = 4.0, .y = 3.0}};
    seed.route.waypointPassages = {{{.x = 4.0, .y = 2.4}, {.x = 4.0, .y = 3.6}}};
    seed.route.waypointFromZoneIds = {"lower-stair"};
    seed.route.waypointZoneIds = {"ground-room"};
    seed.route.waypointFloorIds = {"L1"};
    seed.route.waypointConnectionIds = {"lower-stair-ground-room"};
    seed.route.waypointVerticalTransitions = {false};
    seed.route.nextWaypointIndex = 0;
    seed.route.currentSegmentStart = {.x = 4.2, .y = 3.0};
    seed.route.previousDistanceToWaypoint = 0.2;
    seed.route.stalledSeconds = 1.0;
    seed.route.destinationZoneId = "ground-room";
    seed.wayfinding.visitedZoneIds = {"upper-stair", "lower-stair"};
    seed.wayfinding.currentTargetConnectionId = "lower-stair-ground-room";
    seed.wayfinding.currentTargetZoneId = "ground-room";

    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(seed);

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 47,
    });
    const auto layout = stairWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"ground-room"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"lower-stair-ground-room"});
    SC_EXPECT_NEAR(route.stalledSeconds, 0.0, 1e-9);
    SC_EXPECT_TRUE(state.avoidedConnectionId.empty());
}

SC_TEST(ScenarioWayfindingSystem_PrefersSameFloorExitPathFromLowerStair) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed({.x = 4.2, .y = 3.0}, "lower-stair", "L1"));

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 54,
    });
    const auto layout = stairWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"ground-room"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"lower-stair-ground-room"});
    SC_EXPECT_EQ(state.currentTargetConnectionId, std::string{"lower-stair-ground-room"});
}

SC_TEST(ScenarioSimulationMotionSystem_LocalWayfindingLandsInsideVerticalTargetZone) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingVerticalStairSeed({.x = 1.0, .y = 1.0}));

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 47,
    });
    const auto layout = verticalPortalTransitionLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    addLocalWayfindingMotionSystems(runtime, layout);

    runtime.play();
    stepScenarioRuntime(runtime, 0.001);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& position = runtime.world().query().get<safecrowd::domain::Position>(entities.front());
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.currentFloorId, std::string{"L1"});
    SC_EXPECT_EQ(route.nextWaypointIndex, std::size_t{1});
    SC_EXPECT_NEAR(position.value.x, 1.0, 0.002);
    SC_EXPECT_TRUE(position.value.y < 0.85);
    SC_EXPECT_TRUE(position.value.y > 0.1);
}

SC_TEST(ScenarioSimulationMotionSystem_UsesStablePassageAnchorForTransitionTarget) {
    const safecrowd::domain::Point2D start{.x = 1.8, .y = 0.5};
    safecrowd::domain::ScenarioAgentSeed seed{
        .position = {.value = start},
        .agent = {
            .radius = 0.25f,
            .maxSpeed = 1.0f,
            .sourceZoneId = "room",
        },
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 2.0, .y = 3.5}},
            .waypointPassages = {{{.x = 2.0, .y = 0.0}, {.x = 2.0, .y = 4.0}}},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {"hall"},
            .waypointFloorIds = {"L1"},
            .waypointConnectionIds = {"room-hall-wide"},
            .waypointVerticalTransitions = {false},
            .nextWaypointIndex = 0,
            .currentSegmentStart = start,
            .previousDistanceToWaypoint = 0.2,
            .destinationZoneId = "hall",
            .currentFloorId = "L1",
            .displayFloorId = "L1",
        },
        .wayfinding = {},
        .status = {},
    };

    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(seed);

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 53,
    });
    const auto layout = widePassageAnchorLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(
            layout,
            {},
            safecrowd::domain::ScenarioWayfindingMode::LocalWayfinding),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& position = runtime.world().query().get<safecrowd::domain::Position>(entities.front());
    SC_EXPECT_TRUE(position.value.y > start.y + 0.05);
}

SC_TEST(ScenarioWayfindingSystem_SelectsDemoStairVerticalTransitionFromStairEntry) {
    using Ids = safecrowd::domain::DemoLayouts::TwoFloorEvacuationIds;

    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed(
        {.x = 4.0, .y = 9.2},
        Ids::UpperWestStairZoneId,
        Ids::UpperFloorId));

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 48,
    });
    const auto layout = safecrowd::domain::DemoLayouts::twoFloorEvacuationFacility();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{Ids::LowerWestStairZoneId});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{Ids::WestStairVerticalConnectionId});
    SC_EXPECT_EQ(state.currentTargetConnectionId, std::string{Ids::WestStairVerticalConnectionId});
}

SC_TEST(ScenarioWayfindingSystem_KeepsNearConnectionWaypointForStairEntryCrossing) {
    using Ids = safecrowd::domain::DemoLayouts::TwoFloorEvacuationIds;

    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed(
        {.x = 4.0, .y = 8.99},
        Ids::UpperCorridorZoneId,
        Ids::UpperFloorId));

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 50,
    });
    const auto layout = safecrowd::domain::DemoLayouts::twoFloorEvacuationFacility();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{Ids::UpperWestStairZoneId});
    SC_EXPECT_TRUE(!route.waypoints.empty());
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{Ids::UpperCorridorWestStairConnectionId});
}

SC_TEST(ScenarioWayfindingSystem_DoesNotBacktrackToVisitedDemoStairLanding) {
    using Ids = safecrowd::domain::DemoLayouts::TwoFloorEvacuationIds;

    auto seed = localWayfindingSeed(
        {.x = 2.0, .y = 10.2},
        Ids::LowerWestStairZoneId,
        Ids::LowerFloorId);
    seed.wayfinding.visitedZoneIds = {Ids::UpperWestStairZoneId, Ids::LowerWestStairZoneId};

    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(seed);

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 49,
    });
    const auto layout = safecrowd::domain::DemoLayouts::twoFloorEvacuationFacility();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& state = runtime.world().query().get<safecrowd::domain::WayfindingState>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{Ids::LowerWestVestibuleZoneId});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{Ids::LowerWestStairVestibuleConnectionId});
    SC_EXPECT_EQ(state.currentTargetConnectionId, std::string{Ids::LowerWestStairVestibuleConnectionId});
}

SC_TEST(ScenarioWayfindingSystem_KeepsNearConnectionWaypointAfterStairLanding) {
    using Ids = safecrowd::domain::DemoLayouts::TwoFloorEvacuationIds;

    auto seed = localWayfindingSeed(
        {.x = 2.0, .y = 9.01},
        Ids::LowerWestStairZoneId,
        Ids::LowerFloorId);
    seed.wayfinding.visitedZoneIds = {Ids::UpperWestStairZoneId, Ids::LowerWestStairZoneId};

    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(seed);

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 51,
    });
    const auto layout = safecrowd::domain::DemoLayouts::twoFloorEvacuationFacility();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioWayfindingSystem(layout, {}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -5,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{Ids::LowerWestVestibuleZoneId});
    SC_EXPECT_TRUE(!route.waypoints.empty());
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{Ids::LowerWestStairVestibuleConnectionId});
}

SC_TEST(ScenarioWayfindingSystem_RecognizesAgentsOnDemoZoneBoundaries) {
    using Ids = safecrowd::domain::DemoLayouts::TwoFloorEvacuationIds;

    const auto cache = safecrowd::domain::simulation_internal::buildScenarioLayoutCache(
        safecrowd::domain::DemoLayouts::twoFloorEvacuationFacility());

    SC_EXPECT_EQ(
        safecrowd::domain::simulation_internal::zoneAt(cache, {.x = 3.0, .y = 12.0}, Ids::LowerFloorId),
        std::string{Ids::LowerWestStairZoneId});
    SC_EXPECT_EQ(
        safecrowd::domain::simulation_internal::zoneAt(cache, {.x = 4.0, .y = 8.99}, Ids::UpperFloorId),
        std::string{Ids::UpperCorridorZoneId});
    const auto upperBoundaryZone =
        safecrowd::domain::simulation_internal::zoneAt(cache, {.x = 4.0, .y = 9.0}, Ids::UpperFloorId);
    SC_EXPECT_TRUE(upperBoundaryZone == Ids::UpperCorridorZoneId || upperBoundaryZone == Ids::UpperWestStairZoneId);
}

SC_TEST(ScenarioWayfindingSystem_RechoosesWhenSignedConnectionIsBlocked) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(localWayfindingSeed({.x = 1.0, .y = 3.0}));

    safecrowd::domain::EvacuationSignDraft sign;
    sign.id = "right-arrow";
    sign.floorId = "L1";
    sign.installZoneId = "room";
    sign.position = {.x = 1.0, .y = 3.0};
    sign.orientationRadians = 0.0;
    sign.visibilityRadiusMeters = 10.0;
    sign.complianceRate = 1.0;

    safecrowd::domain::ConnectionBlockDraft block;
    block.id = "blocked-right";
    block.connectionId = "room-right";

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 45,
    });
    const auto layout = branchingWayfindingLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioControlSystem(layout, {block}),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    addLocalWayfindingMotionSystems(runtime, layout, {sign});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::WayfindingState,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"left-corridor"});
    SC_EXPECT_TRUE(!route.waypointConnectionIds.empty());
    SC_EXPECT_EQ(route.waypointConnectionIds.front(), std::string{"room-left"});
}

SC_TEST(ScenarioSpatialIndexSystem_BuildsNearbyAgentResource) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 3,
    });
    runtime.addSystem(std::make_unique<ConfigureScenarioAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    auto& resources = runtime.world().resources();
    SC_EXPECT_TRUE(resources.contains<safecrowd::domain::ScenarioAgentSpatialIndexResource>());
    const auto& index = resources.get<safecrowd::domain::ScenarioAgentSpatialIndexResource>();
    auto nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 1.0},
        std::string{},
        0.4);
    SC_EXPECT_EQ(nearby.size(), std::size_t{1});
}

SC_TEST(ScenarioSpatialIndexSystem_SeparatesNearbyAgentsByFloor) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 31,
    });
    runtime.addSystem(std::make_unique<ConfigureOverlappingFloorAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    const auto& index = runtime.world().resources().get<safecrowd::domain::ScenarioAgentSpatialIndexResource>();
    auto l1Nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 1.0},
        "L1",
        0.4);
    auto l2Nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 1.0},
        "L2",
        0.4);

    SC_EXPECT_EQ(l1Nearby.size(), std::size_t{1});
    SC_EXPECT_EQ(l2Nearby.size(), std::size_t{1});
    SC_EXPECT_TRUE(l1Nearby.front() != l2Nearby.front());
}

SC_TEST(ScenarioSpatialIndexSystem_MergesConnectedStairEndpointsIntoVerticalBucket) {
    const auto layout = connectedStairPhysicsBucketLayout();
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 32,
    });
    runtime.addSystem(std::make_unique<ConfigureConnectedStairEndpointAgentsSystem>(
        layout,
        safecrowd::domain::Point2D{.x = 1.0, .y = 1.0}));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    const auto& index = runtime.world().resources().get<safecrowd::domain::ScenarioAgentSpatialIndexResource>();
    auto verticalNearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 1.0},
        "vertical:stair-vertical",
        0.4);
    auto disconnectedNearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 1.0},
        "L3",
        0.4);

    SC_EXPECT_EQ(verticalNearby.size(), std::size_t{2});
    SC_EXPECT_EQ(disconnectedNearby.size(), std::size_t{1});
}

SC_TEST(ScenarioSpatialIndexSystem_SeparatesConnectedStairEndpointsAwayFromVerticalPortal) {
    const auto layout = connectedStairPhysicsBucketLayout();
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 33,
    });
    runtime.addSystem(std::make_unique<ConfigureConnectedStairEndpointAgentsSystem>(
        layout,
        safecrowd::domain::Point2D{.x = 1.0, .y = 3.5}));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    const auto& index = runtime.world().resources().get<safecrowd::domain::ScenarioAgentSpatialIndexResource>();
    auto verticalNearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 3.5},
        "vertical:stair-vertical",
        0.4);
    auto l1Nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 3.5},
        "L1",
        0.4);
    auto l2Nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 3.5},
        "L2",
        0.4);

    SC_EXPECT_EQ(verticalNearby.size(), std::size_t{0});
    SC_EXPECT_EQ(l1Nearby.size(), std::size_t{1});
    SC_EXPECT_EQ(l2Nearby.size(), std::size_t{1});
}

SC_TEST(ScenarioSpatialIndexSystem_DoesNotMergeActiveVerticalRoutesAwayFromPortal) {
    const auto layout = connectedStairPhysicsBucketLayout();
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 34,
    });
    runtime.addSystem(std::make_unique<ConfigureConnectedStairEndpointAgentsSystem>(
        layout,
        safecrowd::domain::Point2D{.x = 1.0, .y = 3.5},
        true));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    const auto& index = runtime.world().resources().get<safecrowd::domain::ScenarioAgentSpatialIndexResource>();
    auto verticalNearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 3.5},
        "vertical:stair-vertical",
        0.4);
    auto l1Nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 3.5},
        "L1",
        0.4);
    auto l2Nearby = safecrowd::domain::scenarioNearbyAgents(
        runtime.world().query(),
        index,
        {.x = 1.0, .y = 3.5},
        "L2",
        0.4);

    SC_EXPECT_EQ(verticalNearby.size(), std::size_t{0});
    SC_EXPECT_EQ(l1Nearby.size(), std::size_t{1});
    SC_EXPECT_EQ(l2Nearby.size(), std::size_t{1});
}

SC_TEST(ScenarioSpatialIndexSystem_BuildsNearbyBarrierResourceByFloor) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.floors = {
        {.id = "L1"},
        {.id = "L2"},
    };
    layout.barriers.push_back({
        .id = "wall-l1",
        .floorId = "L1",
        .geometry = {.vertices = {{.x = -1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}}},
        .blocksMovement = true,
    });
    layout.barriers.push_back({
        .id = "wall-l2",
        .floorId = "L2",
        .geometry = {.vertices = {{.x = -1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}}},
        .blocksMovement = true,
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 34,
    });
    runtime.addSystem(std::make_unique<ConfigureBarrierIndexedLayoutSystem>(layout));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    const auto& resources = runtime.world().resources();
    const auto& index = resources.get<safecrowd::domain::ScenarioAgentSpatialIndexResource>();
    const auto& activeLayout = resources.get<safecrowd::domain::ScenarioLayoutCacheResource>().layout;
    const auto l1Barriers = safecrowd::domain::scenarioNearbyBarriers(
        activeLayout,
        index,
        {.x = 0.0, .y = 0.15},
        "L1",
        0.4);
    const auto l2Barriers = safecrowd::domain::scenarioNearbyBarriers(
        activeLayout,
        index,
        {.x = 0.0, .y = 0.15},
        "L2",
        0.4);

    SC_EXPECT_EQ(l1Barriers.size(), std::size_t{1});
    SC_EXPECT_EQ(l2Barriers.size(), std::size_t{1});
    SC_EXPECT_EQ(l1Barriers.front()->id, std::string{"wall-l1"});
    SC_EXPECT_EQ(l2Barriers.front()->id, std::string{"wall-l2"});
}

SC_TEST(ScenarioClockSystem_AdvancesClockResourceOnFixedSteps) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 11,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{},
        0.5));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioClockSystem>(0.25),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::FixedStep});

    runtime.play();
    runtime.stepFrame(0.25);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    SC_EXPECT_NEAR(clock.elapsedSeconds, 0.25, 1e-9);
    SC_EXPECT_TRUE(!clock.complete);

    runtime.stepFrame(0.25);
    SC_EXPECT_NEAR(clock.elapsedSeconds, 0.5, 1e-9);
    SC_EXPECT_TRUE(clock.complete);
}

SC_TEST(ScenarioSimulationMotionSystem_AdvancesAgentsFromStepResource) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 0.0, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 0.0}},
            .waypointPassages = {{{.x = 1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"exit"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.0, .y = 0.0},
            .previousDistanceToWaypoint = 1.0,
            .stalledSeconds = 0.0,
            .destinationZoneId = "exit",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 13,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.5});
    runtime.stepFrame(0.0);

    const auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    SC_EXPECT_NEAR(clock.elapsedSeconds, 0.5, 1e-9);
    const auto& frame = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
    SC_EXPECT_EQ(frame.agents.size(), std::size_t{1});
    SC_EXPECT_NEAR(frame.agents.front().position.x, 0.5, 1e-9);
    SC_EXPECT_NEAR(frame.agents.front().velocity.x, 1.0, 1e-9);
    SC_EXPECT_TRUE(!frame.agents.front().stalled);
}

SC_TEST(ScenarioEnvironmentHazardSystem_DelaysFireAvoidanceUntilReactionReady) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back(straightRouteSeed({.x = 0.0, .y = 0.0}, 1.0, 0.5));

    auto fire = hazardDraft(
        "fire-a",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::High,
        {.x = 0.0, .y = 0.4});

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 43,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    addHazardMotionSystems(runtime, straightExitLayout(), {fire});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.25});
    runtime.stepFrame(0.0);

    const auto& reactions =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>();
    const auto& firstState = reactions.agentsById.at(0);
    SC_EXPECT_TRUE(firstState.hazardDetected);
    SC_EXPECT_TRUE(!firstState.hazardAware);

    const auto firstFrame =
        runtime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
    SC_EXPECT_NEAR(firstFrame.agents.front().position.y, 0.0, 1e-6);

    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.25});
    runtime.stepFrame(0.0);
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.25});
    runtime.stepFrame(0.0);

    const auto& awareState =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(0);
    const auto& awareFrame =
        runtime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
    SC_EXPECT_TRUE(awareState.hazardAware);
    SC_EXPECT_TRUE(awareFrame.agents.front().position.y < -0.01);
}

SC_TEST(ScenarioEnvironmentHazardSystem_DelaysDetectionUntilDetectionDelayElapses) {
    auto seed = straightRouteSeed({.x = 0.0, .y = 0.0}, 1.0, 0.0);
    seed.agent.detectionDelaySeconds = 1.0;  // must sense the hazard for 1s before detecting it
    seed.agent.reactionDelaySeconds = 0.0;   // react immediately once detected

    // A large radius keeps the agent inside the hazard while it moves along its route,
    // so the test isolates the detection delay rather than range changes.
    safecrowd::domain::EnvironmentHazardDraft fire;
    fire.id = "fire-detect-delay";
    fire.kind = safecrowd::domain::EnvironmentHazardKind::Fire;
    fire.name = "Test hazard";
    fire.affectedZoneId = "room";
    fire.position = {.x = 0.0, .y = 0.4};
    fire.startSeconds = 0.0;
    fire.endSeconds = 0.0;
    fire.severity = safecrowd::domain::ScenarioElementSeverity::High;
    fire.radiusMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 71,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed}, 10.0));
    addHazardMotionSystems(runtime, straightExitLayout(), {fire});

    runtime.play();

    // After 0.25s the hazard is sensed but the 1s detection delay has not elapsed.
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.25});
    runtime.stepFrame(0.0);

    const auto& earlyState =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(0);
    SC_EXPECT_TRUE(earlyState.hazardInRange);
    SC_EXPECT_TRUE(!earlyState.hazardDetected);
    SC_EXPECT_TRUE(!earlyState.hazardAware);

    // Advance well past the detection delay; detection fires and, with no reaction delay,
    // awareness follows immediately.
    for (int step = 0; step < 7; ++step) {
        runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.25});
        runtime.stepFrame(0.0);
    }

    const auto& lateState =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(0);
    SC_EXPECT_TRUE(lateState.hazardDetected);
    SC_EXPECT_TRUE(lateState.hazardAware);
}

SC_TEST(ScenarioEnvironmentHazardSystem_RestartsDetectionDelayAfterLeavingHazardRange) {
    auto seed = straightRouteSeed({.x = 0.0, .y = 0.0}, 0.0, 0.0);
    seed.agent.detectionDelaySeconds = 1.0;
    seed.agent.reactionDelaySeconds = 0.0;

    auto fire = hazardDraft(
        "fire-reentry-delay",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::High,
        {.x = 0.0, .y = 0.0});
    fire.radiusMeters = 0.5;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 72,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed}, 10.0));
    addHazardMotionSystems(runtime, straightExitLayout(), {fire});

    runtime.play();
    for (int step = 0; step < 5; ++step) {
        stepScenarioRuntime(runtime, 0.25);
    }

    const auto& detectedState =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(0);
    SC_EXPECT_TRUE(detectedState.hazardDetected);
    SC_EXPECT_TRUE(detectedState.hazardAware);

    auto& query = runtime.world().query();
    const auto entity = query.view<safecrowd::domain::Position>().front();
    query.get<safecrowd::domain::Position>(entity).value = {.x = 5.0, .y = 0.0};
    stepScenarioRuntime(runtime, 0.25);

    const auto& outsideState =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(0);
    SC_EXPECT_TRUE(!outsideState.hazardInRange);
    SC_EXPECT_TRUE(!outsideState.hazardDetected);
    SC_EXPECT_TRUE(!outsideState.hazardAware);
    SC_EXPECT_EQ(outsideState.hazardKey, std::string{"fire-reentry-delay"});

    query.get<safecrowd::domain::Position>(entity).value = {.x = 0.0, .y = 0.0};
    stepScenarioRuntime(runtime, 0.25);

    const auto& reenteredState =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(0);
    SC_EXPECT_TRUE(reenteredState.hazardInRange);
    SC_EXPECT_TRUE(!reenteredState.hazardDetected);
    SC_EXPECT_TRUE(!reenteredState.hazardAware);
}

SC_TEST(ScenarioEnvironmentHazardSystem_SmokeSlowsButDoesNotStopAgent) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> baselineSeeds;
    baselineSeeds.push_back(straightRouteSeed({.x = 0.0, .y = 0.0}, 1.0));

    safecrowd::engine::EngineRuntime baselineRuntime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 44,
    });
    baselineRuntime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(baselineSeeds), 10.0));
    baselineRuntime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    baselineRuntime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    baselineRuntime.play();
    baselineRuntime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.5});
    baselineRuntime.stepFrame(0.0);
    const auto baselineVelocity =
        baselineRuntime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame.agents.front().velocity;
    const auto baselineSpeed = std::hypot(baselineVelocity.x, baselineVelocity.y);

    std::vector<safecrowd::domain::ScenarioAgentSeed> smokeSeeds;
    smokeSeeds.push_back(straightRouteSeed({.x = 0.0, .y = 0.0}, 1.0));
    auto smoke = hazardDraft(
        "smoke-a",
        safecrowd::domain::EnvironmentHazardKind::Smoke,
        safecrowd::domain::ScenarioElementSeverity::High,
        {.x = 0.0, .y = 0.0});

    safecrowd::engine::EngineRuntime smokeRuntime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 45,
    });
    smokeRuntime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(smokeSeeds), 10.0));
    addHazardMotionSystems(smokeRuntime, straightExitLayout(), {smoke});

    smokeRuntime.play();
    smokeRuntime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.5});
    smokeRuntime.stepFrame(0.0);

    const auto smokeVelocity =
        smokeRuntime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame.agents.front().velocity;
    const auto smokeSpeed = std::hypot(smokeVelocity.x, smokeVelocity.y);
    const auto& smokeReaction =
        smokeRuntime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(0);
    SC_EXPECT_TRUE(smokeSpeed < baselineSpeed);
    SC_EXPECT_TRUE(smokeSpeed > 0.1);
    SC_EXPECT_NEAR(smokeReaction.hazardSpeedFactor, 0.2, 1e-9);
}

SC_TEST(ScenarioEnvironmentHazardSystem_ReroutesOnlyAfterHazardAwareness) {
    safecrowd::domain::ScenarioAgentSeed seed;
    seed.position = {.value = {.x = 1.2, .y = 0.5}};
    seed.agent = {.radius = 0.25f, .maxSpeed = 1.0f, .hazardSensitivity = 2.0, .reactionDelaySeconds = 0.2};
    seed.velocity = {};
    seed.route = {
        .waypoints = {{.x = 2.0, .y = 0.5}, {.x = 3.0, .y = 0.5}},
        .waypointPassages = {
            {{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}},
            {{.x = 3.0, .y = 0.5}, {.x = 3.0, .y = 0.5}},
        },
        .waypointFromZoneIds = {"room", ""},
        .waypointZoneIds = {"near-exit", "near-exit"},
        .nextWaypointIndex = 0,
        .currentSegmentStart = {.x = 1.2, .y = 0.5},
        .previousDistanceToWaypoint = 0.8,
        .destinationZoneId = "near-exit",
    };

    auto fire = hazardDraft(
        "near-exit-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 3.6, .y = 0.5},
        "near-exit");

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 46,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        10.0));
    addHazardMotionSystems(runtime, twoExitGuidanceDetourLayout(), {fire});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    auto entities = runtime.world().query().view<safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"near-exit"});

    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});

    for (int i = 0; i < 3; ++i) {
        runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
        runtime.stepFrame(0.0);
        SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});
    }
}

SC_TEST(ScenarioEnvironmentHazardSystem_RoutePenaltyUsesPathGeometryNotOnlyAffectedZone) {
    safecrowd::domain::ScenarioAgentSeed seed;
    seed.position = {.value = {.x = 5.0, .y = 5.0}};
    seed.agent = {.radius = 0.25f, .maxSpeed = 1.0f, .hazardSensitivity = 4.0};
    seed.velocity = {};
    seed.route = {
        .waypoints = {{.x = 10.0, .y = 1.0}, {.x = 11.0, .y = 1.0}},
        .waypointPassages = {
            {{.x = 10.0, .y = 0.7}, {.x = 10.0, .y = 1.3}},
            {{.x = 11.0, .y = 1.0}, {.x = 11.0, .y = 1.0}},
        },
        .waypointFromZoneIds = {"room", ""},
        .waypointZoneIds = {"near-exit", "near-exit"},
        .waypointConnectionIds = {"room-near-exit", ""},
        .nextWaypointIndex = 0,
        .currentSegmentStart = {.x = 5.0, .y = 5.0},
        .previousDistanceToWaypoint = 6.4,
        .destinationZoneId = "near-exit",
    };

    auto fire = hazardDraft(
        "room-fire-near-exit",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 9.5, .y = 1.0},
        "room");

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 48,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        10.0));
    addHazardMotionSystems(runtime, wideTwoExitHazardRouteLayout(), {fire});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});
}

SC_TEST(ScenarioEnvironmentHazardSystem_ExposureCountsPhysicalFootprintIndependentOfDetectionSensitivity) {
    auto seed = straightRouteSeed({.x = 0.0, .y = 0.0}, 1.0);
    seed.agent.hazardSensitivity = 0.0;

    auto fire = hazardDraft(
        "physical-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 0.0, .y = 0.0});

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 49,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioEnvironmentHazardSystem(straightExitLayout(), {fire}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -20,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 1.0});
    runtime.stepFrame(0.0);

    const auto& exposure =
        runtime.world().resources().get<safecrowd::domain::ScenarioHazardExposureResource>();
    const auto& metric = exposure.hazardsById.at("physical-fire");
    SC_EXPECT_NEAR(metric.exposedAgentSeconds, 1.0, 1e-9);
    SC_EXPECT_EQ(metric.peakExposedAgentCount, std::size_t{1});

    const auto& reactions =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>();
    const auto reactionIt = reactions.agentsById.find(0);
    SC_EXPECT_TRUE(reactionIt == reactions.agentsById.end() || !reactionIt->second.hazardInRange);
}

SC_TEST(ScenarioEnvironmentHazardSystem_IgnoresInactiveAndDifferentFloorHazards) {
    auto layout = overlappingFloorBottleneckLayout();
    auto seed = straightRouteSeed({.x = 0.25, .y = 0.0}, 1.0, 0.0, "exit-l1");
    seed.route.waypoints = {{.x = 1.0, .y = 0.0}};
    seed.route.waypointZoneIds = {"exit-l1"};
    seed.route.destinationZoneId = "exit-l1";
    seed.route.currentFloorId = "L1";
    seed.route.displayFloorId = "L1";

    auto inactive = hazardDraft(
        "inactive-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::High,
        {.x = 0.25, .y = 0.0},
        "room-l1",
        "L1");
    inactive.startSeconds = 5.0;
    inactive.endSeconds = 10.0;
    auto otherFloor = hazardDraft(
        "other-floor-smoke",
        safecrowd::domain::EnvironmentHazardKind::Smoke,
        safecrowd::domain::ScenarioElementSeverity::High,
        {.x = 0.25, .y = 0.0},
        "room-l2",
        "L2");

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 47,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        10.0));
    addHazardMotionSystems(runtime, layout, {inactive, otherFloor});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.5});
    runtime.stepFrame(0.0);

    const auto& frame =
        runtime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
    SC_EXPECT_TRUE(frame.agents.front().velocity.x > 0.9);
    SC_EXPECT_NEAR(frame.agents.front().velocity.y, 0.0, 1e-6);

    const auto& reactions =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>();
    const auto reactionIt = reactions.agentsById.find(0);
    SC_EXPECT_TRUE(reactionIt == reactions.agentsById.end() || !reactionIt->second.hazardInRange);

    const auto& activeHazards =
        runtime.world().resources().get<safecrowd::domain::ScenarioActiveEnvironmentHazardsResource>();
    SC_EXPECT_EQ(activeHazards.hazards.size(), std::size_t{1});
    SC_EXPECT_TRUE(!activeHazards.signature.empty());
    SC_EXPECT_TRUE(activeHazards.maxRadiusMeters > 0.0);
    SC_EXPECT_TRUE(safecrowd::domain::scenarioNearbyHazardIndices(activeHazards, {.x = 0.25, .y = 0.0}, "L1", 5.0).empty());
    SC_EXPECT_TRUE(!safecrowd::domain::scenarioNearbyHazardIndices(activeHazards, {.x = 0.25, .y = 0.0}, "L2", 5.0).empty());

    const auto& exposure =
        runtime.world().resources().get<safecrowd::domain::ScenarioHazardExposureResource>();
    for (const auto& [_, metric] : exposure.hazardsById) {
        SC_EXPECT_NEAR(metric.exposedAgentSeconds, 0.0, 1e-9);
        SC_EXPECT_NEAR(metric.exposureScore, 0.0, 1e-9);
    }
}

SC_TEST(ScenarioPressureFeedbackSystem_PublishesSoftFeedbackForDenseCluster) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 43,
    });
    runtime.addSystem(std::make_unique<ConfigureDenseActiveAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioPressureFeedbackSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 1.0});
    runtime.stepFrame(0.0);

    const auto& feedback =
        runtime.world().resources().get<safecrowd::domain::ScenarioPressureFeedbackResource>();
    SC_EXPECT_TRUE(feedback.exposedAgentCount > 0);
    SC_EXPECT_TRUE(!feedback.agentsById.empty());
    const auto hasSlowAgent = std::any_of(feedback.agentsById.begin(), feedback.agentsById.end(), [](const auto& entry) {
        return entry.second.speedFactor < 0.999;
    });
    const auto hasBoostedAvoidance = std::any_of(feedback.agentsById.begin(), feedback.agentsById.end(), [](const auto& entry) {
        return entry.second.avoidanceScale > 1.0 && entry.second.barrierScale > 1.0;
    });
    SC_EXPECT_TRUE(hasSlowAgent);
    SC_EXPECT_TRUE(hasBoostedAvoidance);
}

SC_TEST(ScenarioPressureFeedbackSystem_DoesNotPublishFeedbackForLooseCluster) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (const auto& point : std::vector<safecrowd::domain::Point2D>{
             {0.10, 0.10},
             {1.30, 0.10},
             {0.10, 1.30},
             {1.30, 1.30},
             {0.75, 0.75},
         }) {
        seeds.push_back({
            .position = {.value = point},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 1.0, .y = 0.0}},
                .waypointPassages = {{{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = point,
                .previousDistanceToWaypoint = 0.25,
                .destinationZoneId = "exit",
                .currentFloorId = "L1",
                .displayFloorId = "L1",
            },
            .status = {},
        });
    }

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 44,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioPressureFeedbackSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 1.0});
    runtime.stepFrame(0.0);

    const auto& feedback =
        runtime.world().resources().get<safecrowd::domain::ScenarioPressureFeedbackResource>();
    SC_EXPECT_EQ(feedback.exposedAgentCount, std::size_t{0});
    SC_EXPECT_EQ(feedback.criticalAgentCount, std::size_t{0});
    SC_EXPECT_TRUE(feedback.agentsById.empty());
}

SC_TEST(ScenarioPressureFeedbackSystem_SeparatesOverlappingFloors) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 45,
    });
    runtime.addSystem(std::make_unique<ConfigureOverlappingFloorAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioPressureFeedbackSystem(overlappingFloorBottleneckLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 1.0});
    runtime.stepFrame(0.0);

    const auto& feedback =
        runtime.world().resources().get<safecrowd::domain::ScenarioPressureFeedbackResource>();
    SC_EXPECT_EQ(feedback.exposedAgentCount, std::size_t{0});
    SC_EXPECT_EQ(feedback.criticalAgentCount, std::size_t{0});
    SC_EXPECT_TRUE(feedback.agentsById.empty());
}

SC_TEST(ScenarioPressureFeedbackSystem_ReusesPriorCompressionFeedbackBetweenBucketUpdates) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 47,
    });
    runtime.addSystem(std::make_unique<ConfigureQuietAgentWithPriorPressureFeedbackSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioPressureFeedbackSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = -10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.5});
    runtime.stepFrame(0.0);

    const auto& feedback =
        runtime.world().resources().get<safecrowd::domain::ScenarioPressureFeedbackResource>();
    SC_EXPECT_EQ(feedback.exposedAgentCount, std::size_t{1});
    SC_EXPECT_EQ(feedback.criticalAgentCount, std::size_t{0});
    const auto feedbackIt = feedback.agentsById.find(0U);
    SC_EXPECT_TRUE(feedbackIt != feedback.agentsById.end());
    if (feedbackIt != feedback.agentsById.end()) {
        SC_EXPECT_NEAR(feedbackIt->second.compressionForce, 0.22, 1e-9);
        SC_EXPECT_TRUE(feedbackIt->second.exposureSeconds > 0.6);
        SC_EXPECT_TRUE(feedbackIt->second.speedFactor < 1.0);
        SC_EXPECT_TRUE(feedbackIt->second.avoidanceScale > 1.0);
    }
}

SC_TEST(ScenarioSimulationMotionSystem_UsesPressureFeedbackToReduceProgressWithoutFreezing) {
    auto runAverageX = [](bool withFeedback) {
        safecrowd::engine::EngineRuntime runtime({
            .fixedDeltaTime = 1.0 / 30.0,
            .maxCatchUpSteps = 1,
            .baseSeed = 46,
        });
        runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
            pressureFeedbackMotionSeeds(),
            10.0));
        runtime.addSystem(
            std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
            {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
             .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
        if (withFeedback) {
            runtime.addSystem(
                safecrowd::domain::makeScenarioPressureFeedbackSystem(straightExitLayout()),
                {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
                 .order = -10,
                 .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
        }
        runtime.addSystem(
            safecrowd::domain::makeScenarioSimulationMotionSystem(straightExitLayout()),
            {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
             .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
        runtime.addSystem(
            std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
            {.phase = safecrowd::engine::UpdatePhase::RenderSync,
             .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

        runtime.play();
        runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.5});
        runtime.stepFrame(0.0);

        const auto& frame =
            runtime.world().resources().get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
        double xSum = 0.0;
        for (const auto& agent : frame.agents) {
            xSum += agent.position.x;
        }
        return std::pair<double, std::size_t>{
            frame.agents.empty() ? 0.0 : xSum / static_cast<double>(frame.agents.size()),
            frame.agents.size(),
        };
    };

    const auto [baselineAverageX, baselineCount] = runAverageX(false);
    const auto [feedbackAverageX, feedbackCount] = runAverageX(true);

    SC_EXPECT_EQ(baselineCount, std::size_t{3});
    SC_EXPECT_EQ(feedbackCount, std::size_t{3});
    SC_EXPECT_TRUE(baselineAverageX > 0.2);
    SC_EXPECT_TRUE(feedbackAverageX > 0.2);
    SC_EXPECT_TRUE(feedbackAverageX < baselineAverageX);
}

SC_TEST(ScenarioSimulationMotionSystem_TreatsZeroGuidanceDetourAsStrictTolerance) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 0.5, .y = 0.5}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f, .guidancePropensity = 1.0},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 2.0, .y = 0.5}},
            .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {"near-exit"},
            .waypointConnectionIds = {"room-near-exit"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.5, .y = 0.5},
            .previousDistanceToWaypoint = 1.5,
            .destinationZoneId = "near-exit",
            .originalDestinationZoneId = "near-exit",
        },
        .status = {},
    });
    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = "strict-detour-guidance";
    guidance.guidedExitZoneId = "far-exit";
    guidance.baseComplianceRate = 1.0;
    guidance.maxDetourMeters = 0.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 13,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(
            twoExitGuidanceDetourLayout(),
            std::vector<safecrowd::domain::RouteGuidanceDraft>{guidance}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"near-exit"});
    SC_EXPECT_TRUE(!route.followsGuidance);
}

SC_TEST(ScenarioSimulationMotionSystem_AppliesInstalledGuidanceOnlyNearInstallConnection) {
    auto runFromPosition = [](const safecrowd::domain::Point2D& start) {
        std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
        seeds.push_back({
            .position = {.value = start},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f, .guidancePropensity = 1.0},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 2.0, .y = 0.5}},
                .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"near-exit"},
                .waypointConnectionIds = {"room-near-exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = start,
                .previousDistanceToWaypoint = 1.5,
                .destinationZoneId = "near-exit",
                .originalDestinationZoneId = "near-exit",
            },
            .status = {},
        });

        safecrowd::domain::RouteGuidanceDraft guidance;
        guidance.id = "installed-guidance";
        guidance.guidedExitZoneId = "far-exit";
        guidance.installConnectionId = "room-near-exit";
        guidance.baseComplianceRate = 1.0;
        guidance.maxDetourMeters = 100.0;

        safecrowd::engine::EngineRuntime runtime({
            .fixedDeltaTime = 1.0 / 30.0,
            .maxCatchUpSteps = 1,
            .baseSeed = 13,
        });
        runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
        runtime.addSystem(
            safecrowd::domain::makeScenarioSimulationMotionSystem(
                twoExitGuidanceDetourLayout(),
                std::vector<safecrowd::domain::RouteGuidanceDraft>{guidance}),
            {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
             .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

        runtime.play();
        runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
        runtime.stepFrame(0.0);

        const auto entities = runtime.world().query().view<
            safecrowd::domain::Position,
            safecrowd::domain::Agent,
            safecrowd::domain::Velocity,
            safecrowd::domain::AvoidanceState,
            safecrowd::domain::EvacuationRoute,
            safecrowd::domain::EvacuationStatus>();
        SC_EXPECT_EQ(entities.size(), std::size_t{1});
        return runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    };

    const auto unseenRoute = runFromPosition({.x = 0.5, .y = 3.5});
    SC_EXPECT_EQ(unseenRoute.destinationZoneId, std::string{"near-exit"});
    SC_EXPECT_TRUE(!unseenRoute.followsGuidance);
    SC_EXPECT_TRUE(unseenRoute.guidanceEventId.empty());

    const auto visibleRoute = runFromPosition({.x = 1.6, .y = 0.5});
    SC_EXPECT_EQ(visibleRoute.destinationZoneId, std::string{"far-exit"});
    SC_EXPECT_TRUE(visibleRoute.followsGuidance);
    SC_EXPECT_EQ(visibleRoute.guidanceEventId, std::string{"installed-guidance"});
}

SC_TEST(ScenarioSimulationMotionSystem_GuidanceInstalledOnExitDoorUsesThatDoorWhenExitZoneHasMultipleDoors) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 1.6, .y = 3.5}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f, .guidancePropensity = 1.0},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 2.0, .y = 0.5}},
            .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {"shared-exit"},
            .waypointConnectionIds = {"room-exit-lower"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 1.6, .y = 3.5},
            .previousDistanceToWaypoint = 3.0,
            .destinationZoneId = "shared-exit",
            .originalDestinationZoneId = "shared-exit",
        },
        .status = {},
    });

    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = "upper-exit-door-guidance";
    guidance.guidedExitZoneId = "shared-exit";
    guidance.installConnectionId = "room-exit-upper";
    guidance.baseComplianceRate = 1.0;
    guidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 13,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(
            sameExitTwoDoorGuidanceLayout(),
            std::vector<safecrowd::domain::RouteGuidanceDraft>{guidance}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_TRUE(route.followsGuidance);
    SC_EXPECT_EQ(route.guidanceEventId, std::string{"upper-exit-door-guidance"});
    SC_EXPECT_TRUE(std::find(
        route.waypointConnectionIds.begin(),
        route.waypointConnectionIds.end(),
        std::string{"room-exit-upper"}) != route.waypointConnectionIds.end());
    SC_EXPECT_TRUE(std::find(
        route.waypointConnectionIds.begin(),
        route.waypointConnectionIds.end(),
        std::string{"room-exit-lower"}) == route.waypointConnectionIds.end());
}

SC_TEST(ScenarioSimulationMotionSystem_RechecksInstalledGuidanceAsAgentApproachesInstallConnection) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 0.5, .y = 2.15}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f, .guidancePropensity = 1.0},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 2.0, .y = 0.5}},
            .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {"near-exit"},
            .waypointConnectionIds = {"room-near-exit"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.5, .y = 2.15},
            .previousDistanceToWaypoint = 2.27,
            .destinationZoneId = "near-exit",
            .originalDestinationZoneId = "near-exit",
        },
        .status = {},
    });

    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = "installed-guidance";
    guidance.guidedExitZoneId = "far-exit";
    guidance.installConnectionId = "room-near-exit";
    guidance.baseComplianceRate = 1.0;
    guidance.influenceRadiusMeters = 2.0;
    guidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 13,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(
            twoExitGuidanceDetourLayout(),
            std::vector<safecrowd::domain::RouteGuidanceDraft>{guidance}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    for (int step = 0; step < 180; ++step) {
        runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
        runtime.stepFrame(0.0);
    }

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.guidanceEventId, std::string{"installed-guidance"});
    SC_EXPECT_TRUE(route.followsGuidance);
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});
}

SC_TEST(ScenarioSimulationMotionSystem_AppliesRoomGuidanceOnlyNearInstallPosition) {
    auto runFromPosition = [](const safecrowd::domain::Point2D& start) {
        std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
        seeds.push_back({
            .position = {.value = start},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f, .guidancePropensity = 1.0},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 2.0, .y = 0.5}},
                .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"near-exit"},
                .waypointConnectionIds = {"room-near-exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = start,
                .previousDistanceToWaypoint = 1.5,
                .destinationZoneId = "near-exit",
                .originalDestinationZoneId = "near-exit",
            },
            .status = {},
        });

        safecrowd::domain::RouteGuidanceDraft guidance;
        guidance.id = "room-guidance";
        guidance.guidedExitZoneId = "far-exit";
        guidance.installZoneId = "room";
        guidance.installPosition = {.x = 0.8, .y = 3.5};
        guidance.baseComplianceRate = 1.0;
        guidance.maxDetourMeters = 100.0;

        safecrowd::engine::EngineRuntime runtime({
            .fixedDeltaTime = 1.0 / 30.0,
            .maxCatchUpSteps = 1,
            .baseSeed = 13,
        });
        runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
        runtime.addSystem(
            safecrowd::domain::makeScenarioSimulationMotionSystem(
                twoExitGuidanceDetourLayout(),
                std::vector<safecrowd::domain::RouteGuidanceDraft>{guidance}),
            {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
             .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

        runtime.play();
        runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
        runtime.stepFrame(0.0);

        const auto entities = runtime.world().query().view<
            safecrowd::domain::Position,
            safecrowd::domain::Agent,
            safecrowd::domain::Velocity,
            safecrowd::domain::AvoidanceState,
            safecrowd::domain::EvacuationRoute,
            safecrowd::domain::EvacuationStatus>();
        SC_EXPECT_EQ(entities.size(), std::size_t{1});
        return runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    };

    const auto farRoute = runFromPosition({.x = 0.5, .y = 0.5});
    SC_EXPECT_EQ(farRoute.destinationZoneId, std::string{"near-exit"});
    SC_EXPECT_TRUE(!farRoute.followsGuidance);
    SC_EXPECT_TRUE(farRoute.guidanceEventId.empty());

    const auto nearRoute = runFromPosition({.x = 0.8, .y = 3.1});
    SC_EXPECT_EQ(nearRoute.destinationZoneId, std::string{"far-exit"});
    SC_EXPECT_TRUE(nearRoute.followsGuidance);
    SC_EXPECT_EQ(nearRoute.guidanceEventId, std::string{"room-guidance"});
}

SC_TEST(ScenarioSimulationMotionSystem_AppliesMultipleActiveGuidancesIndependently) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 1.6, .y = 0.5}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f, .guidancePropensity = 1.0},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 2.0, .y = 0.5}},
            .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {"near-exit"},
            .waypointConnectionIds = {"room-near-exit"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 1.6, .y = 0.5},
            .previousDistanceToWaypoint = 0.4,
            .destinationZoneId = "near-exit",
            .originalDestinationZoneId = "near-exit",
        },
        .status = {},
    });
    seeds.push_back({
        .position = {.value = {.x = 0.8, .y = 3.1}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f, .guidancePropensity = 1.0},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 2.0, .y = 3.5}},
            .waypointPassages = {{{.x = 2.0, .y = 3.3}, {.x = 2.0, .y = 3.7}}},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {"far-exit"},
            .waypointConnectionIds = {"room-far-exit"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.8, .y = 3.1},
            .previousDistanceToWaypoint = 1.2,
            .destinationZoneId = "near-exit",
            .originalDestinationZoneId = "near-exit",
        },
        .status = {},
    });

    safecrowd::domain::RouteGuidanceDraft doorGuidance;
    doorGuidance.id = "door-guidance";
    doorGuidance.guidedExitZoneId = "far-exit";
    doorGuidance.installConnectionId = "room-near-exit";
    doorGuidance.baseComplianceRate = 1.0;
    doorGuidance.maxDetourMeters = 100.0;

    safecrowd::domain::RouteGuidanceDraft roomGuidance;
    roomGuidance.id = "room-guidance";
    roomGuidance.guidedExitZoneId = "far-exit";
    roomGuidance.installZoneId = "room";
    roomGuidance.installPosition = {.x = 0.8, .y = 3.5};
    roomGuidance.baseComplianceRate = 1.0;
    roomGuidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 13,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(
            twoExitGuidanceDetourLayout(),
            std::vector<safecrowd::domain::RouteGuidanceDraft>{doorGuidance, roomGuidance}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{2});

    int doorGuidanceCount = 0;
    int roomGuidanceCount = 0;
    for (const auto entity : entities) {
        const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entity);
        if (route.guidanceEventId == "door-guidance") {
            SC_EXPECT_TRUE(route.followsGuidance);
            SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});
            doorGuidanceCount += 1;
        } else if (route.guidanceEventId == "room-guidance") {
            SC_EXPECT_TRUE(route.followsGuidance);
            SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});
            roomGuidanceCount += 1;
        }
    }

    SC_EXPECT_EQ(doorGuidanceCount, 1);
    SC_EXPECT_EQ(roomGuidanceCount, 1);
}

SC_TEST(ScenarioSimulationMotionSystem_LaterApplicableGuidanceReplacesRetainedInstalledGuidance) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 1.6, .y = 0.5}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f, .guidancePropensity = 1.0},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 2.0, .y = 0.5}},
            .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {"near-exit"},
            .waypointConnectionIds = {"room-near-exit"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 1.6, .y = 0.5},
            .previousDistanceToWaypoint = 0.4,
            .destinationZoneId = "near-exit",
            .originalDestinationZoneId = "near-exit",
        },
        .status = {},
    });

    safecrowd::domain::RouteGuidanceDraft doorGuidance;
    doorGuidance.id = "door-guidance";
    doorGuidance.guidedExitZoneId = "far-exit";
    doorGuidance.installConnectionId = "room-near-exit";
    doorGuidance.baseComplianceRate = 1.0;
    doorGuidance.maxDetourMeters = 100.0;

    safecrowd::domain::RouteGuidanceDraft roomGuidance;
    roomGuidance.id = "room-guidance";
    roomGuidance.guidedExitZoneId = "near-exit";
    roomGuidance.installZoneId = "room";
    roomGuidance.installPosition = {.x = 1.6, .y = 0.5};
    roomGuidance.periods.push_back({.startSeconds = 0.2, .endSeconds = 10.0});
    roomGuidance.baseComplianceRate = 1.0;
    roomGuidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 13,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(
            twoExitGuidanceDetourLayout(),
            std::vector<safecrowd::domain::RouteGuidanceDraft>{doorGuidance, roomGuidance}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    for (int step = 0; step < 4; ++step) {
        runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
        runtime.stepFrame(0.0);
    }

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.guidanceEventId, std::string{"room-guidance:p0"});
    SC_EXPECT_TRUE(route.followsGuidance);
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"near-exit"});
}

SC_TEST(ScenarioSimulationMotionSystem_PrioritizesAgentsNearInstalledGuidanceBeforeGlobalBudgetSweep) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (int index = 0; index < 60; ++index) {
        const auto y = 3.5 + (0.02 * static_cast<double>(index));
        seeds.push_back({
            .position = {.value = {.x = 0.5, .y = y}},
            .agent = {
                .radius = 0.25f,
                .maxSpeed = 1.0f,
                .sourcePlacementId = "far-agent",
                .guidancePropensity = 1.0,
            },
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 2.0, .y = 0.5}},
                .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"near-exit"},
                .waypointConnectionIds = {"room-near-exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = {.x = 0.5, .y = y},
                .previousDistanceToWaypoint = std::sqrt((1.5 * 1.5) + ((y - 0.5) * (y - 0.5))),
                .destinationZoneId = "near-exit",
                .originalDestinationZoneId = "near-exit",
            },
            .status = {},
        });
    }

    seeds.push_back({
        .position = {.value = {.x = 1.6, .y = 0.5}},
        .agent = {
            .radius = 0.25f,
            .maxSpeed = 1.0f,
            .sourcePlacementId = "priority-near",
            .guidancePropensity = 1.0,
        },
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 2.0, .y = 0.5}},
            .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
            .waypointFromZoneIds = {"room"},
            .waypointZoneIds = {"near-exit"},
            .waypointConnectionIds = {"room-near-exit"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 1.6, .y = 0.5},
            .previousDistanceToWaypoint = 0.4,
            .destinationZoneId = "near-exit",
            .originalDestinationZoneId = "near-exit",
        },
        .status = {},
    });

    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = "installed-guidance";
    guidance.guidedExitZoneId = "far-exit";
    guidance.installConnectionId = "room-near-exit";
    guidance.baseComplianceRate = 1.0;
    guidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 13,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(
            twoExitGuidanceDetourLayout(),
            std::vector<safecrowd::domain::RouteGuidanceDraft>{guidance}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();

    const auto nearIt = std::find_if(entities.begin(), entities.end(), [&](const auto entity) {
        const auto& agent = runtime.world().query().get<safecrowd::domain::Agent>(entity);
        return agent.sourcePlacementId == "priority-near";
    });
    SC_EXPECT_TRUE(nearIt != entities.end());

    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(*nearIt);
    SC_EXPECT_EQ(route.guidanceEventId, std::string{"installed-guidance"});
    SC_EXPECT_TRUE(route.followsGuidance);
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});
}

SC_TEST(ScenarioSimulationMotionSystem_GuidanceFallsBackWhenTargetConnectionBlocked) {
    auto layout = twoExitGuidanceDetourLayout();
    safecrowd::domain::ConnectionBlockDraft block;
    block.id = "block-near-exit";
    block.connectionId = "room-near-exit";

    auto seed = doorRouteSeed(
        {.x = 1.6, .y = 0.5},
        "far-exit",
        "room-far-exit",
        {{.x = 2.0, .y = 3.3}, {.x = 2.0, .y = 3.7}});
    seed.agent.guidancePropensity = 1.0;
    seed.route.originalDestinationZoneId = "far-exit";

    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = "blocked-near-guidance";
    guidance.guidedExitZoneId = "near-exit";
    guidance.installConnectionId = "room-near-exit";
    guidance.baseComplianceRate = 1.0;
    guidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 121,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        10.0));
    addClosureGuidanceMotionSystems(runtime, layout, {block}, {guidance});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_TRUE(route.followsGuidance);
    SC_EXPECT_EQ(route.guidanceEventId, std::string{"blocked-near-guidance"});
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});
    SC_EXPECT_TRUE(std::none_of(
        route.waypointConnectionIds.begin(),
        route.waypointConnectionIds.end(),
        [](const auto& connectionId) {
            return connectionId == "room-near-exit";
        }));
}

SC_TEST(ScenarioSimulationMotionSystem_HazardAwareGuidanceAvoidsUnsafeGuidedRoute) {
    auto seed = doorRouteSeed(
        {.x = 5.0, .y = 5.0},
        "far-exit",
        "room-far-exit",
        {{.x = 10.0, .y = 8.7}, {.x = 10.0, .y = 9.3}});
    seed.agent.guidancePropensity = 1.0;
    seed.agent.hazardSensitivity = 4.0;
    seed.agent.reactionDelaySeconds = 0.0;
    seed.route.originalDestinationZoneId = "far-exit";

    auto fire = hazardDraft(
        "near-route-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 9.5, .y = 1.0},
        "room");

    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = "global-near-guidance";
    guidance.guidedExitZoneId = "near-exit";
    guidance.baseComplianceRate = 1.0;
    guidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 122,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        10.0));
    addHazardGuidanceMotionSystems(runtime, wideTwoExitHazardRouteLayout(), {fire}, {guidance});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& reaction =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(entities.front().index);
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_TRUE(reaction.hazardAware);
    SC_EXPECT_TRUE(route.followsGuidance);
    SC_EXPECT_EQ(route.guidanceEventId, std::string{"global-near-guidance"});
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});
}

SC_TEST(ScenarioSimulationMotionSystem_DelaysGuidanceHazardAvoidanceUntilAwareness) {
    auto seed = doorRouteSeed(
        {.x = 5.0, .y = 5.0},
        "far-exit",
        "room-far-exit",
        {{.x = 10.0, .y = 8.7}, {.x = 10.0, .y = 9.3}});
    seed.agent.guidancePropensity = 1.0;
    seed.agent.hazardSensitivity = 4.0;
    seed.agent.reactionDelaySeconds = 10.0;
    seed.route.originalDestinationZoneId = "far-exit";

    auto fire = hazardDraft(
        "delayed-near-route-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 9.5, .y = 1.0},
        "room");

    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = "delayed-global-near-guidance";
    guidance.guidedExitZoneId = "near-exit";
    guidance.baseComplianceRate = 1.0;
    guidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 123,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        10.0));
    addHazardGuidanceMotionSystems(runtime, wideTwoExitHazardRouteLayout(), {fire}, {guidance});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& reaction =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(entities.front().index);
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_TRUE(reaction.hazardDetected);
    SC_EXPECT_TRUE(!reaction.hazardAware);
    SC_EXPECT_TRUE(route.followsGuidance);
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"near-exit"});
}

SC_TEST(ScenarioSimulationMotionSystem_ExpiredHazardAwarenessDoesNotRevealNewHazards) {
    auto seed = doorRouteSeed(
        {.x = 5.0, .y = 5.0},
        "far-exit",
        "room-far-exit",
        {{.x = 10.0, .y = 8.7}, {.x = 10.0, .y = 9.3}},
        0.0);
    seed.agent.guidancePropensity = 1.0;
    seed.agent.reactionDelaySeconds = 0.0;
    seed.route.originalDestinationZoneId = "far-exit";

    auto expiredFire = hazardDraft(
        "expired-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 5.0, .y = 5.0},
        "room");
    expiredFire.endSeconds = 0.05;

    auto futureFire = hazardDraft(
        "future-near-route-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 9.5, .y = 1.0},
        "room");
    futureFire.startSeconds = 1.0;
    futureFire.endSeconds = 1.0;

    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = "future-global-near-guidance";
    guidance.guidedExitZoneId = "near-exit";
    guidance.baseComplianceRate = 1.0;
    guidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 126,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        10.0));
    addHazardGuidanceMotionSystems(runtime, wideTwoExitHazardRouteLayout(), {expiredFire, futureFire}, {guidance});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& firstReaction =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(entities.front().index);
    SC_EXPECT_TRUE(firstReaction.hazardAware);
    SC_EXPECT_EQ(firstReaction.hazardKey, std::string{"expired-fire"});

    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 1.0;
    stepScenarioRuntime(runtime, 0.1);

    const auto& secondReaction =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(entities.front().index);
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_TRUE(!secondReaction.hazardInRange);
    SC_EXPECT_EQ(secondReaction.hazardKey, std::string{"expired-fire"});
    SC_EXPECT_TRUE(route.followsGuidance);
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"near-exit"});
}

SC_TEST(ScenarioSimulationMotionSystem_ReplansStalledHazardOpposedRouteToSameExit) {
    safecrowd::domain::ScenarioAgentSeed seed{
        .position = {.value = {.x = 5.0, .y = 5.0}},
        .agent = {
            .radius = 0.25f,
            .maxSpeed = 1.0f,
            .hazardSensitivity = 2.0,
            .reactionDelaySeconds = 0.0,
        },
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 5.0, .y = 6.0}},
            .waypointPassages = {{{.x = 5.0, .y = 6.0}, {.x = 5.0, .y = 6.0}}},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 5.0, .y = 5.0},
            .previousDistanceToWaypoint = 1.0,
            .stalledSeconds = 1.0,
            .destinationZoneId = "right-exit",
            .originalDestinationZoneId = "right-exit",
        },
        .status = {},
    };

    auto fire = hazardDraft(
        "overhead-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 5.0, .y = 8.1},
        "room");

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 127,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        10.0));
    addHazardMotionSystems(runtime, rightExitHazardDetourLayout(), {fire});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});

    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    const auto& reaction =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(entities.front().index);
    SC_EXPECT_TRUE(reaction.hazardAware);
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"right-exit"});
    SC_EXPECT_TRUE(std::find(
        route.waypointConnectionIds.begin(),
        route.waypointConnectionIds.end(),
        "room-right-exit") != route.waypointConnectionIds.end());
    SC_EXPECT_TRUE(route.waypoints.front().x > 8.0);
    SC_EXPECT_NEAR(route.stalledSeconds, 0.0, 1e-9);
}

SC_TEST(ScenarioSimulationMotionSystem_BudgetsHazardAwareExitReplans) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (int index = 0; index < 60; ++index) {
        auto seed = doorRouteSeed(
            {.x = 5.0, .y = 5.0},
            "near-exit",
            "room-near-exit",
            {{.x = 10.0, .y = 0.7}, {.x = 10.0, .y = 1.3}});
        seed.agent.hazardSensitivity = 2.0;
        seed.agent.reactionDelaySeconds = 0.0;
        seeds.push_back(seed);
    }

    auto fire = hazardDraft(
        "budget-near-route-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::High,
        {.x = 9.5, .y = 1.0},
        "room");

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 125,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    addHazardMotionSystems(runtime, wideTwoExitHazardRouteLayout(), {fire});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    int farExitCount = 0;
    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Agent,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute,
        safecrowd::domain::EvacuationStatus>();
    for (const auto entity : entities) {
        const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entity);
        if (route.destinationZoneId == "far-exit") {
            ++farExitCount;
        }
    }

    SC_EXPECT_EQ(farExitCount, 50);
}

SC_TEST(ScenarioSimulationMotionSystem_GlobalGuidanceKeepsBudgetedDeterministicSweep) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (int index = 0; index < 60; ++index) {
        const auto y = 0.2 + (0.05 * static_cast<double>(index));
        seeds.push_back({
            .position = {.value = {.x = 0.5, .y = y}},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f, .guidancePropensity = 1.0},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 2.0, .y = 0.5}},
                .waypointPassages = {{{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"near-exit"},
                .waypointConnectionIds = {"room-near-exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = {.x = 0.5, .y = y},
                .previousDistanceToWaypoint = 1.5,
                .destinationZoneId = "near-exit",
                .originalDestinationZoneId = "near-exit",
            },
            .status = {},
        });
    }

    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = "global-guidance";
    guidance.guidedExitZoneId = "far-exit";
    guidance.baseComplianceRate = 1.0;
    guidance.maxDetourMeters = 100.0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 124,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(
            twoExitGuidanceDetourLayout(),
            std::vector<safecrowd::domain::RouteGuidanceDraft>{guidance}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    auto guidedCount = [&]() {
        int count = 0;
        const auto entities = runtime.world().query().view<
            safecrowd::domain::Position,
            safecrowd::domain::Agent,
            safecrowd::domain::Velocity,
            safecrowd::domain::AvoidanceState,
            safecrowd::domain::EvacuationRoute,
            safecrowd::domain::EvacuationStatus>();
        for (const auto entity : entities) {
            const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entity);
            if (route.guidanceEventId == "global-guidance") {
                ++count;
            }
        }
        return count;
    };

    SC_EXPECT_EQ(guidedCount(), 50);

    stepScenarioRuntime(runtime, 0.1);
    SC_EXPECT_EQ(guidedCount(), 60);
}

SC_TEST(ScenarioSimulationMotionSystem_SkipsIntermediateWaypointWhenCrowdPushesAgentPastApproachArea) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 0.82, .y = 0.45}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 0.0}, {.x = 2.0, .y = 0.0}},
            .waypointPassages = {
                {{.x = 1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}},
                {{.x = 2.0, .y = 0.0}, {.x = 2.0, .y = 0.0}},
            },
            .waypointFromZoneIds = {"", ""},
            .waypointZoneIds = {"", "missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.0, .y = 0.0},
            .previousDistanceToWaypoint = 0.5,
            .destinationZoneId = "missing",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 14,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem({}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.1});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto entity = entities.front();
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entity);
    const auto& velocity = runtime.world().query().get<safecrowd::domain::Velocity>(entity);

    SC_EXPECT_EQ(route.nextWaypointIndex, std::size_t{1});
    SC_EXPECT_TRUE(velocity.value.x > 0.0);
}

SC_TEST(ScenarioSimulationMotionSystem_DoesNotBypassStalledVerticalTransition) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 1.0, .y = 1.01}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 1.0}, {.x = 1.8, .y = 0.5}},
            .waypointPassages = {
                {{.x = 0.6, .y = 1.0}, {.x = 1.4, .y = 1.0}},
                {{.x = 1.8, .y = 0.5}, {.x = 1.8, .y = 0.5}},
            },
            .waypointFromZoneIds = {"upper-stair", ""},
            .waypointZoneIds = {"lower-stair", "missing-exit"},
            .waypointFloorIds = {"L1", "L1"},
            .waypointConnectionIds = {"vertical-stair", ""},
            .waypointVerticalTransitions = {true, false},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 1.0, .y = 1.8},
            .previousDistanceToWaypoint = 0.01,
            .stalledSeconds = 1.0,
            .destinationZoneId = "missing-exit",
            .currentFloorId = "L2",
            .displayFloorId = "L2",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 18,
    });
    const auto layout = verticalPortalTransitionLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.001});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.currentFloorId, std::string{"L2"});
    SC_EXPECT_EQ(route.nextWaypointIndex, std::size_t{0});
}

SC_TEST(ScenarioSimulationMotionSystem_DoesNotAdvanceVerticalTransitionBeforePortalPlane) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 1.0, .y = 1.004}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 1.0}, {.x = 1.8, .y = 0.5}},
            .waypointPassages = {
                {{.x = 0.6, .y = 1.0}, {.x = 1.4, .y = 1.0}},
                {{.x = 1.8, .y = 0.5}, {.x = 1.8, .y = 0.5}},
            },
            .waypointFromZoneIds = {"upper-stair", ""},
            .waypointZoneIds = {"lower-stair", "missing-exit"},
            .waypointFloorIds = {"L1", "L1"},
            .waypointConnectionIds = {"vertical-stair", ""},
            .waypointVerticalTransitions = {true, false},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 1.0, .y = 1.8},
            .previousDistanceToWaypoint = 0.004,
            .destinationZoneId = "missing-exit",
            .currentFloorId = "L2",
            .displayFloorId = "L2",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 19,
    });
    const auto layout = verticalPortalTransitionLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.001});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.currentFloorId, std::string{"L2"});
    SC_EXPECT_EQ(route.nextWaypointIndex, std::size_t{0});
}

SC_TEST(ScenarioSimulationMotionSystem_KeepsVerticalLandingOnPortalLine) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 1.0, .y = 1.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 1.0}, {.x = 1.8, .y = 0.5}},
            .waypointPassages = {
                {{.x = 0.6, .y = 1.0}, {.x = 1.4, .y = 1.0}},
                {{.x = 1.8, .y = 0.5}, {.x = 1.8, .y = 0.5}},
            },
            .waypointFromZoneIds = {"upper-stair", ""},
            .waypointZoneIds = {"lower-stair", "missing-exit"},
            .waypointFloorIds = {"L1", "L1"},
            .waypointConnectionIds = {"vertical-stair", ""},
            .waypointVerticalTransitions = {true, false},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 1.0, .y = 1.8},
            .previousDistanceToWaypoint = 0.0,
            .destinationZoneId = "missing-exit",
            .currentFloorId = "L2",
            .displayFloorId = "L2",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 20,
    });
    const auto layout = verticalPortalTransitionLayoutWithBlockedLandingClearance();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.001});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto entity = entities.front();
    const auto& position = runtime.world().query().get<safecrowd::domain::Position>(entity);
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entity);
    SC_EXPECT_EQ(route.currentFloorId, std::string{"L1"});
    SC_EXPECT_EQ(route.nextWaypointIndex, std::size_t{1});
    SC_EXPECT_NEAR(position.value.x, 1.0, 0.002);
    SC_EXPECT_NEAR(position.value.y, 1.0, 0.002);
}

SC_TEST(ScenarioSimulationMotionSystem_DoesNotAdvanceVerticalTransitionOutsidePassageSpan) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 1.7, .y = 0.95}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 1.0}, {.x = 1.8, .y = 0.5}},
            .waypointPassages = {
                {{.x = 0.6, .y = 1.0}, {.x = 1.4, .y = 1.0}},
                {{.x = 1.8, .y = 0.5}, {.x = 1.8, .y = 0.5}},
            },
            .waypointFromZoneIds = {"upper-stair", ""},
            .waypointZoneIds = {"lower-stair", "missing-exit"},
            .waypointFloorIds = {"L1", "L1"},
            .waypointConnectionIds = {"vertical-stair", ""},
            .waypointVerticalTransitions = {true, false},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 1.7, .y = 1.8},
            .previousDistanceToWaypoint = 0.4,
            .destinationZoneId = "missing-exit",
            .currentFloorId = "L2",
            .displayFloorId = "L2",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 19,
    });
    const auto layout = verticalPortalTransitionLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.001});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.currentFloorId, std::string{"L2"});
    SC_EXPECT_EQ(route.nextWaypointIndex, std::size_t{0});
}

SC_TEST(ScenarioSimulationMotionSystem_DoesNotFastForwardVerticalTransitionFromCurrentZoneMatch) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = 1.0, .y = 1.8}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 1.0}, {.x = 1.8, .y = 1.8}},
            .waypointPassages = {
                {{.x = 0.6, .y = 1.0}, {.x = 1.4, .y = 1.0}},
                {{.x = 1.8, .y = 1.8}, {.x = 1.8, .y = 1.8}},
            },
            .waypointFromZoneIds = {"upper-stair", ""},
            .waypointZoneIds = {"lower-stair", "upper-stair"},
            .waypointFloorIds = {"L1", "L2"},
            .waypointConnectionIds = {"vertical-stair", ""},
            .waypointVerticalTransitions = {true, false},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 1.0, .y = 1.8},
            .previousDistanceToWaypoint = 0.8,
            .destinationZoneId = "upper-stair",
            .currentFloorId = "L2",
            .displayFloorId = "L2",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 20,
    });
    const auto layout = verticalPortalTransitionLayout();
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem(layout),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.001});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto& route = runtime.world().query().get<safecrowd::domain::EvacuationRoute>(entities.front());
    SC_EXPECT_EQ(route.currentFloorId, std::string{"L2"});
    SC_EXPECT_EQ(route.nextWaypointIndex, std::size_t{0});
}

SC_TEST(ScenarioSimulationMotionSystem_UsesStableHeadOnAvoidance) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = -0.4, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 0.0}},
            .waypointPassages = {{{.x = 1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = -0.4, .y = 0.0},
            .previousDistanceToWaypoint = 1.4,
            .destinationZoneId = "missing",
        },
        .status = {},
    });
    seeds.push_back({
        .position = {.value = {.x = 0.4, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = -1.0, .y = 0.0}},
            .waypointPassages = {{{.x = -1.0, .y = 0.0}, {.x = -1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.4, .y = 0.0},
            .previousDistanceToWaypoint = 1.4,
            .destinationZoneId = "missing",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 17,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioSpatialIndexSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioSimulationMotionSystem({}),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationStepResource{.deltaSeconds = 0.2});
    runtime.stepFrame(0.0);

    const auto entities = runtime.world().query().view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{2});

    const auto first = entities[0];
    const auto second = entities[1];
    const auto& firstVelocity = runtime.world().query().get<safecrowd::domain::Velocity>(first);
    const auto& secondVelocity = runtime.world().query().get<safecrowd::domain::Velocity>(second);
    const auto& firstAvoidance = runtime.world().query().get<safecrowd::domain::AvoidanceState>(first);
    const auto& secondAvoidance = runtime.world().query().get<safecrowd::domain::AvoidanceState>(second);

    SC_EXPECT_TRUE(firstVelocity.value.x > 0.0);
    SC_EXPECT_TRUE(secondVelocity.value.x < 0.0);
    SC_EXPECT_TRUE(firstVelocity.value.y * secondVelocity.value.y < 0.0);
    SC_EXPECT_TRUE(firstAvoidance.preferredSide != 0);
    SC_EXPECT_EQ(firstAvoidance.preferredSide, secondAvoidance.preferredSide);
}

SC_TEST(ScenarioSimulationMotionSystem_UsesPhysicsBucketForCrossFloorHeadOnAvoidance) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = -0.4, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 0.0}},
            .waypointPassages = {{{.x = 1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = -0.4, .y = 0.0},
            .previousDistanceToWaypoint = 1.4,
            .destinationZoneId = "missing",
            .currentFloorId = "L1",
            .displayFloorId = "L1",
            .physicsFloorId = "vertical:stair-vertical",
        },
        .status = {},
    });
    seeds.push_back({
        .position = {.value = {.x = 0.4, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = -1.0, .y = 0.0}},
            .waypointPassages = {{{.x = -1.0, .y = 0.0}, {.x = -1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.4, .y = 0.0},
            .previousDistanceToWaypoint = 1.4,
            .destinationZoneId = "missing",
            .currentFloorId = "L2",
            .displayFloorId = "L2",
            .physicsFloorId = "vertical:stair-vertical",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 18,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));

    runtime.play();
    runtime.stepFrame(0.0);

    auto& query = runtime.world().query();
    const auto entities = query.view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{2});

    const auto first = entities[0];
    const auto second = entities[1];
    double speedScale = 1.0;
    const auto avoidanceVelocity = safecrowd::domain::simulation_internal::forwardPreservingAgentAvoidanceVelocity(
        query,
        first,
        {second},
        {.x = 1.0, .y = 0.0},
        0.2,
        speedScale);
    const auto& avoidance = query.get<safecrowd::domain::AvoidanceState>(first);

    SC_EXPECT_TRUE(speedScale < 1.0);
    SC_EXPECT_TRUE(avoidanceVelocity.y != 0.0);
    SC_EXPECT_TRUE(avoidance.preferredSide != 0);
}

SC_TEST(ScenarioSimulationMotionSystem_ScalesAvoidanceByDesiredSpeed) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    seeds.push_back({
        .position = {.value = {.x = -0.05, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.5f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = 1.0, .y = 0.0}},
            .waypointPassages = {{{.x = 1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = -0.05, .y = 0.0},
            .previousDistanceToWaypoint = 1.05,
            .destinationZoneId = "missing",
            .currentFloorId = "L1",
            .displayFloorId = "L1",
            .physicsFloorId = "vertical:stair-vertical",
        },
        .status = {},
    });
    seeds.push_back({
        .position = {.value = {.x = 0.05, .y = 0.0}},
        .agent = {.radius = 0.25f, .maxSpeed = 1.5f},
        .velocity = {.value = {}},
        .route = {
            .waypoints = {{.x = -1.0, .y = 0.0}},
            .waypointPassages = {{{.x = -1.0, .y = 0.0}, {.x = -1.0, .y = 0.0}}},
            .waypointFromZoneIds = {""},
            .waypointZoneIds = {"missing"},
            .nextWaypointIndex = 0,
            .currentSegmentStart = {.x = 0.05, .y = 0.0},
            .previousDistanceToWaypoint = 1.05,
            .destinationZoneId = "missing",
            .currentFloorId = "L2",
            .displayFloorId = "L2",
            .physicsFloorId = "vertical:stair-vertical",
        },
        .status = {},
    });

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 34,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));

    runtime.play();
    runtime.stepFrame(0.0);

    auto& query = runtime.world().query();
    const auto entities = query.view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::AvoidanceState,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{2});

    double speedScale = 1.0;
    const auto avoidanceVelocity = safecrowd::domain::simulation_internal::forwardPreservingAgentAvoidanceVelocity(
        query,
        entities[1],
        {entities[0]},
        {.x = -0.8, .y = 0.0},
        0.2,
        speedScale);

    SC_EXPECT_TRUE(std::fabs(avoidanceVelocity.y) > 0.0);
    SC_EXPECT_TRUE(std::fabs(avoidanceVelocity.y) <= (0.8 * 0.45) + 1e-9);
}

SC_TEST(ScenarioSimulationMotionSystem_ScalesBarrierSeparationByReferenceSpeed) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.barriers.push_back({
        .id = "wall",
        .floorId = "L1",
        .geometry = {.vertices = {{.x = -1.0, .y = 0.0}, {.x = 1.0, .y = 0.0}}},
        .blocksMovement = true,
    });
    const safecrowd::domain::Position position{.value = {.x = 0.0, .y = 0.1}};
    const safecrowd::domain::Agent agent{.radius = 0.25f, .maxSpeed = 1.5f};

    const auto stairScaled = safecrowd::domain::simulation_internal::barrierSeparationVelocity(
        layout,
        position,
        agent,
        0.8);
    const auto normalScaled = safecrowd::domain::simulation_internal::barrierSeparationVelocity(
        layout,
        position,
        agent,
        1.5);

    SC_EXPECT_TRUE(std::fabs(stairScaled.y) > 0.0);
    SC_EXPECT_TRUE(std::fabs(stairScaled.y) < std::fabs(normalScaled.y));
}

SC_TEST(ScenarioControlSystem_BlocksConnectionsUsingScenarioClock) {
    auto layout = straightExitLayout();
    safecrowd::domain::ConnectionBlockDraft block;
    block.id = "block-1";
    block.connectionId = "room-exit";
    block.intervals = {
        {.startSeconds = 0.0, .endSeconds = 2.0},
    };

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 21,
    });
    runtime.addSystem(
        safecrowd::domain::makeScenarioControlSystem(layout, {block}),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();

    runtime.world().resources().set(safecrowd::domain::ScenarioSimulationClockResource{
        .elapsedSeconds = 1.0,
        .timeLimitSeconds = 10.0,
        .complete = false,
    });
    runtime.stepFrame(0.0);

    {
        const auto& layoutCache =
            runtime.world().resources().get<safecrowd::domain::ScenarioLayoutCacheResource>();
        SC_EXPECT_EQ(layoutCache.layout.connections.size(), std::size_t{1});
        SC_EXPECT_EQ(
            layoutCache.layout.connections.front().directionality,
            safecrowd::domain::TravelDirection::Closed);
        SC_EXPECT_EQ(layoutCache.layout.barriers.size(), std::size_t{1});
    }

    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 3.0;
    runtime.stepFrame(0.0);

    {
        const auto& layoutCache =
            runtime.world().resources().get<safecrowd::domain::ScenarioLayoutCacheResource>();
        SC_EXPECT_EQ(layoutCache.layout.connections.size(), std::size_t{1});
        SC_EXPECT_EQ(
            layoutCache.layout.connections.front().directionality,
            safecrowd::domain::TravelDirection::Bidirectional);
        SC_EXPECT_EQ(layoutCache.layout.barriers.size(), std::size_t{0});
    }
}

SC_TEST(ScenarioSimulationMotionSystem_SlowsBeforeDoorClosureReroute) {
    auto layout = twoExitGuidanceDetourLayout();
    safecrowd::domain::ConnectionBlockDraft block;
    block.id = "block-near";
    block.connectionId = "room-near-exit";

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 91,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{
            doorRouteSeed(
                {.x = 1.2, .y = 0.5},
                "near-exit",
                "room-near-exit",
                {{.x = 2.0, .y = 0.3}, {.x = 2.0, .y = 0.7}},
                1.0,
                0.5),
        },
        5.0));
    addClosureMotionSystems(runtime, layout, {block});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    auto& query = runtime.world().query();
    const auto entities = query.view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto entity = entities.front();
    const auto& velocityBeforeReady = query.get<safecrowd::domain::Velocity>(entity).value;
    const auto& routeBeforeReady = query.get<safecrowd::domain::EvacuationRoute>(entity);
    SC_EXPECT_EQ(routeBeforeReady.destinationZoneId, std::string{"near-exit"});
    SC_EXPECT_TRUE(safecrowd::domain::simulation_internal::lengthOf(velocityBeforeReady) > 0.0);
    SC_EXPECT_TRUE(safecrowd::domain::simulation_internal::lengthOf(velocityBeforeReady) < 0.5);

    const auto& reactions =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>();
    const auto stateIt = reactions.agentsById.find(entity.index);
    SC_EXPECT_TRUE(stateIt != reactions.agentsById.end());
    SC_EXPECT_TRUE(stateIt->second.closureDetected);
    SC_EXPECT_TRUE(!stateIt->second.closureAware);
    SC_EXPECT_EQ(stateIt->second.blockedConnectionId, std::string{"room-near-exit"});

    for (int i = 0; i < 6; ++i) {
        stepScenarioRuntime(runtime, 0.1);
    }

    const auto& routeAfterReady = query.get<safecrowd::domain::EvacuationRoute>(entity);
    SC_EXPECT_EQ(routeAfterReady.destinationZoneId, std::string{"far-exit"});
    SC_EXPECT_TRUE(!routeAfterReady.noExitAvailable);
}

SC_TEST(ScenarioSimulationMotionSystem_HoldsInsideZoneWhenDoorClosureLeavesNoExit) {
    auto layout = straightExitLayout();
    safecrowd::domain::ConnectionBlockDraft block;
    block.id = "block-exit";
    block.connectionId = "room-exit";

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 92,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{
            doorRouteSeed(
                {.x = 0.8, .y = 0.0},
                "exit",
                "room-exit",
                {{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}),
        },
        5.0));
    addClosureMotionSystems(runtime, layout, {block});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    auto& query = runtime.world().query();
    const auto entities = query.view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto entity = entities.front();
    const auto& route = query.get<safecrowd::domain::EvacuationRoute>(entity);
    const auto& position = query.get<safecrowd::domain::Position>(entity).value;
    const auto& layoutCache =
        runtime.world().resources().get<safecrowd::domain::ScenarioLayoutCacheResource>();

    SC_EXPECT_EQ(
        layoutCache.layout.connections.front().directionality,
        safecrowd::domain::TravelDirection::Closed);
    SC_EXPECT_EQ(route.destinationZoneId, std::string{});
    SC_EXPECT_TRUE(route.noExitAvailable);
    SC_EXPECT_TRUE(route.holdingForClosure);
    SC_EXPECT_TRUE(route.destinationZoneId.empty());
    SC_EXPECT_TRUE(!route.waypoints.empty());
    SC_EXPECT_TRUE(position.x < 0.8);
}

SC_TEST(ScenarioSimulationMotionSystem_RetriesNoExitAfterFiniteDoorClosureReopens) {
    auto layout = straightExitLayout();
    safecrowd::domain::ConnectionBlockDraft block;
    block.id = "block-exit";
    block.connectionId = "room-exit";
    block.intervals.push_back({.startSeconds = 0.0, .endSeconds = 0.3});

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 93,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{
            doorRouteSeed(
                {.x = 0.8, .y = 0.0},
                "exit",
                "room-exit",
                {{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}),
        },
        5.0));
    addClosureMotionSystems(runtime, layout, {block});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    auto& query = runtime.world().query();
    const auto entities = query.view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto entity = entities.front();
    const auto& layoutCache =
        runtime.world().resources().get<safecrowd::domain::ScenarioLayoutCacheResource>();
    SC_EXPECT_EQ(
        layoutCache.layout.connections.front().directionality,
        safecrowd::domain::TravelDirection::Closed);
    SC_EXPECT_EQ(query.get<safecrowd::domain::EvacuationRoute>(entity).destinationZoneId, std::string{});
    SC_EXPECT_TRUE(query.get<safecrowd::domain::EvacuationRoute>(entity).noExitAvailable);

    bool routeRecovered = false;
    for (int i = 0; i < 12; ++i) {
        stepScenarioRuntime(runtime, 0.1);
        const auto& route = query.get<safecrowd::domain::EvacuationRoute>(entity);
        if (!route.noExitAvailable && route.destinationZoneId == "exit") {
            routeRecovered = true;
            break;
        }
    }

    SC_EXPECT_TRUE(routeRecovered);
}

SC_TEST(ScenarioSimulationMotionSystem_CombinesHazardExposureWithDoorClosureReroute) {
    auto layout = wideTwoExitHazardRouteLayout();
    safecrowd::domain::ConnectionBlockDraft block;
    block.id = "block-near-exit";
    block.connectionId = "room-near-exit";

    auto seed = doorRouteSeed(
        {.x = 8.4, .y = 1.0},
        "near-exit",
        "room-near-exit",
        {{.x = 10.0, .y = 0.7}, {.x = 10.0, .y = 1.3}},
        1.0,
        0.2);
    seed.agent.reactionDelaySeconds = 10.0;
    seed.agent.hazardSensitivity = 1.0;
    seed.agent.smokeSensitivity = 1.0;

    auto fire = hazardDraft(
        "combined-fire",
        safecrowd::domain::EnvironmentHazardKind::Fire,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 8.4, .y = 1.0},
        "room");
    auto smoke = hazardDraft(
        "combined-smoke",
        safecrowd::domain::EnvironmentHazardKind::Smoke,
        safecrowd::domain::ScenarioElementSeverity::Low,
        {.x = 8.6, .y = 1.0},
        "room");

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.1,
        .maxCatchUpSteps = 1,
        .baseSeed = 94,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(
        std::vector<safecrowd::domain::ScenarioAgentSeed>{seed},
        5.0));
    addHazardClosureMotionSystems(runtime, layout, {fire, smoke}, {block});

    runtime.play();
    stepScenarioRuntime(runtime, 0.1);

    auto& query = runtime.world().query();
    const auto entities = query.view<
        safecrowd::domain::Position,
        safecrowd::domain::Velocity,
        safecrowd::domain::EvacuationRoute>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
    const auto entity = entities.front();

    const auto& firstState =
        runtime.world().resources().get<safecrowd::domain::ScenarioEnvironmentReactionResource>().agentsById.at(entity.index);
    SC_EXPECT_TRUE(firstState.hazardDetected);
    SC_EXPECT_TRUE(!firstState.hazardAware);
    SC_EXPECT_TRUE(firstState.closureDetected);
    SC_EXPECT_TRUE(!firstState.closureAware);
    SC_EXPECT_EQ(firstState.blockedConnectionId, std::string{"room-near-exit"});

    const auto& activeHazards =
        runtime.world().resources().get<safecrowd::domain::ScenarioActiveEnvironmentHazardsResource>();
    SC_EXPECT_EQ(activeHazards.hazards.size(), std::size_t{2});

    for (int i = 0; i < 4; ++i) {
        stepScenarioRuntime(runtime, 0.1);
    }

    const auto& route = query.get<safecrowd::domain::EvacuationRoute>(entity);
    SC_EXPECT_EQ(route.destinationZoneId, std::string{"far-exit"});
    SC_EXPECT_TRUE(!route.noExitAvailable);
    SC_EXPECT_TRUE(std::none_of(
        route.waypointConnectionIds.begin(),
        route.waypointConnectionIds.end(),
        [](const auto& connectionId) {
            return connectionId == "room-near-exit";
        }));

    const auto& exposure =
        runtime.world().resources().get<safecrowd::domain::ScenarioHazardExposureResource>();
    SC_EXPECT_TRUE(exposure.hazardsById.at("combined-fire").exposedAgentSeconds > 0.0);
    SC_EXPECT_TRUE(exposure.hazardsById.at("combined-smoke").exposedAgentSeconds > 0.0);
    SC_EXPECT_EQ(exposure.hazardsById.at("combined-fire").peakExposedAgentCount, std::size_t{1});
    SC_EXPECT_EQ(exposure.hazardsById.at("combined-smoke").peakExposedAgentCount, std::size_t{1});
}

SC_TEST(ScenarioRiskMetricsSystem_PublishesStalledHotspotAndBottleneckMetrics) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (int index = 0; index < 8; ++index) {
        seeds.push_back({
            .position = {.value = {.x = 0.75 + (static_cast<double>(index) * 0.03), .y = 0.0}},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 1.0, .y = 0.0}},
                .waypointPassages = {{{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = {.x = 0.75, .y = 0.0},
                .previousDistanceToWaypoint = 0.25,
                .stalledSeconds = 1.0,
                .destinationZoneId = "exit",
            },
            .status = {},
        });
    }

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 17,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& snapshot =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
    SC_EXPECT_EQ(snapshot.stalledAgentCount, std::size_t{8});
    SC_EXPECT_TRUE(!snapshot.hotspots.empty());
    SC_EXPECT_NEAR(snapshot.hotspots.front().cellMin.x, 0.0, 1e-9);
    SC_EXPECT_NEAR(snapshot.hotspots.front().cellMin.y, 0.0, 1e-9);
    SC_EXPECT_NEAR(snapshot.hotspots.front().cellMax.x, 1.5, 1e-9);
    SC_EXPECT_NEAR(snapshot.hotspots.front().cellMax.y, 1.5, 1e-9);
    SC_EXPECT_TRUE(!snapshot.pressureHotspots.empty());
    SC_EXPECT_EQ(snapshot.pressureHotspots.front().agentCount, std::size_t{8});
    SC_EXPECT_TRUE(snapshot.pressureHotspots.front().intrudingPairCount > 0);
    SC_EXPECT_TRUE(snapshot.pressureHotspots.front().pressureScore >= 1.0);
    SC_EXPECT_TRUE(!snapshot.bottlenecks.empty());
    SC_EXPECT_EQ(snapshot.bottlenecks.front().label, std::string{"Room -> Exit"});
}

SC_TEST(ScenarioRiskMetricsSystem_DoesNotPublishPressureHotspotsForLooseClusterInSameCell) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (const auto& point : std::vector<safecrowd::domain::Point2D>{
             {0.10, 0.10},
             {1.30, 0.10},
             {0.10, 1.30},
             {1.30, 1.30},
             {0.75, 0.75},
         }) {
        seeds.push_back({
            .position = {.value = point},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 1.0, .y = 0.0}},
                .waypointPassages = {{{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = point,
                .previousDistanceToWaypoint = 0.25,
                .stalledSeconds = 1.0,
                .destinationZoneId = "exit",
            },
            .status = {},
        });
    }

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 37,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& snapshot =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
    SC_EXPECT_TRUE(snapshot.hotspots.empty());
    SC_EXPECT_TRUE(snapshot.pressureHotspots.empty());
}

SC_TEST(ScenarioRiskMetricsSystem_SeparatesDensityHotspotFromPersonalSpacePressure) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 53,
    });
    runtime.addSystem(std::make_unique<ConfigureDensePersonalSpaceAgentsSystem>());
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& snapshot =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
    SC_EXPECT_TRUE(!snapshot.hotspots.empty());
    SC_EXPECT_TRUE(snapshot.pressureHotspots.empty());
    SC_EXPECT_TRUE(snapshot.pressureAgents.empty());
    SC_EXPECT_EQ(snapshot.pressureExposedAgentCount, std::size_t{0});
    SC_EXPECT_EQ(snapshot.criticalPressureAgentCount, std::size_t{0});
}

SC_TEST(ScenarioRiskMetricsSystem_AccumulatesCompressionExposureAndCriticalPressureAgents) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 38,
    });
    runtime.addSystem(std::make_unique<ConfigureDenseActiveAgentsSystem>());
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);
    clock.elapsedSeconds = 3.0;
    runtime.stepFrame(0.0);

    const auto& snapshot =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
    SC_EXPECT_TRUE(snapshot.pressureExposedAgentCount > 0);
    SC_EXPECT_TRUE(snapshot.criticalPressureAgentCount > 0);
    SC_EXPECT_TRUE(!snapshot.pressureAgents.empty());
    SC_EXPECT_TRUE(snapshot.pressureAgents.front().critical);
    SC_EXPECT_TRUE(snapshot.pressureAgents.front().compressionForce >= 1.0);
    SC_EXPECT_TRUE(snapshot.pressureAgents.front().exposureSeconds >= 2.0);
}

SC_TEST(ScenarioRiskMetricsSystem_PublishesCriticalPressureEventAfterSustainedExposure) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 39,
    });
    runtime.addSystem(std::make_unique<ConfigureDenseActiveAgentsSystem>());
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);
    {
        const auto& snapshot =
            runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
        SC_EXPECT_TRUE(snapshot.criticalPressureEvents.empty());
    }

    clock.elapsedSeconds = 3.0;
    runtime.stepFrame(0.0);

    const auto& snapshot =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
    SC_EXPECT_TRUE(snapshot.criticalPressureAgentCount >= 2);
    SC_EXPECT_TRUE(!snapshot.criticalPressureEvents.empty());
    SC_EXPECT_TRUE(snapshot.criticalPressureEvents.front().criticalAgentCount >= 2);
    SC_EXPECT_TRUE(snapshot.criticalPressureEvents.front().durationSeconds >= 1.0);
    SC_EXPECT_TRUE(snapshot.criticalPressureEvents.front().pressureScore > 0.0);
}

SC_TEST(ScenarioRiskMetricsSystem_DoesNotMergeHotspotsAcrossFloors) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 34,
    });
    runtime.addSystem(std::make_unique<ConfigureSplitFloorHotspotAgentsSystem>());
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(overlappingFloorBottleneckLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& snapshot =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
    SC_EXPECT_TRUE(snapshot.hotspots.empty());
    SC_EXPECT_EQ(snapshot.pressureHotspots.size(), std::size_t{2});
    const auto hasL1PressureHotspot = std::any_of(
        snapshot.pressureHotspots.begin(),
        snapshot.pressureHotspots.end(),
        [](const auto& hotspot) {
            return hotspot.floorId == "L1" && hotspot.agentCount == 3;
        });
    const auto hasL2PressureHotspot = std::any_of(
        snapshot.pressureHotspots.begin(),
        snapshot.pressureHotspots.end(),
        [](const auto& hotspot) {
            return hotspot.floorId == "L2" && hotspot.agentCount == 3;
        });
    SC_EXPECT_TRUE(hasL1PressureHotspot);
    SC_EXPECT_TRUE(hasL2PressureHotspot);
}

SC_TEST(ScenarioRiskMetricsSystem_FiltersBottlenecksByConnectionFloor) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 35,
    });
    runtime.addSystem(std::make_unique<ConfigureSecondFloorBottleneckAgentsSystem>());
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(overlappingFloorBottleneckLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& snapshot =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
    SC_EXPECT_EQ(snapshot.bottlenecks.size(), std::size_t{1});
    SC_EXPECT_EQ(snapshot.bottlenecks.front().connectionId, std::string{"door-l2"});
    SC_EXPECT_EQ(snapshot.bottlenecks.front().floorId, std::string{"L2"});
}

SC_TEST(ScenarioRiskMetricsSystem_UsesDisplayFloorForVirtualPhysicsBuckets) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (int index = 0; index < 8; ++index) {
        seeds.push_back({
            .position = {.value = {.x = 0.75 + (static_cast<double>(index) * 0.03), .y = 0.0}},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 1.0, .y = 0.0}},
                .waypointPassages = {{{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}},
                .waypointFromZoneIds = {"room-l2"},
                .waypointZoneIds = {"exit-l2"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = {.x = 0.75, .y = 0.0},
                .previousDistanceToWaypoint = 0.25,
                .stalledSeconds = 1.0,
                .destinationZoneId = "exit-l2",
                .currentFloorId = "L2",
                .displayFloorId = "L2",
                .physicsFloorId = "vertical:stair-vertical",
            },
            .status = {},
        });
    }

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 36,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(overlappingFloorBottleneckLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& snapshot =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().snapshot;
    SC_EXPECT_TRUE(!snapshot.hotspots.empty());
    SC_EXPECT_EQ(snapshot.hotspots.front().floorId, std::string{"L2"});
    SC_EXPECT_TRUE(!snapshot.pressureHotspots.empty());
    SC_EXPECT_EQ(snapshot.pressureHotspots.front().floorId, std::string{"L2"});
    SC_EXPECT_TRUE(!snapshot.bottlenecks.empty());
    SC_EXPECT_EQ(snapshot.bottlenecks.front().floorId, std::string{"L2"});
}

SC_TEST(ScenarioRiskMetricsSystem_PreservesPeakMetricsAfterAllAgentsEvacuate) {
    std::vector<safecrowd::domain::ScenarioAgentSeed> seeds;
    for (int index = 0; index < 8; ++index) {
        seeds.push_back({
            .position = {.value = {.x = 0.75 + (static_cast<double>(index) * 0.03), .y = 0.0}},
            .agent = {.radius = 0.25f, .maxSpeed = 1.0f},
            .velocity = {.value = {}},
            .route = {
                .waypoints = {{.x = 1.0, .y = 0.0}},
                .waypointPassages = {{{.x = 1.0, .y = -0.4}, {.x = 1.0, .y = 0.4}}},
                .waypointFromZoneIds = {"room"},
                .waypointZoneIds = {"exit"},
                .nextWaypointIndex = 0,
                .currentSegmentStart = {.x = 0.75, .y = 0.0},
                .previousDistanceToWaypoint = 0.25,
                .stalledSeconds = 1.0,
                .destinationZoneId = "exit",
            },
            .status = {},
        });
    }

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 19,
    });
    runtime.addSystem(std::make_unique<safecrowd::domain::ScenarioAgentSpawnSystem>(std::move(seeds), 10.0));
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& query = runtime.world().query();
    for (const auto entity : query.view<safecrowd::domain::EvacuationStatus>()) {
        query.get<safecrowd::domain::EvacuationStatus>(entity).evacuated = true;
    }

    runtime.stepFrame(0.0);

    const auto& metrics =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>();
    SC_EXPECT_TRUE(metrics.snapshot.hotspots.empty());
    SC_EXPECT_TRUE(metrics.snapshot.pressureHotspots.empty());
    SC_EXPECT_TRUE(metrics.snapshot.bottlenecks.empty());
    SC_EXPECT_TRUE(!metrics.peakSnapshot.hotspots.empty());
    SC_EXPECT_TRUE(!metrics.peakSnapshot.pressureHotspots.empty());
    SC_EXPECT_TRUE(!metrics.peakSnapshot.bottlenecks.empty());
    SC_EXPECT_EQ(metrics.peakSnapshot.stalledAgentCount, std::size_t{8});
}

SC_TEST(ScenarioRiskMetricsSystem_TracksPeakHotspotTimePerCell) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 56,
    });
    runtime.addSystem(
        std::make_unique<ConfigureStagedHotspotAgentsSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 0,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);

    const auto& hotspots =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().peakSnapshot.hotspots;
    const auto initialIt = std::find_if(hotspots.begin(), hotspots.end(), [](const auto& hotspot) {
        return std::abs(hotspot.cellMin.x - 0.0) <= 1e-9 && std::abs(hotspot.cellMin.y - 0.0) <= 1e-9;
    });
    const auto delayedIt = std::find_if(hotspots.begin(), hotspots.end(), [](const auto& hotspot) {
        return std::abs(hotspot.cellMin.x - 3.0) <= 1e-9 && std::abs(hotspot.cellMin.y - 0.0) <= 1e-9;
    });

    SC_EXPECT_TRUE(initialIt != hotspots.end());
    SC_EXPECT_TRUE(delayedIt != hotspots.end());
    SC_EXPECT_TRUE(initialIt->detectedAtSeconds.has_value());
    SC_EXPECT_TRUE(delayedIt->detectedAtSeconds.has_value());
    SC_EXPECT_NEAR(*initialIt->detectedAtSeconds, 1.0, 1e-9);
    SC_EXPECT_NEAR(*delayedIt->detectedAtSeconds, 2.0, 1e-9);
    SC_EXPECT_TRUE(initialIt->detectionFrame.has_value());
    SC_EXPECT_TRUE(delayedIt->detectionFrame.has_value());
    SC_EXPECT_NEAR(initialIt->detectionFrame->elapsedSeconds, 1.0, 1e-9);
    SC_EXPECT_NEAR(delayedIt->detectionFrame->elapsedSeconds, 2.0, 1e-9);
}

SC_TEST(ScenarioRiskMetricsSystem_TracksPeakBottleneckTimePerConnection) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 55,
    });
    runtime.addSystem(
        std::make_unique<ConfigureStagedBottleneckAgentsSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 0,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(sameExitTwoDoorGuidanceLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);

    const auto& peakBottlenecks =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().peakSnapshot.bottlenecks;
    const auto lowerIt = std::find_if(
        peakBottlenecks.begin(),
        peakBottlenecks.end(),
        [](const auto& bottleneck) {
            return bottleneck.connectionId == "room-exit-lower";
        });
    const auto upperIt = std::find_if(
        peakBottlenecks.begin(),
        peakBottlenecks.end(),
        [](const auto& bottleneck) {
            return bottleneck.connectionId == "room-exit-upper";
        });

    SC_EXPECT_TRUE(lowerIt != peakBottlenecks.end());
    SC_EXPECT_TRUE(upperIt != peakBottlenecks.end());
    SC_EXPECT_EQ(lowerIt->nearbyAgentCount, std::size_t{7});
    SC_EXPECT_EQ(upperIt->nearbyAgentCount, std::size_t{8});
    SC_EXPECT_TRUE(lowerIt->detectedAtSeconds.has_value());
    SC_EXPECT_TRUE(upperIt->detectedAtSeconds.has_value());
    SC_EXPECT_NEAR(*lowerIt->detectedAtSeconds, 1.0, 1e-9);
    SC_EXPECT_NEAR(*upperIt->detectedAtSeconds, 2.0, 1e-9);
}

SC_TEST(ScenarioRiskMetricsSystem_TracksPeakCrossFlowTimePerCell) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 57,
    });
    runtime.addSystem(
        std::make_unique<ConfigureStagedCrossFlowAgentsSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 0,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);

    const auto& cells =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>().peakSnapshot.crossFlowCells;
    const auto initialIt = std::find_if(cells.begin(), cells.end(), [](const auto& cell) {
        return std::abs(cell.cellMin.x - 0.0) <= 1e-9 && std::abs(cell.cellMin.y - 0.0) <= 1e-9;
    });
    const auto delayedIt = std::find_if(cells.begin(), cells.end(), [](const auto& cell) {
        return std::abs(cell.cellMin.x - 2.0) <= 1e-9 && std::abs(cell.cellMin.y - 0.0) <= 1e-9;
    });

    SC_EXPECT_TRUE(initialIt != cells.end());
    SC_EXPECT_TRUE(delayedIt != cells.end());
    SC_EXPECT_TRUE(initialIt->detectedAtSeconds.has_value());
    SC_EXPECT_TRUE(delayedIt->detectedAtSeconds.has_value());
    SC_EXPECT_NEAR(*initialIt->detectedAtSeconds, 1.0, 1e-9);
    SC_EXPECT_NEAR(*delayedIt->detectedAtSeconds, 2.0, 1e-9);
    SC_EXPECT_TRUE(initialIt->detectionFrame.has_value());
    SC_EXPECT_TRUE(delayedIt->detectionFrame.has_value());
    SC_EXPECT_NEAR(initialIt->detectionFrame->elapsedSeconds, 1.0, 1e-9);
    SC_EXPECT_NEAR(delayedIt->detectionFrame->elapsedSeconds, 2.0, 1e-9);
}

SC_TEST(ScenarioRiskMetricsSystem_PreservesPeakCriticalPressureMetricsAfterAgentsEvacuate) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 40,
    });
    runtime.addSystem(std::make_unique<ConfigureDenseActiveAgentsSystem>());
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);
    clock.elapsedSeconds = 3.0;
    runtime.stepFrame(0.0);

    auto& query = runtime.world().query();
    for (const auto entity : query.view<safecrowd::domain::EvacuationStatus>()) {
        query.get<safecrowd::domain::EvacuationStatus>(entity).evacuated = true;
    }
    clock.elapsedSeconds = 4.0;
    runtime.stepFrame(0.0);

    const auto& metrics =
        runtime.world().resources().get<safecrowd::domain::ScenarioRiskMetricsResource>();
    SC_EXPECT_EQ(metrics.snapshot.pressureExposedAgentCount, std::size_t{0});
    SC_EXPECT_EQ(metrics.snapshot.criticalPressureAgentCount, std::size_t{0});
    SC_EXPECT_TRUE(metrics.snapshot.pressureAgents.empty());
    SC_EXPECT_TRUE(metrics.snapshot.criticalPressureEvents.empty());
    SC_EXPECT_TRUE(metrics.peakSnapshot.pressureExposedAgentCount > 0);
    SC_EXPECT_TRUE(metrics.peakSnapshot.criticalPressureAgentCount > 0);
    SC_EXPECT_TRUE(!metrics.peakSnapshot.pressureAgents.empty());
    SC_EXPECT_TRUE(!metrics.peakSnapshot.criticalPressureEvents.empty());
}

SC_TEST(ScenarioResultArtifactsSystem_PublishesEvacuationCurveAndPercentiles) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 23,
    });
    runtime.addSystem(std::make_unique<ConfigureEvacuatedAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& artifacts =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts;
    SC_EXPECT_EQ(artifacts.evacuationProgress.size(), std::size_t{1});
    SC_EXPECT_NEAR(artifacts.evacuationProgress.front().timeSeconds, 7.0, 1e-9);
    SC_EXPECT_EQ(artifacts.evacuationProgress.front().evacuatedCount, std::size_t{3});
    SC_EXPECT_EQ(artifacts.evacuationProgress.front().totalCount, std::size_t{3});
    SC_EXPECT_NEAR(artifacts.evacuationProgress.front().evacuatedRatio, 1.0, 1e-9);
    SC_EXPECT_TRUE(artifacts.timingSummary.t50Seconds.has_value());
    SC_EXPECT_TRUE(artifacts.timingSummary.t90Seconds.has_value());
    SC_EXPECT_TRUE(artifacts.timingSummary.t95Seconds.has_value());
    SC_EXPECT_TRUE(artifacts.timingSummary.finalEvacuationTimeSeconds.has_value());
    SC_EXPECT_NEAR(*artifacts.timingSummary.t50Seconds, 5.0, 1e-9);
    SC_EXPECT_NEAR(*artifacts.timingSummary.t90Seconds, 7.0, 1e-9);
    SC_EXPECT_NEAR(*artifacts.timingSummary.t95Seconds, 7.0, 1e-9);
    SC_EXPECT_NEAR(*artifacts.timingSummary.finalEvacuationTimeSeconds, 7.0, 1e-9);
    SC_EXPECT_NEAR(artifacts.timingSummary.targetTimeSeconds, 10.0, 1e-9);
    SC_EXPECT_TRUE(artifacts.timingSummary.marginSeconds.has_value());
    SC_EXPECT_NEAR(*artifacts.timingSummary.marginSeconds, 3.0, 1e-9);
    SC_EXPECT_EQ(artifacts.exitUsage.size(), std::size_t{1});
    SC_EXPECT_EQ(artifacts.exitUsage.front().exitZoneId, std::string{"exit-a"});
    SC_EXPECT_EQ(artifacts.exitUsage.front().evacuatedCount, std::size_t{3});
    SC_EXPECT_NEAR(artifacts.exitUsage.front().usageRatio, 1.0, 1e-9);
    SC_EXPECT_EQ(artifacts.zoneCompletion.size(), std::size_t{1});
    SC_EXPECT_EQ(artifacts.zoneCompletion.front().zoneId, std::string{"room-a"});
    SC_EXPECT_EQ(artifacts.zoneCompletion.front().evacuatedCount, std::size_t{3});
    SC_EXPECT_EQ(artifacts.placementCompletion.size(), std::size_t{1});
    SC_EXPECT_EQ(artifacts.placementCompletion.front().placementId, std::string{"group-a"});
}

SC_TEST(ScenarioResultArtifactsSystem_UsesActualExitZoneWhenRecorded) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 44,
    });
    runtime.addSystem(std::make_unique<ConfigureEvacuatedAgentWithActualExitSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& artifacts =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts;
    SC_EXPECT_EQ(artifacts.exitUsage.size(), std::size_t{1});
    SC_EXPECT_EQ(artifacts.exitUsage.front().exitZoneId, std::string{"actual-exit"});
}

SC_TEST(ScenarioResultArtifactsSystem_PublishesDensitySummary) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 24,
    });
    runtime.addSystem(std::make_unique<ConfigureDenseActiveAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);

    const auto& artifacts =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts;
    SC_EXPECT_TRUE(artifacts.densitySummary.peakCell.has_value());
    SC_EXPECT_EQ(artifacts.densitySummary.peakAgentCount, std::size_t{10});
    SC_EXPECT_TRUE(artifacts.densitySummary.peakDensityPeoplePerSquareMeter >= 4.0);
    SC_EXPECT_TRUE(artifacts.densitySummary.highDensityDurationSeconds >= 1.0);
    SC_EXPECT_TRUE(!artifacts.densitySummary.peakCells.empty());
    SC_EXPECT_EQ(artifacts.densitySummary.peakCells.size(), std::size_t{5});
    SC_EXPECT_TRUE(artifacts.densitySummary.peakField.cells.size() > artifacts.densitySummary.peakCells.size());
    SC_EXPECT_NEAR(artifacts.densitySummary.peakField.timeSeconds, 2.0, 1e-9);
    SC_EXPECT_NEAR(artifacts.densitySummary.peakField.cellSizeMeters, artifacts.densitySummary.cellSizeMeters, 1e-9);
}

SC_TEST(ScenarioResultArtifactsSystem_AccumulatesDensityPeakFieldByFloorAndCell) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 36,
    });
    runtime.addSystem(std::make_unique<ConfigureMovingFloorDensityAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    auto& query = runtime.world().query();
    int movedIndex = 0;
    for (const auto entity : query.view<
             safecrowd::domain::Position,
             safecrowd::domain::EvacuationRoute,
             safecrowd::domain::EvacuationStatus>()) {
        auto& position = query.get<safecrowd::domain::Position>(entity);
        auto& route = query.get<safecrowd::domain::EvacuationRoute>(entity);
        position.value = {.x = 3.1 + (0.04 * static_cast<double>(movedIndex)), .y = 0.1};
        route.currentFloorId = "L1";
        route.displayFloorId = "L1";
        ++movedIndex;
    }
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);

    const auto& cells =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts.densitySummary.peakField.cells;
    const auto hasL1Cell = std::any_of(cells.begin(), cells.end(), [](const auto& cell) {
        return cell.floorId == "L1";
    });
    const auto hasL2Cell = std::any_of(cells.begin(), cells.end(), [](const auto& cell) {
        return cell.floorId == "L2";
    });
    SC_EXPECT_TRUE(hasL1Cell);
    SC_EXPECT_TRUE(hasL2Cell);
}

SC_TEST(ScenarioResultArtifactsSystem_PublishesPressureSummary) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 41,
    });
    runtime.addSystem(std::make_unique<ConfigureDenseActiveAgentsSystem>());
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 20,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);
    clock.elapsedSeconds = 3.0;
    runtime.stepFrame(0.0);

    const auto& summary =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts.pressureSummary;
    SC_EXPECT_NEAR(summary.cellSizeMeters, safecrowd::domain::kScenarioHotspotCellSize, 1e-9);
    SC_EXPECT_NEAR(summary.hotspotScoreThreshold, safecrowd::domain::kScenarioPressureScoreThreshold, 1e-9);
    SC_EXPECT_NEAR(summary.criticalCompressionForceThreshold, safecrowd::domain::kScenarioCriticalPressureForceThreshold, 1e-9);
    SC_EXPECT_NEAR(summary.criticalExposureThresholdSeconds, safecrowd::domain::kScenarioCriticalPressureExposureThresholdSeconds, 1e-9);
    SC_EXPECT_NEAR(summary.criticalEventDurationThresholdSeconds, safecrowd::domain::kScenarioCriticalPressureEventDurationThresholdSeconds, 1e-9);
    SC_EXPECT_EQ(summary.criticalEventAgentThreshold, safecrowd::domain::kScenarioCriticalPressureEventAgentThreshold);
    SC_EXPECT_TRUE(summary.peakPressureScore > 0.0);
    SC_EXPECT_TRUE(summary.peakAtSeconds.has_value());
    SC_EXPECT_TRUE(summary.peakCell.has_value());
    SC_EXPECT_TRUE(!summary.peakCells.empty());
    SC_EXPECT_TRUE(!summary.peakField.cells.empty());
    SC_EXPECT_TRUE(!summary.peakHotspots.empty());
    SC_EXPECT_TRUE(!summary.peakAgents.empty());
    SC_EXPECT_TRUE(summary.peakAgents.front().critical);
    SC_EXPECT_TRUE(summary.peakExposedAgentCount > 0);
    SC_EXPECT_TRUE(summary.peakCriticalAgentCount > 0);
    SC_EXPECT_TRUE(!summary.criticalEvents.empty());
    SC_EXPECT_TRUE(summary.criticalEvents.front().durationSeconds >= 1.0);
}

SC_TEST(ScenarioResultArtifactsSystem_PressureSummaryIgnoresDensePersonalSpaceGrid) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 54,
    });
    runtime.addSystem(std::make_unique<ConfigureDensePersonalSpaceAgentsSystem>());
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 20,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& artifacts =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts;
    SC_EXPECT_TRUE(artifacts.densitySummary.peakDensityPeoplePerSquareMeter
        >= artifacts.densitySummary.highDensityThresholdPeoplePerSquareMeter);
    SC_EXPECT_TRUE(!artifacts.densitySummary.peakField.cells.empty());
    SC_EXPECT_NEAR(artifacts.pressureSummary.peakPressureScore, 0.0, 1e-9);
    SC_EXPECT_TRUE(!artifacts.pressureSummary.peakAtSeconds.has_value());
    SC_EXPECT_TRUE(artifacts.pressureSummary.peakCells.empty());
    SC_EXPECT_TRUE(artifacts.pressureSummary.peakField.cells.empty());
    SC_EXPECT_TRUE(artifacts.pressureSummary.peakHotspots.empty());
}

SC_TEST(ScenarioResultArtifactsSystem_AccumulatesPressurePeakFieldByFloorAndCell) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 42,
    });
    runtime.addSystem(std::make_unique<ConfigureMovingFloorDensityAgentsSystem>());
    runtime.addSystem(
        safecrowd::domain::makeScenarioRiskMetricsSystem(straightExitLayout()),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 10,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 20,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    auto& query = runtime.world().query();
    int movedIndex = 0;
    for (const auto entity : query.view<
             safecrowd::domain::Position,
             safecrowd::domain::EvacuationRoute,
             safecrowd::domain::EvacuationStatus>()) {
        auto& position = query.get<safecrowd::domain::Position>(entity);
        auto& route = query.get<safecrowd::domain::EvacuationRoute>(entity);
        position.value = {.x = 3.1 + (0.04 * static_cast<double>(movedIndex)), .y = 0.1};
        route.currentFloorId = "L1";
        route.displayFloorId = "L1";
        ++movedIndex;
    }
    auto& clock = runtime.world().resources().get<safecrowd::domain::ScenarioSimulationClockResource>();
    clock.elapsedSeconds = 2.0;
    runtime.stepFrame(0.0);

    const auto& cells =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts.pressureSummary.peakField.cells;
    const auto hasL1Cell = std::any_of(cells.begin(), cells.end(), [](const auto& cell) {
        return cell.floorId == "L1";
    });
    const auto hasL2Cell = std::any_of(cells.begin(), cells.end(), [](const auto& cell) {
        return cell.floorId == "L2";
    });
    SC_EXPECT_TRUE(hasL1Cell);
    SC_EXPECT_TRUE(hasL2Cell);
}

SC_TEST(ScenarioResultArtifactsSystem_PublishesCrossFlowSummary) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 52,
    });
    runtime.addSystem(std::make_unique<ConfigureCrossFlowArtifactsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioResultArtifactsSystem>(1.0),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.0);

    const auto& artifacts =
        runtime.world().resources().get<safecrowd::domain::ScenarioResultArtifactsResource>().artifacts;
    SC_EXPECT_NEAR(artifacts.crossFlowSummary.peakCrossFlowScore, 0.95, 1e-9);
    SC_EXPECT_NEAR(artifacts.crossFlowSummary.totalCrossFlowExposureAgentSeconds, 18.0, 1e-9);
    SC_EXPECT_TRUE(artifacts.crossFlowSummary.peakAtSeconds.has_value());
    SC_EXPECT_NEAR(*artifacts.crossFlowSummary.peakAtSeconds, 12.0, 1e-9);
    SC_EXPECT_NEAR(artifacts.crossFlowSummary.longestCrossFlowDurationSeconds, 11.0, 1e-9);
    SC_EXPECT_EQ(artifacts.crossFlowSummary.crossFlowHotspotCount, std::size_t{1});
    SC_EXPECT_EQ(artifacts.crossFlowTimeline.size(), std::size_t{1});
    SC_EXPECT_NEAR(artifacts.crossFlowTimeline.front().peakCrossFlowScore, 0.95, 1e-9);
    SC_EXPECT_EQ(artifacts.crossFlowTimeline.front().activeCrossFlowCellCount, std::size_t{1});
}

SC_TEST(ScenarioResultArtifacts_FrameDensitySnapshotGroupsAgentsByFloorAndCell) {
    const safecrowd::domain::SimulationFrame frame{
        .elapsedSeconds = 3.5,
        .agents = {
            {.id = 1, .position = {.x = 0.1, .y = 0.1}, .radius = 0.25, .floorId = "L1"},
            {.id = 2, .position = {.x = 0.3, .y = 0.1}, .radius = 0.25, .floorId = "L1"},
            {.id = 3, .position = {.x = 2.1, .y = 0.1}, .radius = 0.25, .floorId = "L1"},
            {.id = 4, .position = {.x = 0.1, .y = 0.1}, .radius = 0.25, .floorId = "L2"},
        },
    };

    const auto snapshot = safecrowd::domain::densityFieldSnapshotFromFrame(frame, 1.0);

    SC_EXPECT_NEAR(snapshot.timeSeconds, 3.5, 1e-9);
    SC_EXPECT_NEAR(snapshot.cellSizeMeters, 1.0, 1e-9);
    SC_EXPECT_EQ(snapshot.cells.size(), std::size_t{3});
    SC_EXPECT_EQ(snapshot.cells.front().floorId, std::string{"L1"});
    SC_EXPECT_EQ(snapshot.cells.front().agentCount, std::size_t{2});
    SC_EXPECT_NEAR(snapshot.cells.front().densityPeoplePerSquareMeter, 2.0, 1e-9);
    SC_EXPECT_NEAR(snapshot.cells.front().center.x, 0.2, 1e-9);
    SC_EXPECT_NEAR(snapshot.cells.front().center.y, 0.1, 1e-9);
}

SC_TEST(ScenarioResultArtifacts_FramePressureSnapshotFiltersNonIntrudingCells) {
    const safecrowd::domain::SimulationFrame frame{
        .elapsedSeconds = 4.0,
        .agents = {
            {.id = 1, .position = {.x = 0.1, .y = 0.1}, .radius = 0.25, .floorId = "L1"},
            {.id = 2, .position = {.x = 0.3, .y = 0.1}, .radius = 0.25, .floorId = "L1"},
            {.id = 3, .position = {.x = 2.1, .y = 0.1}, .radius = 0.25, .floorId = "L1"},
            {.id = 4, .position = {.x = 0.1, .y = 0.1}, .radius = 0.25, .floorId = "L2"},
        },
    };

    const auto snapshot = safecrowd::domain::pressureFieldSnapshotFromFrame(frame, 1.0);

    SC_EXPECT_NEAR(snapshot.timeSeconds, 4.0, 1e-9);
    SC_EXPECT_NEAR(snapshot.cellSizeMeters, 1.0, 1e-9);
    SC_EXPECT_EQ(snapshot.cells.size(), std::size_t{1});
    SC_EXPECT_EQ(snapshot.cells.front().floorId, std::string{"L1"});
    SC_EXPECT_EQ(snapshot.cells.front().agentCount, std::size_t{2});
    SC_EXPECT_EQ(snapshot.cells.front().intrudingPairCount, std::size_t{1});
    SC_EXPECT_TRUE(snapshot.cells.front().pressureScore > 0.0);
}

SC_TEST(ScenarioRoutePassageCrossed_UsesDoorPlaneNearEndpoint) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Room",
        .area = {.outline = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "passage",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Passage",
        .area = {.outline = {{1.0, 0.2}, {2.0, 0.2}, {2.0, 4.0}, {1.0, 4.0}}},
    });

    safecrowd::domain::EvacuationRoute route;
    route.waypoints = {{.x = 1.0, .y = 0.9}};
    route.waypointPassages = {{{.x = 1.0, .y = 0.8}, {.x = 1.0, .y = 1.0}}};
    route.waypointFromZoneIds = {"room"};
    route.waypointZoneIds = {"passage"};
    route.nextWaypointIndex = 0;

    const safecrowd::domain::Point2D crossedNearEndpoint{.x = 1.05, .y = 0.75};
    SC_EXPECT_TRUE(safecrowd::domain::simulation_internal::routePassageCrossed(
        layout,
        route,
        crossedNearEndpoint,
        0.25));
}

SC_TEST(ScenarioRoutePassageCrossed_DoesNotAdvanceWhileCenterRemainsInSourceRoom) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.zones.push_back({
        .id = "upper-room",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Upper Room",
        .area = {.outline = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}},
    });
    layout.zones.push_back({
        .id = "corridor",
        .kind = safecrowd::domain::ZoneKind::Room,
        .label = "Corridor",
        .area = {.outline = {{0.0, 1.0}, {1.0, 1.0}, {1.0, 2.0}, {0.0, 2.0}}},
    });

    safecrowd::domain::EvacuationRoute route;
    route.waypoints = {{.x = 0.5, .y = 1.0}};
    route.waypointPassages = {{{.x = 0.2, .y = 1.0}, {.x = 0.8, .y = 1.0}}};
    route.waypointFromZoneIds = {"upper-room"};
    route.waypointZoneIds = {"corridor"};
    route.nextWaypointIndex = 0;

    const safecrowd::domain::Point2D stillInsideSource{.x = 0.5, .y = 0.93};
    SC_EXPECT_TRUE(!safecrowd::domain::simulation_internal::routePassageCrossed(
        layout,
        route,
        stillInsideSource,
        0.25));

    const safecrowd::domain::Point2D onDoorPlane{.x = 0.5, .y = 1.0};
    SC_EXPECT_TRUE(!safecrowd::domain::simulation_internal::routePassageCrossed(
        layout,
        route,
        onDoorPlane,
        0.25));

    const safecrowd::domain::Point2D crossedIntoCorridor{.x = 0.5, .y = 1.03};
    SC_EXPECT_TRUE(safecrowd::domain::simulation_internal::routePassageCrossed(
        layout,
        route,
        crossedIntoCorridor,
        0.25));
}

SC_TEST(ScenarioFrameSyncSystem_PublishesSimulationFrameResource) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 1.0 / 30.0,
        .maxCatchUpSteps = 1,
        .baseSeed = 5,
    });
    runtime.addSystem(std::make_unique<ConfigureScenarioAgentsSystem>());
    runtime.addSystem(
        std::make_unique<safecrowd::domain::ScenarioFrameSyncSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(1.0 / 30.0);

    auto& resources = runtime.world().resources();
    SC_EXPECT_TRUE(resources.contains<safecrowd::domain::ScenarioSimulationFrameResource>());
    const auto& frame = resources.get<safecrowd::domain::ScenarioSimulationFrameResource>().frame;
    SC_EXPECT_NEAR(frame.elapsedSeconds, 1.5, 1e-9);
    SC_EXPECT_EQ(frame.totalAgentCount, std::size_t{2});
    SC_EXPECT_EQ(frame.evacuatedAgentCount, std::size_t{1});
    SC_EXPECT_EQ(frame.agents.size(), std::size_t{1});
    SC_EXPECT_NEAR(frame.agents.front().velocity.x, 0.5, 1e-9);
}
