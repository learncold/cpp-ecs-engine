#include "TestSupport.h"

#include <cstddef>
#include <exception>
#include <memory>
#include <vector>

#include "engine/EngineRuntime.h"

namespace {

struct Marker {};
struct SharedCounter {
    int value{0};
};

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

class RecordPhaseSystem : public safecrowd::engine::EngineSystem {
public:
    std::vector<int>& log;
    int               marker;

    explicit RecordPhaseSystem(std::vector<int>& l, int value)
        : log(l), marker(value) {}

    void update(safecrowd::engine::EngineWorld&, const safecrowd::engine::EngineStepContext&) override {
        log.push_back(marker);
    }
};

class ResourceSetupSystem : public safecrowd::engine::EngineSystem {
public:
    void configure(safecrowd::engine::EngineWorld& world) override {
        world.resources().set(SharedCounter{7});
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

SC_TEST(EngineRuntime_ExecutesStartupAndFramePhases) {
    std::vector<int> log;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 1,
    });

    runtime.addSystem(
        std::make_unique<RecordPhaseSystem>(log, 10),
        {.phase = safecrowd::engine::UpdatePhase::Startup,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<RecordPhaseSystem>(log, 20),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<RecordPhaseSystem>(log, 30),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::FixedStep});
    runtime.addSystem(
        std::make_unique<RecordPhaseSystem>(log, 40),
        {.phase = safecrowd::engine::UpdatePhase::PostSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});
    runtime.addSystem(
        std::make_unique<RecordPhaseSystem>(log, 50),
        {.phase = safecrowd::engine::UpdatePhase::RenderSync,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame});

    runtime.play();
    runtime.stepFrame(0.50);

    SC_EXPECT_EQ(log.size(), std::size_t{6});
    SC_EXPECT_EQ(log[0], 10);
    SC_EXPECT_EQ(log[1], 20);
    SC_EXPECT_EQ(log[2], 30);
    SC_EXPECT_EQ(log[3], 30);
    SC_EXPECT_EQ(log[4], 40);
    SC_EXPECT_EQ(log[5], 50);
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

SC_TEST(EngineRuntime_AddSystem_RejectsUnsupportedIntervalTriggerPolicy) {
    int count = 0;
    safecrowd::engine::EngineRuntime runtime;

    bool threw = false;
    try {
        runtime.addSystem(
            std::make_unique<UpdateCounterSystem>(count),
            {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
             .triggerPolicy = safecrowd::engine::TriggerPolicy::Interval});
    } catch (const std::exception&) {
        threw = true;
    }

    SC_EXPECT_TRUE(threw);
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

SC_TEST(EngineRuntime_WorldResources_AccessibleThroughEngineWorld) {
    safecrowd::engine::EngineRuntime runtime;

    runtime.addSystem(std::make_unique<ResourceSetupSystem>());
    runtime.initialize();

    SC_EXPECT_TRUE(runtime.world().resources().contains<SharedCounter>());
    SC_EXPECT_EQ(runtime.world().resources().get<SharedCounter>().value, 7);
}

SC_TEST(EngineRuntime_Stop_ClearsWorldResourcesBeforeNextRun) {
    safecrowd::engine::EngineRuntime runtime;

    runtime.world().resources().set(SharedCounter{11});
    SC_EXPECT_TRUE(runtime.world().resources().contains<SharedCounter>());

    runtime.stop();

    SC_EXPECT_TRUE(!runtime.world().resources().contains<SharedCounter>());
}

SC_TEST(EngineRuntime_Initialize_ClearsExistingWorldResources) {
    safecrowd::engine::EngineRuntime runtime;

    runtime.world().resources().set(SharedCounter{13});
    SC_EXPECT_TRUE(runtime.world().resources().contains<SharedCounter>());

    runtime.initialize();

    SC_EXPECT_TRUE(!runtime.world().resources().contains<SharedCounter>());
}
