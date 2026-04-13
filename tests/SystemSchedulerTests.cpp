#include "TestSupport.h"

#include <cstddef>
#include <exception>
#include <memory>
#include <vector>

#include "engine/CommandBuffer.h"
#include "engine/EcsCore.h"
#include "engine/ResourceStore.h"
#include "engine/SystemScheduler.h"
#include "engine/internal/EngineWorldFactory.h"

namespace {

class RecordingSystem : public safecrowd::engine::EngineSystem {
public:
    std::vector<int>& log;
    int id;
    explicit RecordingSystem(std::vector<int>& l, int id) : log(l), id(id) {}
    void update(safecrowd::engine::EngineWorld&,
                const safecrowd::engine::EngineStepContext&) override {
        log.push_back(id);
    }
};

struct Tag {};

class SpawnTagSystem : public safecrowd::engine::EngineSystem {
public:
    void update(safecrowd::engine::EngineWorld& world,
                const safecrowd::engine::EngineStepContext&) override {
        world.commands().spawnEntity(Tag{});
    }
};

class ConfigureSpawnTagSystem : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.commands().spawnEntity(Tag{});
    }

    void update(safecrowd::engine::EngineWorld&,
                const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureObserveTagSystem : public safecrowd::engine::EngineSystem {
public:
    std::size_t& count;

    explicit ConfigureObserveTagSystem(std::size_t& c) : count(c) {}

    void configure(safecrowd::engine::EngineWorld& world) override {
        count = world.query().view<Tag>().size();
    }

    void update(safecrowd::engine::EngineWorld&,
                const safecrowd::engine::EngineStepContext&) override {
    }
};

}  // namespace

SC_TEST(SystemScheduler_ExecutesSystemsInPhaseOrder) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EcsCore dummyCore;
    safecrowd::engine::ResourceStore dummyResources;
    safecrowd::engine::CommandBuffer dummyBuffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(
        dummyCore, dummyResources, dummyBuffer);

    std::vector<int> log;
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 1),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 0,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 2),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .order = 0,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    const safecrowd::engine::EngineStepContext ctx{};
    scheduler.executePhase(
        safecrowd::engine::UpdatePhase::PreSimulation,
        world,
        ctx);
    scheduler.executePhase(
        safecrowd::engine::UpdatePhase::PostSimulation,
        world,
        ctx);

    SC_EXPECT_EQ(log.size(), std::size_t{2});
    SC_EXPECT_EQ(log[0], 2);
    SC_EXPECT_EQ(log[1], 1);
}

SC_TEST(SystemScheduler_ExecutesSystemsInOrderWithinPhase) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EcsCore dummyCore;
    safecrowd::engine::ResourceStore dummyResources;
    safecrowd::engine::CommandBuffer dummyBuffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(
        dummyCore, dummyResources, dummyBuffer);

    std::vector<int> log;
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 10),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation, .order = 1});
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 20),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation, .order = 0});

    const safecrowd::engine::EngineStepContext ctx{};
    scheduler.executePhase(
        safecrowd::engine::UpdatePhase::FixedSimulation,
        world,
        ctx);

    SC_EXPECT_EQ(log.size(), std::size_t{2});
    SC_EXPECT_EQ(log[0], 20);
    SC_EXPECT_EQ(log[1], 10);
}

SC_TEST(SystemScheduler_PhaseIsolation_OtherPhaseSystemsNotExecuted) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EcsCore dummyCore;
    safecrowd::engine::ResourceStore dummyResources;
    safecrowd::engine::CommandBuffer dummyBuffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(
        dummyCore, dummyResources, dummyBuffer);

    std::vector<int> log;
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 1),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .order = 0,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    const safecrowd::engine::EngineStepContext ctx{};
    scheduler.executePhase(
        safecrowd::engine::UpdatePhase::FixedSimulation,
        world,
        ctx);

    SC_EXPECT_EQ(log.size(), std::size_t{0});
}

SC_TEST(SystemScheduler_ConfigureFlushesCommandsBetweenSystems) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::ResourceStore resources;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(
        core, resources, buffer);

    std::size_t configuredCount = 0;
    scheduler.registerSystem(std::make_unique<ConfigureSpawnTagSystem>(), {});
    scheduler.registerSystem(std::make_unique<ConfigureObserveTagSystem>(configuredCount), {});

    scheduler.configure(world);

    SC_EXPECT_EQ(configuredCount, std::size_t{1});
    SC_EXPECT_EQ(world.query().view<Tag>().size(), std::size_t{1});
}

SC_TEST(SystemScheduler_FlushesCommandBufferAfterPhase) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::ResourceStore resources;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    auto world = safecrowd::engine::internal::EngineWorldFactory::create(
        core, resources, buffer);

    scheduler.registerSystem(
        std::make_unique<SpawnTagSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation, .order = 0});

    const safecrowd::engine::EngineStepContext ctx{};
    scheduler.executePhase(
        safecrowd::engine::UpdatePhase::FixedSimulation,
        world,
        ctx);

    const auto entities = world.query().view<Tag>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
}

SC_TEST(SystemScheduler_IntervalSystemsUseFrameCadenceDeterministically) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EcsCore dummyCore;
    safecrowd::engine::ResourceStore dummyResources;
    safecrowd::engine::CommandBuffer dummyBuffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(
        dummyCore, dummyResources, dummyBuffer);

    std::vector<int> log;
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 1),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::Interval,
         .intervalTicks = 2});

    const safecrowd::engine::EngineStepContext ctx{};
    for (int i = 0; i < 5; ++i) {
        scheduler.executePhase(
            safecrowd::engine::UpdatePhase::PreSimulation,
            world,
            ctx);
    }

    SC_EXPECT_EQ(log.size(), std::size_t{3});
}

SC_TEST(SystemScheduler_IntervalSystemsUseFixedStepCadenceDeterministically) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EcsCore dummyCore;
    safecrowd::engine::ResourceStore dummyResources;
    safecrowd::engine::CommandBuffer dummyBuffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(
        dummyCore, dummyResources, dummyBuffer);

    std::vector<int> log;
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 1),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::Interval,
         .intervalTicks = 3});

    const safecrowd::engine::EngineStepContext ctx{};
    for (int i = 0; i < 5; ++i) {
        scheduler.executePhase(
            safecrowd::engine::UpdatePhase::FixedSimulation,
            world,
            ctx);
    }

    SC_EXPECT_EQ(log.size(), std::size_t{2});
}

SC_TEST(SystemScheduler_ResetCadenceState_RestartsIntervalSequence) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EcsCore dummyCore;
    safecrowd::engine::ResourceStore dummyResources;
    safecrowd::engine::CommandBuffer dummyBuffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(
        dummyCore, dummyResources, dummyBuffer);

    std::vector<int> log;
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 1),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::Interval,
         .intervalTicks = 2});

    const safecrowd::engine::EngineStepContext ctx{};
    scheduler.executePhase(
        safecrowd::engine::UpdatePhase::PreSimulation,
        world,
        ctx);
    scheduler.executePhase(
        safecrowd::engine::UpdatePhase::PreSimulation,
        world,
        ctx);

    scheduler.resetCadenceState();
    scheduler.executePhase(
        safecrowd::engine::UpdatePhase::PreSimulation,
        world,
        ctx);

    SC_EXPECT_EQ(log.size(), std::size_t{2});
}

SC_TEST(SystemScheduler_RegisterSystem_RejectsZeroIntervalTicks) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    std::vector<int> log;
    bool threw = false;
    try {
        scheduler.registerSystem(
            std::make_unique<RecordingSystem>(log, 1),
            {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
             .triggerPolicy = safecrowd::engine::TriggerPolicy::Interval,
             .intervalTicks = 0});
    } catch (const std::exception&) {
        threw = true;
    }

    SC_EXPECT_TRUE(threw);
}
