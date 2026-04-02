#include "TestSupport.h"

#include "engine/EcsCore.h"

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

SC_TEST(EcsCore_CreateAndDestroyEntity) {
    safecrowd::engine::EcsCore core;

    const auto e = core.createEntity();
    SC_EXPECT_TRUE(core.isAlive(e));

    core.destroyEntity(e);
    SC_EXPECT_TRUE(!core.isAlive(e));
}

SC_TEST(EcsCore_AddComponent_UpdatesSignatureAndData) {
    safecrowd::engine::EcsCore core;
    const auto e = core.createEntity();

    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));

    core.addComponent(e, Position{1.0f, 2.0f});

    SC_EXPECT_TRUE(core.hasComponent<Position>(e));

    const auto& pos = core.getComponent<Position>(e);
    SC_EXPECT_NEAR(pos.x, 1.0f, 1e-6);
    SC_EXPECT_NEAR(pos.y, 2.0f, 1e-6);
}

SC_TEST(EcsCore_RemoveComponent_UpdatesSignature) {
    safecrowd::engine::EcsCore core;
    const auto e = core.createEntity();

    core.addComponent(e, Position{3.0f, 4.0f});
    SC_EXPECT_TRUE(core.hasComponent<Position>(e));

    core.removeComponent<Position>(e);
    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));
}

SC_TEST(EcsCore_RemoveComponent_NonExistent_IsSafe) {
    safecrowd::engine::EcsCore core;
    const auto e = core.createEntity();

    core.removeComponent<Position>(e);
    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));
}

SC_TEST(EcsCore_DestroyEntity_CleansUpAllComponents) {
    safecrowd::engine::EcsCore core;
    const auto e = core.createEntity();

    core.addComponent(e, Position{5.0f, 6.0f});
    core.addComponent(e, Velocity{1.0f, 0.0f});

    SC_EXPECT_TRUE(core.hasComponent<Position>(e));
    SC_EXPECT_TRUE(core.hasComponent<Velocity>(e));

    core.destroyEntity(e);

    SC_EXPECT_TRUE(!core.isAlive(e));
}

SC_TEST(EcsCore_EntityIndex_Reuse_DoesNotLeakComponents) {
    safecrowd::engine::EcsCore core;

    const auto e1 = core.createEntity();
    core.addComponent(e1, Position{7.0f, 8.0f});

    core.destroyEntity(e1);

    const auto e2 = core.createEntity();
    SC_EXPECT_TRUE(e1.index == e2.index);

    SC_EXPECT_TRUE(!core.hasComponent<Position>(e2));
}

SC_TEST(EcsCore_MultipleComponents_IndependentSignatureBits) {
    safecrowd::engine::EcsCore core;
    const auto e = core.createEntity();

    core.addComponent(e, Position{0.0f, 0.0f});
    core.addComponent(e, Velocity{1.0f, 1.0f});

    SC_EXPECT_TRUE(core.hasComponent<Position>(e));
    SC_EXPECT_TRUE(core.hasComponent<Velocity>(e));

    core.removeComponent<Position>(e);

    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));
    SC_EXPECT_TRUE(core.hasComponent<Velocity>(e));
}
