#include "application/WorkspaceStateCodec.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <QJsonArray>
#include <QJsonValue>

#include "application/ProjectPersistenceJson.h"
#include "application/ResultArtifactsCodec.h"

namespace safecrowd::application {

constexpr int kCurrentWorkspaceStateVersion = 1;

QJsonObject initialPlacementToJson(const safecrowd::domain::InitialPlacement2D& placement) {
    QJsonObject object;
    object["id"] = QString::fromStdString(placement.id);
    object["zoneId"] = QString::fromStdString(placement.zoneId);
    object["floorId"] = QString::fromStdString(placement.floorId);
    object["area"] = polygonToJson(placement.area);
    object["targetAgentCount"] = static_cast<qint64>(placement.targetAgentCount);
    object["initialVelocity"] = pointArray(placement.initialVelocity);
    object["distribution"] = placement.distribution == safecrowd::domain::InitialPlacementDistribution::Random
        ? "random"
        : "uniform";
    object["explicitPositions"] = ringToJson(placement.explicitPositions);
    return object;
}

safecrowd::domain::InitialPlacement2D initialPlacementFromJson(const QJsonObject& object) {
    safecrowd::domain::InitialPlacement2D placement;
    placement.id = object.value("id").toString().toStdString();
    placement.zoneId = object.value("zoneId").toString().toStdString();
    placement.floorId = object.value("floorId").toString().toStdString();
    placement.area = polygonFromJson(object.value("area").toObject());
    placement.targetAgentCount = static_cast<std::size_t>(object.value("targetAgentCount").toInteger());
    placement.initialVelocity = pointFromJson(object.value("initialVelocity"));
    placement.distribution = object.value("distribution").toString() == "random"
        ? safecrowd::domain::InitialPlacementDistribution::Random
        : safecrowd::domain::InitialPlacementDistribution::Uniform;
    placement.explicitPositions = ringFromJson(object.value("explicitPositions").toArray());
    return placement;
}

QJsonObject occupantSourceToJson(const safecrowd::domain::OccupantSource2D& source) {
    QJsonObject object;
    object["id"] = QString::fromStdString(source.id);
    object["zoneId"] = QString::fromStdString(source.zoneId);
    object["floorId"] = QString::fromStdString(source.floorId);
    object["position"] = pointArray(source.position);
    object["targetAgentCount"] = static_cast<qint64>(source.targetAgentCount);
    object["agentsPerSpawn"] = static_cast<qint64>(source.agentsPerSpawn);
    object["startSeconds"] = source.startSeconds;
    object["endSeconds"] = source.endSeconds;
    object["spawnIntervalSeconds"] = source.spawnIntervalSeconds;
    object["initialVelocity"] = pointArray(source.initialVelocity);
    return object;
}

safecrowd::domain::OccupantSource2D occupantSourceFromJson(const QJsonObject& object) {
    safecrowd::domain::OccupantSource2D source;
    source.id = object.value("id").toString().toStdString();
    source.zoneId = object.value("zoneId").toString().toStdString();
    source.floorId = object.value("floorId").toString().toStdString();
    source.position = pointFromJson(object.value("position"));
    source.targetAgentCount = static_cast<std::size_t>(object.value("targetAgentCount").toInteger());
    source.agentsPerSpawn = static_cast<std::size_t>(std::max<qint64>(1, object.value("agentsPerSpawn").toInteger(1)));
    source.startSeconds = object.value("startSeconds").toDouble(0.0);
    source.endSeconds = object.value("endSeconds").toDouble(180.0);
    source.spawnIntervalSeconds = object.value("spawnIntervalSeconds").toDouble(5.0);
    source.initialVelocity = pointFromJson(object.value("initialVelocity"));
    return source;
}

QJsonObject populationToJson(const safecrowd::domain::PopulationSpec& population) {
    QJsonObject object;
    QJsonArray placements;
    for (const auto& placement : population.initialPlacements) {
        placements.append(initialPlacementToJson(placement));
    }
    object["initialPlacements"] = placements;
    QJsonArray sources;
    for (const auto& source : population.occupantSources) {
        sources.append(occupantSourceToJson(source));
    }
    object["occupantSources"] = sources;
    return object;
}

safecrowd::domain::PopulationSpec populationFromJson(const QJsonObject& object) {
    safecrowd::domain::PopulationSpec population;
    for (const auto& value : object.value("initialPlacements").toArray()) {
        population.initialPlacements.push_back(initialPlacementFromJson(value.toObject()));
    }
    for (const auto& value : object.value("occupantSources").toArray()) {
        population.occupantSources.push_back(occupantSourceFromJson(value.toObject()));
    }
    return population;
}

QString hazardKindToJson(safecrowd::domain::EnvironmentHazardKind kind) {
    switch (kind) {
    case safecrowd::domain::EnvironmentHazardKind::Smoke:
        return "Smoke";
    case safecrowd::domain::EnvironmentHazardKind::Fire:
    default:
        return "Fire";
    }
}

safecrowd::domain::EnvironmentHazardKind hazardKindFromJson(const QJsonValue& value) {
    if (value.isDouble()) {
        return value.toInt() == static_cast<int>(safecrowd::domain::EnvironmentHazardKind::Smoke)
            ? safecrowd::domain::EnvironmentHazardKind::Smoke
            : safecrowd::domain::EnvironmentHazardKind::Fire;
    }

    const auto raw = value.toString().toLower();
    if (raw == "smoke") {
        return safecrowd::domain::EnvironmentHazardKind::Smoke;
    }
    return safecrowd::domain::EnvironmentHazardKind::Fire;
}

QString severityToJson(safecrowd::domain::ScenarioElementSeverity severity) {
    switch (severity) {
    case safecrowd::domain::ScenarioElementSeverity::Low:
        return "Low";
    case safecrowd::domain::ScenarioElementSeverity::High:
        return "High";
    case safecrowd::domain::ScenarioElementSeverity::Medium:
    default:
        return "Medium";
    }
}

safecrowd::domain::ScenarioElementSeverity severityFromJson(const QJsonValue& value) {
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

    const auto raw = value.toString().toLower();
    if (raw == "low") {
        return safecrowd::domain::ScenarioElementSeverity::Low;
    }
    if (raw == "high") {
        return safecrowd::domain::ScenarioElementSeverity::High;
    }
    return safecrowd::domain::ScenarioElementSeverity::Medium;
}

QJsonObject hazardToJson(const safecrowd::domain::EnvironmentHazardDraft& hazard) {
    QJsonObject object;
    object["id"] = QString::fromStdString(hazard.id);
    object["kind"] = hazardKindToJson(hazard.kind);
    object["name"] = QString::fromStdString(hazard.name);
    object["affectedZoneId"] = QString::fromStdString(hazard.affectedZoneId);
    object["floorId"] = QString::fromStdString(hazard.floorId);
    object["position"] = pointArray(hazard.position);
    object["startSeconds"] = hazard.startSeconds;
    object["endSeconds"] = hazard.endSeconds;
    object["severity"] = severityToJson(hazard.severity);
    object["note"] = QString::fromStdString(hazard.note);
    return object;
}

safecrowd::domain::EnvironmentHazardDraft hazardFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .kind = hazardKindFromJson(object.value("kind")),
        .name = object.value("name").toString().toStdString(),
        .affectedZoneId = object.value("affectedZoneId").toString().toStdString(),
        .floorId = object.value("floorId").toString().toStdString(),
        .position = pointFromJson(object.value("position")),
        .startSeconds = object.value("startSeconds").toDouble(0.0),
        .endSeconds = object.value("endSeconds").toDouble(0.0),
        .severity = severityFromJson(object.value("severity")),
        .note = object.value("note").toString().toStdString(),
    };
}

QJsonObject environmentToJson(const safecrowd::domain::EnvironmentState& environment) {
    QJsonObject object;
    object["reducedVisibility"] = environment.reducedVisibility;
    object["familiarityProfile"] = QString::fromStdString(environment.familiarityProfile);
    object["guidanceProfile"] = QString::fromStdString(environment.guidanceProfile);
    QJsonArray hazards;
    for (const auto& hazard : environment.hazards) {
        hazards.append(hazardToJson(hazard));
    }
    object["hazards"] = hazards;
    return object;
}

safecrowd::domain::EnvironmentState environmentFromJson(const QJsonObject& object) {
    safecrowd::domain::EnvironmentState environment{
        .reducedVisibility = object.value("reducedVisibility").toBool(false),
        .familiarityProfile = object.value("familiarityProfile").toString().toStdString(),
        .guidanceProfile = object.value("guidanceProfile").toString().toStdString(),
    };
    for (const auto& value : object.value("hazards").toArray()) {
        environment.hazards.push_back(hazardFromJson(value.toObject()));
    }
    return environment;
}

QJsonObject eventToJson(const safecrowd::domain::OperationalEventDraft& event) {
    QJsonObject object;
    object["id"] = QString::fromStdString(event.id);
    object["name"] = QString::fromStdString(event.name);
    object["triggerSummary"] = QString::fromStdString(event.triggerSummary);
    object["targetSummary"] = QString::fromStdString(event.targetSummary);
    return object;
}

safecrowd::domain::OperationalEventDraft eventFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .name = object.value("name").toString().toStdString(),
        .triggerSummary = object.value("triggerSummary").toString().toStdString(),
        .targetSummary = object.value("targetSummary").toString().toStdString(),
    };
}

QJsonObject routeGuidanceToJson(const safecrowd::domain::RouteGuidanceDraft& guidance) {
    QJsonObject object;
    object["id"] = QString::fromStdString(guidance.id);
    object["startSeconds"] = guidance.startSeconds;
    object["endSeconds"] = guidance.endSeconds;
    QJsonArray periods;
    for (const auto& period : guidance.periods) {
        QJsonObject periodObject;
        periodObject["startSeconds"] = period.startSeconds;
        periodObject["endSeconds"] = period.endSeconds;
        periods.append(periodObject);
    }
    object["periods"] = periods;
    object["guidedExitZoneId"] = QString::fromStdString(guidance.guidedExitZoneId);
    object["installConnectionId"] = QString::fromStdString(guidance.installConnectionId);
    object["installFloorId"] = QString::fromStdString(guidance.installFloorId);
    object["installZoneId"] = QString::fromStdString(guidance.installZoneId);
    object["installPositionX"] = guidance.installPosition.x;
    object["installPositionY"] = guidance.installPosition.y;
    object["baseComplianceRate"] = guidance.baseComplianceRate;
    object["influenceRadiusMeters"] = guidance.influenceRadiusMeters;
    object["maxDetourMeters"] = guidance.maxDetourMeters;
    return object;
}

safecrowd::domain::RouteGuidanceDraft routeGuidanceFromJson(const QJsonObject& object) {
    safecrowd::domain::RouteGuidanceDraft guidance;
    guidance.id = object.value("id").toString().toStdString();
    guidance.startSeconds = object.value("startSeconds").toDouble(0.0);
    guidance.endSeconds = object.value("endSeconds").toDouble(10.0);
    const bool hasPeriodsField = object.contains("periods");
    for (const auto& value : object.value("periods").toArray()) {
        const auto periodObject = value.toObject();
        guidance.periods.push_back({
            .startSeconds = periodObject.value("startSeconds").toDouble(0.0),
            .endSeconds = periodObject.value("endSeconds").toDouble(0.0),
        });
    }
    if (!hasPeriodsField && guidance.periods.empty() && (object.contains("startSeconds") || object.contains("endSeconds"))) {
        // Backward compatibility: older projects stored a single scalar period.
        guidance.periods.push_back({
            .startSeconds = guidance.startSeconds,
            .endSeconds = guidance.endSeconds,
        });
    }
    guidance.guidedExitZoneId = object.value("guidedExitZoneId").toString().toStdString();
    guidance.installConnectionId = object.value("installConnectionId").toString().toStdString();
    guidance.installFloorId = object.value("installFloorId").toString().toStdString();
    guidance.installZoneId = object.value("installZoneId").toString().toStdString();
    guidance.installPosition = {
        .x = object.value("installPositionX").toDouble(0.0),
        .y = object.value("installPositionY").toDouble(0.0),
    };
    guidance.baseComplianceRate = object.value("baseComplianceRate").toDouble(0.5);
    guidance.influenceRadiusMeters = object.value("influenceRadiusMeters").toDouble(2.5);
    guidance.maxDetourMeters = object.value("maxDetourMeters").toDouble(20.0);
    return guidance;
}

QJsonObject connectionBlockIntervalToJson(const safecrowd::domain::ConnectionBlockIntervalDraft& interval) {
    QJsonObject object;
    object["startSeconds"] = interval.startSeconds;
    object["endSeconds"] = interval.endSeconds;
    return object;
}

safecrowd::domain::ConnectionBlockIntervalDraft connectionBlockIntervalFromJson(const QJsonObject& object) {
    return {
        .startSeconds = object.value("startSeconds").toDouble(),
        .endSeconds = object.value("endSeconds").toDouble(),
    };
}

QJsonObject connectionBlockToJson(const safecrowd::domain::ConnectionBlockDraft& block) {
    QJsonObject object;
    object["id"] = QString::fromStdString(block.id);
    object["connectionId"] = QString::fromStdString(block.connectionId);
    QJsonArray intervals;
    for (const auto& interval : block.intervals) {
        intervals.append(connectionBlockIntervalToJson(interval));
    }
    object["intervals"] = intervals;
    return object;
}

safecrowd::domain::ConnectionBlockDraft connectionBlockFromJson(const QJsonObject& object) {
    safecrowd::domain::ConnectionBlockDraft block;
    block.id = object.value("id").toString().toStdString();
    block.connectionId = object.value("connectionId").toString().toStdString();
    for (const auto& value : object.value("intervals").toArray()) {
        block.intervals.push_back(connectionBlockIntervalFromJson(value.toObject()));
    }
    return block;
}

QJsonObject controlPlanToJson(const safecrowd::domain::ControlPlan& control) {
    QJsonObject object;
    QJsonArray events;
    for (const auto& event : control.events) {
        events.append(eventToJson(event));
    }
    object["events"] = events;

    QJsonArray routeGuidances;
    for (const auto& guidance : control.routeGuidances) {
        routeGuidances.append(routeGuidanceToJson(guidance));
    }
    object["routeGuidances"] = routeGuidances;

    QJsonArray connectionBlocks;
    for (const auto& block : control.connectionBlocks) {
        connectionBlocks.append(connectionBlockToJson(block));
    }
    object["connectionBlocks"] = connectionBlocks;
    return object;
}

safecrowd::domain::ControlPlan controlPlanFromJson(const QJsonObject& object) {
    safecrowd::domain::ControlPlan control;
    for (const auto& value : object.value("events").toArray()) {
        control.events.push_back(eventFromJson(value.toObject()));
    }
    for (const auto& value : object.value("routeGuidances").toArray()) {
        control.routeGuidances.push_back(routeGuidanceFromJson(value.toObject()));
    }
    for (const auto& value : object.value("connectionBlocks").toArray()) {
        control.connectionBlocks.push_back(connectionBlockFromJson(value.toObject()));
    }
    return control;
}

QJsonObject executionToJson(const safecrowd::domain::ExecutionConfig& execution) {
    QJsonObject object;
    object["timeLimitSeconds"] = execution.timeLimitSeconds;
    object["sampleIntervalSeconds"] = execution.sampleIntervalSeconds;
    object["repeatCount"] = static_cast<int>(execution.repeatCount);
    object["baseSeed"] = static_cast<int>(execution.baseSeed);
    object["recordOccupantHistory"] = execution.recordOccupantHistory;
    return object;
}

safecrowd::domain::ExecutionConfig executionFromJson(const QJsonObject& object) {
    return {
        .timeLimitSeconds = object.value("timeLimitSeconds").toDouble(),
        .sampleIntervalSeconds = object.value("sampleIntervalSeconds").toDouble(),
        .repeatCount = static_cast<std::uint32_t>(object.value("repeatCount").toInt(1)),
        .baseSeed = static_cast<std::uint32_t>(object.value("baseSeed").toInt()),
        .recordOccupantHistory = object.value("recordOccupantHistory").toBool(false),
    };
}

QJsonObject scenarioDraftToJson(const safecrowd::domain::ScenarioDraft& scenario) {
    QJsonObject object;
    object["scenarioId"] = QString::fromStdString(scenario.scenarioId);
    object["name"] = QString::fromStdString(scenario.name);
    object["role"] = static_cast<int>(scenario.role);
    object["population"] = populationToJson(scenario.population);
    object["environment"] = environmentToJson(scenario.environment);
    object["control"] = controlPlanToJson(scenario.control);
    object["execution"] = executionToJson(scenario.execution);
    object["sourceTemplateId"] = QString::fromStdString(scenario.sourceTemplateId);
    object["variationDiffKeys"] = stringArray(scenario.variationDiffKeys);
    object["blockingIssues"] = stringArray(scenario.blockingIssues);
    return object;
}

safecrowd::domain::ScenarioDraft scenarioDraftFromJson(const QJsonObject& object) {
    return {
        .scenarioId = object.value("scenarioId").toString().toStdString(),
        .name = object.value("name").toString().toStdString(),
        .role = static_cast<safecrowd::domain::ScenarioRole>(object.value("role").toInt()),
        .population = populationFromJson(object.value("population").toObject()),
        .environment = environmentFromJson(object.value("environment").toObject()),
        .control = controlPlanFromJson(object.value("control").toObject()),
        .execution = executionFromJson(object.value("execution").toObject()),
        .sourceTemplateId = object.value("sourceTemplateId").toString().toStdString(),
        .variationDiffKeys = stringVectorFromJson(object.value("variationDiffKeys").toArray()),
        .blockingIssues = stringVectorFromJson(object.value("blockingIssues").toArray()),
    };
}

QJsonObject savedScenarioStateToJson(const SavedScenarioState& scenario) {
    QJsonObject object;
    object["draft"] = scenarioDraftToJson(scenario.draft);
    object["baseScenarioId"] = QString::fromStdString(scenario.baseScenarioId);
    object["stagedForRun"] = scenario.stagedForRun;
    return object;
}

SavedScenarioState savedScenarioStateFromJson(const QJsonObject& object) {
    return {
        .draft = scenarioDraftFromJson(object.value("draft").toObject()),
        .baseScenarioId = object.value("baseScenarioId").toString().toStdString(),
        .stagedForRun = object.value("stagedForRun").toBool(false),
    };
}

QJsonObject authoringStateToJson(const SavedScenarioAuthoringState& authoring) {
    QJsonObject object;
    QJsonArray scenarios;
    for (const auto& scenario : authoring.scenarios) {
        scenarios.append(savedScenarioStateToJson(scenario));
    }
    object["scenarios"] = scenarios;
    object["currentScenarioIndex"] = authoring.currentScenarioIndex;
    object["navigationView"] = static_cast<int>(authoring.navigationView);
    object["inspectorPanelVisible"] = authoring.inspectorPanelVisible;
    object["scenarioPanelVisible"] = authoring.scenarioPanelVisible;
    object["rightPanelMode"] = static_cast<int>(authoring.rightPanelMode);
    return object;
}

SavedScenarioAuthoringState authoringStateFromJson(const QJsonObject& object) {
    SavedScenarioAuthoringState authoring;
    for (const auto& value : object.value("scenarios").toArray()) {
        authoring.scenarios.push_back(savedScenarioStateFromJson(value.toObject()));
    }
    authoring.currentScenarioIndex = object.value("currentScenarioIndex").toInt(-1);
    authoring.navigationView = static_cast<SavedNavigationView>(object.value("navigationView").toInt());
    authoring.rightPanelMode = static_cast<SavedRightPanelMode>(object.value("rightPanelMode").toInt(1));
    if (object.contains("inspectorPanelVisible") || object.contains("scenarioPanelVisible")) {
        authoring.inspectorPanelVisible = object.value("inspectorPanelVisible").toBool(true);
        authoring.scenarioPanelVisible = object.value("scenarioPanelVisible").toBool(true);
    } else {
        const bool visible = authoring.rightPanelMode != SavedRightPanelMode::None;
        authoring.inspectorPanelVisible = visible;
        authoring.scenarioPanelVisible = visible;
    }
    return authoring;
}

QJsonObject resultStateToJson(const SavedScenarioResultState& result) {
    QJsonObject object;
    object["scenario"] = scenarioDraftToJson(result.scenario);
    object["frame"] = simulationFrameToJson(result.frame);
    object["risk"] = riskSnapshotToJson(result.risk);
    object["artifacts"] = resultArtifactsToJson(result.artifacts);
    object["navigationView"] = static_cast<int>(result.navigationView);
    return object;
}

SavedScenarioResultState resultStateFromJson(const QJsonObject& object) {
    return {
        .scenario = scenarioDraftFromJson(object.value("scenario").toObject()),
        .frame = simulationFrameFromJson(object.value("frame").toObject()),
        .risk = riskSnapshotFromJson(object.value("risk").toObject()),
        .artifacts = resultArtifactsFromJson(object.value("artifacts").toObject()),
        .navigationView = static_cast<SavedResultNavigationView>(object.value("navigationView").toInt()),
    };
}

QJsonArray scenarioDraftsToJson(const std::vector<safecrowd::domain::ScenarioDraft>& scenarios) {
    QJsonArray array;
    for (const auto& scenario : scenarios) {
        array.append(scenarioDraftToJson(scenario));
    }
    return array;
}

std::vector<safecrowd::domain::ScenarioDraft> scenarioDraftsFromJson(const QJsonArray& array) {
    std::vector<safecrowd::domain::ScenarioDraft> scenarios;
    scenarios.reserve(static_cast<std::size_t>(array.size()));
    for (const auto& value : array) {
        scenarios.push_back(scenarioDraftFromJson(value.toObject()));
    }
    return scenarios;
}

QJsonObject batchResultStateToJson(const SavedScenarioBatchResultState& batch) {
    QJsonObject object;
    QJsonArray results;
    for (const auto& result : batch.results) {
        results.append(resultStateToJson(result));
    }
    object["results"] = results;
    object["currentResultIndex"] = batch.currentResultIndex;
    return object;
}

SavedScenarioBatchResultState batchResultStateFromJson(const QJsonObject& object) {
    SavedScenarioBatchResultState batch;
    for (const auto& value : object.value("results").toArray()) {
        batch.results.push_back(resultStateFromJson(value.toObject()));
    }
    batch.currentResultIndex = object.value("currentResultIndex").toInt(0);
    if (batch.currentResultIndex < 0 || batch.currentResultIndex >= static_cast<int>(batch.results.size())) {
        batch.currentResultIndex = 0;
    }
    return batch;
}

QJsonObject workspaceStateToJson(const ProjectWorkspaceState& state) {
    QJsonObject object;
    object["version"] = 1;
    object["activeView"] = static_cast<int>(state.activeView);
    if (state.authoring.has_value()) {
        object["authoring"] = authoringStateToJson(*state.authoring);
    }
    if (state.runningScenario.has_value()) {
        object["runningScenario"] = scenarioDraftToJson(*state.runningScenario);
    }
    if (!state.runningScenarios.empty()) {
        object["runningScenarios"] = scenarioDraftsToJson(state.runningScenarios);
    }
    if (state.result.has_value()) {
        object["result"] = resultStateToJson(*state.result);
    }
    if (state.batchResult.has_value()) {
        object["batchResult"] = batchResultStateToJson(*state.batchResult);
    }
    return object;
}

ProjectWorkspaceState workspaceStateFromJson(const QJsonObject& object) {
    ProjectWorkspaceState state;
    const auto version = object.value("version").toInt(kCurrentWorkspaceStateVersion);
    (void)version;

    state.activeView = static_cast<ProjectWorkspaceView>(object.value("activeView").toInt());
    if (object.value("authoring").isObject()) {
        state.authoring = authoringStateFromJson(object.value("authoring").toObject());
    }
    if (object.value("runningScenario").isObject()) {
        state.runningScenario = scenarioDraftFromJson(object.value("runningScenario").toObject());
    }
    if (object.value("runningScenarios").isArray()) {
        state.runningScenarios = scenarioDraftsFromJson(object.value("runningScenarios").toArray());
    } else if (state.runningScenario.has_value()) {
        state.runningScenarios.push_back(*state.runningScenario);
    }
    if (object.value("result").isObject()) {
        state.result = resultStateFromJson(object.value("result").toObject());
    }
    if (object.value("batchResult").isObject()) {
        state.batchResult = batchResultStateFromJson(object.value("batchResult").toObject());
    } else if (state.result.has_value()) {
        state.batchResult = SavedScenarioBatchResultState{
            .results = {*state.result},
            .currentResultIndex = 0,
        };
    }
    return state;
}
}  // namespace safecrowd::application
