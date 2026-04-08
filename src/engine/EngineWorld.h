#pragma once

#include "engine/CommandBuffer.h"
#include "engine/WorldQuery.h"

namespace safecrowd::engine {

namespace internal {
class EngineWorldFactory;
}

class EngineWorld {
public:
    [[nodiscard]] WorldQuery& query() noexcept { return query_; }
    [[nodiscard]] const WorldQuery& query() const noexcept { return query_; }
    [[nodiscard]] WorldCommands& commands() noexcept { return commands_; }

private:
    class ConstructionToken {
    private:
        ConstructionToken() = default;

        friend class EngineRuntime;
        friend class internal::EngineWorldFactory;
    };

    EngineWorld() = delete;
    explicit EngineWorld(ConstructionToken, EcsCore& core, CommandBuffer& buffer)
        : query_(core), commands_(buffer) {}

    friend class EngineRuntime;
    friend class internal::EngineWorldFactory;

    WorldQuery    query_;
    WorldCommands commands_;
};

}  // namespace safecrowd::engine
