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

struct OccupancyHeatmapCell {
    Point2D center{};
    Point2D cellMin{};
    Point2D cellMax{};
    std::string floorId{};
    double accumulatedAgentSeconds{0.0};
    double normalizedIntensity{0.0};
};

struct OccupancyHeatmap {
    double cellSizeMeters{0.0};
    double kernelRadiusMeters{0.0};
    double accumulatedSeconds{0.0};
    double peakAccumulatedAgentSeconds{0.0};
    std::vector<OccupancyHeatmapCell> cells{};
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

struct CrossFlowTimelineSample {
    double timeSeconds{0.0};
    double peakCrossFlowScore{0.0};
    std::size_t activeCrossFlowCellCount{0};
};

struct CrossFlowSummary {
    double peakCrossFlowScore{0.0};
    std::optional<double> peakAtSeconds{};
    double totalCrossFlowExposureAgentSeconds{0.0};
    double longestCrossFlowDurationSeconds{0.0};
    std::size_t crossFlowHotspotCount{0};
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
    OccupancyHeatmap occupancyHeatmap{};
    PressureSummary pressureSummary{};
    HazardExposureSummary hazardExposureSummary{};
    CrossFlowSummary crossFlowSummary{};
    std::vector<CrossFlowTimelineSample> crossFlowTimeline{};
    std::vector<ExitUsageMetric> exitUsage{};
    std::vector<ZoneCompletionMetric> zoneCompletion{};
    std::vector<PlacementCompletionMetric> placementCompletion{};
};

}  // namespace safecrowd::domain
