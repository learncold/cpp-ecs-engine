#pragma once

#include <cstdint>

#include "engine/EngineState.h"

namespace safecrowd::engine {

struct EngineStats {
    EngineState state{EngineState::Stopped};
    std::uint64_t frameIndex{0};
    std::uint64_t fixedStepIndex{0};
    std::uint32_t fixedStepsThisFrame{0};
    double alpha{0.0};
};

}  // namespace safecrowd::engine
