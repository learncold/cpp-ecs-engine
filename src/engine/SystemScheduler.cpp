#include "engine/SystemScheduler.h"

#include <algorithm>
#include <stdexcept>

namespace safecrowd::engine {
namespace {

void validateDescriptor(const SystemDescriptor& descriptor) {
    if (descriptor.triggerPolicy == TriggerPolicy::Interval &&
        descriptor.intervalTicks == 0) {
        throw std::invalid_argument(
            "TriggerPolicy::Interval requires intervalTicks > 0.");
    }

    switch (descriptor.phase) {
    case UpdatePhase::Startup:
        if (descriptor.triggerPolicy != TriggerPolicy::EveryFrame) {
            throw std::invalid_argument(
                "Startup systems must use TriggerPolicy::EveryFrame.");
        }
        break;

    case UpdatePhase::FixedSimulation:
        if (descriptor.triggerPolicy == TriggerPolicy::EveryFrame) {
            throw std::invalid_argument(
                "FixedSimulation systems must use TriggerPolicy::FixedStep or "
                "TriggerPolicy::Interval.");
        }
        break;

    case UpdatePhase::PreSimulation:
    case UpdatePhase::PostSimulation:
    case UpdatePhase::RenderSync:
        if (descriptor.triggerPolicy == TriggerPolicy::FixedStep) {
            throw std::invalid_argument(
                "Frame phases must use TriggerPolicy::EveryFrame or "
                "TriggerPolicy::Interval.");
        }
        break;
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

void SystemScheduler::executePhase(UpdatePhase phase, EngineWorld& world,
                                   const EngineStepContext& ctx) {
    auto shouldExecuteInterval = [](Entry& entry) {
        if (entry.intervalCountdown == 0) {
            entry.intervalCountdown = entry.descriptor.intervalTicks - 1;
            return true;
        }

        --entry.intervalCountdown;
        return false;
    };

    for (auto& e : entries_) {
        if (e.descriptor.phase != phase) {
            continue;
        }

        bool shouldExecute = false;
        switch (e.descriptor.triggerPolicy) {
        case TriggerPolicy::EveryFrame:
        case TriggerPolicy::FixedStep:
            shouldExecute = true;
            break;

        case TriggerPolicy::Interval:
            shouldExecute = shouldExecuteInterval(e);
            break;
        }

        if (shouldExecute) {
            e.system->update(world, ctx);
        }
    }
    buffer_.flush(core_);
}

void SystemScheduler::resetCadenceState() {
    for (auto& e : entries_) {
        e.intervalCountdown = 0;
    }
}

}  // namespace safecrowd::engine
