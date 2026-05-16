#include "domain/GeometryQueries.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace safecrowd::domain {
namespace {

constexpr double kGeometryEpsilon = 1e-9;

struct PointBounds {
    double minX{0.0};
    double minY{0.0};
    double maxX{0.0};
    double maxY{0.0};
};

PointBounds boundsOfPoints(const std::vector<Point2D>& points) {
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

Point2D polygonCenter(const Polygon2D& polygon) {
    if (polygon.outline.empty()) {
        return {};
    }

    double x = 0.0;
    double y = 0.0;
    for (const auto& point : polygon.outline) {
        x += point.x;
        y += point.y;
    }
    const auto count = static_cast<double>(polygon.outline.size());
    return {.x = x / count, .y = y / count};
}

bool pointWithinSegmentBounds(
    const Point2D& point,
    const Point2D& start,
    const Point2D& end,
    double margin) {
    const auto minX = std::min(start.x, end.x) - margin;
    const auto maxX = std::max(start.x, end.x) + margin;
    const auto minY = std::min(start.y, end.y) - margin;
    const auto maxY = std::max(start.y, end.y) + margin;
    return point.x >= minX && point.x <= maxX && point.y >= minY && point.y <= maxY;
}

void appendRingYValues(const std::vector<Point2D>& ring, std::vector<double>& values) {
    for (const auto& point : ring) {
        values.push_back(point.y);
    }
}

void appendRingIntersections(const std::vector<Point2D>& ring, double y, std::vector<double>& intersections) {
    if (ring.size() < 2) {
        return;
    }

    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const auto& a = ring[j];
        const auto& b = ring[i];
        if ((a.y > y) == (b.y > y)) {
            continue;
        }

        const auto denominator = b.y - a.y;
        if (std::fabs(denominator) <= kGeometryEpsilon) {
            continue;
        }
        intersections.push_back(a.x + ((y - a.y) * (b.x - a.x) / denominator));
    }
}

std::optional<Point2D> scanlineRepresentativePoint(const Polygon2D& polygon) {
    std::vector<double> yValues;
    yValues.reserve(polygon.outline.size());
    appendRingYValues(polygon.outline, yValues);
    for (const auto& hole : polygon.holes) {
        appendRingYValues(hole, yValues);
    }
    if (yValues.size() < 2) {
        return std::nullopt;
    }

    std::sort(yValues.begin(), yValues.end());
    yValues.erase(
        std::unique(yValues.begin(), yValues.end(), [](double lhs, double rhs) {
            return std::fabs(lhs - rhs) <= kGeometryEpsilon;
        }),
        yValues.end());
    if (yValues.size() < 2) {
        return std::nullopt;
    }

    std::optional<Point2D> bestPoint;
    double bestWidth = 0.0;
    std::vector<double> intersections;
    for (std::size_t yIndex = 1; yIndex < yValues.size(); ++yIndex) {
        const auto lowerY = yValues[yIndex - 1];
        const auto upperY = yValues[yIndex];
        if (upperY - lowerY <= kGeometryEpsilon) {
            continue;
        }

        const auto y = (lowerY + upperY) * 0.5;
        intersections.clear();
        appendRingIntersections(polygon.outline, y, intersections);
        for (const auto& hole : polygon.holes) {
            appendRingIntersections(hole, y, intersections);
        }
        if (intersections.size() < 2) {
            continue;
        }

        std::sort(intersections.begin(), intersections.end());
        for (std::size_t xIndex = 1; xIndex < intersections.size(); xIndex += 2) {
            const auto left = intersections[xIndex - 1];
            const auto right = intersections[xIndex];
            const auto width = right - left;
            if (width <= bestWidth + kGeometryEpsilon) {
                continue;
            }

            const Point2D candidate{.x = (left + right) * 0.5, .y = y};
            if (pointInPolygon(polygon, candidate)) {
                bestWidth = width;
                bestPoint = candidate;
            }
        }
    }

    return bestPoint;
}

std::optional<Point2D> gridRepresentativePoint(const Polygon2D& polygon) {
    const auto bounds = boundsOfPoints(polygon.outline);
    const auto width = bounds.maxX - bounds.minX;
    const auto height = bounds.maxY - bounds.minY;
    if (width <= kGeometryEpsilon || height <= kGeometryEpsilon) {
        return std::nullopt;
    }

    constexpr int kSampleCounts[] = {12, 24, 48};
    for (const auto sampleCount : kSampleCounts) {
        for (int yIndex = 0; yIndex < sampleCount; ++yIndex) {
            const auto y = bounds.minY + (height * (static_cast<double>(yIndex) + 0.5) / sampleCount);
            for (int xIndex = 0; xIndex < sampleCount; ++xIndex) {
                const Point2D candidate{
                    .x = bounds.minX + (width * (static_cast<double>(xIndex) + 0.5) / sampleCount),
                    .y = y,
                };
                if (pointInPolygon(polygon, candidate)) {
                    return candidate;
                }
            }
        }
    }

    return std::nullopt;
}

}  // namespace

bool pointInRing(const std::vector<Point2D>& ring, const Point2D& point) {
    if (ring.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const auto& a = ring[i];
        const auto& b = ring[j];
        const auto intersects = ((a.y > point.y) != (b.y > point.y))
            && (point.x < ((b.x - a.x) * (point.y - a.y) / ((b.y - a.y) == 0.0 ? 1e-9 : (b.y - a.y)) + a.x));
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool pointInPolygon(const Polygon2D& polygon, const Point2D& point) {
    if (!pointInRing(polygon.outline, point)) {
        return false;
    }
    return std::none_of(polygon.holes.begin(), polygon.holes.end(), [&](const auto& hole) {
        return pointInRing(hole, point);
    });
}

double distancePointToSegment(const Point2D& point, const Point2D& start, const Point2D& end) {
    const auto dx = end.x - start.x;
    const auto dy = end.y - start.y;
    const auto lengthSquared = (dx * dx) + (dy * dy);
    if (lengthSquared <= kGeometryEpsilon) {
        return std::hypot(point.x - start.x, point.y - start.y);
    }

    const auto t = std::clamp(
        (((point.x - start.x) * dx) + ((point.y - start.y) * dy)) / lengthSquared,
        0.0,
        1.0);
    return std::hypot(point.x - (start.x + (dx * t)), point.y - (start.y + (dy * t)));
}

double distanceToPolygonBoundary(const Polygon2D& polygon, const Point2D& point) {
    double best = std::numeric_limits<double>::max();
    const auto checkRing = [&](const std::vector<Point2D>& ring) {
        if (ring.size() < 2) {
            return;
        }
        for (std::size_t index = 0; index < ring.size(); ++index) {
            best = std::min(best, distancePointToSegment(point, ring[index], ring[(index + 1) % ring.size()]));
        }
    };

    checkRing(polygon.outline);
    for (const auto& hole : polygon.holes) {
        checkRing(hole);
    }
    return best;
}

std::optional<Point2D> representativePointInPolygon(const Polygon2D& polygon) {
    if (polygon.outline.size() < 3) {
        return std::nullopt;
    }

    const auto center = polygonCenter(polygon);
    if (pointInPolygon(polygon, center)) {
        return center;
    }

    if (auto point = scanlineRepresentativePoint(polygon); point.has_value()) {
        return point;
    }
    return gridRepresentativePoint(polygon);
}

bool pointHasBarrierClearance(
    const FacilityLayout2D& layout,
    const Point2D& point,
    const std::string& floorId,
    double clearance) {
    if (clearance <= kGeometryEpsilon) {
        return true;
    }

    for (const auto& barrier : layout.barriers) {
        if (!floorId.empty() && !barrier.floorId.empty() && barrier.floorId != floorId) {
            continue;
        }
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
            if (pointWithinSegmentBounds(point, vertices[index], vertices[index + 1], clearance)
                && distancePointToSegment(point, vertices[index], vertices[index + 1]) < clearance) {
                return false;
            }
        }
        if (barrier.geometry.closed
            && pointWithinSegmentBounds(point, vertices.back(), vertices.front(), clearance)
            && distancePointToSegment(point, vertices.back(), vertices.front()) < clearance) {
            return false;
        }
    }
    return true;
}

bool pointInsideWalkableZoneWithClearance(
    const FacilityLayout2D& layout,
    const Point2D& point,
    const std::string& floorId,
    double clearance) {
    const auto zoneIt = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        if (!floorId.empty() && !zone.floorId.empty() && zone.floorId != floorId) {
            return false;
        }
        return pointInPolygon(zone.area, point);
    });
    if (zoneIt == layout.zones.end()) {
        return false;
    }
    return pointHasBarrierClearance(layout, point, floorId, clearance);
}

}  // namespace safecrowd::domain
