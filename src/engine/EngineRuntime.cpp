#include "engine/EngineRuntime.h"

#include <algorithm>

namespace ecs_engine
{
EngineRuntime::EngineRuntime(EngineConfig config) noexcept
    : frameClock_(config)
{
    resetStats();
}

std::string_view EngineRuntime::name() const noexcept
{
    return "ECS Engine";
}

std::string_view EngineRuntime::summary() const noexcept
{
    return "Core runtime with fixed-step scheduling, pluggable systems, and frame statistics.";
}

EngineState EngineRuntime::state() const noexcept
{
    return state_;
}

const EngineConfig& EngineRuntime::config() const noexcept
{
    return frameClock_.config();
}

const EngineStats& EngineRuntime::stats() const noexcept
{
    return stats_;
}

void EngineRuntime::addSystem(EngineSystem& system)
{
    if (std::find(systems_.begin(), systems_.end(), &system) != systems_.end()) {
        return;
    }

    systems_.push_back(&system);
    stats_.registeredSystems = systems_.size();
}

void EngineRuntime::start() noexcept
{
    reset();
    state_ = EngineState::Running;
}

void EngineRuntime::pause() noexcept
{
    if (state_ == EngineState::Running) {
        state_ = EngineState::Paused;
    }
}

void EngineRuntime::resume() noexcept
{
    if (state_ == EngineState::Paused) {
        state_ = EngineState::Running;
    }
}

void EngineRuntime::stop() noexcept
{
    state_ = EngineState::Idle;
    reset();
}

void EngineRuntime::reset() noexcept
{
    frameClock_.reset();
    resetStats();
}

void EngineRuntime::tick(double frameDeltaSeconds)
{
    if (state_ != EngineState::Running) {
        return;
    }

    const FramePlan framePlan = frameClock_.advance(frameDeltaSeconds);

    ++stats_.frameCount;
    stats_.wallTimeSeconds += framePlan.frameDeltaSeconds;
    stats_.lagSeconds = framePlan.remainingLagSeconds;

    runFixedUpdate(framePlan.stepsToRun, framePlan.fixedTimeStepSeconds);
}

void EngineRuntime::runFixedUpdate(std::size_t stepsToRun, double stepDeltaSeconds)
{
    for (std::size_t step = 0; step < stepsToRun; ++step) {
        const EngineStepContext context{
            stats_.frameCount,
            stats_.fixedStepCount,
            stepDeltaSeconds,
            stats_.simulationTimeSeconds + stepDeltaSeconds
        };

        for (EngineSystem* system : systems_) {
            system->update(context);
        }

        ++stats_.fixedStepCount;
        stats_.simulationTimeSeconds = context.simulationTimeSeconds;
    }
}

void EngineRuntime::resetStats() noexcept
{
    stats_ = {};
    stats_.registeredSystems = systems_.size();
    stats_.lagSeconds = frameClock_.lagSeconds();
}
} // namespace ecs_engine
