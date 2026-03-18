#pragma once

#include <cstdint>

namespace ecs_engine
{
struct EngineStepContext
{
    std::uint64_t frameIndex{0};
    std::uint64_t stepIndex{0};
    double deltaSeconds{0.0};
    double simulationTimeSeconds{0.0};
};
} // namespace ecs_engine
