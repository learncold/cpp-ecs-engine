#pragma once

#include <cstdint>

#include "engine/TriggerPolicy.h"
#include "engine/UpdatePhase.h"

namespace safecrowd::engine {

struct SystemDescriptor {
    UpdatePhase   phase{UpdatePhase::FixedSimulation};
    int           order{0};
    TriggerPolicy triggerPolicy{TriggerPolicy::FixedStep};
    std::uint32_t intervalTicks{1};
};

}  // namespace safecrowd::engine
