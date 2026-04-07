#pragma once

#include <memory>
#include <vector>

#include "engine/EcsCore.h"
#include "engine/EngineSystem.h"
#include "engine/SystemDescriptor.h"
#include "engine/UpdatePhase.h"

namespace safecrowd::engine {

class SystemScheduler {
public:
    SystemScheduler(EcsCore& core, CommandBuffer& buffer)
        : core_(core), buffer_(buffer) {}

    void registerSystem(std::unique_ptr<EngineSystem> system, SystemDescriptor descriptor);
    void configure(EngineWorld& world);
    void executePhase(UpdatePhase phase, EngineWorld& world, const EngineStepContext& ctx);

private:
    struct Entry {
        std::unique_ptr<EngineSystem> system;
        SystemDescriptor              descriptor;
    };

    EcsCore&       core_;
    CommandBuffer& buffer_;
    std::vector<Entry> entries_;
};

}  // namespace safecrowd::engine
