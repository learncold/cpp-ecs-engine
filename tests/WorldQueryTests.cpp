#include "TestSupport.h"

#include <type_traits>

#include "engine/internal/EngineWorldFactory.h"

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

static_assert(!std::is_constructible_v<safecrowd::engine::WorldQuery, safecrowd::engine::EcsCore&>);
static_assert(!std::is_constructible_v<safecrowd::engine::EngineWorld,
                                       safecrowd::engine::EcsCore&,
                                       safecrowd::engine::CommandBuffer&>);

SC_TEST(WorldQuery_ViewFiltersEntitiesBySignature) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, buffer);

    const auto e1 = core.createEntity();
    const auto e2 = core.createEntity();
    const auto e3 = core.createEntity();

    core.addComponent(e1, Position{});
    core.addComponent(e1, Velocity{});
    core.addComponent(e2, Position{});
    core.addComponent(e3, Velocity{});

    const auto result = world.query().view<Position, Velocity>();
    SC_EXPECT_EQ(result.size(), std::size_t{1});
    SC_EXPECT_TRUE(result[0] == e1);
}

SC_TEST(WorldQuery_ViewReturnsEmptyIfTypeNotRegistered) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, buffer);

    static_cast<void>(core.createEntity());

    const auto result = world.query().view<Position>();
    SC_EXPECT_TRUE(result.empty());
}

SC_TEST(WorldQuery_ViewExcludesDestroyedEntities) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, buffer);

    const auto e1 = core.createEntity();
    core.addComponent(e1, Position{});
    core.destroyEntity(e1);

    const auto e2 = core.createEntity();
    core.addComponent(e2, Position{});

    const auto result = world.query().view<Position>();
    SC_EXPECT_EQ(result.size(), std::size_t{1});
    SC_EXPECT_TRUE(result[0] == e2);
}

SC_TEST(WorldQuery_ContainsReflectsComponentPresence) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, buffer);

    const auto e = core.createEntity();
    core.addComponent(e, Position{});

    SC_EXPECT_TRUE(world.query().contains<Position>(e));
    SC_EXPECT_TRUE(!world.query().contains<Velocity>(e));
}

SC_TEST(WorldQuery_GetReturnsComponentRef) {
    safecrowd::engine::EcsCore core;
    safecrowd::engine::CommandBuffer buffer;
    auto world = safecrowd::engine::internal::EngineWorldFactory::create(core, buffer);

    const auto e = core.createEntity();
    core.addComponent(e, Position{3.0f, 4.0f});

    const auto& pos = world.query().get<Position>(e);
    SC_EXPECT_NEAR(pos.x, 3.0f, 1e-6);
    SC_EXPECT_NEAR(pos.y, 4.0f, 1e-6);
}
