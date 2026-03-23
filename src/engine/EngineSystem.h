#pragma once

#include "engine/EngineStepContext.h"

namespace safecrowd::engine {

class EngineWorld {
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
