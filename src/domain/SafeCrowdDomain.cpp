#include "domain/SafeCrowdDomain.h"

#include <memory>

#include "domain/CompressionSystem.h"
#include "engine/SystemDescriptor.h"
#include "engine/TriggerPolicy.h"
#include "engine/UpdatePhase.h"

namespace safecrowd::domain {

SafeCrowdDomain::SafeCrowdDomain(engine::EngineRuntime& runtime)
    : runtime_(runtime) {
    runtime_.addSystem(
        std::make_unique<CompressionSystem>(runtime_.config().fixedDeltaTime),
        {
            .phase = engine::UpdatePhase::FixedSimulation,
            .order = 0,
            .triggerPolicy = engine::TriggerPolicy::FixedStep,
        });
}

void SafeCrowdDomain::start() {
    runtime_.play();
}

void SafeCrowdDomain::pause() {
    runtime_.pause();
}

void SafeCrowdDomain::stop() {
    runtime_.stop();
}

void SafeCrowdDomain::update(double deltaSeconds) {
    runtime_.stepFrame(deltaSeconds);
}

SimulationSummary SafeCrowdDomain::summary() const {
    const auto& stats = runtime_.stats();
    return {
        .state = stats.state,
        .frameIndex = stats.frameIndex,
        .fixedStepIndex = stats.fixedStepIndex,
        .alpha = stats.alpha,
    };
}

engine::EngineRuntime& SafeCrowdDomain::runtime() noexcept {
    return runtime_;
}

const engine::EngineRuntime& SafeCrowdDomain::runtime() const noexcept {
    return runtime_;
}

}  // namespace safecrowd::domain
