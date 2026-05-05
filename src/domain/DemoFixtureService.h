#pragma once

#include "domain/FacilityLayout2D.h"
#include "domain/PopulationSpec.h"
#include "domain/ScenarioAuthoring.h"

namespace safecrowd::domain {

struct DemoFixture {
    FacilityLayout2D layout{};
    PopulationSpec population{};
    ScenarioDraft baselineScenario{};
};

class DemoFixtureService {
public:
    DemoFixture createSprint1DemoFixture() const;
};

}  // namespace safecrowd::domain
