#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"

namespace safecrowd::domain {

enum class AlternativeRecommendationKind {
    BlockedConnectionRelief,
    BottleneckBypassGuidance,
    ExitUsageBalancing,
    PressureHotspotRelief,
};

struct AlternativeRecommendationEvidence {
    std::string label{};
    std::string value{};
    std::string source{};
};

struct AlternativeRecommendationCandidate {
    std::string id{};
    AlternativeRecommendationKind kind{AlternativeRecommendationKind::BlockedConnectionRelief};
    int priority{0};
    std::string title{};
    std::string summary{};
    std::string expectedImprovement{};
    std::string artifactSource{};
    std::vector<AlternativeRecommendationEvidence> evidence{};
    ScenarioDraft recommendedScenario{};
};

struct AlternativeRecommendationRequest {
    FacilityLayout2D layout{};
    ScenarioDraft sourceScenario{};
    std::optional<ScenarioDraft> baselineScenario{};
    ScenarioRiskSnapshot risk{};
    ScenarioResultArtifacts artifacts{};
};

struct AlternativeRecommendationResult {
    std::vector<AlternativeRecommendationCandidate> candidates{};
    std::vector<std::string> blockingReasons{};
};

class AlternativeRecommendationService {
public:
    AlternativeRecommendationResult recommend(const AlternativeRecommendationRequest& request) const;
};

const char* alternativeRecommendationKindId(AlternativeRecommendationKind kind) noexcept;

}  // namespace safecrowd::domain
