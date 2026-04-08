#pragma once

#include "engine/EngineWorld.h"

namespace safecrowd::engine::internal {

class EngineWorldFactory {
public:
    EngineWorldFactory() = delete;

    [[nodiscard]] static EngineWorld create(EcsCore& core, CommandBuffer& buffer) {
        return EngineWorld(EngineWorld::ConstructionToken{}, core, buffer);
    }
};

}  // namespace safecrowd::engine::internal
