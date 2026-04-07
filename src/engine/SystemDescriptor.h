#pragma once

#include "engine/TriggerPolicy.h"
#include "engine/UpdatePhase.h"

namespace safecrowd::engine {

struct SystemDescriptor {
    UpdatePhase   phase{UpdatePhase::FixedSimulation};
    int           order{0};
    TriggerPolicy triggerPolicy{TriggerPolicy::FixedStep};
};

}  // namespace safecrowd::engine
