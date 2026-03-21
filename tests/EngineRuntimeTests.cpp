#include "TestSupport.h"

#include "engine/EngineRuntime.h"

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
