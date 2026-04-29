#include "domain/ImportValidationService.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace safecrowd::domain {
namespace {

constexpr double kMinimumConnectionWidth = 0.9;

bool canTravel(const Connection2D& connection, const std::string& fromZoneId, const std::string& toZoneId) {
    switch (connection.directionality) {
    case TravelDirection::Bidirectional:
        return (connection.fromZoneId == fromZoneId && connection.toZoneId == toZoneId)
            || (connection.fromZoneId == toZoneId && connection.toZoneId == fromZoneId);
    case TravelDirection::ForwardOnly:
        return connection.fromZoneId == fromZoneId && connection.toZoneId == toZoneId;
    case TravelDirection::ReverseOnly:
        return connection.fromZoneId == toZoneId && connection.toZoneId == fromZoneId;
    case TravelDirection::Closed:
        return false;
    }

    return false;
}

bool hasRouteToExit(
    const FacilityLayout2D& layout,
    const std::unordered_set<std::string>& exitZoneIds,
    const std::string& startZoneId) {
    if (exitZoneIds.contains(startZoneId)) {
        return true;
    }

    std::vector<std::string> frontier = {startZoneId};
    std::unordered_set<std::string> visited = {startZoneId};

    while (!frontier.empty()) {
        const auto currentZoneId = frontier.back();
        frontier.pop_back();

        if (exitZoneIds.contains(currentZoneId)) {
            return true;
        }

        for (const auto& connection : layout.connections) {
            if (connection.directionality == TravelDirection::Closed) {
                continue;
            }

            if (canTravel(connection, currentZoneId, connection.toZoneId) && !visited.contains(connection.toZoneId)) {
                frontier.push_back(connection.toZoneId);
                visited.insert(connection.toZoneId);
            }

            if (canTravel(connection, currentZoneId, connection.fromZoneId) && !visited.contains(connection.fromZoneId)) {
                frontier.push_back(connection.fromZoneId);
                visited.insert(connection.fromZoneId);
            }
        }
    }

    return false;
}

}  // namespace

std::vector<ImportIssue> ImportValidationService::validate(const FacilityLayout2D& layout) const {
    std::vector<ImportIssue> issues;

    std::unordered_set<std::string> exitZoneIds;
    std::size_t roomZoneCount = 0;
    for (const auto& zone : layout.zones) {
        if (zone.kind == ZoneKind::Exit) {
            exitZoneIds.insert(zone.id);
        }
        if (zone.kind == ZoneKind::Room) {
            ++roomZoneCount;
        }
    }

    if (exitZoneIds.empty()) {
        issues.push_back({
            .severity = ImportIssueSeverity::Error,
            .code = ImportIssueCode::MissingExit,
            .message = "Imported layout does not contain an inferred exit zone.",
            .targetId = layout.id,
            .isBlocking = true,
        });
    }

    if (roomZoneCount == 0) {
        issues.push_back({
            .severity = ImportIssueSeverity::Warning,
            .code = ImportIssueCode::MissingRoom,
            .message = "Agents can only be placed inside Room or Exit zones.",
            .targetId = layout.id,
        });
    }

    for (const auto& connection : layout.connections) {
        if (connection.effectiveWidth > 0.0 && connection.effectiveWidth < kMinimumConnectionWidth) {
            issues.push_back({
                .severity = ImportIssueSeverity::Warning,
                .code = ImportIssueCode::WidthBelowMinimum,
                .message = "Connection width is below the demo minimum threshold.",
                .sourceId = connection.id,
                .targetId = connection.toZoneId,
            });
        }
    }

    for (const auto& zone : layout.zones) {
        if (zone.kind == ZoneKind::Exit) {
            continue;
        }

        if (!hasRouteToExit(layout, exitZoneIds, zone.id)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::DisconnectedWalkableArea,
                .message = "Walkable zone is not connected to any inferred exit.",
                .targetId = zone.id,
                .isBlocking = true,
            });
        }
    }

    return issues;
}

}  // namespace safecrowd::domain
