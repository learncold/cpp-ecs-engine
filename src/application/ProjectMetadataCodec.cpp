#include "application/ProjectMetadataCodec.h"

namespace safecrowd::application {
ProjectMetadata projectMetadataFromJson(const QJsonObject& object) {
    return {
        .name = object.value("name").toString(),
        .folderPath = object.value("folderPath").toString(),
        .layoutPath = object.value("layoutPath").toString(),
        .savedAt = object.value("savedAt").toString(),
    };
}

QJsonObject projectMetadataToJson(const ProjectMetadata& metadata) {
    QJsonObject object;
    object["name"] = metadata.name;
    object["folderPath"] = metadata.folderPath;
    object["layoutPath"] = metadata.layoutPath;
    object["savedAt"] = metadata.savedAt;
    return object;
}

}  // namespace safecrowd::application
