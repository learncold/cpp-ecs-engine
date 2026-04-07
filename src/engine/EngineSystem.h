#pragma once

#include "engine/CommandBuffer.h"
#include "engine/EngineStepContext.h"
#include "engine/WorldQuery.h"

namespace safecrowd::engine {

class EngineWorld {
public:
    EngineWorld() = delete;
    explicit EngineWorld(EcsCore& core, CommandBuffer& buffer)
        : query_(core), commands_(buffer) {}

    [[nodiscard]] WorldQuery& query() noexcept { return query_; }
    [[nodiscard]] const WorldQuery& query() const noexcept { return query_; }
    [[nodiscard]] WorldCommands& commands() noexcept { return commands_; }

private:
    WorldQuery query_;
    WorldCommands commands_;
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
