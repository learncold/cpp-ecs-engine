#include "TestSupport.h"

#include "engine/CommandBuffer.h"

namespace {

struct Position {
    float x{0.0f};
    float y{0.0f};
};

struct Velocity {
    float vx{0.0f};
    float vy{0.0f};
};

}  // namespace

SC_TEST(CommandBuffer_DestroyEntity_IsDeferred) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;

    const auto e = core.createEntity();
    buffer.destroyEntity(e);

    SC_EXPECT_TRUE(core.isAlive(e));

    buffer.flush(core);

    SC_EXPECT_TRUE(!core.isAlive(e));
}

SC_TEST(CommandBuffer_AddComponent_IsDeferred) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;

    const auto e = core.createEntity();
    buffer.addComponent(e, Position{1.0f, 2.0f});

    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));

    buffer.flush(core);

    SC_EXPECT_TRUE(core.hasComponent<Position>(e));
    SC_EXPECT_NEAR(core.getComponent<Position>(e).x, 1.0f, 1e-6);
}

SC_TEST(CommandBuffer_RemoveComponent_IsDeferred) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;

    const auto e = core.createEntity();
    core.addComponent(e, Position{});
    buffer.removeComponent<Position>(e);

    SC_EXPECT_TRUE(core.hasComponent<Position>(e));

    buffer.flush(core);

    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));
}

SC_TEST(CommandBuffer_Flush_AppliesCommandsInOrder) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;

    const auto e = core.createEntity();
    buffer.addComponent(e, Position{});
    buffer.removeComponent<Position>(e);

    buffer.flush(core);

    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));
}

SC_TEST(CommandBuffer_Flush_ClearsBuffer) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;

    const auto e = core.createEntity();
    buffer.destroyEntity(e);

    SC_EXPECT_TRUE(!buffer.empty());

    buffer.flush(core);

    SC_EXPECT_TRUE(buffer.empty());
}

SC_TEST(CommandBuffer_SpawnEntity_CreatesEntityWithComponents) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;

    buffer.spawnEntity(Position{3.0f, 4.0f}, Velocity{1.0f, 0.0f});

    SC_EXPECT_TRUE(buffer.empty() == false);

    buffer.flush(core);

    const auto result = [&] {
        std::vector<safecrowd::engine::Entity> alive;
        core.entityRegistry().eachAlive([&](safecrowd::engine::Entity entity, const safecrowd::engine::Signature&) {
            if (core.hasComponent<Position>(entity) && core.hasComponent<Velocity>(entity)) {
                alive.push_back(entity);
            }
        });
        return alive;
    }();

    SC_EXPECT_EQ(result.size(), std::size_t{1});
    SC_EXPECT_NEAR(core.getComponent<Position>(result[0]).x, 3.0f, 1e-6);
    SC_EXPECT_NEAR(core.getComponent<Velocity>(result[0]).vx, 1.0f, 1e-6);
}

SC_TEST(WorldCommands_ForwardsToBuffer) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    safecrowd::engine::WorldCommands cmds{buffer};

    const auto e = core.createEntity();
    cmds.addComponent(e, Position{5.0f, 6.0f});

    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));

    buffer.flush(core);

    SC_EXPECT_TRUE(core.hasComponent<Position>(e));
    SC_EXPECT_NEAR(core.getComponent<Position>(e).y, 6.0f, 1e-6);
}
