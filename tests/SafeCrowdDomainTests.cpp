#include "TestSupport.h"

#include <memory>

#include "domain/AgentComponents.h"
#include "domain/Metrics.h"
#include "domain/SafeCrowdDomain.h"
#include "engine/EngineRuntime.h"
#include "engine/EngineSystem.h"
#include "engine/SystemDescriptor.h"
#include "engine/TriggerPolicy.h"
#include "engine/UpdatePhase.h"

namespace {

class StartupSeedCrowdSystem final : public safecrowd::engine::EngineSystem {
public:
    void update(safecrowd::engine::EngineWorld& world,
                const safecrowd::engine::EngineStepContext&) override {
        world.commands().spawnEntity(
            safecrowd::domain::Position{{0.0, 0.0}},
            safecrowd::domain::Agent{},
            safecrowd::domain::CompressionData{});
        world.commands().spawnEntity(
            safecrowd::domain::Position{{0.0, 0.0}},
            safecrowd::domain::Agent{},
            safecrowd::domain::CompressionData{});
        world.commands().spawnEntity(
            safecrowd::domain::Position{{0.0, 0.0}},
            safecrowd::domain::Agent{},
            safecrowd::domain::CompressionData{});
    }
};

}  // namespace

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

SC_TEST(SafeCrowdDomainExposesRuntimeSnapshotWithCompressionChannels) {
    safecrowd::engine::EngineRuntime runtime({
        .fixedDeltaTime = 0.5,
        .maxCatchUpSteps = 4,
        .baseSeed = 99,
    });

    runtime.addSystem(
        std::make_unique<StartupSeedCrowdSystem>(),
        {
            .phase = safecrowd::engine::UpdatePhase::Startup,
            .order = 0,
            .triggerPolicy = safecrowd::engine::TriggerPolicy::EveryFrame,
        });

    safecrowd::domain::SafeCrowdDomain domain(runtime);
    domain.start();
    domain.update(0.5);

    const auto snapshot = domain.snapshot();
    const auto* forceChannel =
        snapshot.findScalarChannel(safecrowd::domain::kCompressionForceChannelName);
    const auto* exposureChannel =
        snapshot.findScalarChannel(safecrowd::domain::kCompressionExposureChannelName);

    SC_EXPECT_EQ(snapshot.frameIndex, 1ULL);
    SC_EXPECT_EQ(snapshot.fixedStepIndex, 1ULL);
    SC_EXPECT_NEAR(snapshot.simulationTime, 0.5, 1e-9);
    SC_EXPECT_EQ(snapshot.agentCount, 3U);
    SC_EXPECT_EQ(snapshot.agentIds.size(), std::size_t{3});
    SC_EXPECT_EQ(snapshot.positions.size(), std::size_t{3});
    SC_EXPECT_TRUE(forceChannel != nullptr);
    SC_EXPECT_TRUE(exposureChannel != nullptr);
    SC_EXPECT_EQ(forceChannel->values.size(), std::size_t{3});
    SC_EXPECT_EQ(exposureChannel->values.size(), std::size_t{3});
    SC_EXPECT_TRUE(forceChannel->values[0] > 0.5f);
    SC_EXPECT_NEAR(exposureChannel->values[0], 0.5, 1e-6);
}
