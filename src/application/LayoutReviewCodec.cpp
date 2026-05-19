#include "application/LayoutReviewCodec.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <QJsonArray>
#include <QJsonValue>

#include "application/ProjectPersistenceJson.h"
#include "domain/GeometryQueries.h"

namespace safecrowd::application {

constexpr int kCurrentImportArtifactsVersion = 1;

QJsonObject importTraceRefToJson(const safecrowd::domain::ImportTraceRef& traceRef) {
    QJsonObject object;
    object["targetId"] = QString::fromStdString(traceRef.targetId);
    object["sourceIds"] = stringArray(traceRef.sourceIds);
    object["canonicalIds"] = stringArray(traceRef.canonicalIds);
    return object;
}

safecrowd::domain::ImportTraceRef importTraceRefFromJson(const QJsonObject& object) {
    return {
        .targetId = object.value("targetId").toString().toStdString(),
        .sourceIds = stringVectorFromJson(object.value("sourceIds").toArray()),
        .canonicalIds = stringVectorFromJson(object.value("canonicalIds").toArray()),
    };
}

QJsonArray importTraceRefsToJson(const std::vector<safecrowd::domain::ImportTraceRef>& traceRefs) {
    QJsonArray array;
    for (const auto& traceRef : traceRefs) {
        array.append(importTraceRefToJson(traceRef));
    }
    return array;
}

std::vector<safecrowd::domain::ImportTraceRef> importTraceRefsFromJson(const QJsonArray& array) {
    std::vector<safecrowd::domain::ImportTraceRef> traceRefs;
    traceRefs.reserve(static_cast<std::size_t>(array.size()));
    for (const auto& value : array) {
        traceRefs.push_back(importTraceRefFromJson(value.toObject()));
    }
    return traceRefs;
}

QJsonObject sourceFingerprintToJson(const safecrowd::domain::ImportSourceFingerprint& fingerprint) {
    QJsonObject object;
    object["sourcePath"] = QString::fromStdString(fingerprint.sourcePath);
    object["fileSizeBytes"] = static_cast<qint64>(fingerprint.fileSizeBytes);
    object["modifiedTimeTicks"] = static_cast<qint64>(fingerprint.modifiedTimeTicks);
    object["exists"] = fingerprint.exists;
    return object;
}

safecrowd::domain::ImportSourceFingerprint sourceFingerprintFromJson(const QJsonObject& object) {
    return {
        .sourcePath = object.value("sourcePath").toString().toStdString(),
        .fileSizeBytes = static_cast<std::uintmax_t>(object.value("fileSizeBytes").toInteger()),
        .modifiedTimeTicks = object.value("modifiedTimeTicks").toInteger(),
        .exists = object.value("exists").toBool(false),
    };
}

QJsonObject semanticRuleToJson(const safecrowd::domain::ImportSemanticRule& rule) {
    QJsonObject object;
    object["semantic"] = static_cast<int>(rule.semantic);
    object["tokens"] = stringArray(rule.tokens);
    object["confidence"] = rule.confidence;
    return object;
}

safecrowd::domain::ImportSemanticRule semanticRuleFromJson(const QJsonObject& object) {
    return {
        .semantic = static_cast<safecrowd::domain::ImportElementSemantic>(object.value("semantic").toInt()),
        .tokens = stringVectorFromJson(object.value("tokens").toArray()),
        .confidence = object.value("confidence").toDouble(1.0),
    };
}

QJsonObject semanticRuleSetToJson(const safecrowd::domain::ImportSemanticRuleSet& ruleSet) {
    QJsonObject object;
    QJsonArray rules;
    for (const auto& rule : ruleSet.rules) {
        rules.append(semanticRuleToJson(rule));
    }
    object["rules"] = rules;
    return object;
}

safecrowd::domain::ImportSemanticRuleSet semanticRuleSetFromJson(const QJsonObject& object) {
    safecrowd::domain::ImportSemanticRuleSet ruleSet;
    if (!object.value("rules").isArray()) {
        return safecrowd::domain::ImportSemanticRuleSet::defaultRules();
    }

    for (const auto& value : object.value("rules").toArray()) {
        ruleSet.rules.push_back(semanticRuleFromJson(value.toObject()));
    }
    return ruleSet;
}

safecrowd::domain::ImportFallbackPolicy fallbackPolicyFromJson(const QJsonValue& value) {
    return static_cast<safecrowd::domain::ImportFallbackPolicy>(value.toInt(
        static_cast<int>(safecrowd::domain::ImportFallbackPolicy::ReviewableGeometry)));
}

QJsonObject importSummaryToJson(const safecrowd::domain::ImportSummary& summary) {
    QJsonObject object;
    object["rawEntityCount"] = static_cast<qint64>(summary.rawEntityCount);
    object["canonicalElementCount"] = static_cast<qint64>(summary.canonicalElementCount);
    object["layoutElementCount"] = static_cast<qint64>(summary.layoutElementCount);
    object["issueCount"] = static_cast<qint64>(summary.issueCount);
    object["blockingIssueCount"] = static_cast<qint64>(summary.blockingIssueCount);
    object["warningIssueCount"] = static_cast<qint64>(summary.warningIssueCount);
    return object;
}

safecrowd::domain::ImportSummary importSummaryFromJson(const QJsonObject& object) {
    return {
        .rawEntityCount = static_cast<std::size_t>(object.value("rawEntityCount").toInteger()),
        .canonicalElementCount = static_cast<std::size_t>(object.value("canonicalElementCount").toInteger()),
        .layoutElementCount = static_cast<std::size_t>(object.value("layoutElementCount").toInteger()),
        .issueCount = static_cast<std::size_t>(object.value("issueCount").toInteger()),
        .blockingIssueCount = static_cast<std::size_t>(object.value("blockingIssueCount").toInteger()),
        .warningIssueCount = static_cast<std::size_t>(object.value("warningIssueCount").toInteger()),
    };
}

QJsonObject reimportSummaryToJson(const safecrowd::domain::ReimportChangeSummary& summary) {
    QJsonObject object;
    object["hasComparison"] = summary.hasComparison;
    object["addedElements"] = static_cast<qint64>(summary.addedElements);
    object["removedElements"] = static_cast<qint64>(summary.removedElements);
    object["changedElements"] = static_cast<qint64>(summary.changedElements);
    return object;
}

safecrowd::domain::ReimportChangeSummary reimportSummaryFromJson(const QJsonObject& object) {
    return {
        .hasComparison = object.value("hasComparison").toBool(false),
        .addedElements = static_cast<std::size_t>(object.value("addedElements").toInteger()),
        .removedElements = static_cast<std::size_t>(object.value("removedElements").toInteger()),
        .changedElements = static_cast<std::size_t>(object.value("changedElements").toInteger()),
    };
}

QJsonObject importArtifactsToJson(
    const safecrowd::domain::ImportArtifactMetadata& artifacts,
    const std::vector<safecrowd::domain::ImportTraceRef>& traceRefs) {
    QJsonObject object;
    object["version"] = 1;
    object["source"] = sourceFingerprintToJson(artifacts.source);
    object["selectedRules"] = semanticRuleSetToJson(artifacts.selectedRules);
    object["fallbackPolicy"] = static_cast<int>(artifacts.fallbackPolicy);
    object["summary"] = importSummaryToJson(artifacts.summary);
    object["reimport"] = reimportSummaryToJson(artifacts.reimport);
    object["userOverrideTargetIds"] = stringArray(artifacts.userOverrideTargetIds);
    object["traceRefs"] = importTraceRefsToJson(traceRefs);
    return object;
}

void importArtifactsFromJson(
    const QJsonObject& object,
    safecrowd::domain::ImportResult* importResult) {
    if (importResult == nullptr) {
        return;
    }

    const auto version = object.value("version").toInt(kCurrentImportArtifactsVersion);
    (void)version;

    importResult->artifacts.source = sourceFingerprintFromJson(object.value("source").toObject());
    importResult->artifacts.selectedRules = semanticRuleSetFromJson(object.value("selectedRules").toObject());
    importResult->artifacts.fallbackPolicy = fallbackPolicyFromJson(object.value("fallbackPolicy"));
    importResult->artifacts.summary = importSummaryFromJson(object.value("summary").toObject());
    importResult->artifacts.reimport = reimportSummaryFromJson(object.value("reimport").toObject());
    importResult->artifacts.userOverrideTargetIds = stringVectorFromJson(object.value("userOverrideTargetIds").toArray());
    importResult->traceRefs = importTraceRefsFromJson(object.value("traceRefs").toArray());
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

void normalizeFloors(safecrowd::domain::FacilityLayout2D& layout) {
    if (layout.floors.empty()) {
        const auto floorId = layout.levelId.empty() ? std::string("L1") : layout.levelId;
        layout.floors.push_back({
            .id = floorId,
            .label = floorId,
        });
    }

    const auto floorId = safecrowd::domain::defaultFloorId(layout, "L1");
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

}  // namespace safecrowd::application
