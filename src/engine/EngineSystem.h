#pragma once

#include "engine/EcsCore.h"
#include "engine/EngineStepContext.h"

namespace safecrowd::engine {

class EngineWorld {
public:
    [[nodiscard]] EcsCore& ecsCore() noexcept {
        return ecsCore_;
    }

    [[nodiscard]] const EcsCore& ecsCore() const noexcept {
        return ecsCore_;
    }

    void shutdown() {
        ecsCore_.shutdown();
    }

private:
    EcsCore ecsCore_{};
};

class EngineSystem {
public:
    virtual ~EngineSystem() = default;

    virtual void configure(EngineWorld& world) {
        (void)world;
    }

    virtual void update(EngineWorld& world, const EngineStepContext& step) = 0;
};

}  // namespace safecrowd::engine
