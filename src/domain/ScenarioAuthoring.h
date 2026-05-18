#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "domain/FacilityLayout2D.h"
#include "domain/PopulationSpec.h"

namespace safecrowd::domain {

enum class ScenarioRole {
    Baseline,
    Alternative,
    Recommended,
};

enum class EnvironmentHazardKind {
    Fire,
    Smoke,
};

enum class ScenarioElementSeverity {
    Low,
    Medium,
    High,
};

struct EnvironmentHazardDraft {
    std::string id{};
    EnvironmentHazardKind kind{EnvironmentHazardKind::Fire};
    std::string name{};
    std::string affectedZoneId{};
    std::string floorId{};
    Point2D position{};
    double startSeconds{0.0};
    double endSeconds{0.0};
    ScenarioElementSeverity severity{ScenarioElementSeverity::Medium};
    std::string note{};
};

struct EnvironmentHazardRuntimeProfile {
    double radiusMeters{0.0};
    double speedFactor{1.0};
    double routePenaltyMeters{0.0};
    double severityWeight{1.0};
};

struct EnvironmentState {
    bool reducedVisibility{false};
    std::string familiarityProfile{};
    std::string guidanceProfile{};
    std::vector<EnvironmentHazardDraft> hazards{};
};

struct OperationalEventDraft {
    std::string id{};
    std::string name{};
    std::string triggerSummary{};
    std::string targetSummary{};
};

struct RouteGuidancePeriodDraft {
    double startSeconds{0.0};
    double endSeconds{0.0};
};

struct RouteGuidanceDraft {
    std::string id{};
    double startSeconds{0.0};
    double endSeconds{10.0};
    std::vector<RouteGuidancePeriodDraft> periods{};
    std::string guidedExitZoneId{};
    std::string installConnectionId{};
    std::string installFloorId{};
    std::string installZoneId{};
    Point2D installPosition{};
    double baseComplianceRate{0.5};
    double guidanceStrength{0.55};
    double maxDetourMeters{20.0};
};

struct ConnectionBlockIntervalDraft {
    double startSeconds{0.0};
    double endSeconds{0.0};
};

struct ConnectionBlockDraft {
    std::string id{};
    std::string connectionId{};
    std::vector<ConnectionBlockIntervalDraft> intervals{};
};

struct ControlPlan {
    std::vector<OperationalEventDraft> events{};
    std::vector<RouteGuidanceDraft> routeGuidances{};
    std::vector<ConnectionBlockDraft> connectionBlocks{};
};

struct ExecutionConfig {
    double timeLimitSeconds{0.0};
    double sampleIntervalSeconds{0.0};
    std::uint32_t repeatCount{1};
    std::uint32_t baseSeed{0};
    bool recordOccupantHistory{false};
};

struct ScenarioDraft {
    std::string scenarioId{};
    std::string name{};
    ScenarioRole role{ScenarioRole::Alternative};
    PopulationSpec population{};
    EnvironmentState environment{};
    ControlPlan control{};
    ExecutionConfig execution{};
    std::string sourceTemplateId{};
    std::vector<std::string> variationDiffKeys{};
    std::vector<std::string> blockingIssues{};
};

struct ProjectWorkspaceSnapshot {
    std::string projectId{};
    std::string displayName{};
    FacilityLayout2D layout{};
    std::vector<ScenarioDraft> scenarios{};
};

ScenarioDraft duplicateScenarioDraft(const ScenarioDraft& source,
                                    std::string newScenarioId,
                                    std::string newName);

std::vector<std::string> computeScenarioDiffKeys(const ScenarioDraft& baseline,
                                                 const ScenarioDraft& variant);

double environmentHazardRadiusMeters(ScenarioElementSeverity severity);
double environmentHazardRoutePenaltyMeters(ScenarioElementSeverity severity);
double environmentHazardSeverityWeight(ScenarioElementSeverity severity);
double environmentHazardSpeedFactor(EnvironmentHazardKind kind, ScenarioElementSeverity severity);
double environmentHazardSmokeVisibilityMetersAt(const EnvironmentHazardDraft& hazard, double distanceMeters);
double environmentHazardSmokeSpeedMetersPerSecond(double smokeFreeSpeedMetersPerSecond, double visibilityMeters);
double environmentHazardSpeedFactorAt(
    const EnvironmentHazardDraft& hazard,
    double distanceMeters,
    double smokeFreeSpeedMetersPerSecond);
EnvironmentHazardRuntimeProfile environmentHazardRuntimeProfile(const EnvironmentHazardDraft& hazard);
bool environmentHazardHasOpenEndedSchedule(const EnvironmentHazardDraft& hazard);
bool environmentHazardActiveAt(const EnvironmentHazardDraft& hazard, double elapsedSeconds);
std::string environmentHazardFloorId(const FacilityLayout2D& layout, const EnvironmentHazardDraft& hazard);
bool connectionBlockIntervalActiveAt(const ConnectionBlockIntervalDraft& interval, double elapsedSeconds);
bool connectionBlockActiveAt(const ConnectionBlockDraft& block, double elapsedSeconds);

}  // namespace safecrowd::domain
