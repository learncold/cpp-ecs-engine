#pragma once

#include <string>
#include <vector>

#include "domain/RawImportModel.h"

namespace safecrowd::domain {

enum class ImportElementSemantic {
    Unknown,
    Wall,
    Opening,
    Exit,
    Obstacle,
    Walkable,
};

enum class ImportFallbackPolicy {
    Disabled,
    ReviewableGeometry,
};

struct ImportSemanticRule {
    ImportElementSemantic semantic{ImportElementSemantic::Unknown};
    std::vector<std::string> tokens{};
    double confidence{1.0};
};

struct ImportSemanticRuleSet {
    std::vector<ImportSemanticRule> rules{};

    static ImportSemanticRuleSet defaultRules();

    ImportElementSemantic classify(
        const SourceTrace& trace,
        const std::map<std::string, std::string>& metadata = {}) const;
    double confidenceFor(
        ImportElementSemantic semantic,
        const SourceTrace& trace,
        const std::map<std::string, std::string>& metadata = {}) const;
};

const char* toString(ImportElementSemantic semantic) noexcept;
const char* toString(ImportFallbackPolicy policy) noexcept;

}  // namespace safecrowd::domain
