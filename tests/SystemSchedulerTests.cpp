#include "TestSupport.h"

#include <memory>
#include <vector>

#include "engine/CommandBuffer.h"
#include "engine/EcsCore.h"
#include "engine/SystemScheduler.h"

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

}  // namespace

SC_TEST(SystemScheduler_ExecutesSystemsInPhaseOrder) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EcsCore dummyCore;
    safecrowd::engine::CommandBuffer dummyBuffer;
    safecrowd::engine::EngineWorld world{dummyCore, dummyBuffer};

    std::vector<int> log;
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 1),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation, .order = 0});
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 2),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation, .order = 0});

    const safecrowd::engine::EngineStepContext ctx{};
    scheduler.executePhase(safecrowd::engine::UpdatePhase::PreSimulation, world, ctx);
    scheduler.executePhase(safecrowd::engine::UpdatePhase::PostSimulation, world, ctx);

    SC_EXPECT_EQ(log.size(), std::size_t{2});
    SC_EXPECT_EQ(log[0], 2);
    SC_EXPECT_EQ(log[1], 1);
}

SC_TEST(SystemScheduler_ExecutesSystemsInOrderWithinPhase) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EcsCore dummyCore;
    safecrowd::engine::CommandBuffer dummyBuffer;
    safecrowd::engine::EngineWorld world{dummyCore, dummyBuffer};

    std::vector<int> log;
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 10),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation, .order = 1});
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 20),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation, .order = 0});

    const safecrowd::engine::EngineStepContext ctx{};
    scheduler.executePhase(safecrowd::engine::UpdatePhase::FixedSimulation, world, ctx);

    SC_EXPECT_EQ(log.size(), std::size_t{2});
    SC_EXPECT_EQ(log[0], 20);
    SC_EXPECT_EQ(log[1], 10);
}

SC_TEST(SystemScheduler_PhaseIsolation_OtherPhaseSystemsNotExecuted) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EcsCore dummyCore;
    safecrowd::engine::CommandBuffer dummyBuffer;
    safecrowd::engine::EngineWorld world{dummyCore, dummyBuffer};

    std::vector<int> log;
    scheduler.registerSystem(
        std::make_unique<RecordingSystem>(log, 1),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation, .order = 0});

    const safecrowd::engine::EngineStepContext ctx{};
    scheduler.executePhase(safecrowd::engine::UpdatePhase::FixedSimulation, world, ctx);

    SC_EXPECT_EQ(log.size(), std::size_t{0});
}

SC_TEST(SystemScheduler_FlushesCommandBufferAfterPhase) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::SystemScheduler scheduler{core, buffer};

    safecrowd::engine::EngineWorld world{core, buffer};

    scheduler.registerSystem(
        std::make_unique<SpawnTagSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation, .order = 0});

    const safecrowd::engine::EngineStepContext ctx{};
    scheduler.executePhase(safecrowd::engine::UpdatePhase::FixedSimulation, world, ctx);

    const auto entities = safecrowd::engine::WorldQuery{core}.view<Tag>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
}
