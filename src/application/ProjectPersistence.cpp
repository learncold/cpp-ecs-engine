#include "application/ProjectPersistence.h"

#include <algorithm>

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
constexpr auto kWorkspaceFileName = "workspace-state.json";

bool isProjectManagedEntry(const QString& fileName) {
    return fileName.compare(kProjectFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kLayoutFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kReviewFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kWorkspaceFileName, Qt::CaseInsensitive) == 0;
}

QString projectFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kProjectFileName);
}

QString reviewFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kReviewFileName);
}

QString workspaceFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kWorkspaceFileName);
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

safecrowd::domain::ZoneKind zoneKindFromJson(const QJsonObject& object) {
    const int rawKind = object.value("kind").toInt();
    switch (rawKind) {
    case 1:
        return safecrowd::domain::ZoneKind::Room;
    case 2:
        return safecrowd::domain::ZoneKind::Room;
    case 3:
        return safecrowd::domain::ZoneKind::Exit;
    case 4:
        return safecrowd::domain::ZoneKind::Intersection;
    case 5:
        return safecrowd::domain::ZoneKind::Stair;
    case 0:
    default:
        return safecrowd::domain::ZoneKind::Unknown;
    }
}

QJsonObject floorToJson(const safecrowd::domain::Floor2D& floor) {
    QJsonObject object;
    object["id"] = QString::fromStdString(floor.id);
    object["label"] = QString::fromStdString(floor.label);
    object["elevationMeters"] = floor.elevationMeters;
    object["provenance"] = provenanceToJson(floor.provenance);
    return object;
}

safecrowd::domain::Floor2D floorFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .label = object.value("label").toString().toStdString(),
        .elevationMeters = object.value("elevationMeters").toDouble(),
        .provenance = provenanceFromJson(object.value("provenance").toObject()),
    };
}

std::string defaultFloorId(const safecrowd::domain::FacilityLayout2D& layout) {
    if (!layout.floors.empty() && !layout.floors.front().id.empty()) {
        return layout.floors.front().id;
    }
    if (!layout.levelId.empty()) {
        return layout.levelId;
    }
    return "L1";
}

void normalizeFloors(safecrowd::domain::FacilityLayout2D& layout) {
    if (layout.floors.empty()) {
        const auto floorId = layout.levelId.empty() ? std::string("L1") : layout.levelId;
        layout.floors.push_back({
            .id = floorId,
            .label = floorId,
        });
    }

    const auto floorId = defaultFloorId(layout);
    if (layout.levelId.empty()) {
        layout.levelId = floorId;
    }
    for (auto& zone : layout.zones) {
        if (zone.floorId.empty()) {
            zone.floorId = floorId;
        }
    }
    for (auto& connection : layout.connections) {
        if (connection.floorId.empty()) {
            connection.floorId = floorId;
        }
    }
    for (auto& barrier : layout.barriers) {
        if (barrier.floorId.empty()) {
            barrier.floorId = floorId;
        }
    }
    for (auto& control : layout.controls) {
        if (control.floorId.empty()) {
            control.floorId = floorId;
        }
    }
}

QJsonObject zoneToJson(const safecrowd::domain::Zone2D& zone) {
    QJsonObject object;
    object["id"] = QString::fromStdString(zone.id);
    object["floorId"] = QString::fromStdString(zone.floorId);
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
        .floorId = object.value("floorId").toString().toStdString(),
        .kind = zoneKindFromJson(object),
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
    object["floorId"] = QString::fromStdString(connection.floorId);
    object["kind"] = static_cast<int>(connection.kind);
    object["fromZoneId"] = QString::fromStdString(connection.fromZoneId);
    object["toZoneId"] = QString::fromStdString(connection.toZoneId);
    object["effectiveWidth"] = connection.effectiveWidth;
    object["directionality"] = static_cast<int>(connection.directionality);
    object["isStair"] = connection.isStair;
    object["isRamp"] = connection.isRamp;
    object["lowerEntryDirection"] = static_cast<int>(connection.lowerEntryDirection);
    object["upperEntryDirection"] = static_cast<int>(connection.upperEntryDirection);
    object["centerSpan"] = lineToJson(connection.centerSpan);
    object["provenance"] = provenanceToJson(connection.provenance);
    return object;
}

safecrowd::domain::Connection2D connectionFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .floorId = object.value("floorId").toString().toStdString(),
        .kind = static_cast<safecrowd::domain::ConnectionKind>(object.value("kind").toInt()),
        .fromZoneId = object.value("fromZoneId").toString().toStdString(),
        .toZoneId = object.value("toZoneId").toString().toStdString(),
        .effectiveWidth = object.value("effectiveWidth").toDouble(),
        .directionality = static_cast<safecrowd::domain::TravelDirection>(object.value("directionality").toInt()),
        .isStair = object.value("isStair").toBool(false),
        .isRamp = object.value("isRamp").toBool(false),
        .lowerEntryDirection = static_cast<safecrowd::domain::StairEntryDirection>(
            object.value("lowerEntryDirection").toInt(
                static_cast<int>(safecrowd::domain::StairEntryDirection::Unspecified))),
        .upperEntryDirection = static_cast<safecrowd::domain::StairEntryDirection>(
            object.value("upperEntryDirection").toInt(
                static_cast<int>(safecrowd::domain::StairEntryDirection::Unspecified))),
        .centerSpan = lineFromJson(object.value("centerSpan").toObject()),
        .provenance = provenanceFromJson(object.value("provenance").toObject()),
    };
}

QJsonObject barrierToJson(const safecrowd::domain::Barrier2D& barrier) {
    QJsonObject object;
    object["id"] = QString::fromStdString(barrier.id);
    object["floorId"] = QString::fromStdString(barrier.floorId);
    object["geometry"] = polylineToJson(barrier.geometry);
    object["blocksMovement"] = barrier.blocksMovement;
    object["provenance"] = provenanceToJson(barrier.provenance);
    return object;
}

safecrowd::domain::Barrier2D barrierFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .floorId = object.value("floorId").toString().toStdString(),
        .geometry = polylineFromJson(object.value("geometry").toObject()),
        .blocksMovement = object.value("blocksMovement").toBool(true),
        .provenance = provenanceFromJson(object.value("provenance").toObject()),
    };
}

QJsonObject controlToJson(const safecrowd::domain::ControlPoint2D& control) {
    QJsonObject object;
    object["id"] = QString::fromStdString(control.id);
    object["floorId"] = QString::fromStdString(control.floorId);
    object["kind"] = static_cast<int>(control.kind);
    object["targetId"] = QString::fromStdString(control.targetId);
    object["defaultOpen"] = control.defaultOpen;
    object["provenance"] = provenanceToJson(control.provenance);
    return object;
}

safecrowd::domain::ControlPoint2D controlFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .floorId = object.value("floorId").toString().toStdString(),
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

    QJsonArray floors;
    for (const auto& floor : layout.floors) {
        floors.append(floorToJson(floor));
    }
    object["floors"] = floors;

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

    for (const auto& value : object.value("floors").toArray()) {
        layout.floors.push_back(floorFromJson(value.toObject()));
    }
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

    normalizeFloors(layout);
    return layout;
}

QJsonObject initialPlacementToJson(const safecrowd::domain::InitialPlacement2D& placement) {
    QJsonObject object;
    object["id"] = QString::fromStdString(placement.id);
    object["zoneId"] = QString::fromStdString(placement.zoneId);
    object["floorId"] = QString::fromStdString(placement.floorId);
    object["area"] = polygonToJson(placement.area);
    object["targetAgentCount"] = static_cast<qint64>(placement.targetAgentCount);
    object["initialVelocity"] = pointArray(placement.initialVelocity);
    return object;
}

safecrowd::domain::InitialPlacement2D initialPlacementFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toStdString(),
        .zoneId = object.value("zoneId").toString().toStdString(),
        .floorId = object.value("floorId").toString().toStdString(),
        .area = polygonFromJson(object.value("area").toObject()),
        .targetAgentCount = static_cast<std::size_t>(object.value("targetAgentCount").toInteger()),
        .initialVelocity = pointFromJson(object.value("initialVelocity")),
    };
}

QJsonObject populationToJson(const safecrowd::domain::PopulationSpec& population) {
    QJsonObject object;
    QJsonArray placements;
    for (const auto& placement : population.initialPlacements) {
        placements.append(initialPlacementToJson(placement));
    }
    object["initialPlacements"] = placements;
    return object;
}

safecrowd::domain::PopulationSpec populationFromJson(const QJsonObject& object) {
    safecrowd::domain::PopulationSpec population;
    for (const auto& value : object.value("initialPlacements").toArray()) {
        population.initialPlacements.push_back(initialPlacementFromJson(value.toObject()));
    }
    return population;
}

QJsonObject environmentToJson(const safecrowd::domain::EnvironmentState& environment) {
    QJsonObject object;
    object["reducedVisibility"] = environment.reducedVisibility;
    object["familiarityProfile"] = QString::fromStdString(environment.familiarityProfile);
    object["guidanceProfile"] = QString::fromStdString(environment.guidanceProfile);
    return object;
}

safecrowd::domain::EnvironmentState environmentFromJson(const QJsonObject& object) {
    return {
        .reducedVisibility = object.value("reducedVisibility").toBool(false),
        .familiarityProfile = object.value("familiarityProfile").toString().toStdString(),
        .guidanceProfile = object.value("guidanceProfile").toString().toStdString(),
    };
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

QJsonValue optionalDoubleToJson(const std::optional<double>& value) {
    return value.has_value() ? QJsonValue(*value) : QJsonValue(QJsonValue::Null);
}

std::optional<double> optionalDoubleFromJson(const QJsonValue& value) {
    if (value.isNull() || value.isUndefined()) {
        return std::nullopt;
    }
    return value.toDouble();
}

QJsonObject simulationAgentFrameToJson(const safecrowd::domain::SimulationAgentFrame& agent) {
    QJsonObject object;
    object["id"] = QString::number(static_cast<qulonglong>(agent.id));
    object["position"] = pointArray(agent.position);
    object["velocity"] = pointArray(agent.velocity);
    object["radius"] = agent.radius;
    object["floorId"] = QString::fromStdString(agent.floorId);
    return object;
}

safecrowd::domain::SimulationAgentFrame simulationAgentFrameFromJson(const QJsonObject& object) {
    return {
        .id = object.value("id").toString().toULongLong(),
        .position = pointFromJson(object.value("position")),
        .velocity = pointFromJson(object.value("velocity")),
        .radius = object.value("radius").toDouble(0.25),
        .floorId = object.value("floorId").toString().toStdString(),
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
    object["agentCount"] = static_cast<qint64>(hotspot.agentCount);
    return object;
}

safecrowd::domain::ScenarioCongestionHotspot hotspotFromJson(const QJsonObject& object) {
    return {
        .center = pointFromJson(object.value("center")),
        .cellMin = pointFromJson(object.value("cellMin")),
        .cellMax = pointFromJson(object.value("cellMax")),
        .agentCount = static_cast<std::size_t>(object.value("agentCount").toInteger()),
    };
}

QJsonObject bottleneckToJson(const safecrowd::domain::ScenarioBottleneckMetric& bottleneck) {
    QJsonObject object;
    object["connectionId"] = QString::fromStdString(bottleneck.connectionId);
    object["label"] = QString::fromStdString(bottleneck.label);
    object["passage"] = lineToJson(bottleneck.passage);
    object["nearbyAgentCount"] = static_cast<qint64>(bottleneck.nearbyAgentCount);
    object["stalledAgentCount"] = static_cast<qint64>(bottleneck.stalledAgentCount);
    object["averageSpeed"] = bottleneck.averageSpeed;
    return object;
}

safecrowd::domain::ScenarioBottleneckMetric bottleneckFromJson(const QJsonObject& object) {
    return {
        .connectionId = object.value("connectionId").toString().toStdString(),
        .label = object.value("label").toString().toStdString(),
        .passage = lineFromJson(object.value("passage").toObject()),
        .nearbyAgentCount = static_cast<std::size_t>(object.value("nearbyAgentCount").toInteger()),
        .stalledAgentCount = static_cast<std::size_t>(object.value("stalledAgentCount").toInteger()),
        .averageSpeed = object.value("averageSpeed").toDouble(),
    };
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

    QJsonObject timing;
    timing["t50Seconds"] = optionalDoubleToJson(artifacts.timingSummary.t50Seconds);
    timing["t90Seconds"] = optionalDoubleToJson(artifacts.timingSummary.t90Seconds);
    timing["t95Seconds"] = optionalDoubleToJson(artifacts.timingSummary.t95Seconds);
    timing["finalEvacuationTimeSeconds"] = optionalDoubleToJson(artifacts.timingSummary.finalEvacuationTimeSeconds);
    object["timingSummary"] = timing;
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

    const auto timing = object.value("timingSummary").toObject();
    artifacts.timingSummary.t50Seconds = optionalDoubleFromJson(timing.value("t50Seconds"));
    artifacts.timingSummary.t90Seconds = optionalDoubleFromJson(timing.value("t90Seconds"));
    artifacts.timingSummary.t95Seconds = optionalDoubleFromJson(timing.value("t95Seconds"));
    artifacts.timingSummary.finalEvacuationTimeSeconds = optionalDoubleFromJson(timing.value("finalEvacuationTimeSeconds"));
    return artifacts;
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
    return authoring;
}

QJsonObject resultStateToJson(const SavedScenarioResultState& result) {
    QJsonObject object;
    object["scenario"] = scenarioDraftToJson(result.scenario);
    object["frame"] = simulationFrameToJson(result.frame);
    object["risk"] = riskSnapshotToJson(result.risk);
    object["artifacts"] = resultArtifactsToJson(result.artifacts);
    return object;
}

SavedScenarioResultState resultStateFromJson(const QJsonObject& object) {
    return {
        .scenario = scenarioDraftFromJson(object.value("scenario").toObject()),
        .frame = simulationFrameFromJson(object.value("frame").toObject()),
        .risk = riskSnapshotFromJson(object.value("risk").toObject()),
        .artifacts = resultArtifactsFromJson(object.value("artifacts").toObject()),
    };
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
    if (state.result.has_value()) {
        object["result"] = resultStateToJson(*state.result);
    }
    return object;
}

ProjectWorkspaceState workspaceStateFromJson(const QJsonObject& object) {
    ProjectWorkspaceState state;
    state.activeView = static_cast<ProjectWorkspaceView>(object.value("activeView").toInt());
    if (object.value("authoring").isObject()) {
        state.authoring = authoringStateFromJson(object.value("authoring").toObject());
    }
    if (object.value("runningScenario").isObject()) {
        state.runningScenario = scenarioDraftFromJson(object.value("runningScenario").toObject());
    }
    if (object.value("result").isObject()) {
        state.result = resultStateFromJson(object.value("result").toObject());
    }
    return state;
}

bool isLiveValidationIssue(safecrowd::domain::ImportIssueCode code) {
    using safecrowd::domain::ImportIssueCode;

    switch (code) {
    case ImportIssueCode::MissingExit:
    case ImportIssueCode::MissingRoom:
    case ImportIssueCode::DisconnectedWalkableArea:
    case ImportIssueCode::WidthBelowMinimum:
    case ImportIssueCode::InvalidFloorReference:
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

bool ProjectPersistence::loadProjectWorkspace(const ProjectMetadata& metadata, ProjectWorkspaceState* state) {
    if (metadata.isBuiltInDemo() || state == nullptr) {
        return false;
    }

    const auto document = readJsonDocument(workspaceFilePath(metadata.folderPath));
    if (!document.isObject()) {
        return false;
    }

    *state = workspaceStateFromJson(document.object());
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

bool ProjectPersistence::saveProjectWorkspace(
    const ProjectMetadata& metadata,
    const ProjectWorkspaceState& state,
    QString* errorMessage) {
    if (metadata.isBuiltInDemo()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Built-in demo projects do not need to be saved.";
        }
        return false;
    }

    return writeJsonDocument(workspaceFilePath(metadata.folderPath), QJsonDocument(workspaceStateToJson(state)), errorMessage);
}

}  // namespace safecrowd::application
