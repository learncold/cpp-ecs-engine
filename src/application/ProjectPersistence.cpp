#include "application/ProjectPersistence.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "domain/ImportValidationService.h"

namespace safecrowd::application {
namespace {

constexpr auto kProjectFileName = "safecrowd-project.json";
constexpr auto kLayoutFileName = "layout.dxf";
constexpr auto kReviewFileName = "layout-review.json";
constexpr auto kScenarioAuthoringFileName = "scenario-authoring.json";

bool isProjectManagedEntry(const QString& fileName) {
    return fileName.compare(kProjectFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kLayoutFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kReviewFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kScenarioAuthoringFileName, Qt::CaseInsensitive) == 0;
}

QString projectFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kProjectFileName);
}

QString reviewFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kReviewFileName);
}

QString scenarioAuthoringFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kScenarioAuthoringFileName);
}

QString recentProjectsPath() {
    const auto appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return QDir(appData).filePath("recent-projects.json");
}

ProjectMetadata fromJson(const QJsonObject& object) {
    return {
        .name = object.value("name").toString(),
        .folderPath = object.value("folderPath").toString(),
        .layoutPath = object.value("layoutPath").toString(),
        .savedAt = object.value("savedAt").toString(),
    };
}

QJsonObject toJson(const ProjectMetadata& metadata) {
    QJsonObject object;
    object["name"] = metadata.name;
    object["folderPath"] = metadata.folderPath;
    object["layoutPath"] = metadata.layoutPath;
    object["savedAt"] = metadata.savedAt;
    return object;
}

QJsonDocument readJsonDocument(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll());
}

bool writeJsonDocument(const QString& path, const QJsonDocument& document, QString* errorMessage) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    file.write(document.toJson(QJsonDocument::Indented));
    return true;
}

void upsertRecentProject(const ProjectMetadata& metadata) {
    QJsonArray projects;
    const auto recentPath = recentProjectsPath();
    const auto document = readJsonDocument(recentPath);
    if (document.isObject()) {
        projects = document.object().value("projects").toArray();
    }

    QJsonArray updated;
    updated.append(toJson(metadata));

    const auto normalizedFolder = QDir(metadata.folderPath).absolutePath();
    for (const auto& value : projects) {
        const auto existing = fromJson(value.toObject());
        if (QDir(existing.folderPath).absolutePath() == normalizedFolder) {
            continue;
        }
        updated.append(value);
    }

    QJsonObject root;
    root["projects"] = updated;
    QString ignoredError;
    writeJsonDocument(recentPath, QJsonDocument(root), &ignoredError);
}

void removeRecentProject(const QString& folderPath) {
    const auto recentPath = recentProjectsPath();
    const auto document = readJsonDocument(recentPath);
    if (!document.isObject()) {
        return;
    }

    QJsonArray updated;
    const auto normalizedFolder = QDir(folderPath).absolutePath();
    for (const auto& value : document.object().value("projects").toArray()) {
        const auto existing = fromJson(value.toObject());
        if (QDir(existing.folderPath).absolutePath() == normalizedFolder) {
            continue;
        }
        updated.append(value);
    }

    QJsonObject root;
    root["projects"] = updated;
    QString ignoredError;
    writeJsonDocument(recentPath, QJsonDocument(root), &ignoredError);
}

bool canDeleteProjectFolder(const QString& folderPath, QString* errorMessage) {
    QDir folder(folderPath);
    if (!folder.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Project folder does not exist: %1").arg(folderPath);
        }
        return false;
    }

    const QFileInfo folderInfo(folder.absolutePath());
    if (folderInfo.absoluteFilePath() == QDir(folderInfo.absolutePath()).rootPath()) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Refusing to delete a drive root: %1").arg(folderPath);
        }
        return false;
    }

    const auto entries = folder.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const auto& entry : entries) {
        if (!entry.isFile() || !isProjectManagedEntry(entry.fileName())) {
            if (errorMessage != nullptr) {
                *errorMessage = QString(
                    "Refusing to delete project folder because it contains a file or folder not created by SafeCrowd: %1")
                    .arg(entry.fileName());
            }
            return false;
        }
    }

    return true;
}

bool copyLayoutIntoProject(ProjectMetadata& metadata, QString* errorMessage) {
    const auto sourcePath = QFileInfo(metadata.layoutPath).absoluteFilePath();
    const auto targetPath = QDir(metadata.folderPath).filePath(kLayoutFileName);

    if (QFileInfo(sourcePath).absoluteFilePath() == QFileInfo(targetPath).absoluteFilePath()) {
        metadata.layoutPath = targetPath;
        return true;
    }

    QFile::remove(targetPath);
    if (!QFile::copy(sourcePath, targetPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to copy layout file to %1.").arg(targetPath);
        }
        return false;
    }

    metadata.layoutPath = targetPath;
    return true;
}

QJsonArray pointArray(const safecrowd::domain::Point2D& point) {
    return QJsonArray{point.x, point.y};
}

safecrowd::domain::Point2D pointFromJson(const QJsonValue& value) {
    const auto array = value.toArray();
    return {
        .x = array.size() > 0 ? array.at(0).toDouble() : 0.0,
        .y = array.size() > 1 ? array.at(1).toDouble() : 0.0,
    };
}

QJsonArray ringToJson(const std::vector<safecrowd::domain::Point2D>& ring) {
    QJsonArray array;
    for (const auto& point : ring) {
        array.append(pointArray(point));
    }
    return array;
}

std::vector<safecrowd::domain::Point2D> ringFromJson(const QJsonArray& array) {
    std::vector<safecrowd::domain::Point2D> ring;
    ring.reserve(array.size());
    for (const auto& value : array) {
        ring.push_back(pointFromJson(value));
    }
    return ring;
}

QJsonObject polygonToJson(const safecrowd::domain::Polygon2D& polygon) {
    QJsonObject object;
    object["outline"] = ringToJson(polygon.outline);

    QJsonArray holes;
    for (const auto& hole : polygon.holes) {
        holes.append(ringToJson(hole));
    }
    object["holes"] = holes;
    return object;
}

safecrowd::domain::Polygon2D polygonFromJson(const QJsonObject& object) {
    safecrowd::domain::Polygon2D polygon;
    polygon.outline = ringFromJson(object.value("outline").toArray());
    for (const auto& holeValue : object.value("holes").toArray()) {
        polygon.holes.push_back(ringFromJson(holeValue.toArray()));
    }
    return polygon;
}

QJsonObject lineToJson(const safecrowd::domain::LineSegment2D& line) {
    QJsonObject object;
    object["start"] = pointArray(line.start);
    object["end"] = pointArray(line.end);
    return object;
}

safecrowd::domain::LineSegment2D lineFromJson(const QJsonObject& object) {
    return {
        .start = pointFromJson(object.value("start")),
        .end = pointFromJson(object.value("end")),
    };
}

QJsonObject polylineToJson(const safecrowd::domain::Polyline2D& polyline) {
    QJsonObject object;
    object["vertices"] = ringToJson(polyline.vertices);
    object["closed"] = polyline.closed;
    return object;
}

safecrowd::domain::Polyline2D polylineFromJson(const QJsonObject& object) {
    return {
        .vertices = ringFromJson(object.value("vertices").toArray()),
        .closed = object.value("closed").toBool(false),
    };
}

QJsonArray stringArray(const std::vector<std::string>& values) {
    QJsonArray array;
    for (const auto& value : values) {
        array.append(QString::fromStdString(value));
    }
    return array;
}

std::vector<std::string> stringVectorFromJson(const QJsonArray& array) {
    std::vector<std::string> values;
    values.reserve(array.size());
    for (const auto& value : array) {
        values.push_back(value.toString().toStdString());
    }
    return values;
}

QJsonArray placementAreaToJson(const std::vector<safecrowd::domain::Point2D>& area) {
    return ringToJson(area);
}

std::vector<safecrowd::domain::Point2D> placementAreaFromJson(const QJsonArray& array) {
    return ringFromJson(array);
}

QJsonObject provenanceToJson(const safecrowd::domain::ElementProvenance& provenance) {
    QJsonObject object;
    object["sourceIds"] = stringArray(provenance.sourceIds);
    object["canonicalIds"] = stringArray(provenance.canonicalIds);
    return object;
}

safecrowd::domain::ElementProvenance provenanceFromJson(const QJsonObject& object) {
    return {
        .sourceIds = stringVectorFromJson(object.value("sourceIds").toArray()),
        .canonicalIds = stringVectorFromJson(object.value("canonicalIds").toArray()),
    };
}

QJsonObject zoneToJson(const safecrowd::domain::Zone2D& zone) {
    QJsonObject object;
    object["id"] = QString::fromStdString(zone.id);
    object["kind"] = static_cast<int>(zone.kind);
    object["label"] = QString::fromStdString(zone.label);
    object["area"] = polygonToJson(zone.area);
    object["defaultCapacity"] = static_cast<qint64>(zone.defaultCapacity);
    object["isStair"] = zone.isStair;
    object["isRamp"] = zone.isRamp;
    object["provenance"] = provenanceToJson(zone.provenance);
    return object;
}

safecrowd::domain::Zone2D zoneFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .kind = static_cast<safecrowd::domain::ZoneKind>(object.value("kind").toInt()),
        .label = object.value("label").toString().toStdString(),
        .area = polygonFromJson(object.value("area").toObject()),
        .defaultCapacity = static_cast<std::size_t>(object.value("defaultCapacity").toInteger()),
        .isStair = object.value("isStair").toBool(false),
        .isRamp = object.value("isRamp").toBool(false),
        .provenance = provenanceFromJson(object.value("provenance").toObject()),
    };
}

QJsonObject connectionToJson(const safecrowd::domain::Connection2D& connection) {
    QJsonObject object;
    object["id"] = QString::fromStdString(connection.id);
    object["kind"] = static_cast<int>(connection.kind);
    object["fromZoneId"] = QString::fromStdString(connection.fromZoneId);
    object["toZoneId"] = QString::fromStdString(connection.toZoneId);
    object["effectiveWidth"] = connection.effectiveWidth;
    object["directionality"] = static_cast<int>(connection.directionality);
    object["isStair"] = connection.isStair;
    object["isRamp"] = connection.isRamp;
    object["centerSpan"] = lineToJson(connection.centerSpan);
    object["provenance"] = provenanceToJson(connection.provenance);
    return object;
}

safecrowd::domain::Connection2D connectionFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .kind = static_cast<safecrowd::domain::ConnectionKind>(object.value("kind").toInt()),
        .fromZoneId = object.value("fromZoneId").toString().toStdString(),
        .toZoneId = object.value("toZoneId").toString().toStdString(),
        .effectiveWidth = object.value("effectiveWidth").toDouble(),
        .directionality = static_cast<safecrowd::domain::TravelDirection>(object.value("directionality").toInt()),
        .isStair = object.value("isStair").toBool(false),
        .isRamp = object.value("isRamp").toBool(false),
        .centerSpan = lineFromJson(object.value("centerSpan").toObject()),
        .provenance = provenanceFromJson(object.value("provenance").toObject()),
    };
}

QJsonObject barrierToJson(const safecrowd::domain::Barrier2D& barrier) {
    QJsonObject object;
    object["id"] = QString::fromStdString(barrier.id);
    object["geometry"] = polylineToJson(barrier.geometry);
    object["blocksMovement"] = barrier.blocksMovement;
    object["provenance"] = provenanceToJson(barrier.provenance);
    return object;
}

safecrowd::domain::Barrier2D barrierFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .geometry = polylineFromJson(object.value("geometry").toObject()),
        .blocksMovement = object.value("blocksMovement").toBool(true),
        .provenance = provenanceFromJson(object.value("provenance").toObject()),
    };
}

QJsonObject controlToJson(const safecrowd::domain::ControlPoint2D& control) {
    QJsonObject object;
    object["id"] = QString::fromStdString(control.id);
    object["kind"] = static_cast<int>(control.kind);
    object["targetId"] = QString::fromStdString(control.targetId);
    object["defaultOpen"] = control.defaultOpen;
    object["provenance"] = provenanceToJson(control.provenance);
    return object;
}

safecrowd::domain::ControlPoint2D controlFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .kind = static_cast<safecrowd::domain::ControlKind>(object.value("kind").toInt()),
        .targetId = object.value("targetId").toString().toStdString(),
        .defaultOpen = object.value("defaultOpen").toBool(true),
        .provenance = provenanceFromJson(object.value("provenance").toObject()),
    };
}

QJsonObject layoutToJson(const safecrowd::domain::FacilityLayout2D& layout) {
    QJsonObject object;
    object["id"] = QString::fromStdString(layout.id);
    object["name"] = QString::fromStdString(layout.name);
    object["levelId"] = QString::fromStdString(layout.levelId);

    QJsonArray zones;
    for (const auto& zone : layout.zones) {
        zones.append(zoneToJson(zone));
    }
    object["zones"] = zones;

    QJsonArray connections;
    for (const auto& connection : layout.connections) {
        connections.append(connectionToJson(connection));
    }
    object["connections"] = connections;

    QJsonArray barriers;
    for (const auto& barrier : layout.barriers) {
        barriers.append(barrierToJson(barrier));
    }
    object["barriers"] = barriers;

    QJsonArray controls;
    for (const auto& control : layout.controls) {
        controls.append(controlToJson(control));
    }
    object["controls"] = controls;

    return object;
}

safecrowd::domain::FacilityLayout2D layoutFromJson(const QJsonObject& object) {
    safecrowd::domain::FacilityLayout2D layout;
    layout.id = object.value("id").toString().toStdString();
    layout.name = object.value("name").toString().toStdString();
    layout.levelId = object.value("levelId").toString().toStdString();

    for (const auto& value : object.value("zones").toArray()) {
        layout.zones.push_back(zoneFromJson(value.toObject()));
    }
    for (const auto& value : object.value("connections").toArray()) {
        layout.connections.push_back(connectionFromJson(value.toObject()));
    }
    for (const auto& value : object.value("barriers").toArray()) {
        layout.barriers.push_back(barrierFromJson(value.toObject()));
    }
    for (const auto& value : object.value("controls").toArray()) {
        layout.controls.push_back(controlFromJson(value.toObject()));
    }

    return layout;
}

bool isLiveValidationIssue(safecrowd::domain::ImportIssueCode code) {
    using safecrowd::domain::ImportIssueCode;

    switch (code) {
    case ImportIssueCode::MissingExit:
    case ImportIssueCode::DisconnectedWalkableArea:
    case ImportIssueCode::WidthBelowMinimum:
        return true;
    default:
        return false;
    }
}

void updateLiveValidationIssues(safecrowd::domain::ImportResult* importResult) {
    if (importResult == nullptr) {
        return;
    }

    std::vector<safecrowd::domain::ImportIssue> issues;
    issues.reserve(importResult->issues.size());
    for (const auto& issue : importResult->issues) {
        if (!isLiveValidationIssue(issue.code)) {
            issues.push_back(issue);
        }
    }

    if (importResult->layout.has_value()) {
        safecrowd::domain::ImportValidationService validator;
        auto validatedIssues = validator.validate(*importResult->layout);
        issues.insert(issues.end(), validatedIssues.begin(), validatedIssues.end());
    }

    importResult->issues = std::move(issues);
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
        .timeLimitSeconds = std::max(1.0, object.value("timeLimitSeconds").toDouble(600.0)),
        .sampleIntervalSeconds = std::max(0.1, object.value("sampleIntervalSeconds").toDouble(1.0)),
        .repeatCount = static_cast<std::uint32_t>(std::max(1, object.value("repeatCount").toInt(1))),
        .baseSeed = static_cast<std::uint32_t>(object.value("baseSeed").toInt()),
        .recordOccupantHistory = object.value("recordOccupantHistory").toBool(false),
    };
}

safecrowd::domain::ScenarioRole scenarioRoleFromJson(const QJsonValue& value) {
    switch (value.toInt(static_cast<int>(safecrowd::domain::ScenarioRole::Alternative))) {
    case static_cast<int>(safecrowd::domain::ScenarioRole::Baseline):
        return safecrowd::domain::ScenarioRole::Baseline;
    case static_cast<int>(safecrowd::domain::ScenarioRole::Recommended):
        return safecrowd::domain::ScenarioRole::Recommended;
    case static_cast<int>(safecrowd::domain::ScenarioRole::Alternative):
    default:
        return safecrowd::domain::ScenarioRole::Alternative;
    }
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

QJsonArray eventsToJson(const std::vector<safecrowd::domain::OperationalEventDraft>& events) {
    QJsonArray array;
    for (const auto& event : events) {
        array.append(eventToJson(event));
    }
    return array;
}

std::vector<safecrowd::domain::OperationalEventDraft> eventsFromJson(const QJsonArray& array) {
    std::vector<safecrowd::domain::OperationalEventDraft> events;
    events.reserve(array.size());
    for (const auto& value : array) {
        events.push_back(eventFromJson(value.toObject()));
    }
    return events;
}

QJsonObject placementToJson(const ScenarioCrowdPlacement& placement) {
    QJsonObject object;
    object["id"] = placement.id;
    object["name"] = placement.name;
    object["kind"] = static_cast<int>(placement.kind);
    object["zoneId"] = placement.zoneId;
    object["area"] = placementAreaToJson(placement.area);
    object["occupantCount"] = placement.occupantCount;
    object["velocity"] = pointArray(placement.velocity);
    return object;
}

ScenarioCrowdPlacement placementFromJson(const QJsonObject& object) {
    const auto kindValue = object.value("kind").toInt(static_cast<int>(ScenarioCrowdPlacementKind::Individual));
    const auto kind = kindValue == static_cast<int>(ScenarioCrowdPlacementKind::Group)
        ? ScenarioCrowdPlacementKind::Group
        : ScenarioCrowdPlacementKind::Individual;

    return {
        .id = object.value("id").toString(),
        .name = object.value("name").toString(),
        .kind = kind,
        .zoneId = object.value("zoneId").toString(),
        .area = placementAreaFromJson(object.value("area").toArray()),
        .occupantCount = std::max(1, object.value("occupantCount").toInt(1)),
        .velocity = pointFromJson(object.value("velocity")),
    };
}

QJsonArray placementsToJson(const std::vector<ScenarioCrowdPlacement>& placements) {
    QJsonArray array;
    for (const auto& placement : placements) {
        array.append(placementToJson(placement));
    }
    return array;
}

std::vector<ScenarioCrowdPlacement> placementsFromJson(const QJsonArray& array) {
    std::vector<ScenarioCrowdPlacement> placements;
    placements.reserve(array.size());
    for (const auto& value : array) {
        placements.push_back(placementFromJson(value.toObject()));
    }
    return placements;
}

std::unordered_set<std::string> layoutZoneIds(const safecrowd::domain::FacilityLayout2D& layout) {
    std::unordered_set<std::string> zoneIds;
    zoneIds.reserve(layout.zones.size());
    for (const auto& zone : layout.zones) {
        zoneIds.insert(zone.id);
    }
    return zoneIds;
}

void syncDraftFromScenarioState(ScenarioAuthoringWidget::ScenarioState* scenario) {
    if (scenario == nullptr) {
        return;
    }

    scenario->draft.control.events = scenario->events;
    scenario->draft.population.initialPlacements.clear();
    for (const auto& placement : scenario->crowdPlacements) {
        safecrowd::domain::InitialPlacement2D initialPlacement;
        initialPlacement.id = placement.id.toStdString();
        initialPlacement.zoneId = placement.zoneId.toStdString();
        initialPlacement.area.outline = placement.area;
        initialPlacement.targetAgentCount = static_cast<std::size_t>(placement.occupantCount);
        initialPlacement.initialVelocity = placement.velocity;
        scenario->draft.population.initialPlacements.push_back(std::move(initialPlacement));
    }
}

void removePlacementsOutsideLayout(
    const safecrowd::domain::FacilityLayout2D& layout,
    ScenarioAuthoringWidget::ScenarioState* scenario) {
    if (scenario == nullptr || scenario->crowdPlacements.empty()) {
        return;
    }

    const auto validZoneIds = layoutZoneIds(layout);
    const auto oldSize = scenario->crowdPlacements.size();
    scenario->crowdPlacements.erase(
        std::remove_if(
            scenario->crowdPlacements.begin(),
            scenario->crowdPlacements.end(),
            [&validZoneIds](const ScenarioCrowdPlacement& placement) {
                return !validZoneIds.contains(placement.zoneId.toStdString());
            }),
        scenario->crowdPlacements.end());

    if (scenario->crowdPlacements.size() != oldSize) {
        scenario->stagedForRun = false;
        syncDraftFromScenarioState(scenario);
    }
}

QJsonObject scenarioStateToJson(const ScenarioAuthoringWidget::ScenarioState& scenario) {
    QJsonObject object;
    object["scenarioId"] = QString::fromStdString(scenario.draft.scenarioId);
    object["name"] = QString::fromStdString(scenario.draft.name);
    object["role"] = static_cast<int>(scenario.draft.role);
    object["sourceTemplateId"] = QString::fromStdString(scenario.draft.sourceTemplateId);
    object["variationDiffKeys"] = stringArray(scenario.draft.variationDiffKeys);
    object["execution"] = executionToJson(scenario.draft.execution);
    object["events"] = eventsToJson(scenario.events);
    object["placements"] = placementsToJson(scenario.crowdPlacements);
    object["startText"] = scenario.startText;
    object["destinationText"] = scenario.destinationText;
    object["baseScenarioId"] = scenario.baseScenarioId;
    object["stagedForRun"] = scenario.stagedForRun;
    return object;
}

ScenarioAuthoringWidget::ScenarioState scenarioStateFromJson(const QJsonObject& object) {
    ScenarioAuthoringWidget::ScenarioState scenario;
    scenario.draft.scenarioId = object.value("scenarioId").toString().toStdString();
    scenario.draft.name = object.value("name").toString().toStdString();
    scenario.draft.role = scenarioRoleFromJson(object.value("role"));
    scenario.draft.sourceTemplateId = object.value("sourceTemplateId").toString().toStdString();
    scenario.draft.variationDiffKeys = stringVectorFromJson(object.value("variationDiffKeys").toArray());
    scenario.draft.execution = executionFromJson(object.value("execution").toObject());
    scenario.events = eventsFromJson(object.value("events").toArray());
    scenario.crowdPlacements = placementsFromJson(object.value("placements").toArray());
    scenario.startText = object.value("startText").toString();
    scenario.destinationText = object.value("destinationText").toString();
    scenario.baseScenarioId = object.value("baseScenarioId").toString();
    scenario.stagedForRun = object.value("stagedForRun").toBool(false);
    syncDraftFromScenarioState(&scenario);
    return scenario;
}

ScenarioAuthoringWidget::NavigationView navigationViewFromJson(const QJsonValue& value) {
    switch (value.toInt(static_cast<int>(ScenarioAuthoringWidget::NavigationView::Layout))) {
    case static_cast<int>(ScenarioAuthoringWidget::NavigationView::Crowd):
        return ScenarioAuthoringWidget::NavigationView::Crowd;
    case static_cast<int>(ScenarioAuthoringWidget::NavigationView::Events):
        return ScenarioAuthoringWidget::NavigationView::Events;
    case static_cast<int>(ScenarioAuthoringWidget::NavigationView::Layout):
    default:
        return ScenarioAuthoringWidget::NavigationView::Layout;
    }
}

ScenarioAuthoringWidget::RightPanelMode rightPanelModeFromJson(const QJsonValue& value) {
    switch (value.toInt(static_cast<int>(ScenarioAuthoringWidget::RightPanelMode::Scenario))) {
    case static_cast<int>(ScenarioAuthoringWidget::RightPanelMode::None):
        return ScenarioAuthoringWidget::RightPanelMode::None;
    case static_cast<int>(ScenarioAuthoringWidget::RightPanelMode::Run):
        return ScenarioAuthoringWidget::RightPanelMode::Run;
    case static_cast<int>(ScenarioAuthoringWidget::RightPanelMode::Scenario):
    default:
        return ScenarioAuthoringWidget::RightPanelMode::Scenario;
    }
}

}  // namespace

QList<ProjectMetadata> ProjectPersistence::loadRecentProjects() {
    QList<ProjectMetadata> projects;
    projects.push_back(makeBuiltInDemoProject());

    const auto document = readJsonDocument(recentProjectsPath());
    if (!document.isObject()) {
        return projects;
    }

    for (const auto& value : document.object().value("projects").toArray()) {
        const auto indexed = fromJson(value.toObject());
        if (indexed.isBuiltInDemo()) {
            continue;
        }
        const auto loaded = loadProject(indexed.folderPath);
        if (loaded.isValid()) {
            projects.push_back(loaded);
        }
    }

    return projects;
}

ProjectMetadata ProjectPersistence::loadProject(const QString& folderPath) {
    const auto document = readJsonDocument(projectFilePath(folderPath));
    if (!document.isObject()) {
        return {};
    }

    return fromJson(document.object());
}

bool ProjectPersistence::deleteProject(const ProjectMetadata& metadata, QString* errorMessage) {
    if (metadata.isBuiltInDemo()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Built-in demo projects cannot be deleted.";
        }
        return false;
    }

    if (metadata.folderPath.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Project folder is missing.";
        }
        return false;
    }

    const auto projectFile = projectFilePath(metadata.folderPath);
    if (!QFileInfo::exists(projectFile)) {
        removeRecentProject(metadata.folderPath);
        return true;
    }

    const auto loaded = loadProject(metadata.folderPath);
    if (!loaded.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = "The selected folder does not contain a valid SafeCrowd project.";
        }
        return false;
    }

    if (!canDeleteProjectFolder(metadata.folderPath, errorMessage)) {
        return false;
    }

    QDir folder(metadata.folderPath);
    if (!folder.removeRecursively()) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to delete project folder: %1").arg(metadata.folderPath);
        }
        return false;
    }

    removeRecentProject(metadata.folderPath);
    return true;
}

bool ProjectPersistence::loadProjectReview(const ProjectMetadata& metadata, safecrowd::domain::ImportResult* importResult) {
    if (metadata.isBuiltInDemo() || importResult == nullptr) {
        return false;
    }

    const auto document = readJsonDocument(reviewFilePath(metadata.folderPath));
    if (!document.isObject()) {
        return false;
    }

    const auto root = document.object();
    if (!root.contains("layout") || !root.value("layout").isObject()) {
        return false;
    }

    importResult->layout = layoutFromJson(root.value("layout").toObject());
    importResult->reviewStatus = static_cast<safecrowd::domain::ImportReviewStatus>(root.value("reviewStatus").toInt());
    updateLiveValidationIssues(importResult);

    if (safecrowd::domain::hasBlockingImportIssue(importResult->issues)
        && importResult->reviewStatus == safecrowd::domain::ImportReviewStatus::Approved) {
        importResult->reviewStatus = safecrowd::domain::ImportReviewStatus::Pending;
    }

    return true;
}

bool ProjectPersistence::saveProject(ProjectMetadata metadata, QString* errorMessage) {
    if (metadata.isBuiltInDemo()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Built-in demo projects do not need to be saved.";
        }
        return false;
    }

    if (!metadata.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Project name, folder, and layout path are required.";
        }
        return false;
    }

    QDir folder(metadata.folderPath);
    if (!folder.exists() && !QDir().mkpath(metadata.folderPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to create project folder: %1").arg(metadata.folderPath);
        }
        return false;
    }

    if (!copyLayoutIntoProject(metadata, errorMessage)) {
        return false;
    }

    metadata.savedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (!writeJsonDocument(projectFilePath(metadata.folderPath), QJsonDocument(toJson(metadata)), errorMessage)) {
        return false;
    }

    upsertRecentProject(metadata);
    return true;
}

bool ProjectPersistence::saveProjectReview(
    const ProjectMetadata& metadata,
    const safecrowd::domain::ImportResult& importResult,
    QString* errorMessage) {
    if (metadata.isBuiltInDemo()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Built-in demo projects do not need to be saved.";
        }
        return false;
    }

    if (!importResult.layout.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = "No editable layout is loaded for this project.";
        }
        return false;
    }

    QJsonObject root;
    root["reviewStatus"] = static_cast<int>(importResult.reviewStatus);
    root["layout"] = layoutToJson(*importResult.layout);
    return writeJsonDocument(reviewFilePath(metadata.folderPath), QJsonDocument(root), errorMessage);
}

bool ProjectPersistence::loadScenarioAuthoringState(
    const ProjectMetadata& metadata,
    const safecrowd::domain::FacilityLayout2D& layout,
    ScenarioAuthoringWidget::InitialState* state) {
    if (metadata.isBuiltInDemo() || state == nullptr) {
        return false;
    }

    const auto document = readJsonDocument(scenarioAuthoringFilePath(metadata.folderPath));
    if (!document.isObject()) {
        return false;
    }

    const auto root = document.object();
    ScenarioAuthoringWidget::InitialState loaded;
    loaded.currentScenarioIndex = root.value("currentScenarioIndex").toInt(-1);
    loaded.navigationView = navigationViewFromJson(root.value("navigationView"));
    loaded.rightPanelMode = rightPanelModeFromJson(root.value("rightPanelMode"));

    for (const auto& value : root.value("scenarios").toArray()) {
        loaded.scenarios.push_back(scenarioStateFromJson(value.toObject()));
    }
    for (auto& scenario : loaded.scenarios) {
        removePlacementsOutsideLayout(layout, &scenario);
    }

    if (loaded.scenarios.empty()) {
        loaded.currentScenarioIndex = -1;
    } else if (loaded.currentScenarioIndex < 0 || loaded.currentScenarioIndex >= static_cast<int>(loaded.scenarios.size())) {
        loaded.currentScenarioIndex = 0;
    }

    *state = std::move(loaded);
    return true;
}

bool ProjectPersistence::saveScenarioAuthoringState(
    const ProjectMetadata& metadata,
    const ScenarioAuthoringWidget::InitialState& state,
    QString* errorMessage) {
    if (metadata.isBuiltInDemo()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Built-in demo projects do not need to be saved.";
        }
        return false;
    }

    QJsonArray scenarios;
    for (const auto& scenario : state.scenarios) {
        scenarios.append(scenarioStateToJson(scenario));
    }

    QJsonObject root;
    root["currentScenarioIndex"] = state.currentScenarioIndex;
    root["navigationView"] = static_cast<int>(state.navigationView);
    root["rightPanelMode"] = static_cast<int>(state.rightPanelMode);
    root["scenarios"] = scenarios;
    return writeJsonDocument(scenarioAuthoringFilePath(metadata.folderPath), QJsonDocument(root), errorMessage);
}

}  // namespace safecrowd::application
