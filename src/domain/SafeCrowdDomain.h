#pragma once

#include <cstdint>

#include "domain/Snapshot.h"
#include "engine/EngineRuntime.h"
#include "engine/EngineState.h"

namespace safecrowd::domain {

struct SimulationSummary {
    engine::EngineState state{engine::EngineState::Stopped};
    std::uint64_t frameIndex{0};
    std::uint64_t fixedStepIndex{0};
    double alpha{0.0};
};

class SafeCrowdDomain {
public:
    explicit SafeCrowdDomain(engine::EngineRuntime& runtime);

    void start();
    void pause();
    void stop();
    void update(double deltaSeconds);

    SimulationSummary summary() const;
    SimulationSnapshot snapshot() const;
    engine::EngineRuntime& runtime() noexcept;
    const engine::EngineRuntime& runtime() const noexcept;

private:
    engine::EngineRuntime& runtime_;
};

}  // namespace safecrowd::domain
