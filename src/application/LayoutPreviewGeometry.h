#pragma once

#include <optional>
#include <utility>
#include <vector>

#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QStringList>

#include "application/LayoutCanvasRendering.h"
#include "domain/FacilityLayout2D.h"

namespace safecrowd::application {

struct UShapedStairGeometry {
    safecrowd::domain::Polygon2D sourceFootprint{};
    safecrowd::domain::Polygon2D targetFootprint{};
    safecrowd::domain::LineSegment2D sourceEntrySpan{};
    safecrowd::domain::LineSegment2D targetEntrySpan{};
    safecrowd::domain::LineSegment2D verticalSpan{};
    safecrowd::domain::Point2D sourceOutsideSample{};
    safecrowd::domain::Point2D targetOutsideSample{};
    double laneWidth{0.0};
};

struct DoorNeighbors {
    std::vector<std::size_t> firstSide{};
    std::vector<std::size_t> secondSide{};
    QPointF firstSample{};
    QPointF secondSample{};
};

struct DoorSpanCorrectionResult {
    safecrowd::domain::LineSegment2D span{};
    QString barrierId{};
};

bool matchesFloor(const std::string& elementFloorId, const QString& floorId);
QString defaultFloorId(const safecrowd::domain::FacilityLayout2D& layout);
QString nextFloorId(const safecrowd::domain::FacilityLayout2D& layout);
void ensureLayoutFloors(safecrowd::domain::FacilityLayout2D& layout);
void includePolygon(LayoutCanvasBounds& bounds, const safecrowd::domain::Polygon2D& polygon);
LayoutCanvasBounds polygonBounds(const safecrowd::domain::Polygon2D& polygon);
bool hasConnectionPair(const safecrowd::domain::FacilityLayout2D& layout, const QString& fromZoneId, const QString& toZoneId);
bool hasConnectionPairAtSpan(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& fromZoneId,
    const QString& toZoneId,
    const safecrowd::domain::Point2D& spanStart,
    const safecrowd::domain::Point2D& spanEnd);
std::vector<std::size_t> mergeableDoorConnectionIndices(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& floorId,
    safecrowd::domain::ConnectionKind kind,
    const QString& fromZoneId,
    const QString& toZoneId,
    const safecrowd::domain::LineSegment2D& span);
std::optional<QString> mergeDoorConnectionSpan(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& floorId,
    safecrowd::domain::ConnectionKind kind,
    const QString& fromZoneId,
    const QString& toZoneId,
    const safecrowd::domain::LineSegment2D& span);
void retargetControls(safecrowd::domain::FacilityLayout2D& layout, const QString& oldTargetId, const QString& newTargetId);
void retargetConnectionsFromZone(safecrowd::domain::FacilityLayout2D& layout, const QString& oldZoneId, const QString& newZoneId);
bool isVerticalLink(const safecrowd::domain::Connection2D& connection);
bool isVerticalZone(const safecrowd::domain::Zone2D& zone);
const safecrowd::domain::Zone2D* findZoneById(const safecrowd::domain::FacilityLayout2D& layout, const std::string& zoneId);
std::optional<std::size_t> findZoneIndexById(const safecrowd::domain::FacilityLayout2D& layout, const std::string& zoneId);
double floorElevation(const safecrowd::domain::FacilityLayout2D& layout, const std::string& floorId);
std::optional<safecrowd::domain::StairEntryDirection> stairEntryDirectionForFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId);
std::optional<safecrowd::domain::LineSegment2D> stairEntrySpanForFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId);
std::optional<QPointF> stairEntryOutsideSampleForFloor(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Connection2D& connection,
    const std::string& floorId);
QString nextConnectionId(const safecrowd::domain::FacilityLayout2D& layout);
QString nextZoneId(const safecrowd::domain::FacilityLayout2D& layout);
QString nextBarrierId(const safecrowd::domain::FacilityLayout2D& layout, const QString& prefix);
QString nextWallId(const safecrowd::domain::FacilityLayout2D& layout);
QString nextObstructionId(const safecrowd::domain::FacilityLayout2D& layout);
QString nextVerticalConnectionId(const safecrowd::domain::FacilityLayout2D& layout);
bool pointInRing(const std::vector<safecrowd::domain::Point2D>& ring, const QPointF& point);
bool pointInPolygon(const safecrowd::domain::Polygon2D& polygon, const QPointF& point);
double distanceToSegment(const QPointF& point, const QPointF& start, const QPointF& end);
double distanceToLineSegmentWorld(const QPointF& point, const safecrowd::domain::Point2D& start, const safecrowd::domain::Point2D& end);
double distanceToPolygonBoundary(const safecrowd::domain::Polygon2D& polygon, const QPointF& point);
QRectF rectFromWorldPoints(const QPointF& startWorld, const QPointF& endWorld);
QPainterPath worldPolygonPath(const safecrowd::domain::Polygon2D& polygon);
double polygonArea(const QPolygonF& polygon);
double painterFillArea(const QPainterPath& path);
std::vector<safecrowd::domain::Polygon2D> polygonsFromFillPath(const QPainterPath& path);
bool nearlyEqual(double a, double b, double epsilon = 1e-4);
void appendBarrierSegment(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end,
    const std::string& floorId);
void appendAutoWallsForPolygon(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Polygon2D& polygon,
    const std::string& floorId);
void normalizeOpenWallBarriers(safecrowd::domain::FacilityLayout2D& layout, const QStringList& preferredIds = {});
safecrowd::domain::LineSegment2D centerSpanForRectangle(const QRectF& rectangle);
safecrowd::domain::LineSegment2D verticalConnectionSpanForRectangle(
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection direction,
    double effectiveWidth);
safecrowd::domain::LineSegment2D entrySpanForRectangle(
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection direction);
QPointF entrySideMidpoint(const QRectF& rectangle, safecrowd::domain::StairEntryDirection direction);
QPointF entryOutsideSample(const QRectF& rectangle, safecrowd::domain::StairEntryDirection direction);
safecrowd::domain::StairEntryDirection oppositeStairEntryDirection(safecrowd::domain::StairEntryDirection direction);
bool pointNearSegmentWorld(
    const QPointF& point,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end,
    double tolerance = 0.2);
std::optional<QPointF> projectOntoSegment(
    const QPointF& point,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end);
std::vector<std::size_t> uniqueZoneMerge(const std::vector<std::size_t>& first, const std::vector<std::size_t>& second);
std::vector<std::size_t> zonesTouchingSegment(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end,
    const QString& floorId);
std::vector<std::size_t> zonesNearPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& point,
    const QString& floorId);
std::vector<std::size_t> zonesContainingPoint(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QPointF& point,
    const QString& floorId,
    double tolerance = 0.0);
std::optional<std::size_t> choosePrimaryZone(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<std::size_t>& candidates,
    std::optional<std::size_t> exclude = std::nullopt);
UShapedStairGeometry uShapedStairGeometryForRectangle(
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection entryDirection);
DoorNeighbors doorNeighborsAcrossSegment(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end,
    const QString& floorId);
std::optional<safecrowd::domain::Polygon2D> largestPolygonFromPath(const QPainterPath& path);
bool ringIsAxisAligned(const std::vector<safecrowd::domain::Point2D>& ring);
bool polygonIsAxisAligned(const safecrowd::domain::Polygon2D& polygon);
safecrowd::domain::Polygon2D rectanglePolygonForBounds(const LayoutCanvasBounds& bounds);
std::optional<QString> mergeExitZonePolygon(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Polygon2D& candidatePolygon,
    const QPainterPath& candidatePath,
    const std::string& floorId);
QString createExitZoneAtDoor(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Point2D& gapStart,
    const safecrowd::domain::Point2D& gapEnd,
    const QPointF& outsideDirection,
    const std::string& floorId);
void replaceBarrierWithSegments(
    safecrowd::domain::FacilityLayout2D& layout,
    std::size_t barrierIndex,
    const std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>>& segments,
    const std::string& floorId);
bool intervalContainsSpan(double sourceStart, double sourceEnd, double spanStart, double spanEnd);
std::optional<std::size_t> barrierIndexCoveringSpan(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::LineSegment2D& span,
    const QString& floorId);
std::optional<std::vector<std::pair<safecrowd::domain::Point2D, safecrowd::domain::Point2D>>> barrierSegmentsAfterGap(
    const safecrowd::domain::Barrier2D& barrier,
    const safecrowd::domain::LineSegment2D& gap);
bool spanOverlapsPolygonBoundary(
    const safecrowd::domain::Polygon2D& polygon,
    const safecrowd::domain::LineSegment2D& span);
std::optional<QPointF> outsideSampleForBoundarySpan(
    const safecrowd::domain::Polygon2D& polygon,
    const safecrowd::domain::LineSegment2D& span);
void cutBarriersAtSpan(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::LineSegment2D& span,
    const std::string& floorId);
QStringList appendWallsForPolygonExceptGaps(
    safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::Polygon2D& polygon,
    const std::vector<safecrowd::domain::LineSegment2D>& gaps,
    const std::string& floorId);
void appendStairWallsExceptEntry(
    safecrowd::domain::FacilityLayout2D& layout,
    const QRectF& rectangle,
    safecrowd::domain::StairEntryDirection entryDirection,
    const std::string& floorId);
void autoConnectRoomToStairEntries(
    safecrowd::domain::FacilityLayout2D& layout,
    const QString& roomZoneId,
    const QString& floorId);
std::optional<DoorSpanCorrectionResult> correctedDoorSpanForPlacement(
    const safecrowd::domain::FacilityLayout2D& layout,
    const QString& floorId,
    const safecrowd::domain::LineSegment2D& rawSpan);

}  // namespace safecrowd::application
