#pragma once

#include <cstddef>

#include "engine/EngineConfig.h"

namespace ecs_engine
{
struct FramePlan
{
    std::size_t stepsToRun{0};
    double frameDeltaSeconds{0.0};
    double fixedTimeStepSeconds{0.0};
    double remainingLagSeconds{0.0};
};

class FrameClock
{
public:
    explicit FrameClock(EngineConfig config = {}) noexcept;

    [[nodiscard]] const EngineConfig& config() const noexcept;
    [[nodiscard]] double lagSeconds() const noexcept;

    void reset() noexcept;
    [[nodiscard]] FramePlan advance(double frameDeltaSeconds) noexcept;

private:
    EngineConfig config_;
    double lagSeconds_{0.0};
};
} // namespace ecs_engine
