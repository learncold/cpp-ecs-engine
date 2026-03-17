#pragma once

#include <cstddef>
#include <cstdint>

namespace ecs_engine
{
struct EngineStats
{
    std::uint64_t frameCount{0};
    std::uint64_t fixedStepCount{0};
    double wallTimeSeconds{0.0};
    double simulationTimeSeconds{0.0};
    double lagSeconds{0.0};
    std::size_t registeredSystems{0};
};
} // namespace ecs_engine
