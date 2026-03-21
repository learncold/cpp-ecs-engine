#pragma once

#include <cstdint>

#include "engine/EngineConfig.h"

namespace safecrowd::engine {

class FrameClock {
public:
    explicit FrameClock(const EngineConfig& config = {});

    void reset();
    void beginFrame(double deltaSeconds);
    bool shouldRunFixedStep() const;
    void consumeFixedStep();
    double alpha() const;
    std::uint32_t pendingFixedSteps() const noexcept;

private:
    EngineConfig config_;
    double accumulatedSeconds_{0.0};
    std::uint32_t pendingFixedSteps_{0};
};

}  // namespace safecrowd::engine
