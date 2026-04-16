#pragma once

#include "engine/EngineRuntime.h"

namespace safecrowd::engine::internal {

class EngineRuntimeTestAccess {
public:
    [[nodiscard]] static DeterministicRng& rng(EngineRuntime& runtime) noexcept {
        return runtime.rng_;
    }

    [[nodiscard]] static const DeterministicRng& rng(
        const EngineRuntime& runtime) noexcept {
        return runtime.rng_;
    }
};

}  // namespace safecrowd::engine::internal
