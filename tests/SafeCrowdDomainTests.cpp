#include "TestSupport.h"

#include "domain/SafeCrowdDomain.h"
#include "engine/EngineRuntime.h"

SC_TEST(SafeCrowdDomainExposesRuntimeSummary) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.5,
        .maxCatchUpSteps = 4,
        .baseSeed = 42,
    });

    safecrowd::domain::SafeCrowdDomain domain(runtime);
    domain.start();
    domain.update(1.0);

    const auto summary = domain.summary();
    SC_EXPECT_EQ(summary.state, safecrowd::engine::EngineState::Running);
    SC_EXPECT_EQ(summary.frameIndex, 1ULL);
    SC_EXPECT_EQ(summary.fixedStepIndex, 2ULL);
    SC_EXPECT_NEAR(summary.alpha, 0.0, 1e-9);
}

SC_TEST(SafeCrowdDomainStopResetsSummary) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.5,
        .maxCatchUpSteps = 4,
        .baseSeed = 43,
    });

    safecrowd::domain::SafeCrowdDomain domain(runtime);
    domain.start();
    domain.update(0.5);
    domain.stop();

    const auto summary = domain.summary();
    SC_EXPECT_EQ(summary.state, safecrowd::engine::EngineState::Stopped);
    SC_EXPECT_EQ(summary.frameIndex, 0ULL);
    SC_EXPECT_EQ(summary.fixedStepIndex, 0ULL);
}
