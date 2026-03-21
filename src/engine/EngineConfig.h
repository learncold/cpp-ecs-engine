#pragma once

#include <cstdint>

namespace safecrowd::engine {

struct EngineConfig {
    double fixedDeltaTime{1.0 / 60.0};
    std::uint32_t maxCatchUpSteps{4};
    std::uint64_t baseSeed{1};
};

}  // namespace safecrowd::engine
