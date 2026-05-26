#include "application/ResultArtifactsCodec.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include <QJsonArray>
#include <QJsonValue>

#include "application/ProjectPersistenceJson.h"

namespace safecrowd::application {
namespace {

std::uint64_t unsignedIntegerFromJson(const QJsonValue& value) {
    if (value.isString()) {
        return value.toString().toULongLong();
    }

    const auto integer = value.toInteger();
    return integer < 0 ? 0 : static_cast<std::uint64_t>(integer);
}

}  // namespace

QJsonObject simulationAgentFrameToJson(const safecrowd::domain::SimulationAgentFrame& agent) {
    QJsonObject object;
    object["id"] = QString::number(static_cast<qulonglong>(agent.id));
    object["position"] = pointArray(agent.position);
    object["velocity"] = pointArray(agent.velocity);
    object["radius"] = agent.radius;
    object["floorId"] = QString::fromStdString(agent.floorId);
    object["stalled"] = agent.stalled;
    return object;
}

safecrowd::domain::SimulationAgentFrame simulationAgentFrameFromJson(const QJsonObject& object) {
    return {
        .id = unsignedIntegerFromJson(object.value("id")),
        .position = pointFromJson(object.value("position")),
        .velocity = pointFromJson(object.value("velocity")),
        .radius = object.value("radius").toDouble(0.25),
        .floorId = object.value("floorId").toString().toStdString(),
        .stalled = object.value("stalled").toBool(false),
    };
}

QJsonObject simulationFrameToJson(const safecrowd::domain::SimulationFrame& frame) {
    QJsonObject object;
    object["elapsedSeconds"] = frame.elapsedSeconds;
    object["complete"] = frame.complete;
    object["totalAgentCount"] = static_cast<qint64>(frame.totalAgentCount);
    object["evacuatedAgentCount"] = static_cast<qint64>(frame.evacuatedAgentCount);
    QJsonArray agents;
    for (const auto& agent : frame.agents) {
        agents.append(simulationAgentFrameToJson(agent));
    }
    object["agents"] = agents;
    return object;
}

safecrowd::domain::SimulationFrame simulationFrameFromJson(const QJsonObject& object) {
    safecrowd::domain::SimulationFrame frame;
    frame.elapsedSeconds = object.value("elapsedSeconds").toDouble();
    frame.complete = object.value("complete").toBool(false);
    frame.totalAgentCount = static_cast<std::size_t>(object.value("totalAgentCount").toInteger());
    frame.evacuatedAgentCount = static_cast<std::size_t>(object.value("evacuatedAgentCount").toInteger());
    for (const auto& value : object.value("agents").toArray()) {
        frame.agents.push_back(simulationAgentFrameFromJson(value.toObject()));
    }
    return frame;
}

QJsonObject hotspotToJson(const safecrowd::domain::ScenarioCongestionHotspot& hotspot) {
    QJsonObject object;
    object["center"] = pointArray(hotspot.center);
    object["cellMin"] = pointArray(hotspot.cellMin);
    object["cellMax"] = pointArray(hotspot.cellMax);
    object["floorId"] = QString::fromStdString(hotspot.floorId);
    object["agentCount"] = static_cast<qint64>(hotspot.agentCount);
    object["detectedAtSeconds"] = optionalDoubleToJson(hotspot.detectedAtSeconds);
    if (hotspot.detectionFrame.has_value()) {
        object["detectionFrame"] = simulationFrameToJson(*hotspot.detectionFrame);
    }
    return object;
}

safecrowd::domain::ScenarioCongestionHotspot hotspotFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioCongestionHotspot hotspot{
        .center = pointFromJson(object.value("center")),
        .cellMin = pointFromJson(object.value("cellMin")),
        .cellMax = pointFromJson(object.value("cellMax")),
        .floorId = object.value("floorId").toString().toStdString(),
        .agentCount = static_cast<std::size_t>(object.value("agentCount").toInteger()),
    };
    hotspot.detectedAtSeconds = optionalDoubleFromJson(object.value("detectedAtSeconds"));
    if (object.value("detectionFrame").isObject()) {
        hotspot.detectionFrame = simulationFrameFromJson(object.value("detectionFrame").toObject());
    }
    return hotspot;
}

QJsonObject bottleneckToJson(const safecrowd::domain::ScenarioBottleneckMetric& bottleneck) {
    QJsonObject object;
    object["connectionId"] = QString::fromStdString(bottleneck.connectionId);
    object["label"] = QString::fromStdString(bottleneck.label);
    object["floorId"] = QString::fromStdString(bottleneck.floorId);
    object["passage"] = lineToJson(bottleneck.passage);
    object["nearbyAgentCount"] = static_cast<qint64>(bottleneck.nearbyAgentCount);
    object["stalledAgentCount"] = static_cast<qint64>(bottleneck.stalledAgentCount);
    object["averageSpeed"] = bottleneck.averageSpeed;
    object["detectedAtSeconds"] = optionalDoubleToJson(bottleneck.detectedAtSeconds);
    if (bottleneck.detectionFrame.has_value()) {
        object["detectionFrame"] = simulationFrameToJson(*bottleneck.detectionFrame);
    }
    return object;
}

safecrowd::domain::ScenarioBottleneckMetric bottleneckFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioBottleneckMetric bottleneck{
        .connectionId = object.value("connectionId").toString().toStdString(),
        .label = object.value("label").toString().toStdString(),
        .floorId = object.value("floorId").toString().toStdString(),
        .passage = lineFromJson(object.value("passage").toObject()),
        .nearbyAgentCount = static_cast<std::size_t>(object.value("nearbyAgentCount").toInteger()),
        .stalledAgentCount = static_cast<std::size_t>(object.value("stalledAgentCount").toInteger()),
        .averageSpeed = object.value("averageSpeed").toDouble(),
    };
    bottleneck.detectedAtSeconds = optionalDoubleFromJson(object.value("detectedAtSeconds"));
    if (object.value("detectionFrame").isObject()) {
        bottleneck.detectionFrame = simulationFrameFromJson(object.value("detectionFrame").toObject());
    }
    return bottleneck;
}

QJsonObject pressureHotspotToJson(const safecrowd::domain::ScenarioPressureHotspot& hotspot) {
    QJsonObject object;
    object["center"] = pointArray(hotspot.center);
    object["cellMin"] = pointArray(hotspot.cellMin);
    object["cellMax"] = pointArray(hotspot.cellMax);
    object["floorId"] = QString::fromStdString(hotspot.floorId);
    object["agentCount"] = static_cast<qint64>(hotspot.agentCount);
    object["intrudingPairCount"] = static_cast<qint64>(hotspot.intrudingPairCount);
    object["densityPeoplePerSquareMeter"] = hotspot.densityPeoplePerSquareMeter;
    object["pressureScore"] = hotspot.pressureScore;
    object["detectedAtSeconds"] = optionalDoubleToJson(hotspot.detectedAtSeconds);
    if (hotspot.detectionFrame.has_value()) {
        object["detectionFrame"] = simulationFrameToJson(*hotspot.detectionFrame);
    }
    return object;
}

safecrowd::domain::ScenarioPressureHotspot pressureHotspotFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioPressureHotspot hotspot{
        .center = pointFromJson(object.value("center")),
        .cellMin = pointFromJson(object.value("cellMin")),
        .cellMax = pointFromJson(object.value("cellMax")),
        .floorId = object.value("floorId").toString().toStdString(),
        .agentCount = static_cast<std::size_t>(object.value("agentCount").toInteger()),
        .intrudingPairCount = static_cast<std::size_t>(object.value("intrudingPairCount").toInteger()),
        .densityPeoplePerSquareMeter = object.value("densityPeoplePerSquareMeter").toDouble(),
        .pressureScore = object.value("pressureScore").toDouble(),
    };
    hotspot.detectedAtSeconds = optionalDoubleFromJson(object.value("detectedAtSeconds"));
    if (object.value("detectionFrame").isObject()) {
        hotspot.detectionFrame = simulationFrameFromJson(object.value("detectionFrame").toObject());
    }
    return hotspot;
}

QJsonObject pressureAgentMetricToJson(const safecrowd::domain::ScenarioPressureAgentMetric& agent) {
    QJsonObject object;
    object["agentId"] = QString::number(static_cast<qulonglong>(agent.agentId));
    object["position"] = pointArray(agent.position);
    object["floorId"] = QString::fromStdString(agent.floorId);
    object["compressionForce"] = agent.compressionForce;
    object["exposureSeconds"] = agent.exposureSeconds;
    object["critical"] = agent.critical;
    return object;
}

safecrowd::domain::ScenarioPressureAgentMetric pressureAgentMetricFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioPressureAgentMetric agent;
    agent.agentId = unsignedIntegerFromJson(object.value("agentId"));
    agent.position = pointFromJson(object.value("position"));
    agent.floorId = object.value("floorId").toString().toStdString();
    agent.compressionForce = object.value("compressionForce").toDouble();
    agent.exposureSeconds = object.value("exposureSeconds").toDouble();
    agent.critical = object.value("critical").toBool(false);
    return agent;
}

QJsonObject criticalPressureEventToJson(const safecrowd::domain::ScenarioCriticalPressureEvent& event) {
    QJsonObject object;
    object["center"] = pointArray(event.center);
    object["cellMin"] = pointArray(event.cellMin);
    object["cellMax"] = pointArray(event.cellMax);
    object["floorId"] = QString::fromStdString(event.floorId);
    object["exposedAgentCount"] = static_cast<qint64>(event.exposedAgentCount);
    object["criticalAgentCount"] = static_cast<qint64>(event.criticalAgentCount);
    object["pressureScore"] = event.pressureScore;
    object["startedAtSeconds"] = event.startedAtSeconds;
    object["durationSeconds"] = event.durationSeconds;
    object["detectedAtSeconds"] = optionalDoubleToJson(event.detectedAtSeconds);
    if (event.detectionFrame.has_value()) {
        object["detectionFrame"] = simulationFrameToJson(*event.detectionFrame);
    }
    return object;
}

safecrowd::domain::ScenarioCriticalPressureEvent criticalPressureEventFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioCriticalPressureEvent event{
        .center = pointFromJson(object.value("center")),
        .cellMin = pointFromJson(object.value("cellMin")),
        .cellMax = pointFromJson(object.value("cellMax")),
        .floorId = object.value("floorId").toString().toStdString(),
        .exposedAgentCount = static_cast<std::size_t>(object.value("exposedAgentCount").toInteger()),
        .criticalAgentCount = static_cast<std::size_t>(object.value("criticalAgentCount").toInteger()),
        .pressureScore = object.value("pressureScore").toDouble(),
        .startedAtSeconds = object.value("startedAtSeconds").toDouble(),
        .durationSeconds = object.value("durationSeconds").toDouble(),
    };
    event.detectedAtSeconds = optionalDoubleFromJson(object.value("detectedAtSeconds"));
    if (object.value("detectionFrame").isObject()) {
        event.detectionFrame = simulationFrameFromJson(object.value("detectionFrame").toObject());
    }
    return event;
}

QJsonObject crossFlowCellToJson(const safecrowd::domain::ScenarioCrossFlowCellMetric& cell) {
    QJsonObject object;
    object["center"] = pointArray(cell.center);
    object["cellMin"] = pointArray(cell.cellMin);
    object["cellMax"] = pointArray(cell.cellMax);
    object["floorId"] = QString::fromStdString(cell.floorId);
    object["movingAgentCount"] = static_cast<qint64>(cell.movingAgentCount);
    object["peakAgentCount"] = static_cast<qint64>(cell.peakAgentCount);
    object["primaryFlowCount"] = static_cast<qint64>(cell.primaryFlowCount);
    object["crossFlowCount"] = static_cast<qint64>(cell.crossFlowCount);
    object["crossFlowRatio"] = cell.crossFlowRatio;
    object["averageSpeed"] = cell.averageSpeed;
    object["speedDropRatio"] = cell.speedDropRatio;
    object["crossFlowScore"] = cell.crossFlowScore;
    object["durationSeconds"] = cell.durationSeconds;
    object["exposureAgentSeconds"] = cell.exposureAgentSeconds;
    object["detectedAtSeconds"] = optionalDoubleToJson(cell.detectedAtSeconds);
    if (cell.detectionFrame.has_value()) {
        object["detectionFrame"] = simulationFrameToJson(*cell.detectionFrame);
    }
    return object;
}

safecrowd::domain::ScenarioCrossFlowCellMetric crossFlowCellFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioCrossFlowCellMetric cell{
        .center = pointFromJson(object.value("center")),
        .cellMin = pointFromJson(object.value("cellMin")),
        .cellMax = pointFromJson(object.value("cellMax")),
        .floorId = object.value("floorId").toString().toStdString(),
        .movingAgentCount = static_cast<std::size_t>(object.value("movingAgentCount").toInteger()),
        .peakAgentCount = static_cast<std::size_t>(object.value("peakAgentCount").toInteger()),
        .primaryFlowCount = static_cast<std::size_t>(object.value("primaryFlowCount").toInteger()),
        .crossFlowCount = static_cast<std::size_t>(object.value("crossFlowCount").toInteger()),
        .crossFlowRatio = object.value("crossFlowRatio").toDouble(),
        .averageSpeed = object.value("averageSpeed").toDouble(),
        .speedDropRatio = object.value("speedDropRatio").toDouble(),
        .crossFlowScore = object.value("crossFlowScore").toDouble(),
        .durationSeconds = object.value("durationSeconds").toDouble(),
        .exposureAgentSeconds = object.value("exposureAgentSeconds").toDouble(),
    };
    cell.detectedAtSeconds = optionalDoubleFromJson(object.value("detectedAtSeconds"));
    if (object.value("detectionFrame").isObject()) {
        cell.detectionFrame = simulationFrameFromJson(object.value("detectionFrame").toObject());
    }
    return cell;
}

QJsonObject crossFlowTimelineSampleToJson(const safecrowd::domain::CrossFlowTimelineSample& sample) {
    QJsonObject object;
    object["timeSeconds"] = sample.timeSeconds;
    object["peakCrossFlowScore"] = sample.peakCrossFlowScore;
    object["activeCrossFlowCellCount"] = static_cast<qint64>(sample.activeCrossFlowCellCount);
    return object;
}

safecrowd::domain::CrossFlowTimelineSample crossFlowTimelineSampleFromJson(const QJsonObject& object) {
    return {
        .timeSeconds = object.value("timeSeconds").toDouble(),
        .peakCrossFlowScore = object.value("peakCrossFlowScore").toDouble(),
        .activeCrossFlowCellCount = static_cast<std::size_t>(object.value("activeCrossFlowCellCount").toInteger()),
    };
}

QJsonObject crossFlowSummaryToJson(const safecrowd::domain::CrossFlowSummary& summary) {
    QJsonObject object;
    object["peakCrossFlowScore"] = summary.peakCrossFlowScore;
    object["peakAtSeconds"] = optionalDoubleToJson(summary.peakAtSeconds);
    object["totalCrossFlowExposureAgentSeconds"] = summary.totalCrossFlowExposureAgentSeconds;
    object["longestCrossFlowDurationSeconds"] = summary.longestCrossFlowDurationSeconds;
    object["crossFlowHotspotCount"] = static_cast<qint64>(summary.crossFlowHotspotCount);
    return object;
}

safecrowd::domain::CrossFlowSummary crossFlowSummaryFromJson(const QJsonObject& object) {
    safecrowd::domain::CrossFlowSummary summary;
    summary.peakCrossFlowScore = object.value("peakCrossFlowScore").toDouble();
    summary.peakAtSeconds = optionalDoubleFromJson(object.value("peakAtSeconds"));
    summary.totalCrossFlowExposureAgentSeconds = object.value("totalCrossFlowExposureAgentSeconds").toDouble();
    summary.longestCrossFlowDurationSeconds = object.value("longestCrossFlowDurationSeconds").toDouble();
    summary.crossFlowHotspotCount = static_cast<std::size_t>(object.value("crossFlowHotspotCount").toInteger());
    return summary;
}

QJsonObject riskSnapshotToJson(const safecrowd::domain::ScenarioRiskSnapshot& risk) {
    QJsonObject object;
    object["completionRisk"] = static_cast<int>(risk.completionRisk);
    object["stalledAgentCount"] = static_cast<qint64>(risk.stalledAgentCount);
    object["pressureExposedAgentCount"] = static_cast<qint64>(risk.pressureExposedAgentCount);
    object["criticalPressureAgentCount"] = static_cast<qint64>(risk.criticalPressureAgentCount);
    object["crossFlowAgentCount"] = static_cast<qint64>(risk.crossFlowAgentCount);
    object["peakCrossFlowScore"] = risk.peakCrossFlowScore;
    object["totalCrossFlowExposureAgentSeconds"] = risk.totalCrossFlowExposureAgentSeconds;
    QJsonArray hotspots;
    for (const auto& hotspot : risk.hotspots) {
        hotspots.append(hotspotToJson(hotspot));
    }
    object["hotspots"] = hotspots;
    QJsonArray pressureHotspots;
    for (const auto& hotspot : risk.pressureHotspots) {
        pressureHotspots.append(pressureHotspotToJson(hotspot));
    }
    object["pressureHotspots"] = pressureHotspots;
    QJsonArray pressureAgents;
    for (const auto& agent : risk.pressureAgents) {
        pressureAgents.append(pressureAgentMetricToJson(agent));
    }
    object["pressureAgents"] = pressureAgents;
    QJsonArray criticalPressureEvents;
    for (const auto& event : risk.criticalPressureEvents) {
        criticalPressureEvents.append(criticalPressureEventToJson(event));
    }
    object["criticalPressureEvents"] = criticalPressureEvents;
    QJsonArray bottlenecks;
    for (const auto& bottleneck : risk.bottlenecks) {
        bottlenecks.append(bottleneckToJson(bottleneck));
    }
    object["bottlenecks"] = bottlenecks;
    QJsonArray crossFlowCells;
    for (const auto& cell : risk.crossFlowCells) {
        crossFlowCells.append(crossFlowCellToJson(cell));
    }
    object["crossFlowCells"] = crossFlowCells;
    return object;
}

safecrowd::domain::ScenarioRiskSnapshot riskSnapshotFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioRiskSnapshot risk;
    risk.completionRisk = static_cast<safecrowd::domain::ScenarioRiskLevel>(object.value("completionRisk").toInt());
    risk.stalledAgentCount = static_cast<std::size_t>(object.value("stalledAgentCount").toInteger());
    risk.pressureExposedAgentCount =
        static_cast<std::size_t>(object.value("pressureExposedAgentCount").toInteger());
    risk.criticalPressureAgentCount =
        static_cast<std::size_t>(object.value("criticalPressureAgentCount").toInteger());
    risk.crossFlowAgentCount = static_cast<std::size_t>(object.value("crossFlowAgentCount").toInteger());
    risk.peakCrossFlowScore = object.value("peakCrossFlowScore").toDouble();
    risk.totalCrossFlowExposureAgentSeconds = object.value("totalCrossFlowExposureAgentSeconds").toDouble();
    for (const auto& value : object.value("hotspots").toArray()) {
        risk.hotspots.push_back(hotspotFromJson(value.toObject()));
    }
    for (const auto& value : object.value("pressureHotspots").toArray()) {
        risk.pressureHotspots.push_back(pressureHotspotFromJson(value.toObject()));
    }
    for (const auto& value : object.value("pressureAgents").toArray()) {
        risk.pressureAgents.push_back(pressureAgentMetricFromJson(value.toObject()));
    }
    for (const auto& value : object.value("criticalPressureEvents").toArray()) {
        risk.criticalPressureEvents.push_back(criticalPressureEventFromJson(value.toObject()));
    }
    for (const auto& value : object.value("bottlenecks").toArray()) {
        risk.bottlenecks.push_back(bottleneckFromJson(value.toObject()));
    }
    for (const auto& value : object.value("crossFlowCells").toArray()) {
        risk.crossFlowCells.push_back(crossFlowCellFromJson(value.toObject()));
    }
    return risk;
}

QJsonObject densityCellMetricToJson(const safecrowd::domain::DensityCellMetric& cell) {
    QJsonObject object;
    object["center"] = pointArray(cell.center);
    object["cellMin"] = pointArray(cell.cellMin);
    object["cellMax"] = pointArray(cell.cellMax);
    object["floorId"] = QString::fromStdString(cell.floorId);
    object["agentCount"] = static_cast<qint64>(cell.agentCount);
    object["densityPeoplePerSquareMeter"] = cell.densityPeoplePerSquareMeter;
    return object;
}

safecrowd::domain::DensityCellMetric densityCellMetricFromJson(const QJsonObject& object) {
    return {
        .center = pointFromJson(object.value("center")),
        .cellMin = pointFromJson(object.value("cellMin")),
        .cellMax = pointFromJson(object.value("cellMax")),
        .floorId = object.value("floorId").toString().toStdString(),
        .agentCount = static_cast<std::size_t>(object.value("agentCount").toInteger()),
        .densityPeoplePerSquareMeter = object.value("densityPeoplePerSquareMeter").toDouble(),
    };
}

QJsonObject densityFieldSnapshotToJson(const safecrowd::domain::DensityFieldSnapshot& snapshot) {
    QJsonObject object;
    object["timeSeconds"] = snapshot.timeSeconds;
    object["cellSizeMeters"] = snapshot.cellSizeMeters;
    QJsonArray cells;
    for (const auto& cell : snapshot.cells) {
        cells.append(densityCellMetricToJson(cell));
    }
    object["cells"] = cells;
    return object;
}

safecrowd::domain::DensityFieldSnapshot densityFieldSnapshotFromJson(const QJsonObject& object) {
    safecrowd::domain::DensityFieldSnapshot snapshot;
    snapshot.timeSeconds = object.value("timeSeconds").toDouble();
    snapshot.cellSizeMeters = object.value("cellSizeMeters").toDouble();
    for (const auto& value : object.value("cells").toArray()) {
        snapshot.cells.push_back(densityCellMetricFromJson(value.toObject()));
    }
    return snapshot;
}

QJsonObject densitySummaryToJson(const safecrowd::domain::DensitySummary& summary) {
    QJsonObject object;
    object["cellSizeMeters"] = summary.cellSizeMeters;
    object["highDensityThresholdPeoplePerSquareMeter"] = summary.highDensityThresholdPeoplePerSquareMeter;
    object["peakDensityPeoplePerSquareMeter"] = summary.peakDensityPeoplePerSquareMeter;
    object["peakAgentCount"] = static_cast<qint64>(summary.peakAgentCount);
    object["peakAtSeconds"] = optionalDoubleToJson(summary.peakAtSeconds);
    if (summary.peakCell.has_value()) {
        object["peakCell"] = densityCellMetricToJson(*summary.peakCell);
    }
    object["highDensityDurationSeconds"] = summary.highDensityDurationSeconds;
    QJsonArray peakCells;
    for (const auto& cell : summary.peakCells) {
        peakCells.append(densityCellMetricToJson(cell));
    }
    object["peakCells"] = peakCells;
    object["peakField"] = densityFieldSnapshotToJson(summary.peakField);
    return object;
}

safecrowd::domain::DensitySummary densitySummaryFromJson(const QJsonObject& object) {
    safecrowd::domain::DensitySummary summary;
    summary.cellSizeMeters = object.value("cellSizeMeters").toDouble();
    summary.highDensityThresholdPeoplePerSquareMeter =
        object.value("highDensityThresholdPeoplePerSquareMeter").toDouble(4.0);
    summary.peakDensityPeoplePerSquareMeter = object.value("peakDensityPeoplePerSquareMeter").toDouble();
    summary.peakAgentCount = static_cast<std::size_t>(object.value("peakAgentCount").toInteger());
    summary.peakAtSeconds = optionalDoubleFromJson(object.value("peakAtSeconds"));
    if (object.value("peakCell").isObject()) {
        summary.peakCell = densityCellMetricFromJson(object.value("peakCell").toObject());
    }
    summary.highDensityDurationSeconds = object.value("highDensityDurationSeconds").toDouble();
    for (const auto& value : object.value("peakCells").toArray()) {
        summary.peakCells.push_back(densityCellMetricFromJson(value.toObject()));
    }
    if (object.value("peakField").isObject()) {
        summary.peakField = densityFieldSnapshotFromJson(object.value("peakField").toObject());
    }
    return summary;
}

QJsonObject occupancyHeatmapCellToJson(const safecrowd::domain::OccupancyHeatmapCell& cell) {
    QJsonObject object;
    object["center"] = pointArray(cell.center);
    object["cellMin"] = pointArray(cell.cellMin);
    object["cellMax"] = pointArray(cell.cellMax);
    object["floorId"] = QString::fromStdString(cell.floorId);
    object["accumulatedAgentSeconds"] = cell.accumulatedAgentSeconds;
    object["normalizedIntensity"] = cell.normalizedIntensity;
    return object;
}

safecrowd::domain::OccupancyHeatmapCell occupancyHeatmapCellFromJson(const QJsonObject& object) {
    return {
        .center = pointFromJson(object.value("center")),
        .cellMin = pointFromJson(object.value("cellMin")),
        .cellMax = pointFromJson(object.value("cellMax")),
        .floorId = object.value("floorId").toString().toStdString(),
        .accumulatedAgentSeconds = object.value("accumulatedAgentSeconds").toDouble(),
        .normalizedIntensity = object.value("normalizedIntensity").toDouble(),
    };
}

QJsonObject occupancyHeatmapToJson(const safecrowd::domain::OccupancyHeatmap& heatmap) {
    QJsonObject object;
    object["cellSizeMeters"] = heatmap.cellSizeMeters;
    object["kernelRadiusMeters"] = heatmap.kernelRadiusMeters;
    object["accumulatedSeconds"] = heatmap.accumulatedSeconds;
    object["peakAccumulatedAgentSeconds"] = heatmap.peakAccumulatedAgentSeconds;
    QJsonArray cells;
    for (const auto& cell : heatmap.cells) {
        cells.append(occupancyHeatmapCellToJson(cell));
    }
    object["cells"] = cells;
    return object;
}

safecrowd::domain::OccupancyHeatmap occupancyHeatmapFromJson(const QJsonObject& object) {
    safecrowd::domain::OccupancyHeatmap heatmap;
    heatmap.cellSizeMeters = object.value("cellSizeMeters").toDouble();
    heatmap.kernelRadiusMeters = object.value("kernelRadiusMeters").toDouble();
    heatmap.accumulatedSeconds = object.value("accumulatedSeconds").toDouble();
    heatmap.peakAccumulatedAgentSeconds = object.value("peakAccumulatedAgentSeconds").toDouble();
    for (const auto& value : object.value("cells").toArray()) {
        heatmap.cells.push_back(occupancyHeatmapCellFromJson(value.toObject()));
    }
    return heatmap;
}

QJsonObject pressureCellMetricToJson(const safecrowd::domain::PressureCellMetric& cell) {
    QJsonObject object;
    object["center"] = pointArray(cell.center);
    object["cellMin"] = pointArray(cell.cellMin);
    object["cellMax"] = pointArray(cell.cellMax);
    object["floorId"] = QString::fromStdString(cell.floorId);
    object["agentCount"] = static_cast<qint64>(cell.agentCount);
    object["intrudingPairCount"] = static_cast<qint64>(cell.intrudingPairCount);
    object["densityPeoplePerSquareMeter"] = cell.densityPeoplePerSquareMeter;
    object["pressureScore"] = cell.pressureScore;
    return object;
}

safecrowd::domain::PressureCellMetric pressureCellMetricFromJson(const QJsonObject& object) {
    return {
        .center = pointFromJson(object.value("center")),
        .cellMin = pointFromJson(object.value("cellMin")),
        .cellMax = pointFromJson(object.value("cellMax")),
        .floorId = object.value("floorId").toString().toStdString(),
        .agentCount = static_cast<std::size_t>(object.value("agentCount").toInteger()),
        .intrudingPairCount = static_cast<std::size_t>(object.value("intrudingPairCount").toInteger()),
        .densityPeoplePerSquareMeter = object.value("densityPeoplePerSquareMeter").toDouble(),
        .pressureScore = object.value("pressureScore").toDouble(),
    };
}

QJsonObject pressureFieldSnapshotToJson(const safecrowd::domain::PressureFieldSnapshot& snapshot) {
    QJsonObject object;
    object["timeSeconds"] = snapshot.timeSeconds;
    object["cellSizeMeters"] = snapshot.cellSizeMeters;
    QJsonArray cells;
    for (const auto& cell : snapshot.cells) {
        cells.append(pressureCellMetricToJson(cell));
    }
    object["cells"] = cells;
    return object;
}

safecrowd::domain::PressureFieldSnapshot pressureFieldSnapshotFromJson(const QJsonObject& object) {
    safecrowd::domain::PressureFieldSnapshot snapshot;
    snapshot.timeSeconds = object.value("timeSeconds").toDouble();
    snapshot.cellSizeMeters = object.value("cellSizeMeters").toDouble();
    for (const auto& value : object.value("cells").toArray()) {
        snapshot.cells.push_back(pressureCellMetricFromJson(value.toObject()));
    }
    return snapshot;
}

QJsonObject pressureSummaryToJson(const safecrowd::domain::PressureSummary& summary) {
    QJsonObject object;
    object["cellSizeMeters"] = summary.cellSizeMeters;
    object["hotspotScoreThreshold"] = summary.hotspotScoreThreshold;
    object["criticalCompressionForceThreshold"] = summary.criticalCompressionForceThreshold;
    object["criticalExposureThresholdSeconds"] = summary.criticalExposureThresholdSeconds;
    object["criticalEventDurationThresholdSeconds"] = summary.criticalEventDurationThresholdSeconds;
    object["criticalEventAgentThreshold"] = static_cast<qint64>(summary.criticalEventAgentThreshold);
    object["peakPressureScore"] = summary.peakPressureScore;
    object["peakAtSeconds"] = optionalDoubleToJson(summary.peakAtSeconds);
    if (summary.peakCell.has_value()) {
        object["peakCell"] = pressureCellMetricToJson(*summary.peakCell);
    }
    object["peakExposedAgentCount"] = static_cast<qint64>(summary.peakExposedAgentCount);
    object["peakCriticalAgentCount"] = static_cast<qint64>(summary.peakCriticalAgentCount);
    QJsonArray peakCells;
    for (const auto& cell : summary.peakCells) {
        peakCells.append(pressureCellMetricToJson(cell));
    }
    object["peakCells"] = peakCells;
    object["peakField"] = pressureFieldSnapshotToJson(summary.peakField);
    QJsonArray peakHotspots;
    for (const auto& hotspot : summary.peakHotspots) {
        peakHotspots.append(pressureHotspotToJson(hotspot));
    }
    object["peakHotspots"] = peakHotspots;
    QJsonArray peakAgents;
    for (const auto& agent : summary.peakAgents) {
        peakAgents.append(pressureAgentMetricToJson(agent));
    }
    object["peakAgents"] = peakAgents;
    QJsonArray criticalEvents;
    for (const auto& event : summary.criticalEvents) {
        criticalEvents.append(criticalPressureEventToJson(event));
    }
    object["criticalEvents"] = criticalEvents;
    return object;
}

safecrowd::domain::PressureSummary pressureSummaryFromJson(const QJsonObject& object) {
    safecrowd::domain::PressureSummary summary;
    summary.cellSizeMeters = object.value("cellSizeMeters").toDouble();
    summary.hotspotScoreThreshold = object.value("hotspotScoreThreshold").toDouble();
    summary.criticalCompressionForceThreshold = object.value("criticalCompressionForceThreshold").toDouble();
    summary.criticalExposureThresholdSeconds = object.value("criticalExposureThresholdSeconds").toDouble();
    summary.criticalEventDurationThresholdSeconds = object.value("criticalEventDurationThresholdSeconds").toDouble();
    summary.criticalEventAgentThreshold =
        static_cast<std::size_t>(object.value("criticalEventAgentThreshold").toInteger());
    summary.peakPressureScore = object.value("peakPressureScore").toDouble();
    summary.peakAtSeconds = optionalDoubleFromJson(object.value("peakAtSeconds"));
    if (object.value("peakCell").isObject()) {
        summary.peakCell = pressureCellMetricFromJson(object.value("peakCell").toObject());
    }
    summary.peakExposedAgentCount =
        static_cast<std::size_t>(object.value("peakExposedAgentCount").toInteger());
    summary.peakCriticalAgentCount =
        static_cast<std::size_t>(object.value("peakCriticalAgentCount").toInteger());
    for (const auto& value : object.value("peakCells").toArray()) {
        summary.peakCells.push_back(pressureCellMetricFromJson(value.toObject()));
    }
    if (object.value("peakField").isObject()) {
        summary.peakField = pressureFieldSnapshotFromJson(object.value("peakField").toObject());
    }
    for (const auto& value : object.value("peakHotspots").toArray()) {
        summary.peakHotspots.push_back(pressureHotspotFromJson(value.toObject()));
    }
    for (const auto& value : object.value("peakAgents").toArray()) {
        summary.peakAgents.push_back(pressureAgentMetricFromJson(value.toObject()));
    }
    for (const auto& value : object.value("criticalEvents").toArray()) {
        summary.criticalEvents.push_back(criticalPressureEventFromJson(value.toObject()));
    }
    return summary;
}

QJsonObject exitUsageMetricToJson(const safecrowd::domain::ExitUsageMetric& exit) {
    QJsonObject object;
    object["exitZoneId"] = QString::fromStdString(exit.exitZoneId);
    object["exitLabel"] = QString::fromStdString(exit.exitLabel);
    object["floorId"] = QString::fromStdString(exit.floorId);
    object["evacuatedCount"] = static_cast<qint64>(exit.evacuatedCount);
    object["usageRatio"] = exit.usageRatio;
    object["lastExitTimeSeconds"] = optionalDoubleToJson(exit.lastExitTimeSeconds);
    return object;
}

safecrowd::domain::ExitUsageMetric exitUsageMetricFromJson(const QJsonObject& object) {
    safecrowd::domain::ExitUsageMetric exit;
    exit.exitZoneId = object.value("exitZoneId").toString().toStdString();
    exit.exitLabel = object.value("exitLabel").toString().toStdString();
    exit.floorId = object.value("floorId").toString().toStdString();
    exit.evacuatedCount = static_cast<std::size_t>(object.value("evacuatedCount").toInteger());
    exit.usageRatio = object.value("usageRatio").toDouble();
    exit.lastExitTimeSeconds = optionalDoubleFromJson(object.value("lastExitTimeSeconds"));
    return exit;
}

QJsonObject zoneCompletionMetricToJson(const safecrowd::domain::ZoneCompletionMetric& zone) {
    QJsonObject object;
    object["zoneId"] = QString::fromStdString(zone.zoneId);
    object["zoneLabel"] = QString::fromStdString(zone.zoneLabel);
    object["floorId"] = QString::fromStdString(zone.floorId);
    object["initialCount"] = static_cast<qint64>(zone.initialCount);
    object["evacuatedCount"] = static_cast<qint64>(zone.evacuatedCount);
    object["lastCompletionTimeSeconds"] = optionalDoubleToJson(zone.lastCompletionTimeSeconds);
    return object;
}

safecrowd::domain::ZoneCompletionMetric zoneCompletionMetricFromJson(const QJsonObject& object) {
    safecrowd::domain::ZoneCompletionMetric zone;
    zone.zoneId = object.value("zoneId").toString().toStdString();
    zone.zoneLabel = object.value("zoneLabel").toString().toStdString();
    zone.floorId = object.value("floorId").toString().toStdString();
    zone.initialCount = static_cast<std::size_t>(object.value("initialCount").toInteger());
    zone.evacuatedCount = static_cast<std::size_t>(object.value("evacuatedCount").toInteger());
    zone.lastCompletionTimeSeconds = optionalDoubleFromJson(object.value("lastCompletionTimeSeconds"));
    return zone;
}

QJsonObject placementCompletionMetricToJson(const safecrowd::domain::PlacementCompletionMetric& placement) {
    QJsonObject object;
    object["placementId"] = QString::fromStdString(placement.placementId);
    object["zoneId"] = QString::fromStdString(placement.zoneId);
    object["floorId"] = QString::fromStdString(placement.floorId);
    object["initialCount"] = static_cast<qint64>(placement.initialCount);
    object["evacuatedCount"] = static_cast<qint64>(placement.evacuatedCount);
    object["lastCompletionTimeSeconds"] = optionalDoubleToJson(placement.lastCompletionTimeSeconds);
    return object;
}

safecrowd::domain::PlacementCompletionMetric placementCompletionMetricFromJson(const QJsonObject& object) {
    safecrowd::domain::PlacementCompletionMetric placement;
    placement.placementId = object.value("placementId").toString().toStdString();
    placement.zoneId = object.value("zoneId").toString().toStdString();
    placement.floorId = object.value("floorId").toString().toStdString();
    placement.initialCount = static_cast<std::size_t>(object.value("initialCount").toInteger());
    placement.evacuatedCount = static_cast<std::size_t>(object.value("evacuatedCount").toInteger());
    placement.lastCompletionTimeSeconds = optionalDoubleFromJson(object.value("lastCompletionTimeSeconds"));
    return placement;
}

QString hazardExposureKindToJson(safecrowd::domain::EnvironmentHazardKind kind) {
    switch (kind) {
    case safecrowd::domain::EnvironmentHazardKind::Smoke:
        return "smoke";
    case safecrowd::domain::EnvironmentHazardKind::Fire:
    default:
        return "fire";
    }
}

safecrowd::domain::EnvironmentHazardKind hazardExposureKindFromJson(const QJsonValue& value) {
    if (value.isDouble()) {
        return value.toInt() == static_cast<int>(safecrowd::domain::EnvironmentHazardKind::Smoke)
            ? safecrowd::domain::EnvironmentHazardKind::Smoke
            : safecrowd::domain::EnvironmentHazardKind::Fire;
    }

    const auto text = value.toString().trimmed().toLower();
    if (text == "smoke") {
        return safecrowd::domain::EnvironmentHazardKind::Smoke;
    }
    if (text == "fire") {
        return safecrowd::domain::EnvironmentHazardKind::Fire;
    }
    return safecrowd::domain::EnvironmentHazardKind::Fire;
}

QString hazardExposureSeverityToJson(safecrowd::domain::ScenarioElementSeverity severity) {
    switch (severity) {
    case safecrowd::domain::ScenarioElementSeverity::Low:
        return "low";
    case safecrowd::domain::ScenarioElementSeverity::High:
        return "high";
    case safecrowd::domain::ScenarioElementSeverity::Medium:
    default:
        return "medium";
    }
}

safecrowd::domain::ScenarioElementSeverity hazardExposureSeverityFromJson(const QJsonValue& value) {
    if (value.isDouble()) {
        const auto raw = value.toInt();
        if (raw == static_cast<int>(safecrowd::domain::ScenarioElementSeverity::Low)) {
            return safecrowd::domain::ScenarioElementSeverity::Low;
        }
        if (raw == static_cast<int>(safecrowd::domain::ScenarioElementSeverity::High)) {
            return safecrowd::domain::ScenarioElementSeverity::High;
        }
        return safecrowd::domain::ScenarioElementSeverity::Medium;
    }

    const auto text = value.toString().trimmed().toLower();
    if (text == "low") {
        return safecrowd::domain::ScenarioElementSeverity::Low;
    }
    if (text == "high") {
        return safecrowd::domain::ScenarioElementSeverity::High;
    }
    if (text == "medium") {
        return safecrowd::domain::ScenarioElementSeverity::Medium;
    }
    return safecrowd::domain::ScenarioElementSeverity::Medium;
}

QJsonObject hazardExposureMetricToJson(const safecrowd::domain::HazardExposureMetric& metric) {
    QJsonObject object;
    object["hazardId"] = QString::fromStdString(metric.hazardId);
    object["hazardName"] = QString::fromStdString(metric.hazardName);
    object["kind"] = hazardExposureKindToJson(metric.kind);
    object["severity"] = hazardExposureSeverityToJson(metric.severity);
    object["affectedZoneId"] = QString::fromStdString(metric.affectedZoneId);
    object["floorId"] = QString::fromStdString(metric.floorId);
    object["position"] = pointArray(metric.position);
    object["exposedAgentSeconds"] = metric.exposedAgentSeconds;
    object["peakExposedAgentCount"] = static_cast<qint64>(metric.peakExposedAgentCount);
    object["firstExposureSeconds"] = optionalDoubleToJson(metric.firstExposureSeconds);
    object["peakAtSeconds"] = optionalDoubleToJson(metric.peakAtSeconds);
    object["exposureScore"] = metric.exposureScore;
    return object;
}

safecrowd::domain::HazardExposureMetric hazardExposureMetricFromJson(const QJsonObject& object) {
    safecrowd::domain::HazardExposureMetric metric;
    metric.hazardId = object.value("hazardId").toString().toStdString();
    metric.hazardName = object.value("hazardName").toString().toStdString();
    metric.kind = hazardExposureKindFromJson(object.value("kind"));
    metric.severity = hazardExposureSeverityFromJson(object.value("severity"));
    metric.affectedZoneId = object.value("affectedZoneId").toString().toStdString();
    metric.floorId = object.value("floorId").toString().toStdString();
    metric.position = pointFromJson(object.value("position"));
    metric.exposedAgentSeconds = object.value("exposedAgentSeconds").toDouble();
    metric.peakExposedAgentCount = static_cast<std::size_t>(object.value("peakExposedAgentCount").toInteger());
    metric.firstExposureSeconds = optionalDoubleFromJson(object.value("firstExposureSeconds"));
    metric.peakAtSeconds = optionalDoubleFromJson(object.value("peakAtSeconds"));
    metric.exposureScore = object.value("exposureScore").toDouble();
    return metric;
}

QJsonObject hazardExposureSummaryToJson(const safecrowd::domain::HazardExposureSummary& summary) {
    QJsonObject object;
    object["totalExposureScore"] = summary.totalExposureScore;
    QJsonArray hazards;
    for (const auto& metric : summary.hazards) {
        hazards.append(hazardExposureMetricToJson(metric));
    }
    object["hazards"] = hazards;
    return object;
}

safecrowd::domain::HazardExposureSummary hazardExposureSummaryFromJson(const QJsonObject& object) {
    safecrowd::domain::HazardExposureSummary summary;
    summary.totalExposureScore = object.value("totalExposureScore").toDouble();
    for (const auto& value : object.value("hazards").toArray()) {
        summary.hazards.push_back(hazardExposureMetricFromJson(value.toObject()));
    }
    return summary;
}

QJsonObject resultArtifactsToJson(const safecrowd::domain::ScenarioResultArtifacts& artifacts) {
    QJsonObject object;
    QJsonArray progress;
    for (const auto& sample : artifacts.evacuationProgress) {
        QJsonObject sampleObject;
        sampleObject["timeSeconds"] = sample.timeSeconds;
        sampleObject["evacuatedCount"] = static_cast<qint64>(sample.evacuatedCount);
        sampleObject["totalCount"] = static_cast<qint64>(sample.totalCount);
        sampleObject["evacuatedRatio"] = sample.evacuatedRatio;
        progress.append(sampleObject);
    }
    object["evacuationProgress"] = progress;

    QJsonArray replayFrames;
    for (const auto& frame : artifacts.replayFrames) {
        replayFrames.append(simulationFrameToJson(frame));
    }
    object["replayFrames"] = replayFrames;

    QJsonObject timing;
    timing["t50Seconds"] = optionalDoubleToJson(artifacts.timingSummary.t50Seconds);
    timing["t90Seconds"] = optionalDoubleToJson(artifacts.timingSummary.t90Seconds);
    timing["t95Seconds"] = optionalDoubleToJson(artifacts.timingSummary.t95Seconds);
    timing["finalEvacuationTimeSeconds"] = optionalDoubleToJson(artifacts.timingSummary.finalEvacuationTimeSeconds);
    timing["targetTimeSeconds"] = artifacts.timingSummary.targetTimeSeconds;
    timing["marginSeconds"] = optionalDoubleToJson(artifacts.timingSummary.marginSeconds);
    if (artifacts.timingSummary.t90Frame.has_value()) {
        timing["t90Frame"] = simulationFrameToJson(*artifacts.timingSummary.t90Frame);
    }
    if (artifacts.timingSummary.t95Frame.has_value()) {
        timing["t95Frame"] = simulationFrameToJson(*artifacts.timingSummary.t95Frame);
    }
    object["timingSummary"] = timing;

    object["densitySummary"] = densitySummaryToJson(artifacts.densitySummary);
    object["occupancyHeatmap"] = occupancyHeatmapToJson(artifacts.occupancyHeatmap);
    object["pressureSummary"] = pressureSummaryToJson(artifacts.pressureSummary);
    object["hazardExposureSummary"] = hazardExposureSummaryToJson(artifacts.hazardExposureSummary);
    object["crossFlowSummary"] = crossFlowSummaryToJson(artifacts.crossFlowSummary);

    QJsonArray exitUsage;
    for (const auto& exit : artifacts.exitUsage) {
        exitUsage.append(exitUsageMetricToJson(exit));
    }
    object["exitUsage"] = exitUsage;

    QJsonArray zoneCompletion;
    for (const auto& zone : artifacts.zoneCompletion) {
        zoneCompletion.append(zoneCompletionMetricToJson(zone));
    }
    object["zoneCompletion"] = zoneCompletion;

    QJsonArray placementCompletion;
    for (const auto& placement : artifacts.placementCompletion) {
        placementCompletion.append(placementCompletionMetricToJson(placement));
    }
    object["placementCompletion"] = placementCompletion;

    QJsonArray crossFlowTimeline;
    for (const auto& sample : artifacts.crossFlowTimeline) {
        crossFlowTimeline.append(crossFlowTimelineSampleToJson(sample));
    }
    object["crossFlowTimeline"] = crossFlowTimeline;

    return object;
}

safecrowd::domain::ScenarioResultArtifacts resultArtifactsFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioResultArtifacts artifacts;
    for (const auto& value : object.value("evacuationProgress").toArray()) {
        const auto sampleObject = value.toObject();
        artifacts.evacuationProgress.push_back({
            .timeSeconds = sampleObject.value("timeSeconds").toDouble(),
            .evacuatedCount = static_cast<std::size_t>(sampleObject.value("evacuatedCount").toInteger()),
            .totalCount = static_cast<std::size_t>(sampleObject.value("totalCount").toInteger()),
            .evacuatedRatio = sampleObject.value("evacuatedRatio").toDouble(),
        });
    }
    for (const auto& value : object.value("replayFrames").toArray()) {
        artifacts.replayFrames.push_back(simulationFrameFromJson(value.toObject()));
    }

    const auto timing = object.value("timingSummary").toObject();
    artifacts.timingSummary.t50Seconds = optionalDoubleFromJson(timing.value("t50Seconds"));
    artifacts.timingSummary.t90Seconds = optionalDoubleFromJson(timing.value("t90Seconds"));
    artifacts.timingSummary.t95Seconds = optionalDoubleFromJson(timing.value("t95Seconds"));
    artifacts.timingSummary.finalEvacuationTimeSeconds = optionalDoubleFromJson(timing.value("finalEvacuationTimeSeconds"));
    artifacts.timingSummary.targetTimeSeconds = timing.value("targetTimeSeconds").toDouble();
    artifacts.timingSummary.marginSeconds = optionalDoubleFromJson(timing.value("marginSeconds"));
    if (timing.value("t90Frame").isObject()) {
        artifacts.timingSummary.t90Frame = simulationFrameFromJson(timing.value("t90Frame").toObject());
    }
    if (timing.value("t95Frame").isObject()) {
        artifacts.timingSummary.t95Frame = simulationFrameFromJson(timing.value("t95Frame").toObject());
    }

    if (object.value("densitySummary").isObject()) {
        artifacts.densitySummary = densitySummaryFromJson(object.value("densitySummary").toObject());
    }
    if (object.value("occupancyHeatmap").isObject()) {
        artifacts.occupancyHeatmap = occupancyHeatmapFromJson(object.value("occupancyHeatmap").toObject());
    }
    if (object.value("pressureSummary").isObject()) {
        artifacts.pressureSummary = pressureSummaryFromJson(object.value("pressureSummary").toObject());
    }
    if (object.value("hazardExposureSummary").isObject()) {
        artifacts.hazardExposureSummary =
            hazardExposureSummaryFromJson(object.value("hazardExposureSummary").toObject());
    }
    if (object.value("crossFlowSummary").isObject()) {
        artifacts.crossFlowSummary =
            crossFlowSummaryFromJson(object.value("crossFlowSummary").toObject());
    }
    for (const auto& value : object.value("exitUsage").toArray()) {
        artifacts.exitUsage.push_back(exitUsageMetricFromJson(value.toObject()));
    }
    for (const auto& value : object.value("zoneCompletion").toArray()) {
        artifacts.zoneCompletion.push_back(zoneCompletionMetricFromJson(value.toObject()));
    }
    for (const auto& value : object.value("placementCompletion").toArray()) {
        artifacts.placementCompletion.push_back(placementCompletionMetricFromJson(value.toObject()));
    }
    for (const auto& value : object.value("crossFlowTimeline").toArray()) {
        artifacts.crossFlowTimeline.push_back(
            crossFlowTimelineSampleFromJson(value.toObject()));
    }

    return artifacts;
}

}  // namespace safecrowd::application
