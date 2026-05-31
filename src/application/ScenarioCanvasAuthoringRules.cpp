#include "application/ScenarioCanvasAuthoringRules.h"

#include "application/LayoutPreviewGeometry.h"
#include "domain/AgentComponents.h"
#include "domain/GeometryQueries.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <random>

namespace safecrowd::application {
namespace {

constexpr double kDefaultInitialSpeed = 1.3;
constexpr double kOccupantWorldRadius = safecrowd::domain::kDefaultAgentRadiusMeters;
constexpr double kOccupantMinSpacing = kOccupantWorldRadius * 2.0;
constexpr double kGuidancePlacementBarrierClearance = 0.35;
constexpr double kGeometryEpsilon = 1e-9;

using safecrowd::domain::distancePointToSegment;
using safecrowd::domain::pointInPolygon;
using safecrowd::domain::pointInRing;
using safecrowd::domain::polygonCenter;
using safecrowd::domain::representativePointInPolygon;

struct PointBounds {
    double minX{0.0};
    double minY{0.0};
    double maxX{0.0};
    double maxY{0.0};
};

bool pointsEqual(const safecrowd::domain::Point2D& lhs, const safecrowd::domain::Point2D& rhs) {
    return std::abs(lhs.x - rhs.x) <= kGeometryEpsilon
        && std::abs(lhs.y - rhs.y) <= kGeometryEpsilon;
}

QString zoneIdAt(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const safecrowd::domain::Point2D& point) {
    for (const auto& zone : layout.zones) {
        if (!matchesFloor(zone.floorId, currentFloorId)) {
            continue;
        }
        if (pointInPolygon(zone.area, point)) {
            return QString::fromStdString(zone.id);
        }
    }
    return {};
}

const safecrowd::domain::Zone2D* zoneAt(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const safecrowd::domain::Point2D& point) {
    const auto id = zoneIdAt(layout, currentFloorId, point);
    if (id.isEmpty()) {
        return nullptr;
    }

    const auto idStd = id.toStdString();
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == idStd;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

bool placementPointBlocked(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const safecrowd::domain::Point2D& point) {
    for (const auto& barrier : layout.barriers) {
        if (!matchesFloor(barrier.floorId, currentFloorId)) {
            continue;
        }
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        for (std::size_t index = 1; index < vertices.size(); ++index) {
            if (distancePointToSegment(point, vertices[index - 1], vertices[index]) < kOccupantWorldRadius) {
                return true;
            }
        }
        if (barrier.geometry.closed) {
            if (pointInRing(vertices, point)) {
                return true;
            }
            if (distancePointToSegment(point, vertices.back(), vertices.front()) < kOccupantWorldRadius) {
                return true;
            }
        }
    }

    return false;
}

bool placementAreaBlocked(const std::vector<safecrowd::domain::Point2D>& area) {
    return area.empty();
}

safecrowd::domain::Point2D defaultVelocityFrom(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& point) {
    std::optional<safecrowd::domain::Point2D> target;
    for (const auto& zone : layout.zones) {
        if (zone.kind == safecrowd::domain::ZoneKind::Exit) {
            target = polygonCenter(zone.area);
            break;
        }
    }
    if (!target.has_value() && !layout.zones.empty()) {
        target = polygonCenter(layout.zones.back().area);
    }
    if (!target.has_value()) {
        return {};
    }

    const auto dx = target->x - point.x;
    const auto dy = target->y - point.y;
    const auto length = std::hypot(dx, dy);
    if (length <= 1e-9) {
        return {};
    }

    return {
        .x = (dx / length) * kDefaultInitialSpeed,
        .y = (dy / length) * kDefaultInitialSpeed,
    };
}

QString nextPlacementId(
    const std::vector<ScenarioCrowdPlacement>& placements,
    ScenarioCrowdPlacementKind kind) {
    const char* prefix = "group";
    if (kind == ScenarioCrowdPlacementKind::Individual) {
        prefix = "individual";
    } else if (kind == ScenarioCrowdPlacementKind::Source) {
        prefix = "source";
    }
    return QString("%1-%2").arg(prefix).arg(static_cast<int>(placements.size()) + 1);
}

QString nextConnectionBlockId(const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks) {
    return QString("block-%1").arg(static_cast<int>(blocks.size()) + 1);
}

QString nextEnvironmentHazardId(const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards) {
    for (int index = static_cast<int>(hazards.size()) + 1;; ++index) {
        const auto candidate = QString("hazard-%1").arg(index);
        const auto exists = std::any_of(hazards.begin(), hazards.end(), [&](const auto& hazard) {
            return QString::fromStdString(hazard.id) == candidate;
        });
        if (!exists) {
            return candidate;
        }
    }
}

QString nextRouteGuidanceId(const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances) {
    return QString("guidance-%1").arg(static_cast<int>(guidances.size()) + 1);
}

PointBounds boundsOfPoints(const std::vector<safecrowd::domain::Point2D>& points) {
    PointBounds bounds;
    if (points.empty()) {
        return bounds;
    }

    bounds.minX = points.front().x;
    bounds.maxX = points.front().x;
    bounds.minY = points.front().y;
    bounds.maxY = points.front().y;
    for (const auto& point : points) {
        bounds.minX = std::min(bounds.minX, point.x);
        bounds.maxX = std::max(bounds.maxX, point.x);
        bounds.minY = std::min(bounds.minY, point.y);
        bounds.maxY = std::max(bounds.maxY, point.y);
    }
    return bounds;
}

bool overlapsExistingPoint(
    const safecrowd::domain::Point2D& point,
    const std::vector<safecrowd::domain::Point2D>& points) {
    return std::any_of(points.begin(), points.end(), [&](const auto& existing) {
        return std::hypot(existing.x - point.x, existing.y - point.y) < kOccupantMinSpacing;
    });
}

std::uint32_t placementSeed(
    const QString& id,
    const std::vector<safecrowd::domain::Point2D>& area,
    int occupantCount) {
    std::uint64_t seed = static_cast<std::uint64_t>(std::hash<std::string>{}(id.toStdString()));
    seed ^= static_cast<std::uint64_t>(occupantCount + 0x9e3779b9) + (seed << 6) + (seed >> 2);
    for (const auto& point : area) {
        const auto x = static_cast<std::uint64_t>(std::llround(point.x * 1000.0));
        const auto y = static_cast<std::uint64_t>(std::llround(point.y * 1000.0));
        seed ^= x + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= y + 0xbf58476d1ce4e5b9ULL + (seed << 6) + (seed >> 2);
    }
    return static_cast<std::uint32_t>(seed ^ (seed >> 32));
}

using PlacementBlockedPredicate = std::function<bool(const safecrowd::domain::Point2D&)>;

bool appendGeneratedPoint(
    const std::vector<safecrowd::domain::Point2D>& area,
    const PlacementBlockedPredicate& blocked,
    std::vector<safecrowd::domain::Point2D>& positions,
    const safecrowd::domain::Point2D& point) {
    if (!scenarioPointInsidePlacementArea(area, point) || blocked(point) || overlapsExistingPoint(point, positions)) {
        return false;
    }
    positions.push_back(point);
    return true;
}

std::vector<safecrowd::domain::Point2D> generateUniformPlacementPositions(
    const std::vector<safecrowd::domain::Point2D>& area,
    int occupantCount,
    const PlacementBlockedPredicate& blocked) {
    std::vector<safecrowd::domain::Point2D> positions;
    if (occupantCount <= 0 || area.empty()) {
        return positions;
    }
    positions.reserve(static_cast<std::size_t>(occupantCount));

    if (occupantCount == 1) {
        const auto center = scenarioPlacementCenter(area);
        if (appendGeneratedPoint(area, blocked, positions, center)) {
            return positions;
        }
    }

    const auto bounds = boundsOfPoints(area);
    const auto width = bounds.maxX - bounds.minX;
    const auto height = bounds.maxY - bounds.minY;
    if (width <= kGeometryEpsilon || height <= kGeometryEpsilon) {
        return positions;
    }

    const auto idealSpacing = std::sqrt((width * height / std::max(1, occupantCount)) * (2.0 / std::sqrt(3.0)));
    for (int attempt = 0; attempt < 32 && static_cast<int>(positions.size()) < occupantCount; ++attempt) {
        positions.clear();
        const auto spacing = std::max(kOccupantMinSpacing, idealSpacing * std::pow(0.92, attempt));
        const auto rowSpacing = spacing * std::sqrt(3.0) / 2.0;
        int row = 0;
        for (double y = bounds.minY + kOccupantWorldRadius; y <= bounds.maxY - kOccupantWorldRadius + kGeometryEpsilon; y += rowSpacing, ++row) {
            const auto stagger = (row % 2 == 0) ? 0.0 : spacing * 0.5;
            for (double x = bounds.minX + kOccupantWorldRadius + stagger; x <= bounds.maxX - kOccupantWorldRadius + kGeometryEpsilon; x += spacing) {
                appendGeneratedPoint(area, blocked, positions, {.x = x, .y = y});
                if (static_cast<int>(positions.size()) >= occupantCount) {
                    return positions;
                }
            }
        }
        if (spacing <= kOccupantMinSpacing + kGeometryEpsilon) {
            break;
        }
    }

    return positions;
}

std::vector<safecrowd::domain::Point2D> generateRandomPlacementPositions(
    const QString& id,
    const std::vector<safecrowd::domain::Point2D>& area,
    int occupantCount,
    const PlacementBlockedPredicate& blocked) {
    std::vector<safecrowd::domain::Point2D> positions;
    if (occupantCount <= 0 || area.empty()) {
        return positions;
    }
    positions.reserve(static_cast<std::size_t>(occupantCount));

    const auto bounds = boundsOfPoints(area);
    if ((bounds.maxX - bounds.minX) <= kGeometryEpsilon || (bounds.maxY - bounds.minY) <= kGeometryEpsilon) {
        return positions;
    }
    if ((bounds.maxX - bounds.minX) < kOccupantMinSpacing || (bounds.maxY - bounds.minY) < kOccupantMinSpacing) {
        return positions;
    }

    std::mt19937 generator(placementSeed(id, area, occupantCount));
    std::uniform_real_distribution<double> xDistribution(bounds.minX + kOccupantWorldRadius, bounds.maxX - kOccupantWorldRadius);
    std::uniform_real_distribution<double> yDistribution(bounds.minY + kOccupantWorldRadius, bounds.maxY - kOccupantWorldRadius);
    const auto maxAttempts = std::clamp(occupantCount * 800, 5000, 300000);
    for (int attempt = 0; attempt < maxAttempts && static_cast<int>(positions.size()) < occupantCount; ++attempt) {
        appendGeneratedPoint(
            area,
            blocked,
            positions,
            {.x = xDistribution(generator), .y = yDistribution(generator)});
    }

    return positions;
}

std::string pickNearestExitZoneIdForPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& point,
    const std::string& preferredFloorId) {
    const auto pickNearest = [&](bool sameFloorOnly) -> std::string {
        double bestDistanceSq = std::numeric_limits<double>::infinity();
        const safecrowd::domain::Zone2D* bestZone = nullptr;
        for (const auto& zone : layout.zones) {
            if (zone.kind != safecrowd::domain::ZoneKind::Exit) {
                continue;
            }
            if (sameFloorOnly && !preferredFloorId.empty() && !zone.floorId.empty() && zone.floorId != preferredFloorId) {
                continue;
            }

            const auto exitCenter = polygonCenter(zone.area);
            const auto dx = exitCenter.x - point.x;
            const auto dy = exitCenter.y - point.y;
            const auto distanceSq = (dx * dx) + (dy * dy);
            if (distanceSq < bestDistanceSq) {
                bestDistanceSq = distanceSq;
                bestZone = &zone;
            }
        }
        return bestZone == nullptr ? std::string{} : bestZone->id;
    };

    if (auto sameFloor = pickNearest(true); !sameFloor.empty()) {
        return sameFloor;
    }
    return pickNearest(false);
}

std::string pickNearestExitZoneIdForConnection(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection) {
    const auto pickAdjacentExit = [&]() -> std::string {
        if (connection.fromZoneId.empty() && connection.toZoneId.empty()) {
            return {};
        }
        const auto exitIt = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
            if (zone.kind != safecrowd::domain::ZoneKind::Exit) {
                return false;
            }
            return zone.id == connection.fromZoneId || zone.id == connection.toZoneId;
        });
        return exitIt == layout.zones.end() ? std::string{} : exitIt->id;
    };

    if (auto adjacent = pickAdjacentExit(); !adjacent.empty()) {
        return adjacent;
    }

    return pickNearestExitZoneIdForPoint(layout, scenarioConnectionMarkerCenter(connection), connection.floorId);
}

}  // namespace

int scenarioSourceEmissionCount(
    int agentsPerSpawn,
    double durationSeconds,
    double intervalSeconds,
    int targetAgentCount) {
    if (agentsPerSpawn <= 0 || durationSeconds <= 0.0 || intervalSeconds <= 1e-9) {
        return 0;
    }
    const auto ticks = static_cast<long long>(
        std::floor(std::max(0.0, durationSeconds - 1e-9) / intervalSeconds)) + 1;
    const auto count = std::max<long long>(0, ticks) * static_cast<long long>(agentsPerSpawn);
    const auto cappedCount = targetAgentCount > 0 ? std::min<long long>(targetAgentCount, count) : count;
    return static_cast<int>(std::min<long long>(kScenarioMaxSourceOccupantCount, cappedCount));
}

safecrowd::domain::Point2D scenarioPlacementCenter(const std::vector<safecrowd::domain::Point2D>& area) {
    if (area.empty()) {
        return {};
    }

    double x = 0.0;
    double y = 0.0;
    for (const auto& point : area) {
        x += point.x;
        y += point.y;
    }
    const auto count = static_cast<double>(area.size());
    return {.x = x / count, .y = y / count};
}

safecrowd::domain::Point2D scenarioConnectionMarkerCenter(
    const safecrowd::domain::Connection2D& connection) {
    return {
        .x = (connection.centerSpan.start.x + connection.centerSpan.end.x) * 0.5,
        .y = (connection.centerSpan.start.y + connection.centerSpan.end.y) * 0.5,
    };
}

bool scenarioPointInsidePlacementArea(
    const std::vector<safecrowd::domain::Point2D>& area,
    const safecrowd::domain::Point2D& point) {
    return area.size() < 3 || pointInRing(area, point) || std::any_of(area.begin(), area.end(), [&](const auto& vertex) {
        return std::hypot(vertex.x - point.x, vertex.y - point.y) <= kGeometryEpsilon;
    });
}

std::vector<safecrowd::domain::Point2D> scenarioPlacementDisplayPositions(
    const ScenarioCrowdPlacement& placement) {
    if (!placement.generatedPositions.empty()) {
        return placement.generatedPositions;
    }
    if (placement.kind == ScenarioCrowdPlacementKind::Individual || placement.area.size() < 4) {
        return placement.area.empty() ? std::vector<safecrowd::domain::Point2D>{} : std::vector{safecrowd::domain::Point2D{placement.area.front()}};
    }

    std::vector<safecrowd::domain::Point2D> positions;
    const int markerCount = std::min(20, std::max(1, placement.occupantCount));
    const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(markerCount))));
    positions.reserve(static_cast<std::size_t>(markerCount));
    for (int index = 0; index < markerCount; ++index) {
        const int row = index / columns;
        const int column = index % columns;
        positions.push_back({
            .x = placement.area[0].x + (column + 0.5) * (placement.area[2].x - placement.area[0].x) / columns,
            .y = placement.area[0].y + (row + 0.5) * (placement.area[2].y - placement.area[0].y) / columns,
        });
    }
    return positions;
}

bool scenarioHasExplicitGuidanceInstallPosition(
    const safecrowd::domain::RouteGuidanceDraft& guidance) {
    return !guidance.installFloorId.empty() || !guidance.installZoneId.empty();
}

bool scenarioHazardLocationEqual(
    const safecrowd::domain::EnvironmentHazardDraft& lhs,
    const safecrowd::domain::EnvironmentHazardDraft& rhs) {
    return lhs.affectedZoneId == rhs.affectedZoneId
        && lhs.floorId == rhs.floorId
        && pointsEqual(lhs.position, rhs.position);
}

bool scenarioRouteGuidanceLocationEqual(
    const safecrowd::domain::RouteGuidanceDraft& lhs,
    const safecrowd::domain::RouteGuidanceDraft& rhs) {
    return lhs.guidedExitZoneId == rhs.guidedExitZoneId
        && lhs.installConnectionId == rhs.installConnectionId
        && lhs.installFloorId == rhs.installFloorId
        && lhs.installZoneId == rhs.installZoneId
        && pointsEqual(lhs.installPosition, rhs.installPosition);
}

ScenarioPlacementAuthoringResult createScenarioGroupPlacement(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<ScenarioCrowdPlacement>& placements,
    const std::vector<safecrowd::domain::Point2D>& area,
    int occupantCount,
    safecrowd::domain::InitialPlacementDistribution distribution) {
    if (area.empty()) {
        return {};
    }

    const auto zoneId = zoneIdAt(layout, currentFloorId, area.front());
    if (zoneId.isEmpty()) {
        return {};
    }

    const auto id = nextPlacementId(placements, ScenarioCrowdPlacementKind::Group);
    if (placementAreaBlocked(area)) {
        return {};
    }

    const auto pointOccupiedByExistingPlacement = [&](const safecrowd::domain::Point2D& point) {
        for (const auto& placement : placements) {
            if (!currentFloorId.isEmpty() && !placement.floorId.isEmpty() && placement.floorId != currentFloorId) {
                continue;
            }
            for (const auto& existing : scenarioPlacementDisplayPositions(placement)) {
                if (std::hypot(existing.x - point.x, existing.y - point.y) < kOccupantMinSpacing) {
                    return true;
                }
            }
        }
        return false;
    };
    const auto blocked = [&](const safecrowd::domain::Point2D& point) {
        return zoneIdAt(layout, currentFloorId, point) != zoneId
            || placementPointBlocked(layout, currentFloorId, point)
            || pointOccupiedByExistingPlacement(point);
    };
    const auto generatedPositions = distribution == safecrowd::domain::InitialPlacementDistribution::Random
        ? generateRandomPlacementPositions(id, area, occupantCount, blocked)
        : generateUniformPlacementPositions(area, occupantCount, blocked);
    if (static_cast<int>(generatedPositions.size()) < occupantCount) {
        return {
            .placement = std::nullopt,
            .errorMessage = "The selected region is too small or blocked for the requested occupant count.",
        };
    }

    return {
        .placement = ScenarioCrowdPlacement{
            .id = id,
            .name = QString("Group %1").arg(id.section('-', -1)),
            .kind = ScenarioCrowdPlacementKind::Group,
            .zoneId = zoneId,
            .floorId = currentFloorId,
            .area = area,
            .occupantCount = occupantCount,
            .velocity = defaultVelocityFrom(layout, scenarioPlacementCenter(area)),
            .distribution = distribution,
            .generatedPositions = generatedPositions,
        },
    };
}

ScenarioPlacementAuthoringResult createScenarioIndividualPlacement(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<ScenarioCrowdPlacement>& placements,
    const safecrowd::domain::Point2D& point) {
    const auto zoneId = zoneIdAt(layout, currentFloorId, point);
    if (zoneId.isEmpty() || placementPointBlocked(layout, currentFloorId, point)) {
        return {};
    }

    const auto id = nextPlacementId(placements, ScenarioCrowdPlacementKind::Individual);
    return {
        .placement = ScenarioCrowdPlacement{
            .id = id,
            .name = QString("Individual %1").arg(id.section('-', -1)),
            .kind = ScenarioCrowdPlacementKind::Individual,
            .zoneId = zoneId,
            .floorId = currentFloorId,
            .area = {point},
            .occupantCount = 1,
            .velocity = defaultVelocityFrom(layout, point),
        },
    };
}

ScenarioPlacementAuthoringResult createScenarioSourcePlacement(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<ScenarioCrowdPlacement>& placements,
    const safecrowd::domain::Point2D& point,
    int agentsPerSpawn,
    double startSeconds,
    double durationSeconds,
    double intervalSeconds) {
    const auto zoneId = zoneIdAt(layout, currentFloorId, point);
    if (zoneId.isEmpty() || placementPointBlocked(layout, currentFloorId, point)) {
        return {};
    }

    const auto id = nextPlacementId(placements, ScenarioCrowdPlacementKind::Source);
    const auto sourceCount = scenarioSourceEmissionCount(agentsPerSpawn, durationSeconds, intervalSeconds);
    return {
        .placement = ScenarioCrowdPlacement{
            .id = id,
            .name = QString("Source %1").arg(id.section('-', -1)),
            .kind = ScenarioCrowdPlacementKind::Source,
            .zoneId = zoneId,
            .floorId = currentFloorId,
            .area = {point},
            .occupantCount = sourceCount,
            .velocity = defaultVelocityFrom(layout, point),
            .sourceAgentsPerSpawn = agentsPerSpawn,
            .sourceStartSeconds = startSeconds,
            .sourceEndSeconds = startSeconds + durationSeconds,
            .sourceIntervalSeconds = intervalSeconds,
        },
    };
}

ScenarioConnectionBlockAuthoringResult createScenarioConnectionBlock(
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    const safecrowd::domain::Connection2D& connection) {
    if (connection.kind != safecrowd::domain::ConnectionKind::Doorway
        && connection.kind != safecrowd::domain::ConnectionKind::Exit) {
        return {
            .block = std::nullopt,
            .errorMessage = "This tool can only be used on exits or doors.",
        };
    }

    for (const auto& existing : blocks) {
        if (existing.connectionId == connection.id) {
            return {
                .block = std::nullopt,
                .errorMessage = "This door or exit is already blocked.",
            };
        }
    }

    safecrowd::domain::ConnectionBlockDraft draft;
    draft.id = nextConnectionBlockId(blocks).toStdString();
    draft.connectionId = connection.id;
    return {.block = std::move(draft)};
}

ScenarioConnectionBlockAuthoringResult moveScenarioConnectionBlock(
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    std::size_t index,
    const safecrowd::domain::Connection2D& connection) {
    if (index >= blocks.size()) {
        return {};
    }

    for (std::size_t otherIndex = 0; otherIndex < blocks.size(); ++otherIndex) {
        if (otherIndex == index) {
            continue;
        }
        if (blocks[otherIndex].connectionId == connection.id) {
            return {
                .block = std::nullopt,
                .errorMessage = "This door or exit is already blocked.",
            };
        }
    }

    auto block = blocks[index];
    block.connectionId = connection.id;
    return {.block = std::move(block)};
}

ScenarioEnvironmentHazardAuthoringResult createScenarioEnvironmentHazard(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards,
    const safecrowd::domain::Point2D& position,
    safecrowd::domain::EnvironmentHazardKind kind) {
    const auto* zone = zoneAt(layout, currentFloorId, position);
    if (zone == nullptr) {
        return {
            .hazard = std::nullopt,
            .errorMessage = "Click inside a zone to place a fire or smoke hazard.",
        };
    }

    return createScenarioEnvironmentHazardForZone(currentFloorId, hazards, *zone, position, kind);
}

ScenarioEnvironmentHazardAuthoringResult createScenarioEnvironmentHazardForZone(
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards,
    const safecrowd::domain::Zone2D& zone,
    safecrowd::domain::Point2D position,
    safecrowd::domain::EnvironmentHazardKind kind) {
    if (!matchesFloor(zone.floorId, currentFloorId)) {
        return {};
    }

    if (!pointInPolygon(zone.area, position)) {
        const auto fallbackPosition = representativePointInPolygon(zone.area);
        if (!fallbackPosition.has_value()) {
            return {
                .hazard = std::nullopt,
                .errorMessage = "Could not find a valid point inside this zone.",
            };
        }
        position = *fallbackPosition;
    }

    safecrowd::domain::EnvironmentHazardDraft draft;
    draft.id = nextEnvironmentHazardId(hazards).toStdString();
    draft.kind = kind;
    draft.name = QString("%1 hazard %2")
        .arg(kind == safecrowd::domain::EnvironmentHazardKind::Fire ? "Fire" : "Smoke")
        .arg(static_cast<int>(hazards.size()) + 1)
        .toStdString();
    draft.affectedZoneId = zone.id;
    draft.floorId = zone.floorId.empty() ? currentFloorId.toStdString() : zone.floorId;
    draft.position = position;
    draft.startSeconds = 0.0;
    draft.endSeconds = 60.0;
    draft.severity = safecrowd::domain::ScenarioElementSeverity::Medium;
    draft.radiusMeters = safecrowd::domain::environmentHazardRadiusMeters(draft.severity);
    return {.hazard = std::move(draft)};
}

ScenarioEnvironmentHazardAuthoringResult moveScenarioEnvironmentHazard(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards,
    std::size_t index,
    const safecrowd::domain::Point2D& position) {
    if (index >= hazards.size()) {
        return {};
    }

    const auto* zone = zoneAt(layout, currentFloorId, position);
    if (zone == nullptr || !matchesFloor(zone->floorId, currentFloorId) || !pointInPolygon(zone->area, position)) {
        return {
            .hazard = std::nullopt,
            .errorMessage = "Hazards must stay inside walkable room space and not too close to walls.",
        };
    }

    const auto floorId = zone->floorId.empty() ? currentFloorId.toStdString() : zone->floorId;
    if (!safecrowd::domain::pointInsideWalkableZoneWithClearance(
            layout,
            position,
            floorId,
            kGuidancePlacementBarrierClearance)) {
        return {
            .hazard = std::nullopt,
            .errorMessage = "Hazards must stay inside walkable room space and not too close to walls.",
        };
    }

    auto hazard = hazards[index];
    hazard.affectedZoneId = zone->id;
    hazard.floorId = floorId;
    hazard.position = position;
    return {.hazard = std::move(hazard)};
}

ScenarioRouteGuidanceAuthoringResult createScenarioRouteGuidanceForZonePosition(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    const safecrowd::domain::Zone2D& zone,
    safecrowd::domain::Point2D position) {
    if (!matchesFloor(zone.floorId, currentFloorId)) {
        return {};
    }
    if (zone.kind == safecrowd::domain::ZoneKind::Exit) {
        return createScenarioRouteGuidanceForExitZone(currentFloorId, guidances, zone);
    }
    if (!pointInPolygon(zone.area, position)) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "Click inside a walkable room area to place guidance.",
        };
    }

    const auto floorId = zone.floorId.empty() ? currentFloorId.toStdString() : zone.floorId;
    if (!safecrowd::domain::pointInsideWalkableZoneWithClearance(
            layout,
            position,
            floorId,
            kGuidancePlacementBarrierClearance)) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "Guidance must be placed inside walkable room space and not too close to walls.",
        };
    }

    const auto exitZoneId = pickNearestExitZoneIdForPoint(layout, position, floorId);
    if (exitZoneId.empty()) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "Could not find a reachable exit target for this guidance.",
        };
    }

    safecrowd::domain::RouteGuidanceDraft draft;
    draft.id = nextRouteGuidanceId(guidances).toStdString();
    draft.startSeconds = 0.0;
    draft.endSeconds = 0.0;
    draft.periods.clear();
    draft.guidedExitZoneId = exitZoneId;
    draft.installConnectionId.clear();
    draft.installFloorId = floorId;
    draft.installZoneId = zone.id;
    draft.installPosition = position;
    draft.baseComplianceRate = 0.5;
    draft.influenceRadiusMeters = 2.5;
    draft.maxDetourMeters = 20.0;
    return {.guidance = std::move(draft)};
}

ScenarioRouteGuidanceAuthoringResult createScenarioRouteGuidanceForExitZone(
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    const safecrowd::domain::Zone2D& zone) {
    if (zone.kind != safecrowd::domain::ZoneKind::Exit) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "This tool can only be used on exit zones.",
        };
    }

    for (const auto& existing : guidances) {
        if (existing.installConnectionId.empty()
            && existing.guidedExitZoneId == zone.id
            && (existing.installZoneId.empty() || existing.installZoneId == zone.id)) {
            return {
                .guidance = std::nullopt,
                .errorMessage = "Guidance is already installed on this exit.",
            };
        }
    }

    safecrowd::domain::RouteGuidanceDraft draft;
    draft.id = nextRouteGuidanceId(guidances).toStdString();
    draft.startSeconds = 0.0;
    draft.endSeconds = 0.0;
    draft.periods.clear();
    draft.guidedExitZoneId = zone.id;
    draft.installConnectionId.clear();
    draft.installFloorId = zone.floorId.empty() ? currentFloorId.toStdString() : zone.floorId;
    draft.installZoneId = zone.id;
    draft.installPosition = representativePointInPolygon(zone.area).value_or(polygonCenter(zone.area));
    draft.baseComplianceRate = 0.5;
    draft.influenceRadiusMeters = 2.5;
    draft.maxDetourMeters = 20.0;
    return {.guidance = std::move(draft)};
}

ScenarioRouteGuidanceAuthoringResult createScenarioRouteGuidanceForConnection(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    const safecrowd::domain::Connection2D& connection) {
    if (connection.kind != safecrowd::domain::ConnectionKind::Doorway
        && connection.kind != safecrowd::domain::ConnectionKind::Exit) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "This tool can only be used on exits or doors.",
        };
    }

    for (const auto& existing : guidances) {
        if (!existing.installConnectionId.empty() && existing.installConnectionId == connection.id) {
            return {
                .guidance = std::nullopt,
                .errorMessage = "Guidance is already installed on this door or exit.",
            };
        }
    }

    const auto exitZoneId = pickNearestExitZoneIdForConnection(layout, connection);
    if (exitZoneId.empty()) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "Could not find a reachable exit target for this guidance.",
        };
    }
    safecrowd::domain::RouteGuidanceDraft draft;
    draft.id = nextRouteGuidanceId(guidances).toStdString();
    draft.startSeconds = 0.0;
    draft.endSeconds = 0.0;
    draft.periods.clear();
    draft.guidedExitZoneId = exitZoneId;
    draft.installConnectionId = connection.id;
    draft.installFloorId = connection.floorId.empty() ? currentFloorId.toStdString() : connection.floorId;
    draft.installZoneId.clear();
    draft.installPosition = scenarioConnectionMarkerCenter(connection);
    draft.baseComplianceRate = 0.5;
    draft.influenceRadiusMeters = 2.5;
    draft.maxDetourMeters = 20.0;
    return {.guidance = std::move(draft)};
}

ScenarioRouteGuidanceAuthoringResult moveScenarioRouteGuidanceToConnection(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    std::size_t index,
    const safecrowd::domain::Connection2D& connection) {
    if (index >= guidances.size()) {
        return {};
    }
    if (connection.kind != safecrowd::domain::ConnectionKind::Doorway
        && connection.kind != safecrowd::domain::ConnectionKind::Exit) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "Guidance can only be moved onto exits or doors.",
        };
    }

    for (std::size_t otherIndex = 0; otherIndex < guidances.size(); ++otherIndex) {
        if (otherIndex == index) {
            continue;
        }
        if (!guidances[otherIndex].installConnectionId.empty()
            && guidances[otherIndex].installConnectionId == connection.id) {
            return {
                .guidance = std::nullopt,
                .errorMessage = "Guidance is already installed on this door or exit.",
            };
        }
    }

    auto guidance = guidances[index];
    guidance.guidedExitZoneId = pickNearestExitZoneIdForConnection(layout, connection);
    if (guidance.guidedExitZoneId.empty()) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "Could not find a reachable exit target for this guidance.",
        };
    }
    guidance.installConnectionId = connection.id;
    guidance.installFloorId = connection.floorId.empty() ? currentFloorId.toStdString() : connection.floorId;
    guidance.installZoneId.clear();
    guidance.installPosition = scenarioConnectionMarkerCenter(connection);
    return {.guidance = std::move(guidance)};
}

ScenarioRouteGuidanceAuthoringResult moveScenarioRouteGuidanceToExitZone(
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    std::size_t index,
    const safecrowd::domain::Zone2D& zone) {
    if (index >= guidances.size()) {
        return {};
    }

    for (std::size_t otherIndex = 0; otherIndex < guidances.size(); ++otherIndex) {
        if (otherIndex == index) {
            continue;
        }
        const auto& existing = guidances[otherIndex];
        if (existing.installConnectionId.empty()
            && existing.guidedExitZoneId == zone.id
            && (existing.installZoneId.empty() || existing.installZoneId == zone.id)) {
            return {
                .guidance = std::nullopt,
                .errorMessage = "Guidance is already installed on this exit.",
            };
        }
    }

    auto guidance = guidances[index];
    guidance.guidedExitZoneId = zone.id;
    guidance.installConnectionId.clear();
    guidance.installFloorId = zone.floorId.empty() ? currentFloorId.toStdString() : zone.floorId;
    guidance.installZoneId = zone.id;
    guidance.installPosition = representativePointInPolygon(zone.area).value_or(polygonCenter(zone.area));
    return {.guidance = std::move(guidance)};
}

ScenarioRouteGuidanceAuthoringResult moveScenarioRouteGuidanceToZonePosition(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    std::size_t index,
    const safecrowd::domain::Zone2D* zone,
    safecrowd::domain::Point2D position) {
    if (index >= guidances.size()) {
        return {};
    }

    if (zone == nullptr || !matchesFloor(zone->floorId, currentFloorId) || !pointInPolygon(zone->area, position)) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "Guidance must be placed on a walkable room area, exit zone, or a door.",
        };
    }

    const auto floorId = zone->floorId.empty() ? currentFloorId.toStdString() : zone->floorId;
    if (!safecrowd::domain::pointInsideWalkableZoneWithClearance(
            layout,
            position,
            floorId,
            kGuidancePlacementBarrierClearance)) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "Guidance must stay inside walkable room space and not too close to walls.",
        };
    }

    const auto exitZoneId = pickNearestExitZoneIdForPoint(layout, position, floorId);
    if (exitZoneId.empty()) {
        return {
            .guidance = std::nullopt,
            .errorMessage = "Could not find a reachable exit target for this guidance.",
        };
    }

    auto guidance = guidances[index];
    guidance.guidedExitZoneId = exitZoneId;
    guidance.installConnectionId.clear();
    guidance.installFloorId = floorId;
    guidance.installZoneId = zone->id;
    guidance.installPosition = position;
    return {.guidance = std::move(guidance)};
}

}  // namespace safecrowd::application
