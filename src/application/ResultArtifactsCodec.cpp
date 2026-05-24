#include "application/ResultArtifactsCodec.h"

#include <cstddef>
#include <optional>

#include <QJsonArray>
#include <QJsonValue>

#include "application/ProjectPersistenceJson.h"

namespace safecrowd::application {
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
        .id = object.value("id").toString().toULongLong(),
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

QJsonObject riskSnapshotToJson(const safecrowd::domain::ScenarioRiskSnapshot& risk) {
    QJsonObject object;
    object["completionRisk"] = static_cast<int>(risk.completionRisk);
    object["stalledAgentCount"] = static_cast<qint64>(risk.stalledAgentCount);
    QJsonArray hotspots;
    for (const auto& hotspot : risk.hotspots) {
        hotspots.append(hotspotToJson(hotspot));
    }
    object["hotspots"] = hotspots;
    QJsonArray bottlenecks;
    for (const auto& bottleneck : risk.bottlenecks) {
        bottlenecks.append(bottleneckToJson(bottleneck));
    }
    object["bottlenecks"] = bottlenecks;
    return object;
}

safecrowd::domain::ScenarioRiskSnapshot riskSnapshotFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioRiskSnapshot risk;
    risk.completionRisk = static_cast<safecrowd::domain::ScenarioRiskLevel>(object.value("completionRisk").toInt());
    risk.stalledAgentCount = static_cast<std::size_t>(object.value("stalledAgentCount").toInteger());
    for (const auto& value : object.value("hotspots").toArray()) {
        risk.hotspots.push_back(hotspotFromJson(value.toObject()));
    }
    for (const auto& value : object.value("bottlenecks").toArray()) {
        risk.bottlenecks.push_back(bottleneckFromJson(value.toObject()));
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
    object["hazardExposureSummary"] = hazardExposureSummaryToJson(artifacts.hazardExposureSummary);

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
    if (object.value("hazardExposureSummary").isObject()) {
        artifacts.hazardExposureSummary =
            hazardExposureSummaryFromJson(object.value("hazardExposureSummary").toObject());
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

    return artifacts;
}

}  // namespace safecrowd::application
