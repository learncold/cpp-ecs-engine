#pragma once

#include "domain/FacilityLayout2D.h"
#include "domain/PopulationSpec.h"
#include "domain/ScenarioAuthoring.h"
#include "domain/ScenarioResultArtifacts.h"
#include "domain/ScenarioRiskMetrics.h"
#include "domain/ScenarioSimulationFrame.h"

namespace safecrowd::domain {

struct DemoFixture {
    FacilityLayout2D layout{};
    PopulationSpec population{};
    ScenarioDraft baselineScenario{};
};

struct DemoAuthoringFixture {
    FacilityLayout2D layout{};
    PopulationSpec population{};
    ScenarioDraft baselineScenario{};
    ScenarioDraft alternativeScenario{};
};

struct DemoScenarioResultFixture {
    FacilityLayout2D layout{};
    PopulationSpec population{};
    ScenarioDraft baselineScenario{};
    ScenarioDraft alternativeScenario{};
    SimulationFrame frame{};
    ScenarioRiskSnapshot risk{};
    ScenarioResultArtifacts artifacts{};
};

class DemoFixtureService {
public:
    DemoFixture createSprint1DemoFixture() const;
    DemoAuthoringFixture createTwoFloorEvacuationDemoFixture() const;
    DemoScenarioResultFixture createSprint1BlockedDoorResultFixture() const;
};

}  // namespace safecrowd::domain
