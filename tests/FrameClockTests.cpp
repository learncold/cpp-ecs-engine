#include "TestSupport.h"

#include "engine/FrameClock.h"

SC_TEST(FrameClockAccumulatesRemainderAcrossFrames) {
    safecrowd::engine::FrameClock clock({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 4,
        .baseSeed = 7,
    });

    clock.beginFrame(0.10);
    SC_EXPECT_EQ(clock.pendingFixedSteps(), 0U);
    SC_EXPECT_NEAR(clock.alpha(), 0.4, 1e-9);

    clock.beginFrame(0.20);
    SC_EXPECT_EQ(clock.pendingFixedSteps(), 1U);
    SC_EXPECT_TRUE(clock.shouldRunFixedStep());

    clock.consumeFixedStep();
    SC_EXPECT_EQ(clock.pendingFixedSteps(), 0U);
    SC_EXPECT_NEAR(clock.alpha(), 0.2, 1e-9);
}

SC_TEST(FrameClockCapsCatchUpSteps) {
    safecrowd::engine::FrameClock clock({
        .fixedDeltaTime = 0.25,
        .maxCatchUpSteps = 2,
        .baseSeed = 99,
    });

    clock.beginFrame(2.0);
    SC_EXPECT_EQ(clock.pendingFixedSteps(), 2U);

    unsigned int consumedSteps = 0;
    while (clock.shouldRunFixedStep()) {
        clock.consumeFixedStep();
        ++consumedSteps;
    }

    SC_EXPECT_EQ(consumedSteps, 2U);
    SC_EXPECT_NEAR(clock.alpha(), 0.0, 1e-9);
}
