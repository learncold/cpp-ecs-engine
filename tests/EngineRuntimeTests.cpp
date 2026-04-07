#include "TestSupport.h"

#include <cstddef>
#include <memory>

#include "engine/EngineRuntime.h"

namespace {

struct Marker {};

class UpdateCounterSystem : public safecrowd::engine::EngineSystem {
public:
    int& count;
    explicit UpdateCounterSystem(int& c) : count(c) {}
    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
        ++count;
    }
};

class SpawnMarkerSystem : public safecrowd::engine::EngineSystem {
public:
    void update(safecrowd::engine::EngineWorld& world, const safecrowd::engine::EngineStepContext&) override {
        world.commands().spawnEntity(Marker{});
    }
};

class ConfigureSpawnMarkerSystem : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.commands().spawnEntity(Marker{});
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

class ConfigureObserveMarkerSystem : public safecrowd::engine::EngineSystem {
public:
    std::size_t& count;

    explicit ConfigureObserveMarkerSystem(std::size_t& c) : count(c) {}

    void configure(safecrowd::engine::EngineWorld& world) override {
        count = world.query().view<Marker>().size();
    }

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
    }
};

}  // namespace

SC_TEST(EngineRuntimePlayAndStepUpdatesStats) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 10,
    });

    runtime.play();
    runtime.stepFrame(0.50);

    const auto& stats = runtime.stats();
    SC_EXPECT_EQ(runtime.state(), safecrowd::engine::EngineState::Running);
    SC_EXPECT_EQ(stats.frameIndex, 1ULL);
    SC_EXPECT_EQ(stats.fixedStepIndex, 2ULL);
    SC_EXPECT_EQ(stats.fixedStepsThisFrame, 2U);
    SC_EXPECT_NEAR(stats.alpha, 0.0, 1e-9);
}

SC_TEST(EngineRuntime_RegisteredSystem_UpdateCalledOnFixedStep) {
    int count = 0;
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 1,
    });

    runtime.addSystem(std::make_unique<UpdateCounterSystem>(count));
    runtime.play();
    runtime.stepFrame(0.50);  // 2 fixed steps

    SC_EXPECT_EQ(count, 2);
}

SC_TEST(EngineRuntime_WorldCommands_FlushedAfterEachFixedStep) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 1,
    });

    runtime.addSystem(std::make_unique<SpawnMarkerSystem>());
    runtime.play();
    runtime.stepFrame(0.25);  // 1 fixed step

    const auto entities = runtime.world().query().view<Marker>();
    SC_EXPECT_EQ(entities.size(), std::size_t{1});
}

SC_TEST(EngineRuntime_ConfigureCommands_AreVisibleToLaterSystems) {
    std::size_t configuredMarkerCount = 0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 1,
    });

    runtime.addSystem(std::make_unique<ConfigureSpawnMarkerSystem>());
    runtime.addSystem(std::make_unique<ConfigureObserveMarkerSystem>(configuredMarkerCount));

    runtime.play();

    SC_EXPECT_EQ(configuredMarkerCount, std::size_t{1});
    SC_EXPECT_EQ(runtime.world().query().view<Marker>().size(), std::size_t{1});
}

SC_TEST(EngineRuntime_Stop_ClearsWorldAndPendingCommandsBeforeNextRun) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 1,
    });

    runtime.addSystem(std::make_unique<SpawnMarkerSystem>());
    runtime.play();
    runtime.stepFrame(0.25);
    SC_EXPECT_EQ(runtime.world().query().view<Marker>().size(), std::size_t{1});

    runtime.world().commands().spawnEntity(Marker{});
    runtime.stop();

    SC_EXPECT_EQ(runtime.world().query().view<Marker>().size(), std::size_t{0});

    runtime.play();
    runtime.stepFrame(0.25);

    SC_EXPECT_EQ(runtime.world().query().view<Marker>().size(), std::size_t{1});
}

SC_TEST(EngineRuntime_PausedRuntime_DoesNotAdvanceSimulation) {
    int count = 0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 1,
    });

    runtime.addSystem(std::make_unique<UpdateCounterSystem>(count));
    runtime.play();
    runtime.stepFrame(0.25);
    runtime.pause();
    runtime.stepFrame(1.00);

    const auto& pausedStats = runtime.stats();
    SC_EXPECT_EQ(count, 1);
    SC_EXPECT_EQ(pausedStats.frameIndex, 1ULL);
    SC_EXPECT_EQ(pausedStats.fixedStepIndex, 1ULL);
    SC_EXPECT_EQ(pausedStats.fixedStepsThisFrame, 0U);

    runtime.play();
    runtime.stepFrame(0.25);
    SC_EXPECT_EQ(count, 2);
}

SC_TEST(EngineRuntimePauseAndStopResetLifecycleState) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 11,
    });

    runtime.play();
    runtime.pause();
    SC_EXPECT_EQ(runtime.state(), safecrowd::engine::EngineState::Paused);

    runtime.stop();

    const auto& stats = runtime.stats();
    SC_EXPECT_EQ(runtime.state(), safecrowd::engine::EngineState::Stopped);
    SC_EXPECT_EQ(stats.frameIndex, 0ULL);
    SC_EXPECT_EQ(stats.fixedStepIndex, 0ULL);
    SC_EXPECT_NEAR(stats.alpha, 0.0, 1e-9);
}
