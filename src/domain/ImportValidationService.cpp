#include "domain/ImportValidationService.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace safecrowd::domain {
namespace {

constexpr double kMinimumConnectionWidth = 0.9;

bool hasValidFloorReference(const std::unordered_set<std::string>& floorIds, const std::string& floorId) {
    return floorIds.empty() || (!floorId.empty() && floorIds.contains(floorId));
}

bool isVerticalConnection(const Connection2D& connection) {
    return connection.kind == ConnectionKind::Stair || connection.kind == ConnectionKind::Ramp
        || connection.isStair || connection.isRamp;
}

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

    std::unordered_set<std::string> floorIds;
    for (const auto& floor : layout.floors) {
        if (!floor.id.empty()) {
            floorIds.insert(floor.id);
        }
    }

    std::unordered_map<std::string, std::string> zoneFloorIds;
    for (const auto& zone : layout.zones) {
        zoneFloorIds[zone.id] = zone.floorId;
        if (!hasValidFloorReference(floorIds, zone.floorId)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Zone references a missing floor.",
                .targetId = zone.id,
                .isBlocking = true,
            });
        }
    }
    for (const auto& connection : layout.connections) {
        if (!hasValidFloorReference(floorIds, connection.floorId)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Connection references a missing floor.",
                .targetId = connection.id,
                .isBlocking = true,
            });
        }

        const auto fromFloor = zoneFloorIds.find(connection.fromZoneId);
        const auto toFloor = zoneFloorIds.find(connection.toZoneId);
        if (fromFloor != zoneFloorIds.end()
            && toFloor != zoneFloorIds.end()
            && !fromFloor->second.empty()
            && !toFloor->second.empty()
            && fromFloor->second != toFloor->second
            && !isVerticalConnection(connection)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Inter-floor connections must be marked as a stair or ramp.",
                .targetId = connection.id,
                .isBlocking = true,
            });
        }
    }
    for (const auto& barrier : layout.barriers) {
        if (!hasValidFloorReference(floorIds, barrier.floorId)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Wall references a missing floor.",
                .targetId = barrier.id,
                .isBlocking = true,
            });
        }
    }
    for (const auto& control : layout.controls) {
        if (!hasValidFloorReference(floorIds, control.floorId)) {
            issues.push_back({
                .severity = ImportIssueSeverity::Error,
                .code = ImportIssueCode::InvalidFloorReference,
                .message = "Control point references a missing floor.",
                .targetId = control.id,
                .isBlocking = true,
            });
        }
    }

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
