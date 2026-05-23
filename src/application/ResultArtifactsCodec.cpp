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

QJsonObject operationalConflictCellToJson(const safecrowd::domain::ScenarioOperationalConflictCellMetric& cell) {
    QJsonObject object;
    object["center"] = pointArray(cell.center);
    object["cellMin"] = pointArray(cell.cellMin);
    object["cellMax"] = pointArray(cell.cellMax);
    object["floorId"] = QString::fromStdString(cell.floorId);
    object["movingAgentCount"] = static_cast<qint64>(cell.movingAgentCount);
    object["peakAgentCount"] = static_cast<qint64>(cell.peakAgentCount);
    object["forwardCount"] = static_cast<qint64>(cell.forwardCount);
    object["reverseCount"] = static_cast<qint64>(cell.reverseCount);
    object["counterflowRatio"] = cell.counterflowRatio;
    object["averageSpeed"] = cell.averageSpeed;
    object["speedDropRatio"] = cell.speedDropRatio;
    object["conflictScore"] = cell.conflictScore;
    object["durationSeconds"] = cell.durationSeconds;
    object["exposureAgentSeconds"] = cell.exposureAgentSeconds;
    object["nearestConnectionId"] = QString::fromStdString(cell.nearestConnectionId);
    object["nearestConnectionLabel"] = QString::fromStdString(cell.nearestConnectionLabel);
    object["detectedAtSeconds"] = optionalDoubleToJson(cell.detectedAtSeconds);
    if (cell.detectionFrame.has_value()) {
        object["detectionFrame"] = simulationFrameToJson(*cell.detectionFrame);
    }
    return object;
}

safecrowd::domain::ScenarioOperationalConflictCellMetric operationalConflictCellFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioOperationalConflictCellMetric cell{
        .center = pointFromJson(object.value("center")),
        .cellMin = pointFromJson(object.value("cellMin")),
        .cellMax = pointFromJson(object.value("cellMax")),
        .floorId = object.value("floorId").toString().toStdString(),
        .movingAgentCount = static_cast<std::size_t>(object.value("movingAgentCount").toInteger()),
        .peakAgentCount = static_cast<std::size_t>(object.value("peakAgentCount").toInteger()),
        .forwardCount = static_cast<std::size_t>(object.value("forwardCount").toInteger()),
        .reverseCount = static_cast<std::size_t>(object.value("reverseCount").toInteger()),
        .counterflowRatio = object.value("counterflowRatio").toDouble(),
        .averageSpeed = object.value("averageSpeed").toDouble(),
        .speedDropRatio = object.value("speedDropRatio").toDouble(),
        .conflictScore = object.value("conflictScore").toDouble(),
        .durationSeconds = object.value("durationSeconds").toDouble(),
        .exposureAgentSeconds = object.value("exposureAgentSeconds").toDouble(),
        .nearestConnectionId = object.value("nearestConnectionId").toString().toStdString(),
        .nearestConnectionLabel = object.value("nearestConnectionLabel").toString().toStdString(),
    };
    cell.detectedAtSeconds = optionalDoubleFromJson(object.value("detectedAtSeconds"));
    if (object.value("detectionFrame").isObject()) {
        cell.detectionFrame = simulationFrameFromJson(object.value("detectionFrame").toObject());
    }
    return cell;
}

QJsonObject operationalConflictConnectionToJson(const safecrowd::domain::ScenarioOperationalConflictConnectionMetric& connection) {
    QJsonObject object;
    object["connectionId"] = QString::fromStdString(connection.connectionId);
    object["label"] = QString::fromStdString(connection.label);
    object["floorId"] = QString::fromStdString(connection.floorId);
    object["passage"] = lineToJson(connection.passage);
    object["nearbyAgentCount"] = static_cast<qint64>(connection.nearbyAgentCount);
    object["movingAgentCount"] = static_cast<qint64>(connection.movingAgentCount);
    object["queueAgentCount"] = static_cast<qint64>(connection.queueAgentCount);
    object["forwardCount"] = static_cast<qint64>(connection.forwardCount);
    object["reverseCount"] = static_cast<qint64>(connection.reverseCount);
    object["counterflowRatio"] = connection.counterflowRatio;
    object["averageSpeed"] = connection.averageSpeed;
    object["speedDropRatio"] = connection.speedDropRatio;
    object["conflictScore"] = connection.conflictScore;
    object["durationSeconds"] = connection.durationSeconds;
    object["exposureAgentSeconds"] = connection.exposureAgentSeconds;
    object["detectedAtSeconds"] = optionalDoubleToJson(connection.detectedAtSeconds);
    if (connection.detectionFrame.has_value()) {
        object["detectionFrame"] = simulationFrameToJson(*connection.detectionFrame);
    }
    return object;
}

safecrowd::domain::ScenarioOperationalConflictConnectionMetric operationalConflictConnectionFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioOperationalConflictConnectionMetric connection{
        .connectionId = object.value("connectionId").toString().toStdString(),
        .label = object.value("label").toString().toStdString(),
        .floorId = object.value("floorId").toString().toStdString(),
        .passage = lineFromJson(object.value("passage").toObject()),
        .nearbyAgentCount = static_cast<std::size_t>(object.value("nearbyAgentCount").toInteger()),
        .movingAgentCount = static_cast<std::size_t>(object.value("movingAgentCount").toInteger()),
        .queueAgentCount = static_cast<std::size_t>(object.value("queueAgentCount").toInteger()),
        .forwardCount = static_cast<std::size_t>(object.value("forwardCount").toInteger()),
        .reverseCount = static_cast<std::size_t>(object.value("reverseCount").toInteger()),
        .counterflowRatio = object.value("counterflowRatio").toDouble(),
        .averageSpeed = object.value("averageSpeed").toDouble(),
        .speedDropRatio = object.value("speedDropRatio").toDouble(),
        .conflictScore = object.value("conflictScore").toDouble(),
        .durationSeconds = object.value("durationSeconds").toDouble(),
        .exposureAgentSeconds = object.value("exposureAgentSeconds").toDouble(),
    };
    connection.detectedAtSeconds = optionalDoubleFromJson(object.value("detectedAtSeconds"));
    if (object.value("detectionFrame").isObject()) {
        connection.detectionFrame = simulationFrameFromJson(object.value("detectionFrame").toObject());
    }
    return connection;
}

QJsonObject connectionUsageMetricToJson(const safecrowd::domain::ConnectionUsageMetric& connection) {
    QJsonObject object;
    object["connectionId"] = QString::fromStdString(connection.connectionId);
    object["label"] = QString::fromStdString(connection.label);
    object["floorId"] = QString::fromStdString(connection.floorId);
    object["traversalCount"] = static_cast<qint64>(connection.traversalCount);
    object["usageRatio"] = connection.usageRatio;
    object["peakWindowCount"] = static_cast<qint64>(connection.peakWindowCount);
    object["peakAtSeconds"] = optionalDoubleToJson(connection.peakAtSeconds);
    object["forwardTraversals"] = static_cast<qint64>(connection.forwardTraversals);
    object["reverseTraversals"] = static_cast<qint64>(connection.reverseTraversals);
    object["queueExposureAgentSeconds"] = connection.queueExposureAgentSeconds;
    object["peakQueuedAgents"] = static_cast<qint64>(connection.peakQueuedAgents);
    object["averageObservedSpeed"] = connection.averageObservedSpeed;
    object["peakConflictScore"] = connection.peakConflictScore;
    object["longestConflictDurationSeconds"] = connection.longestConflictDurationSeconds;
    object["counterflowEventCount"] = static_cast<qint64>(connection.counterflowEventCount);
    return object;
}

safecrowd::domain::ConnectionUsageMetric connectionUsageMetricFromJson(const QJsonObject& object) {
    safecrowd::domain::ConnectionUsageMetric connection;
    connection.connectionId = object.value("connectionId").toString().toStdString();
    connection.label = object.value("label").toString().toStdString();
    connection.floorId = object.value("floorId").toString().toStdString();
    connection.traversalCount = static_cast<std::size_t>(object.value("traversalCount").toInteger());
    connection.usageRatio = object.value("usageRatio").toDouble();
    connection.peakWindowCount = static_cast<std::size_t>(object.value("peakWindowCount").toInteger());
    connection.peakAtSeconds = optionalDoubleFromJson(object.value("peakAtSeconds"));
    connection.forwardTraversals = static_cast<std::size_t>(object.value("forwardTraversals").toInteger());
    connection.reverseTraversals = static_cast<std::size_t>(object.value("reverseTraversals").toInteger());
    connection.queueExposureAgentSeconds = object.value("queueExposureAgentSeconds").toDouble();
    connection.peakQueuedAgents = static_cast<std::size_t>(object.value("peakQueuedAgents").toInteger());
    connection.averageObservedSpeed = object.value("averageObservedSpeed").toDouble();
    connection.peakConflictScore = object.value("peakConflictScore").toDouble();
    connection.longestConflictDurationSeconds = object.value("longestConflictDurationSeconds").toDouble();
    connection.counterflowEventCount = static_cast<std::size_t>(object.value("counterflowEventCount").toInteger());
    return connection;
}

QJsonObject operationalConflictTimelineSampleToJson(const safecrowd::domain::OperationalConflictTimelineSample& sample) {
    QJsonObject object;
    object["timeSeconds"] = sample.timeSeconds;
    object["peakConflictScore"] = sample.peakConflictScore;
    object["activeConflictCellCount"] = static_cast<qint64>(sample.activeConflictCellCount);
    object["activeConflictConnectionCount"] = static_cast<qint64>(sample.activeConflictConnectionCount);
    object["queuedAgentsNearConnections"] = static_cast<qint64>(sample.queuedAgentsNearConnections);
    return object;
}

safecrowd::domain::OperationalConflictTimelineSample operationalConflictTimelineSampleFromJson(const QJsonObject& object) {
    return {
        .timeSeconds = object.value("timeSeconds").toDouble(),
        .peakConflictScore = object.value("peakConflictScore").toDouble(),
        .activeConflictCellCount = static_cast<std::size_t>(object.value("activeConflictCellCount").toInteger()),
        .activeConflictConnectionCount = static_cast<std::size_t>(object.value("activeConflictConnectionCount").toInteger()),
        .queuedAgentsNearConnections = static_cast<std::size_t>(object.value("queuedAgentsNearConnections").toInteger()),
    };
}

QJsonObject operationalConflictSummaryToJson(const safecrowd::domain::OperationalConflictSummary& summary) {
    QJsonObject object;
    object["peakConflictScore"] = summary.peakConflictScore;
    object["peakAtSeconds"] = optionalDoubleToJson(summary.peakAtSeconds);
    object["totalConflictExposureAgentSeconds"] = summary.totalConflictExposureAgentSeconds;
    object["longestConflictDurationSeconds"] = summary.longestConflictDurationSeconds;
    object["counterflowHotspotCount"] = static_cast<qint64>(summary.counterflowHotspotCount);
    object["conflictConnectionCount"] = static_cast<qint64>(summary.conflictConnectionCount);
    object["connectionConcentrationIndex"] = summary.connectionConcentrationIndex;
    object["peakQueuedAgents"] = static_cast<qint64>(summary.peakQueuedAgents);
    object["topConflictConnectionId"] = QString::fromStdString(summary.topConflictConnectionId);
    object["topConflictConnectionLabel"] = QString::fromStdString(summary.topConflictConnectionLabel);
    return object;
}

safecrowd::domain::OperationalConflictSummary operationalConflictSummaryFromJson(const QJsonObject& object) {
    safecrowd::domain::OperationalConflictSummary summary;
    summary.peakConflictScore = object.value("peakConflictScore").toDouble();
    summary.peakAtSeconds = optionalDoubleFromJson(object.value("peakAtSeconds"));
    summary.totalConflictExposureAgentSeconds = object.value("totalConflictExposureAgentSeconds").toDouble();
    summary.longestConflictDurationSeconds = object.value("longestConflictDurationSeconds").toDouble();
    summary.counterflowHotspotCount = static_cast<std::size_t>(object.value("counterflowHotspotCount").toInteger());
    summary.conflictConnectionCount = static_cast<std::size_t>(object.value("conflictConnectionCount").toInteger());
    summary.connectionConcentrationIndex = object.value("connectionConcentrationIndex").toDouble();
    summary.peakQueuedAgents = static_cast<std::size_t>(object.value("peakQueuedAgents").toInteger());
    summary.topConflictConnectionId = object.value("topConflictConnectionId").toString().toStdString();
    summary.topConflictConnectionLabel = object.value("topConflictConnectionLabel").toString().toStdString();
    return summary;
}

QJsonObject riskSnapshotToJson(const safecrowd::domain::ScenarioRiskSnapshot& risk) {
    QJsonObject object;
    object["completionRisk"] = static_cast<int>(risk.completionRisk);
    object["stalledAgentCount"] = static_cast<qint64>(risk.stalledAgentCount);
    object["pressureExposedAgentCount"] = static_cast<qint64>(risk.pressureExposedAgentCount);
    object["criticalPressureAgentCount"] = static_cast<qint64>(risk.criticalPressureAgentCount);
    object["conflictAgentCount"] = static_cast<qint64>(risk.conflictAgentCount);
    object["peakConflictScore"] = risk.peakConflictScore;
    object["totalConflictExposureAgentSeconds"] = risk.totalConflictExposureAgentSeconds;
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
    QJsonArray operationalConflictCells;
    for (const auto& cell : risk.operationalConflictCells) {
        operationalConflictCells.append(operationalConflictCellToJson(cell));
    }
    object["operationalConflictCells"] = operationalConflictCells;
    QJsonArray operationalConflictConnections;
    for (const auto& connection : risk.operationalConflictConnections) {
        operationalConflictConnections.append(operationalConflictConnectionToJson(connection));
    }
    object["operationalConflictConnections"] = operationalConflictConnections;
    return object;
}

safecrowd::domain::ScenarioRiskSnapshot riskSnapshotFromJson(const QJsonObject& object) {
    safecrowd::domain::ScenarioRiskSnapshot risk;
    risk.completionRisk = static_cast<safecrowd::domain::ScenarioRiskLevel>(object.value("completionRisk").toInt());
    risk.stalledAgentCount = static_cast<std::size_t>(object.value("stalledAgentCount").toInteger());
    risk.pressureExposedAgentCount = static_cast<std::size_t>(object.value("pressureExposedAgentCount").toInteger());
    risk.criticalPressureAgentCount = static_cast<std::size_t>(object.value("criticalPressureAgentCount").toInteger());
    risk.conflictAgentCount = static_cast<std::size_t>(object.value("conflictAgentCount").toInteger());
    risk.peakConflictScore = object.value("peakConflictScore").toDouble();
    risk.totalConflictExposureAgentSeconds = object.value("totalConflictExposureAgentSeconds").toDouble();
    for (const auto& value : object.value("hotspots").toArray()) {
        risk.hotspots.push_back(hotspotFromJson(value.toObject()));
    }
    for (const auto& value : object.value("bottlenecks").toArray()) {
        risk.bottlenecks.push_back(bottleneckFromJson(value.toObject()));
    }
    for (const auto& value : object.value("operationalConflictCells").toArray()) {
        risk.operationalConflictCells.push_back(operationalConflictCellFromJson(value.toObject()));
    }
    for (const auto& value : object.value("operationalConflictConnections").toArray()) {
        risk.operationalConflictConnections.push_back(operationalConflictConnectionFromJson(value.toObject()));
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

    object["operationalConflictSummary"] = operationalConflictSummaryToJson(artifacts.operationalConflictSummary);

    QJsonArray connectionUsage;
    for (const auto& connection : artifacts.connectionUsage) {
        connectionUsage.append(connectionUsageMetricToJson(connection));
    }
    object["connectionUsage"] = connectionUsage;

    QJsonArray operationalConflictTimeline;
    for (const auto& sample : artifacts.operationalConflictTimeline) {
        operationalConflictTimeline.append(operationalConflictTimelineSampleToJson(sample));
    }
    object["operationalConflictTimeline"] = operationalConflictTimeline;

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
    for (const auto& value : object.value("exitUsage").toArray()) {
        artifacts.exitUsage.push_back(exitUsageMetricFromJson(value.toObject()));
    }
    for (const auto& value : object.value("zoneCompletion").toArray()) {
        artifacts.zoneCompletion.push_back(zoneCompletionMetricFromJson(value.toObject()));
    }
    for (const auto& value : object.value("placementCompletion").toArray()) {
        artifacts.placementCompletion.push_back(placementCompletionMetricFromJson(value.toObject()));
    }
    if (object.value("operationalConflictSummary").isObject()) {
        artifacts.operationalConflictSummary =
            operationalConflictSummaryFromJson(object.value("operationalConflictSummary").toObject());
    }
    for (const auto& value : object.value("connectionUsage").toArray()) {
        artifacts.connectionUsage.push_back(connectionUsageMetricFromJson(value.toObject()));
    }
    for (const auto& value : object.value("operationalConflictTimeline").toArray()) {
        artifacts.operationalConflictTimeline.push_back(
            operationalConflictTimelineSampleFromJson(value.toObject()));
    }

    return artifacts;
}

}  // namespace safecrowd::application
