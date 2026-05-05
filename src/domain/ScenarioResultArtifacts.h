#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "domain/Geometry2D.h"
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
    std::vector<ExitUsageMetric> exitUsage{};
    std::vector<ZoneCompletionMetric> zoneCompletion{};
    std::vector<PlacementCompletionMetric> placementCompletion{};
};

}  // namespace safecrowd::domain
