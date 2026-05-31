#include "application/SimulationCanvasWidget.h"

#include "application/UiStyle.h"
#include "domain/AgentComponents.h"
#include "domain/GeometryQueries.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>

#include <QCoreApplication>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRect>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QToolTip>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

constexpr double kViewportPadding = 32.0;
constexpr double kDefaultHotspotCellSize = 1.5;
constexpr double kHotspotFocusZoom = 2.8;
constexpr double kBottleneckFocusZoom = 2.4;
constexpr double kCrossFlowCellFocusZoom = 2.8;
constexpr double kDefaultDensityScaleMaxPeoplePerSquareMeter = 4.0;
constexpr double kPressureInfluenceRadiusMultiplier = 1.45;
constexpr double kPressureMinimumScreenRadius = 12.0;
constexpr double kDefaultPressureScaleMaxScore = 1.0;
constexpr double kCrossFlowInfluenceRadiusMultiplier = 1.3;
constexpr double kCrossFlowMinimumScreenRadius = 16.0;
constexpr int kHotspotMinCoreAlpha = 72;
constexpr int kHotspotMaxCoreAlpha = 190;
constexpr int kFloorSelectorMargin = 14;
constexpr double kHeatmapScreenRasterScale = 0.5;
constexpr double kHeatmapBarrierIndexCellSizeMeters = 2.0;
constexpr double kHeatmapMinWorldPixelsPerMeter = 1.0;
constexpr double kHeatmapMaxWorldPixelsPerMeter = 16.0;
constexpr int kHeatmapMaxWorldRasterDimension = 4096;
const QColor kMovingAgentColor("#1f5fae");
const QColor kStalledAgentColor("#7c3aed");

enum class TimelineVisualState {
    Future,
    Active,
    Expired,
};

using safecrowd::domain::matchesFloor;

struct HeatmapRasterContribution {
    safecrowd::domain::Point2D center{};
    QRect rect{};
};

struct HeatmapWorldSource {
    safecrowd::domain::Point2D center{};
    safecrowd::domain::Point2D cellMin{};
    safecrowd::domain::Point2D cellMax{};
    double intensity{0.0};
};

struct HeatmapBarrierSegment {
    safecrowd::domain::Point2D start{};
    safecrowd::domain::Point2D end{};
};

struct HeatmapClosedBarrier {
    std::vector<safecrowd::domain::Point2D> vertices{};
};

struct HeatmapBarrierSpatialIndex {
    double cellSize{kHeatmapBarrierIndexCellSizeMeters};
    std::vector<HeatmapBarrierSegment> segments{};
    std::vector<HeatmapClosedBarrier> closedBarriers{};
    std::unordered_map<long long, std::vector<std::size_t>> segmentIndicesByCell{};
    std::vector<std::uint32_t> visitMarks{};
    std::uint32_t visitToken{1};
};

struct GeneratedHeatmapWorldCache {
    QImage image{};
    LayoutCanvasBounds bounds{};
};

QString hazardKindLabel(safecrowd::domain::EnvironmentHazardKind kind) {
    switch (kind) {
    case safecrowd::domain::EnvironmentHazardKind::Smoke:
        return "Smoke";
    case safecrowd::domain::EnvironmentHazardKind::Fire:
    default:
        return "Fire";
    }
}

QString severityLabel(safecrowd::domain::ScenarioElementSeverity severity) {
    switch (severity) {
    case safecrowd::domain::ScenarioElementSeverity::Low:
        return "Low";
    case safecrowd::domain::ScenarioElementSeverity::High:
        return "High";
    case safecrowd::domain::ScenarioElementSeverity::Medium:
    default:
        return "Medium";
    }
}

QString formatEnvironmentHazardTooltip(const safecrowd::domain::EnvironmentHazardDraft& hazard) {
    QString text = QString("%1 hazard").arg(hazardKindLabel(hazard.kind));
    if (!hazard.name.empty()) {
        text.append(QString("\n%1").arg(QString::fromStdString(hazard.name)));
    }
    const auto start = std::max(0.0, hazard.startSeconds);
    if (safecrowd::domain::environmentHazardHasOpenEndedSchedule(hazard)) {
        text.append(QString("\nActive: %1s ~ open").arg(start, 0, 'f', 1));
    } else {
        text.append(QString("\nActive: %1s ~ %2s")
            .arg(start, 0, 'f', 1)
            .arg(std::max(start, hazard.endSeconds), 0, 'f', 1));
    }
    text.append(QString("\nSeverity: %1").arg(severityLabel(hazard.severity)));
    text.append(QString("\nInfluence radius: %1m")
        .arg(safecrowd::domain::environmentHazardRadiusMeters(hazard), 0, 'f', 1));
    text.append(QStringLiteral("\nDetection varies by agent sensitivity"));
    return text;
}

QString visualStateLabel(TimelineVisualState state) {
    switch (state) {
    case TimelineVisualState::Future:
        return QStringLiteral("Future");
    case TimelineVisualState::Expired:
        return QStringLiteral("Expired");
    case TimelineVisualState::Active:
    default:
        return QStringLiteral("Active");
    }
}

std::optional<TimelineVisualState> environmentHazardVisualState(
    const safecrowd::domain::EnvironmentHazardDraft& hazard,
    double elapsedSeconds) {
    if (safecrowd::domain::environmentHazardActiveAt(hazard, elapsedSeconds)) {
        return TimelineVisualState::Active;
    }
    const auto start = std::max(0.0, hazard.startSeconds);
    if (elapsedSeconds + 1e-9 < start) {
        return TimelineVisualState::Future;
    }
    return TimelineVisualState::Expired;
}

QString formatEnvironmentHazardTooltip(
    const safecrowd::domain::EnvironmentHazardDraft& hazard,
    TimelineVisualState state) {
    auto text = formatEnvironmentHazardTooltip(hazard);
    text.append(QString("\nState: %1").arg(visualStateLabel(state)));
    return text;
}

safecrowd::domain::Point2D connectionCenter(const safecrowd::domain::Connection2D& connection) {
    return {
        .x = (connection.centerSpan.start.x + connection.centerSpan.end.x) * 0.5,
        .y = (connection.centerSpan.start.y + connection.centerSpan.end.y) * 0.5,
    };
}

using safecrowd::domain::polygonCenter;

bool hasExplicitGuidanceInstallPosition(const safecrowd::domain::RouteGuidanceDraft& guidance) {
    return !guidance.installFloorId.empty() || !guidance.installZoneId.empty();
}

QColor densityHeatmapColor(double ratio, int alpha) {
    const auto t = std::clamp(ratio, 0.0, 1.0);
    if (t < 0.22) {
        return QColor(29, 78, 216, alpha);
    }
    if (t < 0.45) {
        return QColor(6, 182, 212, alpha);
    }
    if (t < 0.65) {
        return QColor(34, 197, 94, alpha);
    }
    if (t < 0.82) {
        return QColor(250, 204, 21, alpha);
    }
    if (t < 1.0) {
        return QColor(249, 115, 22, alpha);
    }
    return QColor(220, 38, 38, alpha);
}

QColor interpolateColor(const QColor& from, const QColor& to, double ratio, int alpha) {
    const auto t = std::clamp(ratio, 0.0, 1.0);
    return QColor(
        static_cast<int>(std::lround(from.red() + ((to.red() - from.red()) * t))),
        static_cast<int>(std::lround(from.green() + ((to.green() - from.green()) * t))),
        static_cast<int>(std::lround(from.blue() + ((to.blue() - from.blue()) * t))),
        alpha);
}

QColor occupancyHeatmapColor(double ratio, int alpha) {
    const auto t = std::clamp(ratio, 0.0, 1.0);
    if (t < 0.24) {
        return interpolateColor(QColor("#1d4ed8"), QColor("#06b6d4"), t / 0.24, alpha);
    }
    if (t < 0.48) {
        return interpolateColor(QColor("#06b6d4"), QColor("#22c55e"), (t - 0.24) / 0.24, alpha);
    }
    if (t < 0.70) {
        return interpolateColor(QColor("#22c55e"), QColor("#facc15"), (t - 0.48) / 0.22, alpha);
    }
    if (t < 0.86) {
        return interpolateColor(QColor("#facc15"), QColor("#f97316"), (t - 0.70) / 0.16, alpha);
    }
    return interpolateColor(QColor("#f97316"), QColor("#dc2626"), (t - 0.86) / 0.14, alpha);
}

std::vector<double> boxBlurField(const std::vector<double>& source, int width, int height, int radius) {
    if (source.empty() || width <= 0 || height <= 0 || radius <= 0) {
        return source;
    }

    std::vector<double> horizontal(source.size(), 0.0);
    std::vector<double> blurred(source.size(), 0.0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            double sum = 0.0;
            int count = 0;
            const auto left = std::max(0, x - radius);
            const auto right = std::min(width - 1, x + radius);
            for (int sampleX = left; sampleX <= right; ++sampleX) {
                sum += source[static_cast<std::size_t>(y * width + sampleX)];
                ++count;
            }
            horizontal[static_cast<std::size_t>(y * width + x)] = count == 0 ? 0.0 : sum / static_cast<double>(count);
        }
    }

    for (int y = 0; y < height; ++y) {
        const auto top = std::max(0, y - radius);
        const auto bottom = std::min(height - 1, y + radius);
        for (int x = 0; x < width; ++x) {
            double sum = 0.0;
            int count = 0;
            for (int sampleY = top; sampleY <= bottom; ++sampleY) {
                sum += horizontal[static_cast<std::size_t>(sampleY * width + x)];
                ++count;
            }
            blurred[static_cast<std::size_t>(y * width + x)] = count == 0 ? 0.0 : sum / static_cast<double>(count);
        }
    }
    return blurred;
}

double heatmapScreenPixelsPerMeter(const LayoutCanvasTransform& transform) {
    const safecrowd::domain::Point2D origin{.x = 0.0, .y = 0.0};
    const safecrowd::domain::Point2D unitX{.x = 1.0, .y = 0.0};
    return std::abs(transform.map(unitX).x() - transform.map(origin).x());
}

double heatmapPixelsPerMeterKey(const LayoutCanvasTransform& transform) {
    auto requested = heatmapScreenPixelsPerMeter(transform) * kHeatmapScreenRasterScale;
    if (!std::isfinite(requested) || requested <= 0.0) {
        requested = kHeatmapMinWorldPixelsPerMeter;
    }
    requested = std::clamp(
        requested,
        kHeatmapMinWorldPixelsPerMeter,
        kHeatmapMaxWorldPixelsPerMeter);

    double bucket = kHeatmapMinWorldPixelsPerMeter;
    while (bucket < requested && bucket < kHeatmapMaxWorldPixelsPerMeter) {
        bucket *= 2.0;
    }
    return std::min(bucket, kHeatmapMaxWorldPixelsPerMeter);
}

void includeHeatmapWorldSource(LayoutCanvasBounds& bounds, const HeatmapWorldSource& source) {
    includeLayoutCanvasPoint(bounds, source.cellMin);
    includeLayoutCanvasPoint(bounds, source.cellMax);
}

LayoutCanvasBounds expandedHeatmapBounds(
    const LayoutCanvasBounds& bounds,
    double paddingMeters) {
    return {
        .minX = bounds.minX - paddingMeters,
        .minY = bounds.minY - paddingMeters,
        .maxX = bounds.maxX + paddingMeters,
        .maxY = bounds.maxY + paddingMeters,
    };
}

int heatmapRasterX(double worldX, const LayoutCanvasBounds& bounds, double pixelsPerMeter) {
    return static_cast<int>(std::floor((worldX - bounds.minX) * pixelsPerMeter));
}

int heatmapRasterY(double worldY, const LayoutCanvasBounds& bounds, double pixelsPerMeter) {
    return static_cast<int>(std::floor((bounds.maxY - worldY) * pixelsPerMeter));
}

safecrowd::domain::Point2D heatmapWorldPointForRasterPixel(
    int x,
    int y,
    const LayoutCanvasBounds& bounds,
    double pixelsPerMeter) {
    return {
        .x = bounds.minX + ((static_cast<double>(x) + 0.5) / pixelsPerMeter),
        .y = bounds.maxY - ((static_cast<double>(y) + 0.5) / pixelsPerMeter),
    };
}

void appendBarrierSegmentToIndex(
    HeatmapBarrierSpatialIndex& index,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end) {
    const auto segmentIndex = index.segments.size();
    index.segments.push_back({.start = start, .end = end});

    const safecrowd::domain::Point2D minPoint{
        .x = std::min(start.x, end.x),
        .y = std::min(start.y, end.y),
    };
    const safecrowd::domain::Point2D maxPoint{
        .x = std::max(start.x, end.x),
        .y = std::max(start.y, end.y),
    };
    const auto minCell = safecrowd::domain::spatialCellFor(minPoint, index.cellSize);
    const auto maxCell = safecrowd::domain::spatialCellFor(maxPoint, index.cellSize);
    for (int y = minCell.y; y <= maxCell.y; ++y) {
        for (int x = minCell.x; x <= maxCell.x; ++x) {
            index.segmentIndicesByCell[safecrowd::domain::spatialKey({.x = x, .y = y})].push_back(segmentIndex);
        }
    }
}

HeatmapBarrierSpatialIndex buildHeatmapBarrierSpatialIndex(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& floorId) {
    HeatmapBarrierSpatialIndex index;
    for (const auto& barrier : layout.barriers) {
        if (!matchesFloor(barrier.floorId, floorId)) {
            continue;
        }
        if (!barrier.blocksMovement || barrier.geometry.vertices.size() < 2) {
            continue;
        }

        const auto& vertices = barrier.geometry.vertices;
        for (std::size_t vertexIndex = 0; vertexIndex + 1 < vertices.size(); ++vertexIndex) {
            appendBarrierSegmentToIndex(index, vertices[vertexIndex], vertices[vertexIndex + 1]);
        }
        if (barrier.geometry.closed) {
            appendBarrierSegmentToIndex(index, vertices.back(), vertices.front());
            if (vertices.size() >= 3) {
                index.closedBarriers.push_back({.vertices = vertices});
            }
        }
    }
    index.visitMarks.assign(index.segments.size(), 0);
    return index;
}

void advanceHeatmapBarrierVisitToken(HeatmapBarrierSpatialIndex& index) {
    if (index.visitToken == std::numeric_limits<std::uint32_t>::max()) {
        std::fill(index.visitMarks.begin(), index.visitMarks.end(), 0);
        index.visitToken = 1;
        return;
    }
    ++index.visitToken;
}

bool segmentCrossesIndexedMovementBarrier(
    HeatmapBarrierSpatialIndex& index,
    const safecrowd::domain::Point2D& from,
    const safecrowd::domain::Point2D& to) {
    if (std::hypot(to.x - from.x, to.y - from.y) <= 1e-9) {
        return false;
    }

    for (const auto& barrier : index.closedBarriers) {
        if (safecrowd::domain::pointInRing(barrier.vertices, to)) {
            return true;
        }
    }
    if (index.segments.empty()) {
        return false;
    }

    advanceHeatmapBarrierVisitToken(index);
    const safecrowd::domain::Point2D minPoint{
        .x = std::min(from.x, to.x),
        .y = std::min(from.y, to.y),
    };
    const safecrowd::domain::Point2D maxPoint{
        .x = std::max(from.x, to.x),
        .y = std::max(from.y, to.y),
    };
    const auto minCell = safecrowd::domain::spatialCellFor(minPoint, index.cellSize);
    const auto maxCell = safecrowd::domain::spatialCellFor(maxPoint, index.cellSize);
    for (int y = minCell.y; y <= maxCell.y; ++y) {
        for (int x = minCell.x; x <= maxCell.x; ++x) {
            const auto bucketIt = index.segmentIndicesByCell.find(
                safecrowd::domain::spatialKey({.x = x, .y = y}));
            if (bucketIt == index.segmentIndicesByCell.end()) {
                continue;
            }
            for (const auto segmentIndex : bucketIt->second) {
                if (segmentIndex >= index.segments.size()) {
                    continue;
                }
                if (index.visitMarks[segmentIndex] == index.visitToken) {
                    continue;
                }
                index.visitMarks[segmentIndex] = index.visitToken;
                const auto& segment = index.segments[segmentIndex];
                if (safecrowd::domain::lineSegmentsIntersect(from, to, segment.start, segment.end)) {
                    return true;
                }
            }
        }
    }
    return false;
}

void markBarrierAwareHeatmapVisibility(
    std::vector<unsigned char>& visibility,
    int width,
    int height,
    const LayoutCanvasBounds& bounds,
    HeatmapBarrierSpatialIndex& barrierIndex,
    const std::vector<HeatmapRasterContribution>& contributions,
    double pixelsPerMeter,
    int radius) {
    if (visibility.empty() || width <= 0 || height <= 0 || pixelsPerMeter <= 0.0) {
        return;
    }

    const auto effectiveRadius = std::max(0, radius);
    for (const auto& contribution : contributions) {
        const auto x0 = std::clamp(contribution.rect.left() - effectiveRadius, 0, width - 1);
        const auto x1 = std::clamp(contribution.rect.right() + effectiveRadius, x0, width - 1);
        const auto y0 = std::clamp(contribution.rect.top() - effectiveRadius, 0, height - 1);
        const auto y1 = std::clamp(contribution.rect.bottom() + effectiveRadius, y0, height - 1);
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const auto samplePoint = heatmapWorldPointForRasterPixel(x, y, bounds, pixelsPerMeter);
                if (segmentCrossesIndexedMovementBarrier(barrierIndex, contribution.center, samplePoint)) {
                    continue;
                }
                visibility[static_cast<std::size_t>(y * width + x)] = 1;
            }
        }
    }
}

std::optional<GeneratedHeatmapWorldCache> buildHeatmapWorldCacheImage(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& floorId,
    const std::vector<HeatmapWorldSource>& sources,
    double pixelsPerMeterKey,
    ResultOverlayMode mode) {
    if (sources.empty() || pixelsPerMeterKey <= 0.0) {
        return std::nullopt;
    }

    LayoutCanvasBounds baseBounds;
    double maxCellWorldWidth = 0.0;
    for (const auto& source : sources) {
        includeHeatmapWorldSource(baseBounds, source);
        maxCellWorldWidth = std::max(
            maxCellWorldWidth,
            std::max(
                std::abs(source.cellMax.x - source.cellMin.x),
                std::abs(source.cellMax.y - source.cellMin.y)));
    }
    if (!baseBounds.valid()) {
        return std::nullopt;
    }

    const auto maxBlurRadius = mode == ResultOverlayMode::Density ? 10 : 5;
    auto pixelsPerMeter = pixelsPerMeterKey;
    LayoutCanvasBounds imageBounds;
    int blurRadius = 1;
    int visibilityRadius = 2;
    int rasterWidth = 0;
    int rasterHeight = 0;
    for (int attempt = 0; attempt < 3; ++attempt) {
        blurRadius = std::clamp(
            static_cast<int>(std::ceil(std::max(1.0, maxCellWorldWidth * pixelsPerMeter * 0.5))),
            1,
            maxBlurRadius);
        visibilityRadius = blurRadius + std::max(1, blurRadius / 2);
        imageBounds = expandedHeatmapBounds(baseBounds, static_cast<double>(visibilityRadius) / pixelsPerMeter);
        rasterWidth = std::max(1, static_cast<int>(std::ceil((imageBounds.maxX - imageBounds.minX) * pixelsPerMeter)));
        rasterHeight = std::max(1, static_cast<int>(std::ceil((imageBounds.maxY - imageBounds.minY) * pixelsPerMeter)));
        if (rasterWidth <= kHeatmapMaxWorldRasterDimension
            && rasterHeight <= kHeatmapMaxWorldRasterDimension) {
            break;
        }

        const auto widthScale = static_cast<double>(kHeatmapMaxWorldRasterDimension) / static_cast<double>(rasterWidth);
        const auto heightScale = static_cast<double>(kHeatmapMaxWorldRasterDimension) / static_cast<double>(rasterHeight);
        pixelsPerMeter = std::max(
            kHeatmapMinWorldPixelsPerMeter,
            pixelsPerMeter * std::min(widthScale, heightScale));
    }

    if (rasterWidth <= 0
        || rasterHeight <= 0
        || rasterWidth > kHeatmapMaxWorldRasterDimension
        || rasterHeight > kHeatmapMaxWorldRasterDimension) {
        return std::nullopt;
    }

    std::vector<double> field(static_cast<std::size_t>(rasterWidth * rasterHeight), 0.0);
    std::vector<unsigned char> visibility(field.size(), 0);
    std::vector<HeatmapRasterContribution> contributions;
    contributions.reserve(sources.size());
    auto barrierIndex = buildHeatmapBarrierSpatialIndex(layout, floorId);

    for (const auto& source : sources) {
        const auto left = std::min(source.cellMin.x, source.cellMax.x);
        const auto right = std::max(source.cellMin.x, source.cellMax.x);
        const auto bottom = std::min(source.cellMin.y, source.cellMax.y);
        const auto top = std::max(source.cellMin.y, source.cellMax.y);
        const auto x0 = std::clamp(heatmapRasterX(left, imageBounds, pixelsPerMeter), 0, rasterWidth - 1);
        const auto x1 = std::clamp(
            static_cast<int>(std::ceil((right - imageBounds.minX) * pixelsPerMeter)),
            x0,
            rasterWidth - 1);
        const auto y0 = std::clamp(heatmapRasterY(top, imageBounds, pixelsPerMeter), 0, rasterHeight - 1);
        const auto y1 = std::clamp(
            static_cast<int>(std::ceil((imageBounds.maxY - bottom) * pixelsPerMeter)),
            y0,
            rasterHeight - 1);

        bool wroteContribution = false;
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                const auto samplePoint = heatmapWorldPointForRasterPixel(x, y, imageBounds, pixelsPerMeter);
                if (segmentCrossesIndexedMovementBarrier(barrierIndex, source.center, samplePoint)) {
                    continue;
                }
                const auto index = static_cast<std::size_t>(y * rasterWidth + x);
                field[index] = std::max(field[index], source.intensity);
                wroteContribution = true;
            }
        }
        if (wroteContribution) {
            contributions.push_back({
                .center = source.center,
                .rect = QRect(x0, y0, x1 - x0 + 1, y1 - y0 + 1),
            });
        }
    }

    markBarrierAwareHeatmapVisibility(
        visibility,
        rasterWidth,
        rasterHeight,
        imageBounds,
        barrierIndex,
        contributions,
        pixelsPerMeter,
        visibilityRadius);

    auto blurred = boxBlurField(field, rasterWidth, rasterHeight, blurRadius);
    blurred = boxBlurField(blurred, rasterWidth, rasterHeight, std::max(1, blurRadius / 2));
    if (*std::max_element(blurred.begin(), blurred.end()) <= 1e-9) {
        return std::nullopt;
    }

    QImage image(rasterWidth, rasterHeight, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    for (int y = 0; y < rasterHeight; ++y) {
        for (int x = 0; x < rasterWidth; ++x) {
            const auto index = static_cast<std::size_t>(y * rasterWidth + x);
            if (visibility[index] == 0) {
                continue;
            }
            const auto value = blurred[index];
            if (value <= 0.002) {
                continue;
            }

            if (mode == ResultOverlayMode::Density) {
                const auto intensity = std::clamp(std::pow(value, 0.82), 0.0, 1.0);
                const auto alpha = std::clamp(
                    52 + static_cast<int>(178.0 * std::sqrt(intensity)),
                    0,
                    230);
                image.setPixelColor(x, y, densityHeatmapColor(intensity, alpha));
            } else {
                const auto intensity = std::clamp(std::pow(value, 0.70), 0.0, 1.0);
                const auto alpha = std::clamp(
                    44 + static_cast<int>(199.0 * std::sqrt(intensity)),
                    0,
                    243);
                image.setPixelColor(x, y, occupancyHeatmapColor(intensity, alpha));
            }
        }
    }

    return GeneratedHeatmapWorldCache{
        .image = std::move(image),
        .bounds = imageBounds,
    };
}

void drawHeatmapWorldCacheImage(
    QPainter& painter,
    const LayoutCanvasTransform& transform,
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& floorId,
    const QImage& image,
    const LayoutCanvasBounds& bounds) {
    if (image.isNull() || !bounds.valid()) {
        return;
    }

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    QPainterPath walkableClip;
    for (const auto& zone : layout.zones) {
        if (!matchesFloor(zone.floorId, floorId)) {
            continue;
        }
        walkableClip.addPath(layoutCanvasPolygonPath(zone.area, transform));
    }
    if (!walkableClip.isEmpty()) {
        painter.setClipPath(walkableClip);
    }

    const safecrowd::domain::Point2D topLeftWorld{.x = bounds.minX, .y = bounds.maxY};
    const safecrowd::domain::Point2D bottomRightWorld{.x = bounds.maxX, .y = bounds.minY};
    const auto topLeft = transform.map(topLeftWorld);
    const auto bottomRight = transform.map(bottomRightWorld);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRectF(topLeft, bottomRight), image);
    painter.restore();
}

QColor pressureHeatmapColor(double ratio, int alpha) {
    const auto t = std::clamp(ratio, 0.0, 1.0);
    if (t < 0.25) {
        return QColor(250, 204, 21, alpha);
    }
    if (t < 0.55) {
        return QColor(249, 115, 22, alpha);
    }
    if (t < 0.8) {
        return QColor(239, 68, 68, alpha);
    }
    return QColor(153, 27, 27, alpha);
}

QColor crossFlowHeatmapColor(double ratio, int alpha) {
    const auto t = std::clamp(ratio, 0.0, 1.0);
    if (t < 0.3) {
        return QColor(245, 158, 11, alpha);
    }
    if (t < 0.65) {
        return QColor(249, 115, 22, alpha);
    }
    return QColor(220, 38, 38, alpha);
}

void drawArrowHead(QPainter& painter, const QPointF& tip, const QPointF& tail, const QColor& color, double size) {
    const auto line = QLineF(tail, tip);
    if (line.length() <= 1e-6) {
        return;
    }
    const auto angle = std::atan2(line.dy(), line.dx());
    const QPointF left(
        tip.x() - (std::cos(angle - 0.45) * size),
        tip.y() - (std::sin(angle - 0.45) * size));
    const QPointF right(
        tip.x() - (std::cos(angle + 0.45) * size),
        tip.y() - (std::sin(angle + 0.45) * size));
    painter.save();
    painter.setPen(QPen(color, std::max(1.8, size * 0.32), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(tip, left);
    painter.drawLine(tip, right);
    painter.restore();
}

QString formatScheduleTooltip(const safecrowd::domain::ConnectionBlockDraft& block) {
    if (block.connectionId.empty()) {
        return {};
    }

    QString text = QStringLiteral("Block schedule");
    if (block.intervals.empty()) {
        text.append("\n- Always");
        return text;
    }

    for (const auto& interval : block.intervals) {
        const auto start = std::max(0.0, interval.startSeconds);
        if (interval.endSeconds <= interval.startSeconds) {
            text.append(QString("\n- %1s ~ open").arg(start, 0, 'f', 1));
        } else {
            text.append(QString("\n- %1s ~ %2s").arg(start, 0, 'f', 1).arg(std::max(start, interval.endSeconds), 0, 'f', 1));
        }
    }
    return text;
}

std::optional<TimelineVisualState> connectionBlockVisualState(
    const safecrowd::domain::ConnectionBlockDraft& block,
    double elapsedSeconds) {
    if (block.connectionId.empty()) {
        return std::nullopt;
    }
    if (safecrowd::domain::connectionBlockActiveAt(block, elapsedSeconds)) {
        return TimelineVisualState::Active;
    }
    if (block.intervals.empty()) {
        return TimelineVisualState::Active;
    }
    const auto hasFutureInterval = std::any_of(block.intervals.begin(), block.intervals.end(), [&](const auto& interval) {
        return elapsedSeconds + 1e-9 < std::max(0.0, interval.startSeconds);
    });
    return hasFutureInterval ? TimelineVisualState::Future : TimelineVisualState::Expired;
}

QString formatScheduleTooltip(
    const safecrowd::domain::ConnectionBlockDraft& block,
    TimelineVisualState state) {
    auto text = formatScheduleTooltip(block);
    text.append(QString("\nState: %1").arg(visualStateLabel(state)));
    return text;
}

QString formatRouteGuidanceTooltip(const safecrowd::domain::RouteGuidanceDraft& guidance) {
    QString text = QStringLiteral("Route guidance");
    if (guidance.periods.empty()) {
        text.append(QStringLiteral("\n Always"));
    } else {
        for (const auto& period : guidance.periods) {
            const auto start = std::max(0.0, period.startSeconds);
            const auto end = std::max(start, std::max(0.0, period.endSeconds));
            text.append(QString("\n %1s~%2s").arg(start, 0, 'f', 1).arg(end, 0, 'f', 1));
        }
    }
    if (hasExplicitGuidanceInstallPosition(guidance)) {
        text.append(QString("\n Location: (%1, %2)")
            .arg(guidance.installPosition.x, 0, 'f', 1)
            .arg(guidance.installPosition.y, 0, 'f', 1));
    }
    text.append(QString("\n Base compliance: %1").arg(std::clamp(guidance.baseComplianceRate, 0.0, 1.0), 0, 'f', 2));
    text.append(QString("\n Influence radius: %1m").arg(std::max(0.0, guidance.influenceRadiusMeters), 0, 'f', 1));
    text.append(QString("\n Max detour: %1m").arg(std::max(0.0, guidance.maxDetourMeters), 0, 'f', 1));
    return text;
}

double routeGuidanceInfluenceRadiusPixels(
    const LayoutCanvasTransform& transform,
    const safecrowd::domain::RouteGuidanceDraft& guidance) {
    const auto radiusMeters = std::max(0.0, guidance.influenceRadiusMeters);
    if (!std::isfinite(radiusMeters) || radiusMeters <= 0.0) {
        return 0.0;
    }

    const auto origin = transform.map({.x = 0.0, .y = 0.0});
    const auto radiusPoint = transform.map({.x = radiusMeters, .y = 0.0});
    return QLineF(origin, radiusPoint).length();
}

void drawRouteGuidanceInfluenceRadius(
    QPainter& painter,
    const QPointF& center,
    double radiusPixels) {
    if (!std::isfinite(radiusPixels) || radiusPixels <= 0.0) {
        return;
    }

    painter.save();
    painter.setPen(QPen(QColor(31, 95, 174, 82), 1.4, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(31, 95, 174, 22));
    painter.drawEllipse(center, radiusPixels, radiusPixels);
    painter.restore();
}

std::optional<std::size_t> hoveredConnectionBlockIndex(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    const LayoutCanvasTransform& transform,
    const std::string& currentFloorId,
    double elapsedSeconds,
    const QPointF& screenPosition) {
    constexpr double kHoverRadiusPixels = 14.0;

    std::optional<std::size_t> closestIndex;
    double closestDistanceSq = kHoverRadiusPixels * kHoverRadiusPixels;

    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto& block = blocks[index];
        if (!connectionBlockVisualState(block, elapsedSeconds).has_value()) {
            continue;
        }
        const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
            return connection.id == block.connectionId;
        });
        if (it == layout.connections.end()) {
            continue;
        }
        if (!matchesFloor(it->floorId, currentFloorId)) {
            continue;
        }

        const auto center = transform.map(connectionCenter(*it));
        const auto dx = center.x() - screenPosition.x();
        const auto dy = center.y() - screenPosition.y();
        const auto distanceSq = (dx * dx) + (dy * dy);
        if (distanceSq <= closestDistanceSq) {
            closestDistanceSq = distanceSq;
            closestIndex = index;
        }
    }

    return closestIndex;
}

std::optional<std::size_t> hoveredEnvironmentHazardIndex(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards,
    const LayoutCanvasTransform& transform,
    const std::string& currentFloorId,
    double elapsedSeconds,
    const QPointF& screenPosition) {
    constexpr double kHoverRadiusPixels = 15.0;

    std::optional<std::size_t> closestIndex;
    double closestDistanceSq = kHoverRadiusPixels * kHoverRadiusPixels;
    for (std::size_t index = 0; index < hazards.size(); ++index) {
        const auto& hazard = hazards[index];
        if (!environmentHazardVisualState(hazard, elapsedSeconds).has_value()) {
            continue;
        }
        if (!matchesFloor(safecrowd::domain::environmentHazardFloorId(layout, hazard), currentFloorId)) {
            continue;
        }

        const auto center = transform.map(hazard.position);
        const auto dx = center.x() - screenPosition.x();
        const auto dy = center.y() - screenPosition.y();
        const auto distanceSq = (dx * dx) + (dy * dy);
        if (distanceSq <= closestDistanceSq) {
            closestDistanceSq = distanceSq;
            closestIndex = index;
        }
    }
    return closestIndex;
}

struct ActiveRouteGuidanceSelection {
    std::size_t guidanceIndex{0};
    std::size_t periodIndex{0};
    double startSeconds{0.0};
    double endSeconds{0.0};
};

std::vector<ActiveRouteGuidanceSelection> activeRouteGuidanceSelections(
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    double elapsedSeconds) {
    std::vector<ActiveRouteGuidanceSelection> active;
    for (std::size_t guidanceIndex = 0; guidanceIndex < guidances.size(); ++guidanceIndex) {
        const auto& guidance = guidances[guidanceIndex];
        if (guidance.periods.empty()) {
            const double start = 0.0;
            const double end = 1e18;
            if (elapsedSeconds + 1e-9 < start || elapsedSeconds > end + 1e-9) {
                continue;
            }
            active.push_back(ActiveRouteGuidanceSelection{
                .guidanceIndex = guidanceIndex,
                .periodIndex = 0,
                .startSeconds = start,
                .endSeconds = end,
            });
            continue;
        }

        for (std::size_t periodIndex = 0; periodIndex < guidance.periods.size(); ++periodIndex) {
            const auto& period = guidance.periods[periodIndex];
            const auto start = std::max(0.0, period.startSeconds);
            const auto end = std::max(start, std::max(0.0, period.endSeconds));
            if (elapsedSeconds + 1e-9 < start) {
                continue;
            }
            if (elapsedSeconds > end + 1e-9) {
                continue;
            }
            active.push_back(ActiveRouteGuidanceSelection{
                .guidanceIndex = guidanceIndex,
                .periodIndex = periodIndex,
                .startSeconds = start,
                .endSeconds = end,
            });
        }
    }
    return active;
}

std::optional<QPointF> routeGuidanceMarkerCenter(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::RouteGuidanceDraft& guidance,
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    const LayoutCanvasTransform& transform,
    const std::string& currentFloorId,
    double elapsedSeconds) {
    QPointF center;
    if (hasExplicitGuidanceInstallPosition(guidance)) {
        if (!matchesFloor(guidance.installFloorId, currentFloorId)) {
            return std::nullopt;
        }
        center = transform.map(guidance.installPosition);
    } else if (!guidance.installConnectionId.empty()) {
        const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
            return connection.id == guidance.installConnectionId;
        });
        if (it == layout.connections.end()) {
            return std::nullopt;
        }
        if (!matchesFloor(it->floorId, currentFloorId)) {
            return std::nullopt;
        }
        center = transform.map(connectionCenter(*it));
    } else if (!guidance.guidedExitZoneId.empty()) {
        const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
            return zone.id == guidance.guidedExitZoneId;
        });
        if (it == layout.zones.end() || it->kind != safecrowd::domain::ZoneKind::Exit) {
            return std::nullopt;
        }
        if (!matchesFloor(it->floorId, currentFloorId)) {
            return std::nullopt;
        }
        center = transform.map(polygonCenter(it->area));
    } else {
        return std::nullopt;
    }

    constexpr double kMinSeparationPixels = 28.0;
    constexpr double kStackOffsetPixels = 34.0;

    std::vector<QPointF> blockedCenters;
    blockedCenters.reserve(blocks.size());
    for (const auto& block : blocks) {
        if (!connectionBlockVisualState(block, elapsedSeconds).has_value()) {
            continue;
        }
        const auto connectionIt = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
            return connection.id == block.connectionId;
        });
        if (connectionIt == layout.connections.end()) {
            continue;
        }
        if (!matchesFloor(connectionIt->floorId, currentFloorId)) {
            continue;
        }
        blockedCenters.push_back(transform.map(connectionCenter(*connectionIt)));
    }
    if (blockedCenters.empty()) {
        return center;
    }

    const auto minDistanceToBlocks = [&](const QPointF& candidate) {
        double minDistance = 1e12;
        for (const auto& blocked : blockedCenters) {
            minDistance = std::min(minDistance, QLineF(candidate, blocked).length());
        }
        return minDistance;
    };

    const auto baseMinDistance = minDistanceToBlocks(center);
    if (baseMinDistance >= kMinSeparationPixels) {
        return center;
    }

    const QPointF up = center + QPointF(0.0, -kStackOffsetPixels);
    const QPointF down = center + QPointF(0.0, kStackOffsetPixels);
    const auto upDistance = minDistanceToBlocks(up);
    const auto downDistance = minDistanceToBlocks(down);
    return upDistance >= downDistance ? up : down;
}

std::optional<std::size_t> hoveredActiveRouteGuidanceIndex(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    const LayoutCanvasTransform& transform,
    const std::string& currentFloorId,
    double elapsedSeconds,
    const QPointF& screenPosition) {
    constexpr double kHoverRadiusPixels = 14.0;

    const auto activeSelections = activeRouteGuidanceSelections(guidances, elapsedSeconds);
    std::optional<std::size_t> closestIndex;
    double closestDistanceSq = kHoverRadiusPixels * kHoverRadiusPixels;
    for (const auto& active : activeSelections) {
        const auto& guidance = guidances[active.guidanceIndex];
        const auto center = routeGuidanceMarkerCenter(
            layout,
            guidance,
            blocks,
            transform,
            currentFloorId,
            elapsedSeconds);
        if (!center.has_value()) {
            continue;
        }

        const auto dx = center->x() - screenPosition.x();
        const auto dy = center->y() - screenPosition.y();
        const auto distanceSq = (dx * dx) + (dy * dy);
        if (distanceSq <= closestDistanceSq) {
            closestDistanceSq = distanceSq;
            closestIndex = active.guidanceIndex;
        }
    }
    return closestIndex;
}

double agentRadiusPixels(const LayoutCanvasTransform& transform, double radiusMeters) {
    const auto safeRadius = std::isfinite(radiusMeters) && radiusMeters > 0.0
        ? radiusMeters
        : static_cast<double>(safecrowd::domain::kDefaultAgentRadiusMeters);
    return transform.mapDistance(safeRadius);
}

}  // namespace

SimulationCanvasWidget::SimulationCanvasWidget(safecrowd::domain::FacilityLayout2D layout, QWidget* parent)
    : QWidget(parent),
      layout_(std::move(layout)) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(520, 360);
    ui::polishCanvasSurface(this);
    currentFloorId_ = safecrowd::domain::defaultFloorId(layout_);
    layoutBounds_ = collectLayoutCanvasBounds(layout_, currentFloorId_);
    QCoreApplication::instance()->installEventFilter(this);
    setupFloorSelector();
}

SimulationCanvasWidget::~SimulationCanvasWidget() {
    if (auto* app = QCoreApplication::instance(); app != nullptr) {
        app->removeEventFilter(this);
    }
}

void SimulationCanvasWidget::setFrame(safecrowd::domain::SimulationFrame frame) {
    frame_ = std::move(frame);
    const auto firstAgentFloor = std::find_if(frame_.agents.begin(), frame_.agents.end(), [](const auto& agent) {
        return !agent.floorId.empty();
    });
    const auto hasAgentOnCurrentFloor = std::any_of(frame_.agents.begin(), frame_.agents.end(), [this](const auto& agent) {
        return matchesFloor(agent.floorId, currentFloorId_);
    });
    if (!manualFloorSelection_
        && !hasAgentOnCurrentFloor
        && firstAgentFloor != frame_.agents.end()
        && firstAgentFloor->floorId != currentFloorId_) {
        setCurrentFloorId(firstAgentFloor->floorId, false);
    }
    update();
}

void SimulationCanvasWidget::setConnectionBlocks(std::vector<safecrowd::domain::ConnectionBlockDraft> blocks) {
    connectionBlocks_ = std::move(blocks);
    update();
}

void SimulationCanvasWidget::setEnvironmentHazards(std::vector<safecrowd::domain::EnvironmentHazardDraft> hazards) {
    environmentHazards_ = std::move(hazards);
    update();
}

void SimulationCanvasWidget::setRouteGuidances(std::vector<safecrowd::domain::RouteGuidanceDraft> guidances) {
    routeGuidances_ = std::move(guidances);
    update();
}

void SimulationCanvasWidget::setDensityOverlay(
    std::vector<safecrowd::domain::DensityCellMetric> densityCells,
    double scaleMaxPeoplePerSquareMeter) {
    densityOverlay_ = std::move(densityCells);
    densityScaleMaxPeoplePerSquareMeter_ =
        std::isfinite(scaleMaxPeoplePerSquareMeter) && scaleMaxPeoplePerSquareMeter > 0.0
        ? scaleMaxPeoplePerSquareMeter
        : kDefaultDensityScaleMaxPeoplePerSquareMeter;
    ++heatmapOverlayRevision_;
    invalidateHeatmapWorldCache();
    invalidateOverlayCache();
    update();
}

void SimulationCanvasWidget::setOccupancyHeatmapOverlay(safecrowd::domain::OccupancyHeatmap heatmap) {
    occupancyHeatmapOverlay_ = std::move(heatmap);
    ++heatmapOverlayRevision_;
    invalidateHeatmapWorldCache();
    invalidateOverlayCache();
    update();
}

void SimulationCanvasWidget::setPressureOverlay(
    std::vector<safecrowd::domain::PressureCellMetric> pressureCells,
    double scaleMaxPressureScore) {
    pressureOverlay_ = std::move(pressureCells);
    pressureScaleMaxScore_ =
        std::isfinite(scaleMaxPressureScore) && scaleMaxPressureScore > 0.0
        ? scaleMaxPressureScore
        : kDefaultPressureScaleMaxScore;
    invalidateOverlayCache();
    update();
}

void SimulationCanvasWidget::setHotspotOverlay(std::vector<safecrowd::domain::ScenarioCongestionHotspot> hotspots) {
    hotspotOverlay_ = std::move(hotspots);
    if (focusedHotspotIndex_.has_value() && *focusedHotspotIndex_ >= hotspotOverlay_.size()) {
        focusedHotspotIndex_.reset();
    }
    invalidateOverlayCache();
    update();
}

void SimulationCanvasWidget::setBottleneckOverlay(std::vector<safecrowd::domain::ScenarioBottleneckMetric> bottlenecks) {
    bottleneckOverlay_ = std::move(bottlenecks);
    if (focusedBottleneckIndex_.has_value() && *focusedBottleneckIndex_ >= bottleneckOverlay_.size()) {
        focusedBottleneckIndex_.reset();
    }
    invalidateOverlayCache();
    update();
}

void SimulationCanvasWidget::setCrossFlowOverlay(std::vector<safecrowd::domain::ScenarioCrossFlowCellMetric> cells) {
    crossFlowCellOverlay_ = std::move(cells);
    if (focusedCrossFlowCellIndex_.has_value()
        && *focusedCrossFlowCellIndex_ >= crossFlowCellOverlay_.size()) {
        focusedCrossFlowCellIndex_.reset();
    }
    invalidateOverlayCache();
    update();
}

void SimulationCanvasWidget::setResultOverlayMode(ResultOverlayMode mode) {
    if (overlayMode_ == mode) {
        return;
    }
    overlayMode_ = mode;
    invalidateOverlayCache();
    update();
}

void SimulationCanvasWidget::focusHotspot(std::size_t index) {
    if (index >= hotspotOverlay_.size()) {
        return;
    }

    if (!hotspotOverlay_[index].floorId.empty() && hotspotOverlay_[index].floorId != currentFloorId_) {
        setCurrentFloorId(hotspotOverlay_[index].floorId, true);
    }
    focusedHotspotIndex_ = index;
    focusedBottleneckIndex_.reset();
    focusedCrossFlowCellIndex_.reset();
    invalidateOverlayCache();
    focusWorldPoint(hotspotOverlay_[index].center, std::max(camera_.zoom(), kHotspotFocusZoom));
}

void SimulationCanvasWidget::focusBottleneck(std::size_t index) {
    if (index >= bottleneckOverlay_.size()) {
        return;
    }

    const auto& passage = bottleneckOverlay_[index].passage;
    if (!bottleneckOverlay_[index].floorId.empty() && bottleneckOverlay_[index].floorId != currentFloorId_) {
        setCurrentFloorId(bottleneckOverlay_[index].floorId, true);
    }
    focusedBottleneckIndex_ = index;
    focusedHotspotIndex_.reset();
    focusedCrossFlowCellIndex_.reset();
    invalidateOverlayCache();
    focusWorldPoint(
        {.x = (passage.start.x + passage.end.x) / 2.0, .y = (passage.start.y + passage.end.y) / 2.0},
        std::max(camera_.zoom(), kBottleneckFocusZoom));
}

void SimulationCanvasWidget::focusCrossFlowCell(std::size_t index) {
    if (index >= crossFlowCellOverlay_.size()) {
        return;
    }

    const auto& cell = crossFlowCellOverlay_[index];
    if (!cell.floorId.empty() && cell.floorId != currentFloorId_) {
        setCurrentFloorId(cell.floorId, true);
    }
    focusedCrossFlowCellIndex_ = index;
    focusedHotspotIndex_.reset();
    focusedBottleneckIndex_.reset();
    invalidateOverlayCache();
    focusWorldPoint(cell.center, std::max(camera_.zoom(), kCrossFlowCellFocusZoom));
}

bool SimulationCanvasWidget::eventFilter(QObject* watched, QEvent* event) {
    (void)watched;
    camera_.handleGlobalKeyEvent(event);
    return QWidget::eventFilter(watched, event);
}

void SimulationCanvasWidget::keyPressEvent(QKeyEvent* event) {
    if (camera_.handleKeyPress(event)) {
        return;
    }
    QWidget::keyPressEvent(event);
}

void SimulationCanvasWidget::keyReleaseEvent(QKeyEvent* event) {
    if (camera_.handleKeyRelease(event)) {
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void SimulationCanvasWidget::leaveEvent(QEvent* event) {
    hoveredConnectionBlockId_.clear();
    hoveredEnvironmentHazardId_.clear();
    hoveredRouteGuidanceId_.clear();
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}

void SimulationCanvasWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        camera_.reset();
        layoutCacheValid_ = false;
        invalidateOverlayCache();
        update();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void SimulationCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (camera_.updatePan(event)) {
        if (!hoveredConnectionBlockId_.empty()
            || !hoveredEnvironmentHazardId_.empty()
            || !hoveredRouteGuidanceId_.empty()) {
            hoveredConnectionBlockId_.clear();
            hoveredEnvironmentHazardId_.clear();
            hoveredRouteGuidanceId_.clear();
            QToolTip::hideText();
        }
        layoutCacheValid_ = false;
        invalidateOverlayCache();
        update();
        return;
    }

    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        if (!hoveredConnectionBlockId_.empty()
            || !hoveredEnvironmentHazardId_.empty()
            || !hoveredRouteGuidanceId_.empty()) {
            hoveredConnectionBlockId_.clear();
            hoveredEnvironmentHazardId_.clear();
            hoveredRouteGuidanceId_.clear();
            QToolTip::hideText();
        }
        QWidget::mouseMoveEvent(event);
        return;
    }

    const auto transform = currentTransform(*bounds);
    const auto elapsedSeconds = std::max(0.0, frame_.elapsedSeconds);
    const auto hoveredGuidance = hoveredActiveRouteGuidanceIndex(
        layout_,
        routeGuidances_,
        connectionBlocks_,
        transform,
        currentFloorId_,
        elapsedSeconds,
        event->position());
    const auto hoveredIndex = hoveredConnectionBlockIndex(
        layout_,
        connectionBlocks_,
        transform,
        currentFloorId_,
        elapsedSeconds,
        event->position());
    const auto hoveredHazard = hoveredEnvironmentHazardIndex(
        layout_,
        environmentHazards_,
        transform,
        currentFloorId_,
        elapsedSeconds,
        event->position());

    if (hoveredGuidance.has_value()) {
        const auto& guidance = routeGuidances_[*hoveredGuidance];
        const auto tooltip = formatRouteGuidanceTooltip(guidance);
        const auto hoveredId = guidance.id.empty()
            ? (!guidance.installConnectionId.empty() ? guidance.installConnectionId : guidance.guidedExitZoneId)
            : guidance.id;
        if (hoveredId != hoveredRouteGuidanceId_) {
            hoveredRouteGuidanceId_ = hoveredId;
            hoveredConnectionBlockId_.clear();
            hoveredEnvironmentHazardId_.clear();
            QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
        }
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (hoveredIndex.has_value()) {
        const auto& block = connectionBlocks_[*hoveredIndex];
        const auto state = connectionBlockVisualState(block, elapsedSeconds);
        const auto tooltip = state.has_value() ? formatScheduleTooltip(block, *state) : formatScheduleTooltip(block);
        if (tooltip.isEmpty()) {
            QWidget::mouseMoveEvent(event);
            return;
        }

        const auto hoveredId = block.id.empty() ? block.connectionId : block.id;
        if (hoveredId != hoveredConnectionBlockId_) {
            hoveredConnectionBlockId_ = hoveredId;
            hoveredEnvironmentHazardId_.clear();
            hoveredRouteGuidanceId_.clear();
            QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
        }
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (hoveredHazard.has_value()) {
        const auto& hazard = environmentHazards_[*hoveredHazard];
        const auto state = environmentHazardVisualState(hazard, elapsedSeconds);
        const auto tooltip = state.has_value()
            ? formatEnvironmentHazardTooltip(hazard, *state)
            : formatEnvironmentHazardTooltip(hazard);
        const auto hoveredId = hazard.id.empty()
            ? QString("%1:%2:%3")
                  .arg(hazardKindLabel(hazard.kind))
                  .arg(hazard.position.x, 0, 'f', 3)
                  .arg(hazard.position.y, 0, 'f', 3)
                  .toStdString()
            : hazard.id;
        if (hoveredId != hoveredEnvironmentHazardId_) {
            hoveredEnvironmentHazardId_ = hoveredId;
            hoveredConnectionBlockId_.clear();
            hoveredRouteGuidanceId_.clear();
            QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
        }
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (!hoveredConnectionBlockId_.empty()
        || !hoveredEnvironmentHazardId_.empty()
        || !hoveredRouteGuidanceId_.empty()) {
        hoveredConnectionBlockId_.clear();
        hoveredEnvironmentHazardId_.clear();
        hoveredRouteGuidanceId_.clear();
        QToolTip::hideText();
    }
    QWidget::mouseMoveEvent(event);
}

void SimulationCanvasWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);
    if (camera_.beginPan(event)) {
        return;
    }
    QWidget::mousePressEvent(event);
}

void SimulationCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (camera_.finishPan(event)) {
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void SimulationCanvasWidget::paintEvent(QPaintEvent* event) {
    (void)event;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#f4f7fb"));

    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        painter.setPen(QColor("#4f5d6b"));
        painter.drawText(rect(), Qt::AlignCenter, "No layout available");
        return;
    }

    refreshLayoutCache(*bounds);
    if (layoutCache_.isNull()) {
        return;
    }
    painter.drawPixmap(0, 0, layoutCache_);

    const auto transform = currentTransform(*bounds);
    if (overlayMode_ == ResultOverlayMode::Occupancy) {
        drawOccupancyHeatmapOverlay(painter, transform);
    } else if (overlayMode_ == ResultOverlayMode::Density) {
        drawDensityOverlay(painter, transform);
    } else {
        refreshOverlayCache(*bounds);
        if (!overlayCache_.isNull()) {
            painter.drawPixmap(0, 0, overlayCache_);
        }
    }
    drawEnvironmentHazardOverlay(painter, transform);
    drawConnectionBlockOverlay(painter, transform);
    drawRouteGuidanceOverlay(painter, transform);
    for (const auto& agent : frame_.agents) {
        if (!matchesFloor(agent.floorId, currentFloorId_)) {
            continue;
        }
        const auto origin = transform.map(agent.position);
        const auto radius = agentRadiusPixels(transform, agent.radius);
        painter.setPen(Qt::NoPen);
        painter.setBrush(agent.stalled ? kStalledAgentColor : kMovingAgentColor);
        painter.drawEllipse(origin, radius, radius);
    }
}

void SimulationCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    repositionFloorSelector();
}

void SimulationCanvasWidget::wheelEvent(QWheelEvent* event) {
    if (switchFloorByWheel(event)) {
        return;
    }

    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        QWidget::wheelEvent(event);
        return;
    }
    if (camera_.zoomAt(event, *bounds, previewViewport())) {
        layoutCacheValid_ = false;
        invalidateOverlayCache();
        update();
        return;
    }
    QWidget::wheelEvent(event);
}

std::optional<LayoutCanvasBounds> SimulationCanvasWidget::collectBounds() const {
    return layoutBounds_;
}

LayoutCanvasTransform SimulationCanvasWidget::currentTransform(const LayoutCanvasBounds& bounds) const {
    return LayoutCanvasTransform(bounds, previewViewport(), camera_.zoom(), camera_.panOffset());
}

void SimulationCanvasWidget::refreshLayoutCache(const LayoutCanvasBounds& bounds) {
    const auto currentSize = size();
    if (currentSize.isEmpty()) {
        layoutCache_ = QPixmap();
        layoutCacheSize_ = currentSize;
        layoutCacheDevicePixelRatio_ = 0.0;
        layoutCacheValid_ = false;
        return;
    }

    const auto devicePixelRatio = devicePixelRatioF();
    if (layoutCacheValid_
        && layoutCacheSize_ == currentSize
        && layoutCacheDevicePixelRatio_ == devicePixelRatio
        && layoutCacheZoom_ == camera_.zoom()
        && layoutCachePan_ == camera_.panOffset()) {
        return;
    }

    const QSize physicalSize{
        std::max(1, static_cast<int>(std::ceil(currentSize.width() * devicePixelRatio))),
        std::max(1, static_cast<int>(std::ceil(currentSize.height() * devicePixelRatio))),
    };
    layoutCache_ = QPixmap(physicalSize);
    layoutCache_.setDevicePixelRatio(devicePixelRatio);
    layoutCache_.fill(QColor("#f4f7fb"));
    QPainter painter(&layoutCache_);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto transform = currentTransform(bounds);
    drawLayoutCanvasSurface(painter, QRectF(rect()));
    drawFacilityLayoutCanvas(painter, layout_, transform, currentFloorId_);

    layoutCacheSize_ = currentSize;
    layoutCacheZoom_ = camera_.zoom();
    layoutCachePan_ = camera_.panOffset();
    layoutCacheDevicePixelRatio_ = devicePixelRatio;
    layoutCacheValid_ = true;
}

void SimulationCanvasWidget::refreshOverlayCache(const LayoutCanvasBounds& bounds) {
    if (overlayMode_ == ResultOverlayMode::None) {
        overlayCache_ = QPixmap();
        overlayCacheValid_ = true;
        overlayCacheMode_ = overlayMode_;
        overlayCacheFloorId_ = currentFloorId_;
        return;
    }
    if (overlayMode_ == ResultOverlayMode::Occupancy || overlayMode_ == ResultOverlayMode::Density) {
        overlayCache_ = QPixmap();
        overlayCacheValid_ = true;
        overlayCacheMode_ = overlayMode_;
        overlayCacheFloorId_ = currentFloorId_;
        return;
    }

    const auto currentSize = size();
    if (currentSize.isEmpty()) {
        overlayCache_ = QPixmap();
        overlayCacheSize_ = currentSize;
        overlayCacheDevicePixelRatio_ = 0.0;
        overlayCacheValid_ = false;
        return;
    }

    const auto devicePixelRatio = devicePixelRatioF();
    if (overlayCacheValid_
        && overlayCacheSize_ == currentSize
        && overlayCacheDevicePixelRatio_ == devicePixelRatio
        && overlayCacheZoom_ == camera_.zoom()
        && overlayCachePan_ == camera_.panOffset()
        && overlayCacheMode_ == overlayMode_
        && overlayCacheFloorId_ == currentFloorId_) {
        return;
    }

    const QSize physicalSize{
        std::max(1, static_cast<int>(std::ceil(currentSize.width() * devicePixelRatio))),
        std::max(1, static_cast<int>(std::ceil(currentSize.height() * devicePixelRatio))),
    };
    overlayCache_ = QPixmap(physicalSize);
    overlayCache_.setDevicePixelRatio(devicePixelRatio);
    overlayCache_.fill(Qt::transparent);

    QPainter painter(&overlayCache_);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const auto transform = currentTransform(bounds);
    if (overlayMode_ == ResultOverlayMode::Occupancy) {
        drawOccupancyHeatmapOverlay(painter, transform);
    } else if (overlayMode_ == ResultOverlayMode::Density) {
        drawDensityOverlay(painter, transform);
    } else if (overlayMode_ == ResultOverlayMode::Pressure) {
        drawPressureOverlay(painter, transform);
    } else if (overlayMode_ == ResultOverlayMode::Hotspots) {
        drawHotspotOverlay(painter, transform);
    } else if (overlayMode_ == ResultOverlayMode::Bottlenecks) {
        drawBottleneckOverlay(painter, transform);
    } else if (overlayMode_ == ResultOverlayMode::CrossFlow) {
        drawCrossFlowOverlay(painter, transform);
    }

    overlayCacheSize_ = currentSize;
    overlayCacheZoom_ = camera_.zoom();
    overlayCachePan_ = camera_.panOffset();
    overlayCacheDevicePixelRatio_ = devicePixelRatio;
    overlayCacheMode_ = overlayMode_;
    overlayCacheFloorId_ = currentFloorId_;
    overlayCacheValid_ = true;
}

void SimulationCanvasWidget::invalidateOverlayCache() {
    overlayCacheValid_ = false;
}

void SimulationCanvasWidget::invalidateHeatmapWorldCache() {
    heatmapWorldCache_ = QImage();
    heatmapWorldCacheValid_ = false;
}

QRectF SimulationCanvasWidget::previewViewport() const {
    return QRectF(rect()).adjusted(kViewportPadding, kViewportPadding, -kViewportPadding, -kViewportPadding);
}

void SimulationCanvasWidget::focusWorldPoint(const safecrowd::domain::Point2D& point, double zoom) {
    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        return;
    }

    const auto viewport = previewViewport();
    if (viewport.width() <= 0.0 || viewport.height() <= 0.0) {
        return;
    }

    camera_.setZoom(std::clamp(zoom, 0.1, 50.0));
    camera_.setPanOffset({});

    const LayoutCanvasTransform transform(*bounds, viewport, camera_.zoom(), {});
    camera_.setPanOffset(viewport.center() - transform.map(point));
    layoutCacheValid_ = false;
    invalidateOverlayCache();
    update();
}

void SimulationCanvasWidget::drawConnectionBlockOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (connectionBlocks_.empty()) {
        return;
    }

    const auto elapsedSeconds = std::max(0.0, frame_.elapsedSeconds);

    painter.save();
    for (const auto& block : connectionBlocks_) {
        const auto state = connectionBlockVisualState(block, elapsedSeconds);
        if (!state.has_value()) {
            continue;
        }

        const auto it = std::find_if(layout_.connections.begin(), layout_.connections.end(), [&](const auto& connection) {
            return connection.id == block.connectionId;
        });
        if (it == layout_.connections.end()) {
            continue;
        }
        if (!matchesFloor(it->floorId, currentFloorId_)) {
            continue;
        }

        const auto center = transform.map(connectionCenter(*it));
        QColor color("#c0392b");
        Qt::PenStyle penStyle = Qt::SolidLine;
        double penWidth = 2.8;
        if (*state == TimelineVisualState::Future) {
            color = QColor("#64748b");
            penStyle = Qt::DashLine;
            penWidth = 2.2;
        } else if (*state == TimelineVisualState::Expired) {
            color = QColor(100, 116, 139, 120);
            penStyle = Qt::DotLine;
            penWidth = 2.0;
        }

        const double r = 10.0;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(color, penWidth, penStyle, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(center, r, r);
        if (*state == TimelineVisualState::Future) {
            painter.drawLine(center, QPointF(center.x(), center.y() - 5.8));
            painter.drawLine(center, QPointF(center.x() + 5.2, center.y()));
        } else if (*state == TimelineVisualState::Expired) {
            painter.drawLine(QPointF(center.x() - 5.6, center.y() + 0.4), QPointF(center.x() - 1.8, center.y() + 4.2));
            painter.drawLine(QPointF(center.x() - 1.8, center.y() + 4.2), QPointF(center.x() + 6.0, center.y() - 5.0));
        } else {
            painter.drawLine(QPointF(center.x() - 6.5, center.y() + 6.5), QPointF(center.x() + 6.5, center.y() - 6.5));
        }
    }

    painter.restore();
}

void SimulationCanvasWidget::drawEnvironmentHazardOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (environmentHazards_.empty()) {
        return;
    }

    const auto elapsedSeconds = std::max(0.0, frame_.elapsedSeconds);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    for (const auto& hazard : environmentHazards_) {
        const auto state = environmentHazardVisualState(hazard, elapsedSeconds);
        if (!state.has_value()) {
            continue;
        }
        if (!matchesFloor(safecrowd::domain::environmentHazardFloorId(layout_, hazard), currentFloorId_)) {
            continue;
        }

        const auto center = transform.map(hazard.position);
        const auto radiusMeters = safecrowd::domain::environmentHazardRuntimeProfile(hazard).radiusMeters;
        const auto radiusAnchor = transform.map({
            .x = hazard.position.x + radiusMeters,
            .y = hazard.position.y,
        });
        const auto radius = std::max(
            18.0,
            std::hypot(radiusAnchor.x() - center.x(), radiusAnchor.y() - center.y()));

        const auto isFire = hazard.kind == safecrowd::domain::EnvironmentHazardKind::Fire;
        QColor core = isFire ? QColor(220, 38, 38, 110) : QColor(71, 85, 105, 92);
        QColor mid = isFire ? QColor(249, 115, 22, 46) : QColor(148, 163, 184, 40);
        QColor edge = isFire ? QColor(249, 115, 22, 0) : QColor(148, 163, 184, 0);
        QColor outline = isFire ? QColor(185, 28, 28, 180) : QColor(71, 85, 105, 165);
        Qt::PenStyle outlineStyle = Qt::DashLine;
        QColor markerFill = isFire ? QColor("#c2410c") : QColor("#64748b");
        if (*state == TimelineVisualState::Future) {
            core = isFire ? QColor(220, 38, 38, 42) : QColor(71, 85, 105, 34);
            mid = isFire ? QColor(249, 115, 22, 16) : QColor(148, 163, 184, 14);
            edge = QColor(0, 0, 0, 0);
            outline = QColor(100, 116, 139, 135);
            outlineStyle = Qt::DashLine;
            markerFill = QColor(100, 116, 139, 170);
        } else if (*state == TimelineVisualState::Expired) {
            core = QColor(100, 116, 139, 28);
            mid = QColor(100, 116, 139, 10);
            edge = QColor(100, 116, 139, 0);
            outline = QColor(100, 116, 139, 90);
            outlineStyle = Qt::DotLine;
            markerFill = QColor(100, 116, 139, 115);
        }
        QRadialGradient gradient(center, radius);
        auto falloffColor = [&](double ratio, QColor color) {
            const auto influence = safecrowd::domain::environmentHazardInfluenceAt(
                hazard,
                radiusMeters * std::clamp(ratio, 0.0, 1.0));
            color.setAlpha(std::clamp(static_cast<int>(std::lround(static_cast<double>(color.alpha()) * influence)), 0, 255));
            return color;
        };
        gradient.setColorAt(0.0, core);
        gradient.setColorAt(0.35, falloffColor(0.35, core));
        gradient.setColorAt(0.65, falloffColor(0.65, mid));
        gradient.setColorAt(1.0, edge);
        painter.setPen(Qt::NoPen);
        painter.setBrush(gradient);
        painter.drawEllipse(center, radius, radius);

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(
            outline,
            1.8,
            outlineStyle,
            Qt::RoundCap,
            Qt::RoundJoin));
        painter.drawEllipse(center, radius, radius);

        painter.setPen(Qt::NoPen);
        painter.setBrush(markerFill);
        painter.drawEllipse(center, 11.0, 11.0);

        painter.setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        if (isFire) {
            QPainterPath flame;
            flame.moveTo(center + QPointF(0.0, 6.0));
            flame.cubicTo(center + QPointF(-5.0, 2.0), center + QPointF(-3.5, -4.0), center + QPointF(-0.5, -7.0));
            flame.cubicTo(center + QPointF(4.0, -3.0), center + QPointF(4.0, 3.0), center + QPointF(0.0, 6.0));
            painter.drawPath(flame);
        } else {
            painter.drawArc(QRectF(center.x() - 7.0, center.y() - 1.0, 9.0, 7.0), 20 * 16, 220 * 16);
            painter.drawArc(QRectF(center.x() - 1.0, center.y() - 3.0, 10.0, 7.0), 20 * 16, 220 * 16);
            painter.drawArc(QRectF(center.x() - 5.0, center.y() - 8.0, 8.0, 6.0), 20 * 16, 220 * 16);
        }
    }

    painter.restore();
}

void SimulationCanvasWidget::drawRouteGuidanceOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    const auto elapsedSeconds = std::max(0.0, frame_.elapsedSeconds);
    const auto activeSelections = activeRouteGuidanceSelections(routeGuidances_, elapsedSeconds);
    if (activeSelections.empty()) {
        return;
    }

    painter.save();
    painter.setPen(Qt::NoPen);

    for (const auto& active : activeSelections) {
        const auto& guidance = routeGuidances_[active.guidanceIndex];
        const auto center = routeGuidanceMarkerCenter(
            layout_,
            guidance,
            connectionBlocks_,
            transform,
            currentFloorId_,
            elapsedSeconds);
        if (!center.has_value()) {
            continue;
        }

        drawRouteGuidanceInfluenceRadius(
            painter,
            *center,
            routeGuidanceInfluenceRadiusPixels(transform, guidance));

        painter.setBrush(QColor("#1f5fae"));

        const double r = 10.0;
        painter.drawEllipse(*center, r, r);

        painter.save();
        painter.translate(*center);
        painter.rotate(-25.0);
        painter.translate(-(*center));
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(QRectF(center->x() - 1.8, center->y() - 7.0, 3.6, 10.5), 1.4, 1.4);
        painter.drawRoundedRect(QRectF(center->x() - 1.5, center->y() + 2.2, 3.0, 5.2), 1.2, 1.2);
        painter.restore();

        painter.setPen(QPen(Qt::white, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(center->x() + 5.3, center->y() - 5.0), QPointF(center->x() + 8.2, center->y() - 7.7));
        painter.drawLine(QPointF(center->x() + 6.3, center->y() - 2.0), QPointF(center->x() + 9.2, center->y() - 2.8));
        painter.drawLine(QPointF(center->x() + 3.7, center->y() - 7.2), QPointF(center->x() + 4.8, center->y() - 9.8));
        painter.setPen(Qt::NoPen);
    }

    painter.restore();
}

void SimulationCanvasWidget::drawOccupancyHeatmapOverlay(QPainter& painter, const LayoutCanvasTransform& transform) {
    if (occupancyHeatmapOverlay_.cells.empty()
        || occupancyHeatmapOverlay_.peakAccumulatedAgentSeconds <= 0.0) {
        return;
    }

    const auto pixelsPerMeterKey = heatmapPixelsPerMeterKey(transform);
    if (!heatmapWorldCacheValid_
        || heatmapWorldCacheMode_ != ResultOverlayMode::Occupancy
        || heatmapWorldCacheFloorId_ != currentFloorId_
        || heatmapWorldCacheRevision_ != heatmapOverlayRevision_
        || std::abs(heatmapWorldCachePixelsPerMeterKey_ - pixelsPerMeterKey) > 1e-9) {
        std::vector<HeatmapWorldSource> sources;
        sources.reserve(occupancyHeatmapOverlay_.cells.size());
        for (const auto& cell : occupancyHeatmapOverlay_.cells) {
            if (!matchesFloor(cell.floorId, currentFloorId_) || cell.normalizedIntensity <= 0.0) {
                continue;
            }
            sources.push_back({
                .center = cell.center,
                .cellMin = cell.cellMin,
                .cellMax = cell.cellMax,
                .intensity = std::clamp(cell.normalizedIntensity, 0.0, 1.0),
            });
        }

        heatmapWorldCache_ = QImage();
        if (auto generated = buildHeatmapWorldCacheImage(
                layout_,
                currentFloorId_,
                sources,
                pixelsPerMeterKey,
                ResultOverlayMode::Occupancy);
            generated.has_value()) {
            heatmapWorldCache_ = std::move(generated->image);
            heatmapWorldCacheBounds_ = generated->bounds;
        }
        heatmapWorldCachePixelsPerMeterKey_ = pixelsPerMeterKey;
        heatmapWorldCacheMode_ = ResultOverlayMode::Occupancy;
        heatmapWorldCacheFloorId_ = currentFloorId_;
        heatmapWorldCacheRevision_ = heatmapOverlayRevision_;
        heatmapWorldCacheValid_ = true;
    }

    drawHeatmapWorldCacheImage(
        painter,
        transform,
        layout_,
        currentFloorId_,
        heatmapWorldCache_,
        heatmapWorldCacheBounds_);
}

void SimulationCanvasWidget::drawDensityOverlay(QPainter& painter, const LayoutCanvasTransform& transform) {
    if (densityOverlay_.empty()) {
        return;
    }

    const auto scaleMax =
        std::isfinite(densityScaleMaxPeoplePerSquareMeter_) && densityScaleMaxPeoplePerSquareMeter_ > 0.0
        ? densityScaleMaxPeoplePerSquareMeter_
        : kDefaultDensityScaleMaxPeoplePerSquareMeter;

    const auto pixelsPerMeterKey = heatmapPixelsPerMeterKey(transform);
    if (!heatmapWorldCacheValid_
        || heatmapWorldCacheMode_ != ResultOverlayMode::Density
        || heatmapWorldCacheFloorId_ != currentFloorId_
        || heatmapWorldCacheRevision_ != heatmapOverlayRevision_
        || std::abs(heatmapWorldCachePixelsPerMeterKey_ - pixelsPerMeterKey) > 1e-9) {
        std::vector<HeatmapWorldSource> sources;
        sources.reserve(densityOverlay_.size());
        for (const auto& cell : densityOverlay_) {
            if (!matchesFloor(cell.floorId, currentFloorId_)) {
                continue;
            }
            const auto intensity = std::clamp(cell.densityPeoplePerSquareMeter / scaleMax, 0.0, 1.0);
            if (intensity <= 0.0) {
                continue;
            }
            sources.push_back({
                .center = cell.center,
                .cellMin = cell.cellMin,
                .cellMax = cell.cellMax,
                .intensity = intensity,
            });
        }

        heatmapWorldCache_ = QImage();
        if (auto generated = buildHeatmapWorldCacheImage(
                layout_,
                currentFloorId_,
                sources,
                pixelsPerMeterKey,
                ResultOverlayMode::Density);
            generated.has_value()) {
            heatmapWorldCache_ = std::move(generated->image);
            heatmapWorldCacheBounds_ = generated->bounds;
        }
        heatmapWorldCachePixelsPerMeterKey_ = pixelsPerMeterKey;
        heatmapWorldCacheMode_ = ResultOverlayMode::Density;
        heatmapWorldCacheFloorId_ = currentFloorId_;
        heatmapWorldCacheRevision_ = heatmapOverlayRevision_;
        heatmapWorldCacheValid_ = true;
    }

    drawHeatmapWorldCacheImage(
        painter,
        transform,
        layout_,
        currentFloorId_,
        heatmapWorldCache_,
        heatmapWorldCacheBounds_);
}

void SimulationCanvasWidget::drawPressureOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (pressureOverlay_.empty()) {
        return;
    }

    std::vector<const safecrowd::domain::PressureCellMetric*> visibleCells;
    visibleCells.reserve(pressureOverlay_.size());
    for (const auto& cell : pressureOverlay_) {
        if (!matchesFloor(cell.floorId, currentFloorId_)) {
            continue;
        }
        if (cell.pressureScore <= 0.0) {
            continue;
        }
        visibleCells.push_back(&cell);
    }
    if (visibleCells.empty()) {
        return;
    }

    const auto scaleMax =
        std::isfinite(pressureScaleMaxScore_) && pressureScaleMaxScore_ > 0.0
        ? pressureScaleMaxScore_
        : kDefaultPressureScaleMaxScore;
    std::sort(visibleCells.begin(), visibleCells.end(), [](const auto* lhs, const auto* rhs) {
        if (lhs->pressureScore != rhs->pressureScore) {
            return lhs->pressureScore < rhs->pressureScore;
        }
        return lhs->intrudingPairCount < rhs->intrudingPairCount;
    });

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    QPainterPath walkableClip;
    for (const auto& zone : layout_.zones) {
        if (!matchesFloor(zone.floorId, currentFloorId_)) {
            continue;
        }
        walkableClip.addPath(layoutCanvasPolygonPath(zone.area, transform));
    }
    if (!walkableClip.isEmpty()) {
        painter.setClipPath(walkableClip);
    }

    for (const auto* cell : visibleCells) {
        const auto center = transform.map(cell->center);
        const auto cellWidth = cell->cellMax.x > cell->cellMin.x
            ? cell->cellMax.x - cell->cellMin.x
            : kDefaultHotspotCellSize;
        const auto cellHeight = cell->cellMax.y > cell->cellMin.y
            ? cell->cellMax.y - cell->cellMin.y
            : kDefaultHotspotCellSize;
        const auto influenceRadiusWorld =
            std::max(cellWidth, cellHeight) * kPressureInfluenceRadiusMultiplier;
        const auto radiusAnchor = transform.map({
            .x = cell->center.x + influenceRadiusWorld,
            .y = cell->center.y,
        });
        const auto radius = std::max(
            kPressureMinimumScreenRadius,
            std::hypot(radiusAnchor.x() - center.x(), radiusAnchor.y() - center.y()));
        const auto intensity = std::clamp(cell->pressureScore / scaleMax, 0.0, 1.0);
        const auto coreAlpha = 62 + static_cast<int>(138.0 * intensity);
        const auto coreColor = pressureHeatmapColor(intensity, std::clamp(coreAlpha, 62, 200));
        const auto middleColor = pressureHeatmapColor(intensity, static_cast<int>(coreAlpha * 0.46));

        QRadialGradient gradient(center, radius);
        gradient.setColorAt(0.0, coreColor);
        gradient.setColorAt(0.34, middleColor);
        gradient.setColorAt(1.0, pressureHeatmapColor(intensity, 0));
        painter.setBrush(gradient);
        painter.drawEllipse(center, radius, radius);
    }
    painter.restore();
}

void SimulationCanvasWidget::drawHotspotOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (hotspotOverlay_.empty()) {
        return;
    }

    std::size_t maxAgentCount = 0;
    std::vector<const safecrowd::domain::ScenarioCongestionHotspot*> visibleHotspots;
    visibleHotspots.reserve(hotspotOverlay_.size());
    for (const auto& hotspot : hotspotOverlay_) {
        if (!matchesFloor(hotspot.floorId, currentFloorId_)) {
            continue;
        }
        maxAgentCount = std::max(maxAgentCount, hotspot.agentCount);
        visibleHotspots.push_back(&hotspot);
    }
    if (maxAgentCount == 0 || visibleHotspots.empty()) {
        return;
    }

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    QPainterPath walkableClip;
    for (const auto& zone : layout_.zones) {
        if (!matchesFloor(zone.floorId, currentFloorId_)) {
            continue;
        }
        walkableClip.addPath(layoutCanvasPolygonPath(zone.area, transform));
    }
    if (!walkableClip.isEmpty()) {
        painter.setClipPath(walkableClip);
    }

    for (const auto* hotspot : visibleHotspots) {
        if (hotspot->agentCount == 0) {
            continue;
        }
        const auto intensity = static_cast<double>(hotspot->agentCount) / static_cast<double>(maxAgentCount);
        const auto center = transform.map(hotspot->center);
        const auto cellWidth = hotspot->cellMax.x > hotspot->cellMin.x
            ? hotspot->cellMax.x - hotspot->cellMin.x
            : kDefaultHotspotCellSize;
        const auto cellHeight = hotspot->cellMax.y > hotspot->cellMin.y
            ? hotspot->cellMax.y - hotspot->cellMin.y
            : kDefaultHotspotCellSize;
        const auto sourceRadiusWorld = std::max(cellWidth, cellHeight) * (1.2 + (0.85 * std::sqrt(intensity)));
        const auto radiusAnchor = transform.map({.x = hotspot->center.x + sourceRadiusWorld, .y = hotspot->center.y});
        const auto radius = std::max(12.0, std::hypot(radiusAnchor.x() - center.x(), radiusAnchor.y() - center.y()));
        const auto coreAlpha = kHotspotMinCoreAlpha
            + static_cast<int>((kHotspotMaxCoreAlpha - kHotspotMinCoreAlpha) * intensity);

        QRadialGradient gradient(center, radius);
        gradient.setColorAt(0.0, QColor(185, 28, 28, std::clamp(coreAlpha, kHotspotMinCoreAlpha, kHotspotMaxCoreAlpha)));
        gradient.setColorAt(0.28, QColor(220, 38, 38, static_cast<int>(coreAlpha * 0.62)));
        gradient.setColorAt(0.58, QColor(249, 115, 22, static_cast<int>(coreAlpha * 0.28)));
        gradient.setColorAt(1.0, QColor(249, 115, 22, 0));
        painter.setBrush(gradient);
        painter.drawEllipse(center, radius, radius);

        if (focusedHotspotIndex_.has_value()
            && *focusedHotspotIndex_ < hotspotOverlay_.size()
            && hotspot == &hotspotOverlay_[*focusedHotspotIndex_]) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(127, 29, 29, 220), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawEllipse(center, radius + 4.0, radius + 4.0);
            painter.setPen(Qt::NoPen);
        }
    }
    painter.restore();
}

void SimulationCanvasWidget::drawBottleneckOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (bottleneckOverlay_.empty()) {
        return;
    }

    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (std::size_t index = 0; index < bottleneckOverlay_.size(); ++index) {
        if (!matchesFloor(bottleneckOverlay_[index].floorId, currentFloorId_)) {
            continue;
        }
        const auto focused = focusedBottleneckIndex_.has_value() && *focusedBottleneckIndex_ == index;
        painter.setPen(QPen(
            focused ? QColor(127, 29, 29, 235) : QColor(220, 38, 38, 150),
            focused ? 6.0 : 4.0,
            Qt::SolidLine,
            Qt::RoundCap));
        painter.drawLine(
            transform.map(bottleneckOverlay_[index].passage.start),
            transform.map(bottleneckOverlay_[index].passage.end));
    }
    painter.restore();
}

void SimulationCanvasWidget::drawCrossFlowOverlay(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (crossFlowCellOverlay_.empty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    QPainterPath walkableClip;
    for (const auto& zone : layout_.zones) {
        if (!matchesFloor(zone.floorId, currentFloorId_)) {
            continue;
        }
        walkableClip.addPath(layoutCanvasPolygonPath(zone.area, transform));
    }
    if (!walkableClip.isEmpty()) {
        painter.setClipPath(walkableClip);
    }

    std::vector<const safecrowd::domain::ScenarioCrossFlowCellMetric*> visibleCells;
    visibleCells.reserve(crossFlowCellOverlay_.size());
    for (const auto& cell : crossFlowCellOverlay_) {
        if (!matchesFloor(cell.floorId, currentFloorId_)) {
            continue;
        }
        if (cell.crossFlowScore <= 0.0) {
            continue;
        }
        visibleCells.push_back(&cell);
    }
    std::sort(visibleCells.begin(), visibleCells.end(), [](const auto* lhs, const auto* rhs) {
        if (std::fabs(lhs->crossFlowScore - rhs->crossFlowScore) > 1e-9) {
            return lhs->crossFlowScore < rhs->crossFlowScore;
        }
        return lhs->movingAgentCount < rhs->movingAgentCount;
    });

    painter.setPen(Qt::NoPen);
    for (const auto* cell : visibleCells) {
        const auto center = transform.map(cell->center);
        const auto cellWidth = cell->cellMax.x > cell->cellMin.x
            ? cell->cellMax.x - cell->cellMin.x
            : safecrowd::domain::kScenarioCrossFlowCellSize;
        const auto cellHeight = cell->cellMax.y > cell->cellMin.y
            ? cell->cellMax.y - cell->cellMin.y
            : safecrowd::domain::kScenarioCrossFlowCellSize;
        const auto influenceRadiusWorld =
            std::max(cellWidth, cellHeight) * kCrossFlowInfluenceRadiusMultiplier;
        const auto radiusAnchor = transform.map({
            .x = cell->center.x + influenceRadiusWorld,
            .y = cell->center.y,
        });
        const auto radius = std::max(
            kCrossFlowMinimumScreenRadius,
            std::hypot(radiusAnchor.x() - center.x(), radiusAnchor.y() - center.y()));
        const auto intensity = std::clamp(cell->crossFlowScore, 0.0, 1.0);
        const auto coreAlpha = 68 + static_cast<int>(144.0 * intensity);

        QRadialGradient gradient(center, radius);
        gradient.setColorAt(0.0, crossFlowHeatmapColor(intensity, std::clamp(coreAlpha, 68, 212)));
        gradient.setColorAt(0.4, crossFlowHeatmapColor(intensity, static_cast<int>(coreAlpha * 0.45)));
        gradient.setColorAt(1.0, crossFlowHeatmapColor(intensity, 0));
        painter.setBrush(gradient);
        painter.drawEllipse(center, radius, radius);

        if (focusedCrossFlowCellIndex_.has_value()
            && *focusedCrossFlowCellIndex_ < crossFlowCellOverlay_.size()
            && cell == &crossFlowCellOverlay_[*focusedCrossFlowCellIndex_]) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(120, 53, 15, 230), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawEllipse(center, radius + 4.0, radius + 4.0);
            painter.setPen(Qt::NoPen);
        }
    }

    painter.restore();
}

bool SimulationCanvasWidget::switchFloorByWheel(QWheelEvent* event) {
    if (event == nullptr
        || !(event->modifiers() & Qt::ControlModifier)
        || layout_.floors.size() <= 1) {
        return false;
    }

    const auto delta = event->angleDelta().y() != 0 ? event->angleDelta().y() : event->pixelDelta().y();
    if (delta == 0) {
        return false;
    }

    auto currentIndex = 0;
    for (std::size_t index = 0; index < layout_.floors.size(); ++index) {
        if (layout_.floors[index].id == currentFloorId_) {
            currentIndex = static_cast<int>(index);
            break;
        }
    }

    const auto direction = delta > 0 ? 1 : -1;
    const auto nextIndex = std::clamp(
        currentIndex + direction,
        0,
        static_cast<int>(layout_.floors.size() - 1));
    const auto& nextFloorId = layout_.floors[static_cast<std::size_t>(nextIndex)].id;
    if (nextIndex != currentIndex && !nextFloorId.empty()) {
        setCurrentFloorId(nextFloorId, true);
    }

    event->accept();
    return true;
}

void SimulationCanvasWidget::setCurrentFloorId(std::string floorId, bool manualSelection) {
    if (floorId == currentFloorId_ && manualSelection == manualFloorSelection_) {
        return;
    }

    currentFloorId_ = std::move(floorId);
    manualFloorSelection_ = manualSelection;
    layoutBounds_ = collectLayoutCanvasBounds(layout_, currentFloorId_);
    layoutCacheValid_ = false;
    invalidateOverlayCache();
    camera_.reset();

    if (floorComboBox_ != nullptr) {
        const auto index = floorComboBox_->findData(QString::fromStdString(currentFloorId_));
        if (index >= 0 && floorComboBox_->currentIndex() != index) {
            const QSignalBlocker blocker(floorComboBox_);
            floorComboBox_->setCurrentIndex(index);
        }
    }
    update();
}

void SimulationCanvasWidget::setupFloorSelector() {
    if (layout_.floors.size() <= 1) {
        return;
    }

    floorSelectorFrame_ = new QFrame(this);
    floorSelectorFrame_->setObjectName("simulationFloorSelector");
    ui::polishSimulationFloorSelector(floorSelectorFrame_);

    auto* layout = new QHBoxLayout(floorSelectorFrame_);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(8);
    auto* label = new QLabel("Floor", floorSelectorFrame_);
    floorComboBox_ = new QComboBox(floorSelectorFrame_);
    for (const auto& floor : layout_.floors) {
        const auto id = QString::fromStdString(floor.id);
        const auto labelText = floor.label.empty()
            ? id
            : QString("%1 (%2)").arg(QString::fromStdString(floor.label), id);
        floorComboBox_->addItem(labelText, id);
    }
    const auto currentIndex = floorComboBox_->findData(QString::fromStdString(currentFloorId_));
    if (currentIndex >= 0) {
        floorComboBox_->setCurrentIndex(currentIndex);
    }
    layout->addWidget(label);
    layout->addWidget(floorComboBox_);

    connect(floorComboBox_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (floorComboBox_ == nullptr || index < 0) {
            return;
        }
        setCurrentFloorId(floorComboBox_->itemData(index).toString().toStdString(), true);
    });

    floorSelectorFrame_->adjustSize();
    repositionFloorSelector();
    floorSelectorFrame_->raise();
}

void SimulationCanvasWidget::repositionFloorSelector() {
    if (floorSelectorFrame_ == nullptr) {
        return;
    }
    floorSelectorFrame_->adjustSize();
    floorSelectorFrame_->move(kFloorSelectorMargin, kFloorSelectorMargin);
    floorSelectorFrame_->raise();
}

}  // namespace safecrowd::application
