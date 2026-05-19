#pragma once

#include <vector>

#include <QJsonObject>

#include "domain/FacilityLayout2D.h"
#include "domain/ImportResult.h"

namespace safecrowd::application {

QJsonObject layoutToJson(const safecrowd::domain::FacilityLayout2D& layout);
safecrowd::domain::FacilityLayout2D layoutFromJson(const QJsonObject& object);
QJsonObject importArtifactsToJson(
    const safecrowd::domain::ImportArtifactMetadata& artifacts,
    const std::vector<safecrowd::domain::ImportTraceRef>& traceRefs);
void importArtifactsFromJson(const QJsonObject& object, safecrowd::domain::ImportResult* importResult);

}  // namespace safecrowd::application
