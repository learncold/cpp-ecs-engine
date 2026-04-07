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
      world_(core_, buffer_),
      frameClock_(config_) {
}

void EngineRuntime::addSystem(std::unique_ptr<EngineSystem> system) {
    systems_.push_back(std::move(system));
}

void EngineRuntime::initialize() {
    frameClock_.reset();
    core_ = EcsCore{};
    buffer_ = CommandBuffer{};
    stats_ = {};
    stats_.state = EngineState::Ready;
    ++runIndex_;

    for (auto& system : systems_) {
        system->configure(world_);
        buffer_.flush(core_);
    }
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

    while (frameClock_.shouldRunFixedStep()) {
        frameClock_.consumeFixedStep();
        ++stats_.fixedStepIndex;
        ++stats_.fixedStepsThisFrame;

        const EngineStepContext ctx{
            .frameIndex      = stats_.frameIndex,
            .fixedStepIndex  = stats_.fixedStepIndex,
            .alpha           = frameClock_.alpha(),
            .runIndex        = runIndex_,
            .derivedSeed     = 0,
        };

        for (auto& system : systems_) {
            system->update(world_, ctx);
        }

        buffer_.flush(core_);
    }

    stats_.alpha = frameClock_.alpha();
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
