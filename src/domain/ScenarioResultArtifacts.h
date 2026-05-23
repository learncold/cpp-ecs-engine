#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "domain/Geometry2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"

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
    double targetTimeSeconds{0.0};
    std::optional<double> marginSeconds{};
    std::optional<SimulationFrame> t90Frame{};
    std::optional<SimulationFrame> t95Frame{};
};

struct DensityCellMetric {
    Point2D center{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::string floorId{};
    std::size_t agentCount{0};
    double densityPeoplePerSquareMeter{0.0};
};

struct DensityFieldSnapshot {
    double timeSeconds{0.0};
    double cellSizeMeters{0.0};
    std::vector<DensityCellMetric> cells{};
};

struct DensitySummary {
    double cellSizeMeters{0.0};
    double highDensityThresholdPeoplePerSquareMeter{4.0};
    double peakDensityPeoplePerSquareMeter{0.0};
    std::size_t peakAgentCount{0};
    std::optional<double> peakAtSeconds{};
    std::optional<DensityCellMetric> peakCell{};
    double highDensityDurationSeconds{0.0};
    std::vector<DensityCellMetric> peakCells{};
    DensityFieldSnapshot peakField{};
};

struct PressureCellMetric {
    Point2D center{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::string floorId{};
    std::size_t agentCount{0};
    std::size_t intrudingPairCount{0};
    double densityPeoplePerSquareMeter{0.0};
    double pressureScore{0.0};
};

struct PressureFieldSnapshot {
    double timeSeconds{0.0};
    double cellSizeMeters{0.0};
    std::vector<PressureCellMetric> cells{};
};

struct PressureSummary {
    double cellSizeMeters{0.0};
    double hotspotScoreThreshold{0.0};
    double criticalCompressionForceThreshold{0.0};
    double criticalExposureThresholdSeconds{0.0};
    double criticalEventDurationThresholdSeconds{0.0};
    std::size_t criticalEventAgentThreshold{0};
    double peakPressureScore{0.0};
    std::optional<double> peakAtSeconds{};
    std::optional<PressureCellMetric> peakCell{};
    std::size_t peakExposedAgentCount{0};
    std::size_t peakCriticalAgentCount{0};
    std::vector<PressureCellMetric> peakCells{};
    PressureFieldSnapshot peakField{};
    std::vector<ScenarioPressureHotspot> peakHotspots{};
    std::vector<ScenarioPressureAgentMetric> peakAgents{};
    std::vector<ScenarioCriticalPressureEvent> criticalEvents{};
};

struct HazardExposureMetric {
    std::string hazardId{};
    std::string hazardName{};
    EnvironmentHazardKind kind{EnvironmentHazardKind::Fire};
    ScenarioElementSeverity severity{ScenarioElementSeverity::Medium};
    std::string affectedZoneId{};
    std::string floorId{};
    Point2D position{};
    double exposedAgentSeconds{0.0};
    std::size_t peakExposedAgentCount{0};
    std::optional<double> firstExposureSeconds{};
    std::optional<double> peakAtSeconds{};
    double exposureScore{0.0};
};

struct HazardExposureSummary {
    double totalExposureScore{0.0};
    std::vector<HazardExposureMetric> hazards{};
};

struct ConnectionUsageMetric {
    std::string connectionId{};
    std::string label{};
    std::string floorId{};
    std::size_t traversalCount{0};
    double usageRatio{0.0};
    std::size_t peakWindowCount{0};
    std::optional<double> peakAtSeconds{};
    std::size_t forwardTraversals{0};
    std::size_t reverseTraversals{0};
    double queueExposureAgentSeconds{0.0};
    std::size_t peakQueuedAgents{0};
    double averageObservedSpeed{0.0};
    double peakConflictScore{0.0};
    double longestConflictDurationSeconds{0.0};
    std::size_t counterflowEventCount{0};
};

struct OperationalConflictTimelineSample {
    double timeSeconds{0.0};
    double peakConflictScore{0.0};
    std::size_t activeConflictCellCount{0};
    std::size_t activeConflictConnectionCount{0};
    std::size_t queuedAgentsNearConnections{0};
};

struct OperationalConflictSummary {
    double peakConflictScore{0.0};
    std::optional<double> peakAtSeconds{};
    double totalConflictExposureAgentSeconds{0.0};
    double longestConflictDurationSeconds{0.0};
    std::size_t counterflowHotspotCount{0};
    std::size_t conflictConnectionCount{0};
    double connectionConcentrationIndex{0.0};
    std::size_t peakQueuedAgents{0};
    std::string topConflictConnectionId{};
    std::string topConflictConnectionLabel{};
};

struct ExitUsageMetric {
    std::string exitZoneId{};
    std::string exitLabel{};
    std::string floorId{};
    std::size_t evacuatedCount{0};
    double usageRatio{0.0};
    std::optional<double> lastExitTimeSeconds{};
};

struct ZoneCompletionMetric {
    std::string zoneId{};
    std::string zoneLabel{};
    std::string floorId{};
    std::size_t initialCount{0};
    std::size_t evacuatedCount{0};
    std::optional<double> lastCompletionTimeSeconds{};
};

struct PlacementCompletionMetric {
    std::string placementId{};
    std::string zoneId{};
    std::string floorId{};
    std::size_t initialCount{0};
    std::size_t evacuatedCount{0};
    std::optional<double> lastCompletionTimeSeconds{};
};

struct ScenarioResultArtifacts {
    std::vector<EvacuationProgressSample> evacuationProgress{};
    std::vector<SimulationFrame> replayFrames{};
    EvacuationTimingSummary timingSummary{};
    DensitySummary densitySummary{};
    PressureSummary pressureSummary{};
    HazardExposureSummary hazardExposureSummary{};
    OperationalConflictSummary operationalConflictSummary{};
    std::vector<ConnectionUsageMetric> connectionUsage{};
    std::vector<OperationalConflictTimelineSample> operationalConflictTimeline{};
    std::vector<ExitUsageMetric> exitUsage{};
    std::vector<ZoneCompletionMetric> zoneCompletion{};
    std::vector<PlacementCompletionMetric> placementCompletion{};
};

}  // namespace safecrowd::domain
