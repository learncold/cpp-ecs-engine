#pragma once

namespace safecrowd::engine {

enum class UpdatePhase {
    Startup,
    PreSimulation,
    FixedSimulation,
    PostSimulation,
    RenderSync,
};

}  // namespace safecrowd::engine
