#pragma once

#include <QJsonObject>

#include "application/ProjectMetadata.h"

namespace safecrowd::application {

ProjectMetadata projectMetadataFromJson(const QJsonObject& object);
QJsonObject projectMetadataToJson(const ProjectMetadata& metadata);

}  // namespace safecrowd::application
