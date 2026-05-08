#include "domain/ScenarioBatchRunner.h"

#include <stdexcept>
#include <utility>

namespace safecrowd::domain {

ScenarioBatchRunner::ScenarioBatchRunner(FacilityLayout2D layout, std::vector<ScenarioDraft> scenarios) {
    reset(std::move(layout), std::move(scenarios));
}

void ScenarioBatchRunner::reset(FacilityLayout2D layout, std::vector<ScenarioDraft> scenarios) {
    layout_ = std::move(layout);
    runners_.clear();
    runs_.clear();
    runners_.reserve(scenarios.size());
    runs_.reserve(scenarios.size());

    for (auto& scenario : scenarios) {
        runners_.emplace_back(layout_, scenario);
        runs_.push_back({
            .scenario = std::move(scenario),
        });
    }
    syncLiveRuns();
}

void ScenarioBatchRunner::step(double deltaSeconds) {
    if (deltaSeconds <= 0.0) {
        return;
    }
    for (auto& runner : runners_) {
        if (!runner.complete()) {
            runner.step(deltaSeconds);
        }
    }
    syncLiveRuns();
}

const std::vector<ScenarioBatchRunState>& ScenarioBatchRunner::runs() const noexcept {
    return runs_;
}

const ScenarioBatchRunState& ScenarioBatchRunner::run(std::size_t index) const {
    if (index >= runs_.size()) {
        throw std::out_of_range("ScenarioBatchRunner run index is out of range");
    }
    return runs_[index];
}

void ScenarioBatchRunner::syncResultArtifacts() {
    syncLiveRuns();
    for (std::size_t index = 0; index < runners_.size() && index < runs_.size(); ++index) {
        syncResultRun(index);
    }
}

bool ScenarioBatchRunner::empty() const noexcept {
    return runs_.empty();
}

bool ScenarioBatchRunner::complete() const noexcept {
    if (runs_.empty()) {
        return false;
    }
    for (const auto& run : runs_) {
        if (!run.complete) {
            return false;
        }
    }
    return true;
}

std::size_t ScenarioBatchRunner::size() const noexcept {
    return runs_.size();
}

void ScenarioBatchRunner::syncLiveRuns() {
    for (std::size_t index = 0; index < runners_.size() && index < runs_.size(); ++index) {
        const auto& runner = runners_[index];
        auto& run = runs_[index];
        run.frame = runner.frame();
        run.risk = runner.riskSnapshot();
        run.timeLimitSeconds = runner.timeLimitSeconds();
        run.complete = runner.complete();
        if (run.complete && !run.resultSynced) {
            syncResultRun(index);
        }
    }
}

void ScenarioBatchRunner::syncResultRun(std::size_t index) {
    if (index >= runners_.size() || index >= runs_.size()) {
        return;
    }
    const auto& runner = runners_[index];
    auto& run = runs_[index];
    run.resultRisk = runner.resultRiskSnapshot();
    run.artifacts = runner.resultArtifacts();
    run.resultSynced = true;
}

}  // namespace safecrowd::domain
