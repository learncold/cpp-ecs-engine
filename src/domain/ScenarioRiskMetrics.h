#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/Geometry2D.h"
#include "domain/PressureTuning.h"
#include "domain/ScenarioSimulationFrame.h"

namespace safecrowd::domain {

inline constexpr double kScenarioStalledSpeedThreshold = 0.12;
inline constexpr double kScenarioStalledSecondsThreshold = 0.75;
inline constexpr double kScenarioHotspotCellSize = kPressureHotspotCellSizeMeters;
inline constexpr double kScenarioHotspotDensityThresholdPeoplePerSquareMeter =
    kPressureHighDensityThresholdPeoplePerSquareMeter;
inline constexpr double kScenarioPressureHotspotDensityThresholdPeoplePerSquareMeter =
    kPressureHighDensityThresholdPeoplePerSquareMeter;
inline constexpr double kScenarioPressureScoreThreshold = kPressureCriticalScoreThreshold;
inline constexpr double kScenarioPressureFeedbackForceThreshold = 0.18;
inline constexpr double kScenarioPressureFeedbackExposureRecoveryPerSecond = 1.0;
inline constexpr double kScenarioPressureFeedbackNeighborProbeRadius = kPressureReferenceDistanceMeters;
inline constexpr std::size_t kScenarioPressureFeedbackDenseNeighborThreshold = 2;
inline constexpr std::uint64_t kScenarioPressureFeedbackCrowdedUpdateDivisor = 2;
inline constexpr std::uint64_t kScenarioPressureFeedbackQuietUpdateDivisor = 3;
inline constexpr double kScenarioPressureFeedbackMaxExposedSlowdown = 0.10;
inline constexpr double kScenarioPressureFeedbackMaxCriticalSlowdown = 0.25;
inline constexpr double kScenarioPressureFeedbackMaxAvoidanceScale = 1.35;
inline constexpr double kScenarioPressureFeedbackMaxBarrierScale = 1.25;
inline constexpr double kScenarioCriticalPressureForceThreshold = kPressureCriticalScoreThreshold;
inline constexpr double kScenarioCriticalPressureExposureThresholdSeconds =
    kPressureCriticalExposureThresholdSeconds;
inline constexpr double kScenarioCriticalPressureEventDurationThresholdSeconds = 1.0;
inline constexpr std::size_t kScenarioCriticalPressureEventAgentThreshold = 2;
inline constexpr double kScenarioBottleneckRadius = 1.25;
inline constexpr std::size_t kScenarioBottleneckAgentThreshold = 3;
inline constexpr double kScenarioOperationalConflictCellSize = 2.0;
inline constexpr double kScenarioOperationalConflictInfluenceRadius = 1.41;
inline constexpr double kScenarioOperationalConflictReferenceSpeedMetersPerSecond = 1.30;
inline constexpr double kScenarioOperationalConflictMinimumExpectedSpeedMetersPerSecond = 0.97;
inline constexpr std::size_t kScenarioOperationalConflictDirectionBinCount = 16;
inline constexpr std::size_t kScenarioOperationalConflictMinMovingAgents = 4;
inline constexpr double kScenarioOperationalConflictSideRatioThreshold = 0.30;
inline constexpr double kScenarioOperationalConflictCosineThreshold = -0.5;
inline constexpr double kScenarioOperationalConflictQueueSpeedThreshold = kScenarioStalledSpeedThreshold;
inline constexpr double kScenarioOperationalConflictWindowSeconds = 5.0;

enum class ScenarioRiskLevel {
    Low,
    Medium,
    High,
};

struct ScenarioCongestionHotspot {
    Point2D center{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::string floorId{};
    std::size_t agentCount{0};
    std::optional<double> detectedAtSeconds{};
    std::optional<SimulationFrame> detectionFrame{};
};

struct ScenarioPressureHotspot {
    Point2D center{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::string floorId{};
    std::size_t agentCount{0};
    std::size_t intrudingPairCount{0};
    double densityPeoplePerSquareMeter{0.0};
    double pressureScore{0.0};
    std::optional<double> detectedAtSeconds{};
    std::optional<SimulationFrame> detectionFrame{};
};

struct ScenarioPressureAgentMetric {
    std::uint64_t agentId{0};
    Point2D position{};
    std::string floorId{};
    double compressionForce{0.0};
    double exposureSeconds{0.0};
    bool critical{false};
};

struct ScenarioCriticalPressureEvent {
    Point2D center{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::string floorId{};
    std::size_t exposedAgentCount{0};
    std::size_t criticalAgentCount{0};
    double pressureScore{0.0};
    double startedAtSeconds{0.0};
    double durationSeconds{0.0};
    std::optional<double> detectedAtSeconds{};
    std::optional<SimulationFrame> detectionFrame{};
};

struct ScenarioBottleneckMetric {
    std::string connectionId{};
    std::string label{};
    std::string floorId{};
    LineSegment2D passage{};
    std::size_t nearbyAgentCount{0};
    std::size_t stalledAgentCount{0};
    double averageSpeed{0.0};
    std::optional<double> detectedAtSeconds{};
    std::optional<SimulationFrame> detectionFrame{};
};

struct ScenarioOperationalConflictCellMetric {
    Point2D center{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::string floorId{};
    std::size_t movingAgentCount{0};
    std::size_t peakAgentCount{0};
    std::size_t forwardCount{0};
    std::size_t reverseCount{0};
    double counterflowRatio{0.0};
    double averageSpeed{0.0};
    double speedDropRatio{0.0};
    double conflictScore{0.0};
    double durationSeconds{0.0};
    double exposureAgentSeconds{0.0};
    std::string nearestConnectionId{};
    std::string nearestConnectionLabel{};
    std::optional<double> detectedAtSeconds{};
    std::optional<SimulationFrame> detectionFrame{};
};

struct ScenarioOperationalConflictConnectionMetric {
    std::string connectionId{};
    std::string label{};
    std::string floorId{};
    LineSegment2D passage{};
    std::size_t nearbyAgentCount{0};
    std::size_t movingAgentCount{0};
    std::size_t queueAgentCount{0};
    std::size_t forwardCount{0};
    std::size_t reverseCount{0};
    double counterflowRatio{0.0};
    double averageSpeed{0.0};
    double speedDropRatio{0.0};
    double conflictScore{0.0};
    double durationSeconds{0.0};
    double exposureAgentSeconds{0.0};
    std::optional<double> detectedAtSeconds{};
    std::optional<SimulationFrame> detectionFrame{};
};

struct ScenarioRiskSnapshot {
    ScenarioRiskLevel completionRisk{ScenarioRiskLevel::Low};
    std::size_t stalledAgentCount{0};
    std::size_t pressureExposedAgentCount{0};
    std::size_t criticalPressureAgentCount{0};
    std::size_t conflictAgentCount{0};
    double peakConflictScore{0.0};
    double totalConflictExposureAgentSeconds{0.0};
    std::vector<ScenarioCongestionHotspot> hotspots{};
    std::vector<ScenarioPressureHotspot> pressureHotspots{};
    std::vector<ScenarioPressureAgentMetric> pressureAgents{};
    std::vector<ScenarioCriticalPressureEvent> criticalPressureEvents{};
    std::vector<ScenarioBottleneckMetric> bottlenecks{};
    std::vector<ScenarioOperationalConflictCellMetric> operationalConflictCells{};
    std::vector<ScenarioOperationalConflictConnectionMetric> operationalConflictConnections{};
};

const char* scenarioRiskLevelLabel(ScenarioRiskLevel level) noexcept;
const char* scenarioRiskDefinition() noexcept;
const char* scenarioStalledDefinition() noexcept;
const char* scenarioHotspotDefinition() noexcept;
const char* scenarioPressureHotspotDefinition() noexcept;
const char* scenarioBottleneckDefinition() noexcept;
const char* scenarioOperationalConflictDefinition() noexcept;
bool scenarioAgentStalled(double speedMetersPerSecond, double routeStalledSeconds) noexcept;

}  // namespace safecrowd::domain
