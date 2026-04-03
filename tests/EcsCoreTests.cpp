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
    const auto posType = core.componentRegistry().tryTypeOf<Position>();
    SC_EXPECT_TRUE(posType.has_value());
    SC_EXPECT_TRUE(core.entityRegistry().signatureOf(e).test(posType.value()));

    const auto& pos = core.getComponent<Position>(e);
    SC_EXPECT_NEAR(pos.x, 1.0f, 1e-6);
    SC_EXPECT_NEAR(pos.y, 2.0f, 1e-6);
}

SC_TEST(EcsCore_RemoveComponent_UpdatesSignature) {
    safecrowd::engine::EcsCore core;
    const auto e = core.createEntity();

    core.addComponent(e, Position{3.0f, 4.0f});
    SC_EXPECT_TRUE(core.hasComponent<Position>(e));

    const auto posType = core.componentRegistry().tryTypeOf<Position>();
    SC_EXPECT_TRUE(posType.has_value());
    SC_EXPECT_TRUE(core.entityRegistry().signatureOf(e).test(posType.value()));

    core.removeComponent<Position>(e);
    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));
    SC_EXPECT_TRUE(!core.entityRegistry().signatureOf(e).test(posType.value()));
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

SC_TEST(EcsCore_AddComponent_StaleEntity_DoesNotMutateStorage) {
    safecrowd::engine::EcsCore core(1);
    const auto e = core.createEntity();
    core.destroyEntity(e);

    bool threwOnStaleEntity = false;
    try {
        core.addComponent(e, Position{9.0f, 10.0f});
    } catch (const std::invalid_argument&) {
        threwOnStaleEntity = true;
    }

    SC_EXPECT_TRUE(threwOnStaleEntity);
    SC_EXPECT_TRUE(!core.componentRegistry().tryTypeOf<Position>().has_value());
}

SC_TEST(EcsCore_EntityIndex_Reuse_DoesNotLeakComponents) {
    safecrowd::engine::EcsCore core(1);

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

    const auto posType = core.componentRegistry().tryTypeOf<Position>();
    const auto velType = core.componentRegistry().tryTypeOf<Velocity>();
    SC_EXPECT_TRUE(posType.has_value());
    SC_EXPECT_TRUE(velType.has_value());
    SC_EXPECT_TRUE(core.entityRegistry().signatureOf(e).test(posType.value()));
    SC_EXPECT_TRUE(core.entityRegistry().signatureOf(e).test(velType.value()));

    core.removeComponent<Position>(e);

    SC_EXPECT_TRUE(!core.hasComponent<Position>(e));
    SC_EXPECT_TRUE(core.hasComponent<Velocity>(e));
    SC_EXPECT_TRUE(!core.entityRegistry().signatureOf(e).test(posType.value()));
    SC_EXPECT_TRUE(core.entityRegistry().signatureOf(e).test(velType.value()));
}
