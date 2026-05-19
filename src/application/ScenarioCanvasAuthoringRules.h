#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <QString>

#include "application/ScenarioCanvasWidget.h"
#include "domain/FacilityLayout2D.h"
#include "domain/ScenarioAuthoring.h"

namespace safecrowd::application {

inline constexpr int kScenarioMaxSourceOccupantCount = 5000;

struct ScenarioPlacementAuthoringResult {
    std::optional<ScenarioCrowdPlacement> placement{};
    QString errorMessage{};
};

struct ScenarioConnectionBlockAuthoringResult {
    std::optional<safecrowd::domain::ConnectionBlockDraft> block{};
    QString errorMessage{};
};

struct ScenarioEnvironmentHazardAuthoringResult {
    std::optional<safecrowd::domain::EnvironmentHazardDraft> hazard{};
    QString errorMessage{};
};

struct ScenarioRouteGuidanceAuthoringResult {
    std::optional<safecrowd::domain::RouteGuidanceDraft> guidance{};
    QString errorMessage{};
};

int scenarioSourceEmissionCount(
    int agentsPerSpawn,
    double durationSeconds,
    double intervalSeconds,
    int targetAgentCount = 0);

safecrowd::domain::Point2D scenarioPolygonCenter(const safecrowd::domain::Polygon2D& polygon);
safecrowd::domain::Point2D scenarioPlacementCenter(const std::vector<safecrowd::domain::Point2D>& area);
safecrowd::domain::Point2D scenarioConnectionMarkerCenter(const safecrowd::domain::Connection2D& connection);
bool scenarioPointInsidePlacementArea(
    const std::vector<safecrowd::domain::Point2D>& area,
    const safecrowd::domain::Point2D& point);
std::vector<safecrowd::domain::Point2D> scenarioPlacementDisplayPositions(
    const ScenarioCrowdPlacement& placement);
bool scenarioHasExplicitGuidanceInstallPosition(
    const safecrowd::domain::RouteGuidanceDraft& guidance);
bool scenarioHazardLocationEqual(
    const safecrowd::domain::EnvironmentHazardDraft& lhs,
    const safecrowd::domain::EnvironmentHazardDraft& rhs);
bool scenarioRouteGuidanceLocationEqual(
    const safecrowd::domain::RouteGuidanceDraft& lhs,
    const safecrowd::domain::RouteGuidanceDraft& rhs);

ScenarioPlacementAuthoringResult createScenarioGroupPlacement(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<ScenarioCrowdPlacement>& placements,
    const std::vector<safecrowd::domain::Point2D>& area,
    int occupantCount,
    safecrowd::domain::InitialPlacementDistribution distribution);

ScenarioPlacementAuthoringResult createScenarioIndividualPlacement(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<ScenarioCrowdPlacement>& placements,
    const safecrowd::domain::Point2D& point);

ScenarioPlacementAuthoringResult createScenarioSourcePlacement(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<ScenarioCrowdPlacement>& placements,
    const safecrowd::domain::Point2D& point,
    int agentsPerSpawn,
    double startSeconds,
    double durationSeconds,
    double intervalSeconds);

ScenarioConnectionBlockAuthoringResult createScenarioConnectionBlock(
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    const safecrowd::domain::Connection2D& connection);

ScenarioConnectionBlockAuthoringResult moveScenarioConnectionBlock(
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    std::size_t index,
    const safecrowd::domain::Connection2D& connection);

ScenarioEnvironmentHazardAuthoringResult createScenarioEnvironmentHazard(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards,
    const safecrowd::domain::Point2D& position,
    safecrowd::domain::EnvironmentHazardKind kind);

ScenarioEnvironmentHazardAuthoringResult createScenarioEnvironmentHazardForZone(
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards,
    const safecrowd::domain::Zone2D& zone,
    safecrowd::domain::Point2D position,
    safecrowd::domain::EnvironmentHazardKind kind);

ScenarioEnvironmentHazardAuthoringResult moveScenarioEnvironmentHazard(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards,
    std::size_t index,
    const safecrowd::domain::Point2D& position);

ScenarioRouteGuidanceAuthoringResult createScenarioRouteGuidanceForZonePosition(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    const safecrowd::domain::Zone2D& zone,
    safecrowd::domain::Point2D position);

ScenarioRouteGuidanceAuthoringResult createScenarioRouteGuidanceForExitZone(
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    const safecrowd::domain::Zone2D& zone);

ScenarioRouteGuidanceAuthoringResult createScenarioRouteGuidanceForConnection(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    const safecrowd::domain::Connection2D& connection);

ScenarioRouteGuidanceAuthoringResult moveScenarioRouteGuidanceToConnection(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    std::size_t index,
    const safecrowd::domain::Connection2D& connection);

ScenarioRouteGuidanceAuthoringResult moveScenarioRouteGuidanceToExitZone(
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    std::size_t index,
    const safecrowd::domain::Zone2D& zone);

ScenarioRouteGuidanceAuthoringResult moveScenarioRouteGuidanceToZonePosition(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& currentFloorId,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    std::size_t index,
    const safecrowd::domain::Zone2D* zone,
    safecrowd::domain::Point2D position);

}  // namespace safecrowd::application
