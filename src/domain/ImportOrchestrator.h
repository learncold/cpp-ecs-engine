#pragma once

#include <filesystem>

#include "domain/ImportResult.h"

namespace safecrowd::domain {

struct ImportRequest {
    std::filesystem::path sourcePath{};
    ImportedFileFormat requestedFormat{ImportedFileFormat::Unknown};
    bool preserveRawModel{true};
    bool runValidation{true};
};

class ImportOrchestrator {
public:
    virtual ~ImportOrchestrator() = default;

    virtual ImportResult importFile(const ImportRequest& request) = 0;
};

}  // namespace safecrowd::domain
