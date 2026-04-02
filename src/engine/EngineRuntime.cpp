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
      frameClock_(config_) {
}

void EngineRuntime::initialize() {
    frameClock_.reset();
    stats_ = {};
    stats_.state = EngineState::Ready;
    ++runIndex_;
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
    world_.shutdown();
    frameClock_.reset();
    stats_ = {};
    stats_.state = EngineState::Stopped;
}

void EngineRuntime::stepFrame(double deltaSeconds) {
    if (stats_.state == EngineState::Stopped) {
        initialize();
    }

    frameClock_.beginFrame(deltaSeconds);

    ++stats_.frameIndex;
    stats_.fixedStepsThisFrame = 0;

    while (frameClock_.shouldRunFixedStep()) {
        frameClock_.consumeFixedStep();
        ++stats_.fixedStepIndex;
        ++stats_.fixedStepsThisFrame;
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
