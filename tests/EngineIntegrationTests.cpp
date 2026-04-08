#include "TestSupport.h"

#include <cstddef>
#include <memory>

#include "engine/EngineRuntime.h"

namespace {

struct Position {
    float x{0.0f};
    float y{0.0f};
};

struct Activated {
    int fixedStepSeen{0};
};

class StartupSpawnPositionSystem : public safecrowd::engine::EngineSystem {
public:
    void update(safecrowd::engine::EngineWorld& world,
                const safecrowd::engine::EngineStepContext&) override {
        world.commands().spawnEntity(Position{3.0f, 4.0f});
    }
};

class QueryAndActivateSystem : public safecrowd::engine::EngineSystem {
public:
    std::size_t& queriedCount;
    std::size_t& visibleActivatedCountDuringUpdate;
    std::size_t& scheduledActivationCount;

    QueryAndActivateSystem(std::size_t& queriedCountRef,
                           std::size_t& visibleActivatedCountDuringUpdateRef,
                           std::size_t& scheduledActivationCountRef)
        : queriedCount(queriedCountRef),
          visibleActivatedCountDuringUpdate(visibleActivatedCountDuringUpdateRef),
          scheduledActivationCount(scheduledActivationCountRef) {}

    void update(safecrowd::engine::EngineWorld& world,
                const safecrowd::engine::EngineStepContext& step) override {
        const auto positionedEntities = world.query().view<Position>();
        queriedCount = positionedEntities.size();
        scheduledActivationCount = 0;

        for (const auto entity : positionedEntities) {
            if (!world.query().contains<Activated>(entity)) {
                world.commands().addComponent(entity, Activated{
                                                         .fixedStepSeen = static_cast<int>(step.fixedStepIndex),
                                                     });
                ++scheduledActivationCount;
            }
        }

        visibleActivatedCountDuringUpdate = world.query().view<Activated>().size();
    }
};

}  // namespace

SC_TEST(EngineIntegration_QueryReadAndDeferredMutationFlow_AppliesAfterStepFrame) {
    std::size_t queriedCount = 0;
    std::size_t visibleActivatedCountDuringUpdate = 0;
    std::size_t scheduledActivationCount = 0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 7,
    });

    runtime.addSystem(
        std::make_unique<StartupSpawnPositionSystem>(),
        {.phase = safecrowd::engine::UpdatePhase::Startup,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(std::make_unique<QueryAndActivateSystem>(
        queriedCount,
        visibleActivatedCountDuringUpdate,
        scheduledActivationCount));

    runtime.play();

    const auto beforeStepPositioned = runtime.world().query().view<Position>();
    const auto beforeStepActivated = runtime.world().query().view<Activated>();
    SC_EXPECT_EQ(beforeStepPositioned.size(), std::size_t{1});
    SC_EXPECT_TRUE(beforeStepActivated.empty());

    runtime.stepFrame(0.25);

    const auto activatedEntities = runtime.world().query().view<Activated>();
    const auto fullyMatchedEntities = runtime.world().query().view<Position, Activated>();

    SC_EXPECT_EQ(queriedCount, std::size_t{1});
    SC_EXPECT_EQ(scheduledActivationCount, std::size_t{1});
    SC_EXPECT_EQ(visibleActivatedCountDuringUpdate, std::size_t{0});
    SC_EXPECT_EQ(activatedEntities.size(), std::size_t{1});
    SC_EXPECT_EQ(fullyMatchedEntities.size(), std::size_t{1});

    const auto& activated = runtime.world().query().get<Activated>(activatedEntities.front());
    SC_EXPECT_EQ(activated.fixedStepSeen, 1);
}
