#pragma once

#include <cstddef>

namespace ecs_engine
{
struct EngineConfig
{
    double fixedTimeStepSeconds{1.0 / 60.0};
    double maxFrameDeltaSeconds{0.25};
    std::size_t maxCatchUpSteps{8};

    [[nodiscard]] bool isValid() const noexcept
    {
        return fixedTimeStepSeconds > 0.0
            && maxFrameDeltaSeconds >= fixedTimeStepSeconds
            && maxCatchUpSteps > 0;
    }
};
} // namespace ecs_engine
