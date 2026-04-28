#pragma once

#include <functional>

#include <QString>
#include <QWidget>

#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"

namespace safecrowd::application {

class ScenarioResultWidget : public QWidget {
public:
    explicit ScenarioResultWidget(
        QString projectName,
        safecrowd::domain::FacilityLayout2D layout,
        safecrowd::domain::ScenarioDraft scenario,
        safecrowd::domain::SimulationFrame frame,
        safecrowd::domain::ScenarioRiskSnapshot risk,
        std::function<void()> saveProjectHandler,
        std::function<void()> openProjectHandler,
        QWidget* parent = nullptr);
};

}  // namespace safecrowd::application
