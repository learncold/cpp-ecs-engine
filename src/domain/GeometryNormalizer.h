#pragma once

#include <vector>

#include "domain/CanonicalGeometry.h"
#include "domain/ImportIssue.h"
#include "domain/ImportResult.h"
#include "domain/ImportSemanticRules.h"
#include "domain/RawImportModel.h"

namespace safecrowd::domain {

struct GeometryNormalizerOptions {
    ImportSemanticRuleSet semanticRules{ImportSemanticRuleSet::defaultRules()};
    ImportFallbackPolicy fallbackPolicy{ImportFallbackPolicy::ReviewableGeometry};
};

struct GeometryNormalizerResult {
    CanonicalGeometry geometry{};
    std::vector<ImportIssue> issues{};
    std::vector<ImportTraceRef> traceRefs{};
};

class GeometryNormalizer {
public:
    GeometryNormalizerResult normalize(
        const RawImportModel& rawModel,
        const GeometryNormalizerOptions& options = {}) const;
};

}  // namespace safecrowd::domain
