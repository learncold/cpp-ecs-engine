#pragma once

#include <optional>
#include <vector>

#include "domain/CanonicalGeometry.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ImportIssue.h"
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

struct ImportResult {
    std::optional<RawImportModel> rawModel{};
    std::optional<CanonicalGeometry> canonicalGeometry{};
    std::optional<FacilityLayout2D> layout{};
    std::vector<ImportIssue> issues{};
    std::vector<ImportTraceRef> traceRefs{};
    ImportReviewStatus reviewStatus{ImportReviewStatus::Pending};

    bool readyForSimulation() const noexcept {
        return layout.has_value()
            && !hasBlockingImportIssue(issues)
            && (reviewStatus == ImportReviewStatus::NotRequired
                || reviewStatus == ImportReviewStatus::Approved);
    }
};

}  // namespace safecrowd::domain
