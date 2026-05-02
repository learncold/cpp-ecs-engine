#pragma once

#include <string>
#include <vector>

namespace safecrowd::domain {

enum class ImportIssueSeverity {
    Info,
    Warning,
    Error,
};

enum class ImportIssueCode {
    Unknown,
    UnsupportedFormat,
    FileReadFailed,
    UnsupportedEntity,
    MissingSourceGeometry,
    MissingRoom,
    MissingBlockDefinition,
    InvalidGeometry,
    DisconnectedWalkableArea,
    MissingExit,
    WidthBelowMinimum,
    UnmappedElement,
    InvalidFloorReference,
};

struct ImportIssue {
    ImportIssueSeverity severity{ImportIssueSeverity::Warning};
    ImportIssueCode code{ImportIssueCode::Unknown};
    std::string message{};
    std::string sourceId{};
    std::string targetId{};
    bool isBlocking{false};

    bool blocksSimulation() const noexcept;
};

const char* toString(ImportIssueSeverity severity) noexcept;
const char* toString(ImportIssueCode code) noexcept;
bool hasBlockingImportIssue(const std::vector<ImportIssue>& issues) noexcept;

}  // namespace safecrowd::domain
