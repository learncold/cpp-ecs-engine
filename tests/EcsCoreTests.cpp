#include "TestSupport.h"

#include <exception>

#include "engine/EngineRuntime.h"
#include "engine/EcsCore.h"

namespace {

struct Position {
    int x{0};
};

}  // namespace

SC_TEST(EcsCoreThrowsOnAddingUnregisteredComponentType) {
    safecrowd::engine::EcsCore core;
    const auto entity = core.createEntity();

    bool threw = false;
    try {
        core.addComponent<Position>(entity, Position{1});
    } catch (const std::logic_error&) {
        threw = true;
    } catch (const std::exception&) {
        threw = true;
    }

    SC_EXPECT_TRUE(threw);
}

SC_TEST(EcsCoreUpdatesEntitySignatureOnAddRemove) {
    safecrowd::engine::EcsCore core;
    core.registerType<Position>();

    const auto entity = core.createEntity();
    auto sig0 = core.signatureOf(entity);
    SC_EXPECT_TRUE(sig0.none());

    core.addComponent<Position>(entity, Position{5});
    auto sig1 = core.signatureOf(entity);
    SC_EXPECT_EQ(sig1.count(), 1U);

    core.removeComponent<Position>(entity);
    auto sig2 = core.signatureOf(entity);
    SC_EXPECT_TRUE(sig2.none());
}

SC_TEST(EcsCoreDestroysComponentsOnDestroyEntity) {
    safecrowd::engine::EcsCore core;
    core.registerType<Position>();

    const auto entity = core.createEntity();
    core.addComponent<Position>(entity, Position{42});
    SC_EXPECT_TRUE(core.containsComponent<Position>(entity));

    core.destroyEntity(entity);
    SC_EXPECT_TRUE(!core.isAlive(entity));
    SC_EXPECT_TRUE(!core.containsComponent<Position>(entity));
}

SC_TEST(EngineRuntimeStopShutsDownEcsCore) {
    safecrowd::engine::EngineRuntime runtime;
    auto& ecs = runtime.world().ecsCore();

    ecs.registerType<Position>();
    const auto entity = ecs.createEntity();
    ecs.addComponent<Position>(entity, Position{7});
    SC_EXPECT_TRUE(ecs.containsComponent<Position>(entity));

    runtime.stop();

    SC_EXPECT_TRUE(!ecs.isAlive(entity));
    SC_EXPECT_TRUE(!ecs.containsComponent<Position>(entity));
}

