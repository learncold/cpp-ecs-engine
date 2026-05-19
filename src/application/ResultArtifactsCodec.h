#pragma once

#include <QJsonObject>

#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"

namespace safecrowd::application {

QJsonObject simulationFrameToJson(const safecrowd::domain::SimulationFrame& frame);
safecrowd::domain::SimulationFrame simulationFrameFromJson(const QJsonObject& object);
QJsonObject riskSnapshotToJson(const safecrowd::domain::ScenarioRiskSnapshot& risk);
safecrowd::domain::ScenarioRiskSnapshot riskSnapshotFromJson(const QJsonObject& object);
QJsonObject resultArtifactsToJson(const safecrowd::domain::ScenarioResultArtifacts& artifacts);
safecrowd::domain::ScenarioResultArtifacts resultArtifactsFromJson(const QJsonObject& object);

}  // namespace safecrowd::application
