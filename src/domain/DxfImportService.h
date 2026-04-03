#pragma once

#include "domain/ImportOrchestrator.h"
#include "domain/ImportResult.h"

namespace safecrowd::domain {

class DxfImportService {
public:
    ImportResult importFile(const ImportRequest& request) const;
};

}  // namespace safecrowd::domain
