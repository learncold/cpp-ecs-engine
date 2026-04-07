#include "engine/SystemScheduler.h"

#include <algorithm>
#include <stdexcept>

namespace safecrowd::engine {
namespace {

void validateDescriptor(const SystemDescriptor& descriptor) {
    if (descriptor.triggerPolicy == TriggerPolicy::Interval) {
        throw std::invalid_argument("TriggerPolicy::Interval is not supported yet.");
    }

    if (descriptor.phase == UpdatePhase::FixedSimulation &&
        descriptor.triggerPolicy != TriggerPolicy::FixedStep) {
        throw std::invalid_argument(
            "FixedSimulation systems must use TriggerPolicy::FixedStep.");
    }

    if (descriptor.phase != UpdatePhase::FixedSimulation &&
        descriptor.phase != UpdatePhase::Startup &&
        descriptor.triggerPolicy != TriggerPolicy::EveryFrame) {
        throw std::invalid_argument(
            "Frame phases must use TriggerPolicy::EveryFrame.");
    }
}

}  // namespace

void SystemScheduler::registerSystem(std::unique_ptr<EngineSystem> system,
                                     SystemDescriptor descriptor) {
    validateDescriptor(descriptor);
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
        buffer_.flush(core_);
    }
}

void SystemScheduler::executeStartup(EngineWorld& world, const EngineStepContext& ctx) {
    for (auto& e : entries_) {
        if (e.descriptor.phase == UpdatePhase::Startup) {
            e.system->update(world, ctx);
        }
    }
    buffer_.flush(core_);
}

void SystemScheduler::executePhase(UpdatePhase phase, TriggerPolicy triggerPolicy,
                                   EngineWorld& world, const EngineStepContext& ctx) {
    for (auto& e : entries_) {
        if (e.descriptor.phase == phase &&
            e.descriptor.triggerPolicy == triggerPolicy) {
            e.system->update(world, ctx);
        }
    }
    buffer_.flush(core_);
}

}  // namespace safecrowd::engine
