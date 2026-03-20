#include "engine/FrameClock.h"

#include <algorithm>
#include <cmath>

namespace safecrowd::engine {
namespace {

EngineConfig normalizeConfig(EngineConfig config) {
    if (config.fixedDeltaTime <= 0.0) {
        config.fixedDeltaTime = 1.0 / 60.0;
    }

    if (config.maxCatchUpSteps == 0) {
        config.maxCatchUpSteps = 1;
    }

    return config;
}

}  // namespace

FrameClock::FrameClock(const EngineConfig& config)
    : config_(normalizeConfig(config)) {
}

void FrameClock::reset() {
    accumulatedSeconds_ = 0.0;
    pendingFixedSteps_ = 0;
}

void FrameClock::beginFrame(double deltaSeconds) {
    const double safeDeltaSeconds = std::max(0.0, deltaSeconds);
    const double maxAccumulatedSeconds =
        config_.fixedDeltaTime * static_cast<double>(config_.maxCatchUpSteps);

    accumulatedSeconds_ = std::min(accumulatedSeconds_ + safeDeltaSeconds, maxAccumulatedSeconds);

    const double rawFixedSteps = std::floor(accumulatedSeconds_ / config_.fixedDeltaTime);
    pendingFixedSteps_ = static_cast<std::uint32_t>(rawFixedSteps);
}

bool FrameClock::shouldRunFixedStep() const {
    return pendingFixedSteps_ > 0;
}

void FrameClock::consumeFixedStep() {
    if (!shouldRunFixedStep()) {
        return;
    }

    --pendingFixedSteps_;
    accumulatedSeconds_ = std::max(0.0, accumulatedSeconds_ - config_.fixedDeltaTime);
}

double FrameClock::alpha() const {
    const double alphaValue = accumulatedSeconds_ / config_.fixedDeltaTime;
    return std::clamp(alphaValue, 0.0, 1.0);
}

std::uint32_t FrameClock::pendingFixedSteps() const noexcept {
    return pendingFixedSteps_;
}

}  // namespace safecrowd::engine
