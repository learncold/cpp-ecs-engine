#include "engine/FrameClock.h"

#include <algorithm>

namespace ecs_engine
{
namespace
{
EngineConfig sanitizeConfig(EngineConfig config) noexcept
{
    const EngineConfig defaults{};

    if (config.fixedTimeStepSeconds <= 0.0) {
        config.fixedTimeStepSeconds = defaults.fixedTimeStepSeconds;
    }

    if (config.maxFrameDeltaSeconds < config.fixedTimeStepSeconds) {
        config.maxFrameDeltaSeconds = std::max(defaults.maxFrameDeltaSeconds, config.fixedTimeStepSeconds);
    }

    if (config.maxCatchUpSteps == 0) {
        config.maxCatchUpSteps = defaults.maxCatchUpSteps;
    }

    return config;
}
} // namespace

FrameClock::FrameClock(EngineConfig config) noexcept
    : config_(sanitizeConfig(config))
{
}

const EngineConfig& FrameClock::config() const noexcept
{
    return config_;
}

double FrameClock::lagSeconds() const noexcept
{
    return lagSeconds_;
}

void FrameClock::reset() noexcept
{
    lagSeconds_ = 0.0;
}

FramePlan FrameClock::advance(double frameDeltaSeconds) noexcept
{
    const double clampedFrameDelta = std::clamp(frameDeltaSeconds, 0.0, config_.maxFrameDeltaSeconds);
    const double maxLagWindow = config_.fixedTimeStepSeconds * static_cast<double>(config_.maxCatchUpSteps);

    lagSeconds_ = std::min(lagSeconds_ + clampedFrameDelta, maxLagWindow);

    const std::size_t stepsToRun = static_cast<std::size_t>(lagSeconds_ / config_.fixedTimeStepSeconds);
    lagSeconds_ -= static_cast<double>(stepsToRun) * config_.fixedTimeStepSeconds;

    return {
        stepsToRun,
        clampedFrameDelta,
        config_.fixedTimeStepSeconds,
        lagSeconds_
    };
}
} // namespace ecs_engine
