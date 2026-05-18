#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "domain/CanonicalGeometry.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ImportIssue.h"
#include "domain/ImportSemanticRules.h"
#include "domain/RawImportModel.h"

namespace safecrowd::domain {

enum class ImportReviewStatus {
    NotRequired,
    Pending,
    Approved,
    Rejected,
};

struct ImportTraceRef {
    std::string targetId{};
    std::vector<std::string> sourceIds{};
    std::vector<std::string> canonicalIds{};
};

struct ImportSourceFingerprint {
    std::string sourcePath{};
    std::uintmax_t fileSizeBytes{0};
    long long modifiedTimeTicks{0};
    bool exists{false};
};

struct ImportSummary {
    std::size_t rawEntityCount{0};
    std::size_t canonicalElementCount{0};
    std::size_t layoutElementCount{0};
    std::size_t issueCount{0};
    std::size_t blockingIssueCount{0};
    std::size_t warningIssueCount{0};
};

struct ReimportChangeSummary {
    bool hasComparison{false};
    std::size_t addedElements{0};
    std::size_t removedElements{0};
    std::size_t changedElements{0};
};

struct ImportArtifactMetadata {
    ImportSourceFingerprint source{};
    ImportSemanticRuleSet selectedRules{ImportSemanticRuleSet::defaultRules()};
    ImportFallbackPolicy fallbackPolicy{ImportFallbackPolicy::ReviewableGeometry};
    ImportSummary summary{};
    ReimportChangeSummary reimport{};
    std::vector<std::string> userOverrideTargetIds{};
};

struct ImportResult {
    std::optional<RawImportModel> rawModel{};
    std::optional<CanonicalGeometry> canonicalGeometry{};
    std::optional<FacilityLayout2D> layout{};
    std::vector<ImportIssue> issues{};
    std::vector<ImportTraceRef> traceRefs{};
    ImportArtifactMetadata artifacts{};
    std::string statusMessage{};
    ImportReviewStatus reviewStatus{ImportReviewStatus::Pending};

    bool readyForSimulation() const noexcept {
        return layout.has_value()
            && !hasBlockingImportIssue(issues)
            && (reviewStatus == ImportReviewStatus::NotRequired
                || reviewStatus == ImportReviewStatus::Approved);
    }
};

}  // namespace safecrowd::domain
