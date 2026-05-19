#pragma once

#include <QJsonObject>

#include "application/ProjectWorkspaceState.h"
#include "domain/ScenarioAuthoring.h"

namespace safecrowd::application {

QJsonObject scenarioDraftToJson(const safecrowd::domain::ScenarioDraft& scenario);
safecrowd::domain::ScenarioDraft scenarioDraftFromJson(const QJsonObject& object);
QJsonObject workspaceStateToJson(const ProjectWorkspaceState& state);
ProjectWorkspaceState workspaceStateFromJson(const QJsonObject& object);

}  // namespace safecrowd::application
