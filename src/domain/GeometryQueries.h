#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "domain/FacilityLayout2D.h"
#include "domain/Geometry2D.h"

namespace safecrowd::domain {

struct SpatialCell {
    int x{0};
    int y{0};
};

bool matchesFloor(std::string_view elementFloorId, std::string_view floorId);
std::string defaultFloorId(const FacilityLayout2D& layout, std::string_view fallback = {});
Point2D polygonCenter(const Polygon2D& polygon);
bool pointInRing(const std::vector<Point2D>& ring, const Point2D& point);
bool pointInPolygon(const Polygon2D& polygon, const Point2D& point);
Point2D closestPointOnSegment(const Point2D& point, const Point2D& start, const Point2D& end);
double distancePointToSegment(const Point2D& point, const Point2D& start, const Point2D& end);
double distancePointToSegment(const Point2D& point, const LineSegment2D& segment);
double distanceToPolygonBoundary(const Polygon2D& polygon, const Point2D& point);
std::optional<Point2D> representativePointInPolygon(const Polygon2D& polygon);
SpatialCell spatialCellFor(const Point2D& point, double cellSize);
long long spatialKey(const SpatialCell& cell);
Point2D spatialCellMin(const SpatialCell& cell, double cellSize);
Point2D spatialCellMax(const SpatialCell& cell, double cellSize);
std::vector<SpatialCell> spatialCellsForBounds(const Point2D& minPoint, const Point2D& maxPoint, double cellSize);
bool pointHasBarrierClearance(
    const FacilityLayout2D& layout,
    const Point2D& point,
    const std::string& floorId,
    double clearance);
bool pointInsideWalkableZoneWithClearance(
    const FacilityLayout2D& layout,
    const Point2D& point,
    const std::string& floorId,
    double clearance);

}  // namespace safecrowd::domain
