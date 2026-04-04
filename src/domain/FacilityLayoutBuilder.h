#pragma once

#include <vector>

#include "domain/CanonicalGeometry.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ImportIssue.h"

namespace safecrowd::domain {

struct FacilityLayoutBuildResult {
    FacilityLayout2D layout{};
    std::vector<ImportIssue> issues{};
};

class FacilityLayoutBuilder {
public:
    FacilityLayoutBuildResult build(const CanonicalGeometry& geometry) const;
};

}  // namespace safecrowd::domain
