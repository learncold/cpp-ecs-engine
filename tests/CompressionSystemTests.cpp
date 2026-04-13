#include "TestSupport.h"

#include <vector>

#include "domain/AgentComponents.h"
#include "domain/CompressionSystem.h"
#include "domain/FacilityLayout2D.h"
#include "domain/Metrics.h"
#include "engine/CommandBuffer.h"
#include "engine/EcsCore.h"
#include "engine/ResourceStore.h"
#include "engine/internal/EngineWorldFactory.h"

namespace {

using safecrowd::domain::Agent;
using safecrowd::domain::Barrier2D;
using safecrowd::domain::CompressionData;
using safecrowd::domain::CompressionSystem;
using safecrowd::domain::Point2D;
using safecrowd::domain::Position;
using safecrowd::engine::CommandBuffer;
using safecrowd::engine::EcsCore;
using safecrowd::engine::Entity;

Entity addAgent(EcsCore& core, double x, double y) {
    const Entity entity = core.createEntity();
    core.addComponent(entity, Position{Point2D{x, y}});
    core.addComponent(entity, Agent{});
    core.addComponent(entity, CompressionData{});
    return entity;
}

void addBarrier(EcsCore& core,
                std::vector<Point2D> vertices,
                bool closed = false,
                bool blocksMovement = true) {
    Barrier2D barrier;
    barrier.geometry.vertices = std::move(vertices);
    barrier.geometry.closed = closed;
    barrier.blocksMovement = blocksMovement;

    const Entity entity = core.createEntity();
    core.addComponent(entity, std::move(barrier));
}

}  // namespace

SC_TEST(CompressionSystem_UpdatesAgentOverlapWithoutBarrierEntitiesAndPreservesExposure) {
    EcsCore core;
    safecrowd::engine::ResourceStore resources;
    CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, resources, buffer);

    const Entity first = addAgent(core, 0.0, 0.0);
    const Entity second = addAgent(core, 0.0, 0.0);
    const Entity third = addAgent(core, 0.0, 0.0);

    CompressionSystem system(0.5);
    system.update(world, {});

    const auto& clusteredMetrics = world.query().get<CompressionData>(first);
    SC_EXPECT_TRUE(clusteredMetrics.force > 0.5f);
    SC_EXPECT_NEAR(clusteredMetrics.exposure, 0.5, 1e-6);
    SC_EXPECT_TRUE(!clusteredMetrics.isCritical);

    world.query().get<Position>(second).value = Point2D{10.0, 0.0};
    world.query().get<Position>(third).value = Point2D{-10.0, 0.0};
    system.update(world, {});

    const auto& separatedMetrics = world.query().get<CompressionData>(first);
    SC_EXPECT_NEAR(separatedMetrics.force, 0.0, 1e-6);
    SC_EXPECT_NEAR(separatedMetrics.exposure, 0.5, 1e-6);
    SC_EXPECT_TRUE(!separatedMetrics.isCritical);
}

SC_TEST(CompressionSystem_CombinesExposureWithCurrentForceForCriticalState) {
    EcsCore core;
    safecrowd::engine::ResourceStore resources;
    CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, resources, buffer);

    const Entity first = addAgent(core, 0.0, 0.0);
    addAgent(core, 0.0, 0.0);
    addAgent(core, 0.0, 0.0);
    addBarrier(core, {Point2D{-0.1, -1.0}, Point2D{-0.1, 1.0}});

    CompressionSystem system(1.0);
    system.update(world, {});
    system.update(world, {});

    const auto& highRiskMetrics = world.query().get<CompressionData>(first);
    SC_EXPECT_TRUE(highRiskMetrics.force > 0.5f);
    SC_EXPECT_NEAR(highRiskMetrics.exposure, 2.0, 1e-6);
    SC_EXPECT_TRUE(highRiskMetrics.isCritical);

    world.query().get<Position>(first).value = Point2D{10.0, 0.0};
    system.update(world, {});

    const auto& recoveredMetrics = world.query().get<CompressionData>(first);
    SC_EXPECT_NEAR(recoveredMetrics.exposure, 2.0, 1e-6);
    SC_EXPECT_TRUE(!recoveredMetrics.isCritical);
}
