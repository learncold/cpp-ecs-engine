#pragma once

#include <vector>

#include "domain/FacilityLayout2D.h"
#include "domain/ImportIssue.h"

namespace safecrowd::domain {

class ImportValidationService {
public:
    std::vector<ImportIssue> validate(const FacilityLayout2D& layout) const;
};

}  // namespace safecrowd::domain
