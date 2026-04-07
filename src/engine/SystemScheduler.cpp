#include "engine/SystemScheduler.h"

#include <algorithm>

namespace safecrowd::engine {

void SystemScheduler::registerSystem(std::unique_ptr<EngineSystem> system,
                                     SystemDescriptor descriptor) {
    entries_.push_back({std::move(system), descriptor});
    std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) {
        if (a.descriptor.phase != b.descriptor.phase) {
            return static_cast<int>(a.descriptor.phase) < static_cast<int>(b.descriptor.phase);
        }
        return a.descriptor.order < b.descriptor.order;
    });
}

void SystemScheduler::configure(EngineWorld& world) {
    for (auto& e : entries_) {
        e.system->configure(world);
    }
}

void SystemScheduler::executePhase(UpdatePhase phase, EngineWorld& world,
                                   const EngineStepContext& ctx) {
    for (auto& e : entries_) {
        if (e.descriptor.phase == phase) {
            e.system->update(world, ctx);
        }
    }
    buffer_.flush(core_);
}

}  // namespace safecrowd::engine
