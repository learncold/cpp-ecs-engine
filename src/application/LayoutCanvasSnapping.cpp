#include "application/LayoutCanvasSnapping.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
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

const safecrowd::domain::Zone2D* findZone(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& zoneId) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == zoneId;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

bool isVerticalConnection(const safecrowd::domain::Connection2D& connection) {
    return connection.kind == safecrowd::domain::ConnectionKind::Stair
        || connection.kind == safecrowd::domain::ConnectionKind::Ramp
        || connection.isStair
        || connection.isRamp;
}

bool isVerticalZone(const safecrowd::domain::Zone2D* zone) {
    return zone != nullptr
        && (zone->kind == safecrowd::domain::ZoneKind::Stair || zone->isStair || zone->isRamp);
}

double floorElevation(const safecrowd::domain::FacilityLayout2D& layout, const std::string& floorId) {
    const auto it = std::find_if(layout.floors.begin(), layout.floors.end(), [&](const auto& floor) {
        return floor.id == floorId;
    });
    return it == layout.floors.end() ? 0.0 : it->elevationMeters;
}

std::optional<safecrowd::domain::StairEntryDirection> stairEntryDirectionForFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId) {
    if (!isVerticalConnection(connection)) {
        return std::nullopt;
    }
    const auto* fromZone = findZone(layout, connection.fromZoneId);
    const auto* toZone = findZone(layout, connection.toZoneId);
    if (fromZone == nullptr || toZone == nullptr || fromZone->floorId == toZone->floorId) {
        return std::nullopt;
    }

    const bool fromIsLower = floorElevation(layout, fromZone->floorId) <= floorElevation(layout, toZone->floorId);
    if (floorId == fromZone->floorId) {
        return fromIsLower ? connection.lowerEntryDirection : connection.upperEntryDirection;
    }
    if (floorId == toZone->floorId) {
        return fromIsLower ? connection.upperEntryDirection : connection.lowerEntryDirection;
    }
    return std::nullopt;
}

std::optional<safecrowd::domain::LineSegment2D> stairEntrySpanForFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId) {
    const auto direction = stairEntryDirectionForFloor(layout, connection, floorId);
    if (!direction.has_value() || *direction == safecrowd::domain::StairEntryDirection::Unspecified) {
        return std::nullopt;
    }

    const auto* fromZone = findZone(layout, connection.fromZoneId);
    const auto* toZone = findZone(layout, connection.toZoneId);
    const auto* zone = fromZone != nullptr && fromZone->floorId == floorId ? fromZone : toZone;
    if (!isVerticalZone(zone) || zone->area.outline.empty()) {
        return std::nullopt;
    }

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    for (const auto& point : zone->area.outline) {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }
    const auto halfWidth = std::max(0.35, std::min(maxX - minX, maxY - minY) * 0.35);
    switch (*direction) {
    case safecrowd::domain::StairEntryDirection::North: {
        const auto x = (minX + maxX) * 0.5;
        return safecrowd::domain::LineSegment2D{{.x = x - halfWidth, .y = maxY}, {.x = x + halfWidth, .y = maxY}};
    }
    case safecrowd::domain::StairEntryDirection::South: {
        const auto x = (minX + maxX) * 0.5;
        return safecrowd::domain::LineSegment2D{{.x = x - halfWidth, .y = minY}, {.x = x + halfWidth, .y = minY}};
    }
    case safecrowd::domain::StairEntryDirection::East: {
        const auto y = (minY + maxY) * 0.5;
        return safecrowd::domain::LineSegment2D{{.x = maxX, .y = y - halfWidth}, {.x = maxX, .y = y + halfWidth}};
    }
    case safecrowd::domain::StairEntryDirection::West: {
        const auto y = (minY + maxY) * 0.5;
        return safecrowd::domain::LineSegment2D{{.x = minX, .y = y - halfWidth}, {.x = minX, .y = y + halfWidth}};
    }
    case safecrowd::domain::StairEntryDirection::Unspecified:
        break;
    }
    return std::nullopt;
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
    for (const auto& connection : layout.connections) {
        const auto entrySpan = stairEntrySpanForFloor(layout, connection, floorId);
        if (!entrySpan.has_value()) {
            continue;
        }
        vertices.push_back(entrySpan->start);
        vertices.push_back(entrySpan->end);
        vertices.push_back({
            .x = (entrySpan->start.x + entrySpan->end.x) * 0.5,
            .y = (entrySpan->start.y + entrySpan->end.y) * 0.5,
        });
        edges.push_back(*entrySpan);
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
