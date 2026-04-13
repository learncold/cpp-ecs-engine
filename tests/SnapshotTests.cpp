#include "TestSupport.h"

#include <cstdint>

#include "domain/AgentComponents.h"
#include "domain/Metrics.h"
#include "domain/Snapshot.h"
#include "engine/CommandBuffer.h"
#include "engine/EcsCore.h"
#include "engine/internal/EngineWorldFactory.h"

namespace {

using safecrowd::domain::Agent;
using safecrowd::domain::CompressionData;
using safecrowd::domain::Point2D;
using safecrowd::domain::Position;
using safecrowd::engine::CommandBuffer;
using safecrowd::engine::EcsCore;
using safecrowd::engine::Entity;

Entity addAgent(EcsCore& core, double x, double y, bool withMetrics) {
    const Entity entity = core.createEntity();
    core.addComponent(entity, Position{Point2D{x, y}});
    core.addComponent(entity, Agent{});

    if (withMetrics) {
        core.addComponent(entity, CompressionData{});
    }

    return entity;
}

std::uint64_t packEntityId(Entity entity) {
    return (static_cast<std::uint64_t>(entity.generation) << 32U) |
           static_cast<std::uint64_t>(entity.index);
}

}  // namespace

SC_TEST(SimulationSnapshot_OmitsCompressionChannelsWhenMetricsAreIncomplete) {
    EcsCore core;
    CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, buffer);

    addAgent(core, 0.0, 0.0, true);
    addAgent(core, 1.0, 1.0, false);

    const auto snapshot = safecrowd::domain::buildSnapshot(world.query(), 7, 11, 3.5);

    SC_EXPECT_EQ(snapshot.frameIndex, 7ULL);
    SC_EXPECT_EQ(snapshot.fixedStepIndex, 11ULL);
    SC_EXPECT_NEAR(snapshot.simulationTime, 3.5, 1e-9);
    SC_EXPECT_EQ(snapshot.agentCount, 2U);
    SC_EXPECT_EQ(snapshot.agentIds.size(), std::size_t{2});
    SC_EXPECT_EQ(snapshot.positions.size(), std::size_t{2});
    SC_EXPECT_TRUE(snapshot.findScalarChannel(safecrowd::domain::kCompressionForceChannelName) == nullptr);
    SC_EXPECT_TRUE(snapshot.findScalarChannel(safecrowd::domain::kCompressionExposureChannelName) == nullptr);
    SC_EXPECT_TRUE(snapshot.findFlagChannel(safecrowd::domain::kCompressionCriticalChannelName) == nullptr);
}

SC_TEST(SimulationSnapshot_BuildsAlignedCompressionChannelsWhenMetricsExistForAllAgents) {
    EcsCore core;
    CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, buffer);

    const Entity first = addAgent(core, 0.0, 0.0, true);
    const Entity second = addAgent(core, 2.0, 3.0, true);

    world.query().get<CompressionData>(first) = CompressionData{.force = 1.25f, .exposure = 0.75f, .isCritical = true};
    world.query().get<CompressionData>(second) = CompressionData{.force = 0.25f, .exposure = 0.0f, .isCritical = false};

    const auto snapshot = safecrowd::domain::buildSnapshot(world.query(), 2, 5, 1.25);
    const auto* forceChannel = snapshot.findScalarChannel(safecrowd::domain::kCompressionForceChannelName);
    const auto* exposureChannel = snapshot.findScalarChannel(safecrowd::domain::kCompressionExposureChannelName);
    const auto* criticalChannel = snapshot.findFlagChannel(safecrowd::domain::kCompressionCriticalChannelName);

    SC_EXPECT_EQ(snapshot.agentCount, 2U);
    SC_EXPECT_TRUE(forceChannel != nullptr);
    SC_EXPECT_TRUE(exposureChannel != nullptr);
    SC_EXPECT_TRUE(criticalChannel != nullptr);
    SC_EXPECT_EQ(forceChannel->values.size(), std::size_t{2});
    SC_EXPECT_EQ(exposureChannel->values.size(), std::size_t{2});
    SC_EXPECT_EQ(criticalChannel->values.size(), std::size_t{2});
    SC_EXPECT_NEAR(forceChannel->values[0], 1.25, 1e-6);
    SC_EXPECT_NEAR(exposureChannel->values[0], 0.75, 1e-6);
    SC_EXPECT_EQ(criticalChannel->values[0], static_cast<std::uint8_t>(1));
}

SC_TEST(SimulationSnapshot_PacksEntityGenerationIntoStableIds) {
    EcsCore core(1);
    CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, buffer);

    const Entity original = addAgent(core, 0.0, 0.0, false);
    core.destroyEntity(original);
    const Entity recycled = addAgent(core, 5.0, 6.0, false);

    const auto snapshot = safecrowd::domain::buildSnapshot(world.query(), 1, 1, 0.5);

    SC_EXPECT_EQ(original.index, recycled.index);
    SC_EXPECT_TRUE(original.generation != recycled.generation);
    SC_EXPECT_EQ(snapshot.agentCount, 1U);
    SC_EXPECT_EQ(snapshot.agentIds.size(), std::size_t{1});
    SC_EXPECT_EQ(snapshot.agentIds[0], packEntityId(recycled));
    SC_EXPECT_TRUE(snapshot.agentIds[0] != packEntityId(original));
}
