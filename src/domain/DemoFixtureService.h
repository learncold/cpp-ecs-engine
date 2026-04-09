#pragma once

#include "domain/FacilityLayout2D.h"
#include "domain/PopulationSpec.h"

namespace safecrowd::domain {

struct DemoFixture {
    FacilityLayout2D layout{};
    PopulationSpec population{};
};

class DemoFixtureService {
public:
    DemoFixture createSprint1DemoFixture() const;
};

}  // namespace safecrowd::domain
