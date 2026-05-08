#pragma once

#include <cstddef>
#include <vector>

#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"
#include "domain/ScenarioSimulationRunner.h"

namespace safecrowd::domain {

struct ScenarioBatchRunState {
    ScenarioDraft scenario{};
    SimulationFrame frame{};
    ScenarioRiskSnapshot risk{};
    ScenarioRiskSnapshot resultRisk{};
    ScenarioResultArtifacts artifacts{};
    double timeLimitSeconds{0.0};
    bool complete{false};
    bool resultSynced{false};
};

class ScenarioBatchRunner {
public:
    ScenarioBatchRunner() = default;
    ScenarioBatchRunner(FacilityLayout2D layout, std::vector<ScenarioDraft> scenarios);

    void reset(FacilityLayout2D layout, std::vector<ScenarioDraft> scenarios);
    void step(double deltaSeconds);

    const std::vector<ScenarioBatchRunState>& runs() const noexcept;
    const ScenarioBatchRunState& run(std::size_t index) const;
    void syncResultArtifacts();
    bool empty() const noexcept;
    bool complete() const noexcept;
    std::size_t size() const noexcept;

private:
    void syncLiveRuns();
    void syncResultRun(std::size_t index);

    FacilityLayout2D layout_{};
    std::vector<ScenarioSimulationRunner> runners_{};
    std::vector<ScenarioBatchRunState> runs_{};
};

}  // namespace safecrowd::domain
