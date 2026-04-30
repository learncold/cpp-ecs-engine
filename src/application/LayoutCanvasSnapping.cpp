#include "application/LayoutCanvasSnapping.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace safecrowd::application {
namespace {

bool matchesFloor(const std::string& elementFloorId, const std::string& floorId) {
    return floorId.empty() || elementFloorId.empty() || elementFloorId == floorId;
}

safecrowd::domain::Point2D operator-(const safecrowd::domain::Point2D& lhs, const safecrowd::domain::Point2D& rhs) {
    return {.x = lhs.x - rhs.x, .y = lhs.y - rhs.y};
}

safecrowd::domain::Point2D operator+(const safecrowd::domain::Point2D& lhs, const safecrowd::domain::Point2D& rhs) {
    return {.x = lhs.x + rhs.x, .y = lhs.y + rhs.y};
}

safecrowd::domain::Point2D operator*(const safecrowd::domain::Point2D& point, double scalar) {
    return {.x = point.x * scalar, .y = point.y * scalar};
}

double dot(const safecrowd::domain::Point2D& lhs, const safecrowd::domain::Point2D& rhs) {
    return (lhs.x * rhs.x) + (lhs.y * rhs.y);
}

double screenDistance(
    const LayoutCanvasTransform& transform,
    const safecrowd::domain::Point2D& lhs,
    const safecrowd::domain::Point2D& rhs) {
    const auto a = transform.map(lhs);
    const auto b = transform.map(rhs);
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

safecrowd::domain::Point2D closestPointOnSegment(
    const safecrowd::domain::Point2D& point,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end) {
    const auto segment = end - start;
    const auto lengthSquared = dot(segment, segment);
    if (lengthSquared <= 1e-9) {
        return start;
    }

    const auto t = std::clamp(dot(point - start, segment) / lengthSquared, 0.0, 1.0);
    return start + (segment * t);
}

void appendPolygonSnapGeometry(
    const safecrowd::domain::Polygon2D& polygon,
    std::vector<safecrowd::domain::Point2D>& vertices,
    std::vector<safecrowd::domain::LineSegment2D>& edges) {
    const auto appendRing = [&](const std::vector<safecrowd::domain::Point2D>& ring) {
        if (ring.empty()) {
            return;
        }
        vertices.insert(vertices.end(), ring.begin(), ring.end());
        if (ring.size() < 2) {
            return;
        }
        for (std::size_t index = 0; index < ring.size(); ++index) {
            edges.push_back({
                .start = ring[index],
                .end = ring[(index + 1) % ring.size()],
            });
        }
    };

    appendRing(polygon.outline);
    for (const auto& hole : polygon.holes) {
        appendRing(hole);
    }
}

void appendPolylineSnapGeometry(
    const safecrowd::domain::Polyline2D& polyline,
    std::vector<safecrowd::domain::Point2D>& vertices,
    std::vector<safecrowd::domain::LineSegment2D>& edges) {
    vertices.insert(vertices.end(), polyline.vertices.begin(), polyline.vertices.end());
    if (polyline.vertices.size() < 2) {
        return;
    }

    for (std::size_t index = 1; index < polyline.vertices.size(); ++index) {
        edges.push_back({
            .start = polyline.vertices[index - 1],
            .end = polyline.vertices[index],
        });
    }
    if (polyline.closed && polyline.vertices.size() > 2) {
        edges.push_back({
            .start = polyline.vertices.back(),
            .end = polyline.vertices.front(),
        });
    }
}

}  // namespace

LayoutSnapResult snapLayoutPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& floorId,
    const safecrowd::domain::Point2D& point,
    const LayoutCanvasTransform& transform,
    const LayoutSnapOptions& options) {
    std::vector<safecrowd::domain::Point2D> vertices;
    std::vector<safecrowd::domain::LineSegment2D> edges;

    for (const auto& zone : layout.zones) {
        if (matchesFloor(zone.floorId, floorId)) {
            appendPolygonSnapGeometry(zone.area, vertices, edges);
        }
    }
    for (const auto& barrier : layout.barriers) {
        if (matchesFloor(barrier.floorId, floorId)) {
            appendPolylineSnapGeometry(barrier.geometry, vertices, edges);
        }
    }
    for (const auto& connection : layout.connections) {
        if (!matchesFloor(connection.floorId, floorId)) {
            continue;
        }
        vertices.push_back(connection.centerSpan.start);
        vertices.push_back(connection.centerSpan.end);
        edges.push_back(connection.centerSpan);
    }

    LayoutSnapResult result{.point = point};
    double bestDistance = options.tolerancePixels;

    if (options.snapVertices) {
        for (const auto& vertex : vertices) {
            const auto distance = screenDistance(transform, point, vertex);
            if (distance <= bestDistance) {
                bestDistance = distance;
                result = {.point = vertex, .snapped = true};
            }
        }
    }

    if (options.snapEdges) {
        for (const auto& edge : edges) {
            const auto candidate = closestPointOnSegment(point, edge.start, edge.end);
            const auto distance = screenDistance(transform, point, candidate);
            if (distance <= bestDistance) {
                bestDistance = distance;
                result = {.point = candidate, .snapped = true};
            }
        }
    }

    return result;
}

}  // namespace safecrowd::application
