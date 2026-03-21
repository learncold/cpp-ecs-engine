#pragma once

#include <cstdint>

namespace safecrowd::engine {

struct EngineStepContext {
    std::uint64_t frameIndex{0};
    std::uint64_t fixedStepIndex{0};
    double alpha{0.0};
    std::uint64_t runIndex{0};
    std::uint64_t derivedSeed{0};
};

}  // namespace safecrowd::engine
