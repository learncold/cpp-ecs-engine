#include "TestSupport.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <vector>

#include "engine/EngineRuntime.h"
#include "engine/internal/EngineRuntimeTestAccess.h"

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

class RecordSeedSystem : public safecrowd::engine::EngineSystem {
public:
    std::vector<std::uint64_t>& seeds;
    std::vector<std::uint64_t>& runs;

    RecordSeedSystem(std::vector<std::uint64_t>& seedLog,
                     std::vector<std::uint64_t>& runLog)
        : seeds(seedLog), runs(runLog) {}

    void update(safecrowd::engine::EngineWorld&,
                const safecrowd::engine::EngineStepContext& step) override {
        seeds.push_back(step.derivedSeed);
        runs.push_back(step.runIndex);
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

SC_TEST(EngineRuntime_IntervalSystemsFollowTheirPhaseCadence) {
    int frameCadenceCount = 0;
    int fixedCadenceCount = 0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 1,
    });

    runtime.addSystem(
        std::make_unique<UpdateCounterSystem>(frameCadenceCount),
        {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::Interval,
         .intervalTicks = 2});
    runtime.addSystem(
        std::make_unique<UpdateCounterSystem>(fixedCadenceCount),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::Interval,
         .intervalTicks = 2});

    runtime.play();

    runtime.stepFrame(0.50);
    SC_EXPECT_EQ(frameCadenceCount, 1);
    SC_EXPECT_EQ(fixedCadenceCount, 1);

    runtime.stepFrame(0.25);
    SC_EXPECT_EQ(frameCadenceCount, 1);
    SC_EXPECT_EQ(fixedCadenceCount, 2);

    runtime.stepFrame(0.25);
    SC_EXPECT_EQ(frameCadenceCount, 2);
    SC_EXPECT_EQ(fixedCadenceCount, 2);
}

SC_TEST(EngineRuntime_AddSystem_RejectsZeroIntervalTicks) {
    int count = 0;
    safecrowd::engine::EngineRuntime runtime;

    bool threw = false;
    try {
        runtime.addSystem(
            std::make_unique<UpdateCounterSystem>(count),
            {.phase = safecrowd::engine::UpdatePhase::PreSimulation,
             .triggerPolicy = safecrowd::engine::TriggerPolicy::Interval,
             .intervalTicks = 0});
    } catch (const std::exception&) {
        threw = true;
    }

    SC_EXPECT_TRUE(threw);
}

SC_TEST(EngineRuntime_InitializeAndStop_ResetIntervalCadenceState) {
    int count = 0;

    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 1,
    });

    runtime.addSystem(
        std::make_unique<UpdateCounterSystem>(count),
        {.phase = safecrowd::engine::UpdatePhase::FixedSimulation,
         .triggerPolicy = safecrowd::engine::TriggerPolicy::Interval,
         .intervalTicks = 3});

    runtime.play();
    runtime.stepFrame(0.25);
    runtime.stepFrame(0.25);
    SC_EXPECT_EQ(count, 1);

    runtime.initialize();
    runtime.stepFrame(0.25);
    SC_EXPECT_EQ(count, 2);

    runtime.stop();
    runtime.play();
    runtime.stepFrame(0.25);
    SC_EXPECT_EQ(count, 3);
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

SC_TEST(EngineRuntime_Initialize_RebuildsDeterministicRngState) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 23,
    });

    runtime.initialize();
    auto& rng = safecrowd::engine::internal::EngineRuntimeTestAccess::rng(runtime);
    const auto firstAfterInitialize = rng.next();
    (void)rng.next();

    runtime.initialize();

    SC_EXPECT_EQ(rng.next(), firstAfterInitialize);
}

SC_TEST(EngineRuntime_Stop_RebuildsDeterministicRngState) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 29,
    });

    auto& rng = safecrowd::engine::internal::EngineRuntimeTestAccess::rng(runtime);
    const auto expectedFirstValue = rng.next();
    (void)rng.next();

    runtime.stop();

    SC_EXPECT_EQ(rng.next(), expectedFirstValue);
}

SC_TEST(EngineRuntime_StopAndRestart_RebuildsDeterministicSeedStream) {
    std::vector<std::uint64_t> firstSeeds;
    std::vector<std::uint64_t> firstRuns;
    safecrowd::engine::EngineRuntime firstRuntime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 19,
    });

    firstRuntime.addSystem(std::make_unique<RecordSeedSystem>(firstSeeds, firstRuns));
    firstRuntime.play();
    firstRuntime.stepFrame(0.25);
    firstRuntime.stop();
    firstRuntime.play();
    firstRuntime.stepFrame(0.25);

    std::vector<std::uint64_t> secondSeeds;
    std::vector<std::uint64_t> secondRuns;
    safecrowd::engine::EngineRuntime secondRuntime({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 19,
    });

    secondRuntime.addSystem(std::make_unique<RecordSeedSystem>(secondSeeds, secondRuns));
    secondRuntime.play();
    secondRuntime.stepFrame(0.25);
    secondRuntime.stop();
    secondRuntime.play();
    secondRuntime.stepFrame(0.25);

    SC_EXPECT_EQ(firstSeeds.size(), std::size_t{2});
    SC_EXPECT_EQ(secondSeeds.size(), std::size_t{2});
    SC_EXPECT_EQ(firstRuns.size(), std::size_t{2});
    SC_EXPECT_EQ(secondRuns.size(), std::size_t{2});
    SC_EXPECT_EQ(firstRuns[0], 1ULL);
    SC_EXPECT_EQ(firstRuns[1], 2ULL);
    SC_EXPECT_EQ(secondRuns[0], 1ULL);
    SC_EXPECT_EQ(secondRuns[1], 2ULL);
    SC_EXPECT_EQ(firstSeeds[0], secondSeeds[0]);
    SC_EXPECT_EQ(firstSeeds[1], secondSeeds[1]);
    SC_EXPECT_TRUE(firstSeeds[0] != firstSeeds[1]);
}
