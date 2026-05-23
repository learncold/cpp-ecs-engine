#include "domain/ScenarioBatchRunner.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace safecrowd::domain {
namespace {

std::uint32_t normalizedRepeatCount(std::uint32_t repeatCount) {
    return std::clamp<std::uint32_t>(repeatCount, 1, kScenarioExecutionMaxRepeatCount);
}

std::uint32_t seedForRepeat(std::uint32_t baseSeed, std::uint32_t zeroBasedRepeatIndex) {
    constexpr auto kMaxSeed = std::numeric_limits<std::uint32_t>::max();
    const auto base = static_cast<std::uint64_t>(baseSeed == 0 ? 1 : baseSeed);
    const auto candidate = base + static_cast<std::uint64_t>(zeroBasedRepeatIndex);
    if (candidate <= kMaxSeed) {
        return static_cast<std::uint32_t>(candidate);
    }
    return static_cast<std::uint32_t>(((candidate - 1) % kMaxSeed) + 1);
}

ScenarioDraft makeRunScenario(ScenarioDraft scenario, std::uint32_t repeatIndex, std::uint32_t repeatCount) {
    const auto runSeed = seedForRepeat(scenario.execution.baseSeed, repeatIndex - 1);
    if (repeatCount > 1) {
        const auto repeatText = std::to_string(repeatIndex);
        const auto repeatTotalText = std::to_string(repeatCount);
        scenario.scenarioId = scenario.scenarioId.empty()
            ? "scenario-repeat-" + repeatText
            : scenario.scenarioId + "-repeat-" + repeatText;
        scenario.name = scenario.name.empty()
            ? "Scenario run " + repeatText + "/" + repeatTotalText
            : scenario.name + " (run " + repeatText + "/" + repeatTotalText + ", seed " + std::to_string(runSeed) + ")";
        scenario.execution.baseSeed = runSeed;
    }
    scenario.execution.repeatCount = 1;
    return scenario;
}

}  // namespace

ScenarioBatchRunner::ScenarioBatchRunner(FacilityLayout2D layout, std::vector<ScenarioDraft> scenarios) {
    reset(std::move(layout), std::move(scenarios));
}

void ScenarioBatchRunner::reset(FacilityLayout2D layout, std::vector<ScenarioDraft> scenarios) {
    layout_ = std::move(layout);
    runners_.clear();
    runs_.clear();
    std::size_t runCount = 0;
    for (const auto& scenario : scenarios) {
        runCount += normalizedRepeatCount(scenario.execution.repeatCount);
    }
    runners_.reserve(runCount);
    runs_.reserve(runCount);

    for (std::size_t scenarioIndex = 0; scenarioIndex < scenarios.size(); ++scenarioIndex) {
        const auto& scenario = scenarios[scenarioIndex];
        const auto repeatCount = normalizedRepeatCount(scenario.execution.repeatCount);
        for (std::uint32_t repeatIndex = 1; repeatIndex <= repeatCount; ++repeatIndex) {
            auto runScenario = makeRunScenario(scenario, repeatIndex, repeatCount);
            const auto runSeed = runScenario.execution.baseSeed == 0 ? 1 : runScenario.execution.baseSeed;
            runners_.emplace_back(layout_, runScenario);
            runs_.push_back({
                .scenario = std::move(runScenario),
                .sourceScenarioIndex = scenarioIndex,
                .repeatIndex = repeatIndex,
                .repeatCount = repeatCount,
                .runSeed = runSeed,
            });
        }
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
