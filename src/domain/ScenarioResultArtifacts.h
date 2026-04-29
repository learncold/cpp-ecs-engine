#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace safecrowd::domain {

struct EvacuationProgressSample {
    double timeSeconds{0.0};
    std::size_t evacuatedCount{0};
    std::size_t totalCount{0};
    double evacuatedRatio{0.0};
};

struct EvacuationTimingSummary {
    std::optional<double> t50Seconds{};
    std::optional<double> t90Seconds{};
    std::optional<double> t95Seconds{};
    std::optional<double> finalEvacuationTimeSeconds{};
};

struct ScenarioResultArtifacts {
    std::vector<EvacuationProgressSample> evacuationProgress{};
    EvacuationTimingSummary timingSummary{};
};

}  // namespace safecrowd::domain
