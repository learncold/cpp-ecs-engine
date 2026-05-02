#include "domain/ImportIssue.h"

namespace safecrowd::domain {

bool ImportIssue::blocksSimulation() const noexcept {
    return isBlocking || severity == ImportIssueSeverity::Error;
}

const char* toString(ImportIssueSeverity severity) noexcept {
    switch (severity) {
    case ImportIssueSeverity::Info:
        return "Info";
    case ImportIssueSeverity::Warning:
        return "Warning";
    case ImportIssueSeverity::Error:
        return "Error";
    }

    return "Unknown";
}

const char* toString(ImportIssueCode code) noexcept {
    switch (code) {
    case ImportIssueCode::Unknown:
        return "Unknown";
    case ImportIssueCode::UnsupportedFormat:
        return "UnsupportedFormat";
    case ImportIssueCode::FileReadFailed:
        return "FileReadFailed";
    case ImportIssueCode::UnsupportedEntity:
        return "UnsupportedEntity";
    case ImportIssueCode::MissingSourceGeometry:
        return "MissingSourceGeometry";
    case ImportIssueCode::MissingRoom:
        return "MissingRoom";
    case ImportIssueCode::MissingBlockDefinition:
        return "MissingBlockDefinition";
    case ImportIssueCode::InvalidGeometry:
        return "InvalidGeometry";
    case ImportIssueCode::DisconnectedWalkableArea:
        return "DisconnectedWalkableArea";
    case ImportIssueCode::MissingExit:
        return "MissingExit";
    case ImportIssueCode::WidthBelowMinimum:
        return "WidthBelowMinimum";
    case ImportIssueCode::UnmappedElement:
        return "UnmappedElement";
    case ImportIssueCode::InvalidFloorReference:
        return "InvalidFloorReference";
    }

    return "Unknown";
}

bool hasBlockingImportIssue(const std::vector<ImportIssue>& issues) noexcept {
    for (const auto& issue : issues) {
        if (issue.blocksSimulation()) {
            return true;
        }
    }

    return false;
}

}  // namespace safecrowd::domain
