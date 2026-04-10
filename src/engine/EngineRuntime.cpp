#include "engine/EngineRuntime.h"

namespace safecrowd::engine {
namespace {

EngineConfig normalizeConfig(EngineConfig config) {
    if (config.fixedDeltaTime <= 0.0) {
        config.fixedDeltaTime = 1.0 / 60.0;
    }

    if (config.maxCatchUpSteps == 0) {
        config.maxCatchUpSteps = 1;
    }

    if (config.baseSeed == 0) {
        config.baseSeed = 1;
    }

    return config;
}

}  // namespace

EngineRuntime::EngineRuntime(EngineConfig config)
    : config_(normalizeConfig(config)),
      scheduler_(core_, buffer_),
      world_(EngineWorld::ConstructionToken{}, core_, resources_, buffer_),
      frameClock_(config_) {
}

void EngineRuntime::addSystem(std::unique_ptr<EngineSystem> system,
                              SystemDescriptor descriptor) {
    scheduler_.registerSystem(std::move(system), descriptor);
}

void EngineRuntime::initialize() {
    frameClock_.reset();
    core_ = EcsCore{};
    buffer_ = CommandBuffer{};
    stats_ = {};
    stats_.state = EngineState::Ready;
    ++runIndex_;

    scheduler_.configure(world_);

    const EngineStepContext startupCtx{
        .frameIndex      = stats_.frameIndex,
        .fixedStepIndex  = stats_.fixedStepIndex,
        .alpha           = 0.0,
        .runIndex        = runIndex_,
        .derivedSeed     = 0,
    };
    scheduler_.executeStartup(world_, startupCtx);
}

void EngineRuntime::play() {
    if (stats_.state == EngineState::Stopped) {
        initialize();
    }

    stats_.state = EngineState::Running;
}

void EngineRuntime::pause() {
    if (stats_.state == EngineState::Running) {
        stats_.state = EngineState::Paused;
    }
}

void EngineRuntime::stop() {
    frameClock_.reset();
    core_ = EcsCore{};
    buffer_ = CommandBuffer{};
    stats_ = {};
    stats_.state = EngineState::Stopped;
}

void EngineRuntime::stepFrame(double deltaSeconds) {
    if (stats_.state == EngineState::Stopped) {
        initialize();
    }

    if (stats_.state == EngineState::Paused) {
        stats_.fixedStepsThisFrame = 0;
        return;
    }

    frameClock_.beginFrame(deltaSeconds);

    ++stats_.frameIndex;
    stats_.fixedStepsThisFrame = 0;

    EngineStepContext ctx{
        .frameIndex      = stats_.frameIndex,
        .fixedStepIndex  = stats_.fixedStepIndex,
        .alpha           = frameClock_.alpha(),
        .runIndex        = runIndex_,
        .derivedSeed     = 0,
    };

    scheduler_.executePhase(UpdatePhase::PreSimulation, TriggerPolicy::EveryFrame,
                            world_, ctx);

    while (frameClock_.shouldRunFixedStep()) {
        frameClock_.consumeFixedStep();
        ++stats_.fixedStepIndex;
        ++stats_.fixedStepsThisFrame;

        ctx = EngineStepContext{
            .frameIndex      = stats_.frameIndex,
            .fixedStepIndex  = stats_.fixedStepIndex,
            .alpha           = frameClock_.alpha(),
            .runIndex        = runIndex_,
            .derivedSeed     = 0,
        };

        scheduler_.executePhase(UpdatePhase::FixedSimulation, TriggerPolicy::FixedStep,
                                world_, ctx);
    }

    ctx.alpha = frameClock_.alpha();
    scheduler_.executePhase(UpdatePhase::PostSimulation, TriggerPolicy::EveryFrame,
                            world_, ctx);

    stats_.alpha = frameClock_.alpha();

    ctx.alpha = stats_.alpha;
    scheduler_.executePhase(UpdatePhase::RenderSync, TriggerPolicy::EveryFrame,
                            world_, ctx);
}

EngineWorld& EngineRuntime::world() noexcept {
    return world_;
}

const EngineWorld& EngineRuntime::world() const noexcept {
    return world_;
}

const EngineConfig& EngineRuntime::config() const noexcept {
    return config_;
}

const EngineStats& EngineRuntime::stats() const noexcept {
    return stats_;
}

EngineState EngineRuntime::state() const noexcept {
    return stats_.state;
}

std::uint64_t EngineRuntime::runIndex() const noexcept {
    return runIndex_;
}

}  // namespace safecrowd::engine
