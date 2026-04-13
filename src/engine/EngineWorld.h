#pragma once

#include "engine/CommandBuffer.h"
#include "engine/ResourceStore.h"
#include "engine/WorldQuery.h"

namespace safecrowd::engine {

namespace internal {
class EngineWorldFactory;
}

class EngineWorld {
public:
    [[nodiscard]] WorldQuery& query() noexcept { return query_; }
    [[nodiscard]] const WorldQuery& query() const noexcept { return query_; }
    [[nodiscard]] WorldResources& resources() noexcept { return resources_; }
    [[nodiscard]] const WorldResources& resources() const noexcept { return resources_; }
    [[nodiscard]] WorldCommands& commands() noexcept { return commands_; }

private:
    class ConstructionToken {
    private:
        ConstructionToken() = default;

        friend class EngineRuntime;
        friend class internal::EngineWorldFactory;
    };

    EngineWorld() = delete;
    explicit EngineWorld(ConstructionToken, EcsCore& core, ResourceStore& resources,
                         CommandBuffer& buffer)
        : query_(core), resources_(resources), commands_(buffer) {}

    friend class EngineRuntime;
    friend class internal::EngineWorldFactory;

    WorldQuery     query_;
    WorldResources resources_;
    WorldCommands  commands_;
};

}  // namespace safecrowd::engine
