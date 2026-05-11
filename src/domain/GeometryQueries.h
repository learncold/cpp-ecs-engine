#pragma once

#include <optional>
#include <vector>

#include "domain/Geometry2D.h"

namespace safecrowd::domain {

bool pointInRing(const std::vector<Point2D>& ring, const Point2D& point);
bool pointInPolygon(const Polygon2D& polygon, const Point2D& point);
double distancePointToSegment(const Point2D& point, const Point2D& start, const Point2D& end);
double distanceToPolygonBoundary(const Polygon2D& polygon, const Point2D& point);
std::optional<Point2D> representativePointInPolygon(const Polygon2D& polygon);

}  // namespace safecrowd::domain
