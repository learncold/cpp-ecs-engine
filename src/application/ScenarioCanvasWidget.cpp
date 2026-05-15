#include "application/ScenarioCanvasWidget.h"

#include "application/ToolIconResources.h"
#include "application/UiStyle.h"
#include "domain/GeometryQueries.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <random>

#include <QCoreApplication>
#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

constexpr int kTopToolbarHeight = 44;
constexpr int kPropertyPanelHeight = 42;
constexpr int kToolbarButtonSize = 44;
constexpr double kViewportPadding = 24.0;
constexpr double kDefaultInitialSpeed = 1.3;
constexpr double kOccupantMarkerRadius = 5.0;
constexpr double kOccupantWorldRadius = 0.25;
constexpr double kOccupantMinSpacing = kOccupantWorldRadius * 2.0;
constexpr int kMaxSourceOccupantCount = 5000;
constexpr int kDefaultSourceAgentsPerSpawn = 1;
constexpr double kDefaultSourceStartSeconds = 0.0;
constexpr double kDefaultSourceDurationSeconds = 180.0;
constexpr double kDefaultSourceIntervalSeconds = 5.0;
constexpr double kGeometryEpsilon = 1e-9;
constexpr double kSelectionDragThresholdPixels = 4.0;
const QColor kSelectionHighlightColor("#0b3d78");

[[nodiscard]] safecrowd::domain::Point2D polygonCenter(const safecrowd::domain::Polygon2D& polygon);

using safecrowd::domain::distancePointToSegment;
using safecrowd::domain::pointInPolygon;
using safecrowd::domain::pointInRing;
using safecrowd::domain::representativePointInPolygon;

struct PointBounds {
    double minX{0.0};
    double minY{0.0};
    double maxX{0.0};
    double maxY{0.0};
};

struct OccupantSourceSettings {
    int agentsPerSpawn{kDefaultSourceAgentsPerSpawn};
    double startSeconds{kDefaultSourceStartSeconds};
    double durationSeconds{kDefaultSourceDurationSeconds};
    double intervalSeconds{kDefaultSourceIntervalSeconds};
};

bool matchesFloor(const std::string& elementFloorId, const QString& floorId) {
    return floorId.isEmpty() || elementFloorId.empty() || QString::fromStdString(elementFloorId) == floorId;
}

int sourceEmissionCount(int agentsPerSpawn, double durationSeconds, double intervalSeconds) {
    if (agentsPerSpawn <= 0 || durationSeconds <= 0.0 || intervalSeconds <= 1e-9) {
        return 0;
    }
    const auto ticks = static_cast<long long>(
        std::floor(std::max(0.0, durationSeconds - 1e-9) / intervalSeconds)) + 1;
    const auto count = std::max<long long>(0, ticks) * static_cast<long long>(agentsPerSpawn);
    return static_cast<int>(std::min<long long>(kMaxSourceOccupantCount, count));
}

bool editOccupantSourceSettings(
    QWidget* parent,
    OccupantSourceSettings* settings,
    const QPoint& screenPosition,
    const QString& title) {
    if (settings == nullptr) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    auto* layout = new QGridLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setHorizontalSpacing(12);
    layout->setVerticalSpacing(10);

    auto* peopleSpin = new QSpinBox(&dialog);
    peopleSpin->setRange(1, kMaxSourceOccupantCount);
    peopleSpin->setSuffix(" people");
    peopleSpin->setValue(std::max(1, settings->agentsPerSpawn));

    auto* intervalSpin = new QDoubleSpinBox(&dialog);
    intervalSpin->setRange(0.1, 3600.0);
    intervalSpin->setDecimals(1);
    intervalSpin->setSuffix(" sec");
    intervalSpin->setValue(std::max(0.1, settings->intervalSeconds));

    auto* durationSpin = new QDoubleSpinBox(&dialog);
    durationSpin->setRange(0.1, 1440.0);
    durationSpin->setDecimals(1);
    durationSpin->setSuffix(" min");
    durationSpin->setValue(std::max(0.1, settings->durationSeconds / 60.0));

    auto* totalLabel = new QLabel(&dialog);
    totalLabel->setStyleSheet("QLabel { color: #4f5d6b; }");
    const auto refreshSummary = [=]() {
        const auto total = sourceEmissionCount(
            peopleSpin->value(),
            durationSpin->value() * 60.0,
            intervalSpin->value());
        totalLabel->setText(QString("Total emitted: %1 people").arg(total));
    };
    refreshSummary();
    QObject::connect(peopleSpin, qOverload<int>(&QSpinBox::valueChanged), &dialog, refreshSummary);
    QObject::connect(intervalSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, refreshSummary);
    QObject::connect(durationSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), &dialog, refreshSummary);

    layout->addWidget(new QLabel("People each time", &dialog), 0, 0);
    layout->addWidget(peopleSpin, 0, 1);
    layout->addWidget(new QLabel("Every", &dialog), 1, 0);
    layout->addWidget(intervalSpin, 1, 1);
    layout->addWidget(new QLabel("Duration", &dialog), 2, 0);
    layout->addWidget(durationSpin, 2, 1);
    layout->addWidget(totalLabel, 3, 0, 1, 2);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons, 4, 0, 1, 2);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.move(screenPosition);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    settings->agentsPerSpawn = peopleSpin->value();
    settings->intervalSeconds = intervalSpin->value();
    settings->durationSeconds = durationSpin->value() * 60.0;
    return true;
}

safecrowd::domain::Point2D connectionMarkerCenter(const safecrowd::domain::Connection2D& connection) {
    return {
        .x = (connection.centerSpan.start.x + connection.centerSpan.end.x) * 0.5,
        .y = (connection.centerSpan.start.y + connection.centerSpan.end.y) * 0.5,
    };
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

    const auto doorCenter = connectionMarkerCenter(connection);

    const auto pickNearest = [&](bool sameFloorOnly) -> std::string {
        double bestDistanceSq = std::numeric_limits<double>::infinity();
        const safecrowd::domain::Zone2D* bestZone = nullptr;
        for (const auto& zone : layout.zones) {
            if (zone.kind != safecrowd::domain::ZoneKind::Exit) {
                continue;
            }
            if (sameFloorOnly && !connection.floorId.empty() && !zone.floorId.empty() && zone.floorId != connection.floorId) {
                continue;
            }

            const auto exitCenter = polygonCenter(zone.area);
            const auto dx = exitCenter.x - doorCenter.x;
            const auto dy = exitCenter.y - doorCenter.y;
            const auto d2 = (dx * dx) + (dy * dy);
            if (d2 < bestDistanceSq) {
                bestDistanceSq = d2;
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

QString formatConnectionBlockTooltip(const safecrowd::domain::ConnectionBlockDraft& block) {
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
        const auto end = std::max(start, interval.endSeconds);
        text.append(QString("\n- %1s ~ %2s").arg(start, 0, 'f', 1).arg(end, 0, 'f', 1));
    }
    return text;
}

std::optional<std::size_t> hoveredConnectionBlockIndex(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    const LayoutCanvasTransform& transform,
    const QString& currentFloorId,
    const QPointF& screenPosition) {
    constexpr double kHoverRadiusPixels = 14.0;

    std::optional<std::size_t> closestIndex;
    double closestDistanceSq = kHoverRadiusPixels * kHoverRadiusPixels;

    for (std::size_t index = 0; index < blocks.size(); ++index) {
        const auto& block = blocks[index];
        if (block.connectionId.empty()) {
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

        const auto center = transform.map({.x = (it->centerSpan.start.x + it->centerSpan.end.x) * 0.5,
                                          .y = (it->centerSpan.start.y + it->centerSpan.end.y) * 0.5});
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
    return text;
}

std::optional<std::size_t> hoveredEnvironmentHazardIndex(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<safecrowd::domain::EnvironmentHazardDraft>& hazards,
    const LayoutCanvasTransform& transform,
    const QString& currentFloorId,
    const QPointF& screenPosition) {
    constexpr double kHoverRadiusPixels = 15.0;

    std::optional<std::size_t> closestIndex;
    double closestDistanceSq = kHoverRadiusPixels * kHoverRadiusPixels;
    for (std::size_t index = 0; index < hazards.size(); ++index) {
        const auto& hazard = hazards[index];
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

QString formatRouteGuidanceTooltip(
    const safecrowd::domain::RouteGuidanceDraft& guidance) {
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
    text.append(QString("\n Base compliance: %1").arg(std::clamp(guidance.baseComplianceRate, 0.0, 1.0), 0, 'f', 2));
    text.append(QString("\n Strength: %1").arg(std::clamp(guidance.guidanceStrength, 0.0, 1.0), 0, 'f', 2));
    text.append(QString("\n Max detour:%1m").arg(std::max(0.0, guidance.maxDetourMeters), 0, 'f', 1));
    return text;
}

std::optional<QPointF> routeGuidanceMarkerCenter(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::RouteGuidanceDraft& guidance,
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    const LayoutCanvasTransform& transform,
    const QString& currentFloorId) {
    std::optional<QPointF> center;
    if (!guidance.installConnectionId.empty()) {
        const auto it = std::find_if(layout.connections.begin(), layout.connections.end(), [&](const auto& connection) {
            return connection.id == guidance.installConnectionId;
        });
        if (it == layout.connections.end()) {
            return std::nullopt;
        }
        if (!matchesFloor(it->floorId, currentFloorId)) {
            return std::nullopt;
        }
        center = transform.map(connectionMarkerCenter(*it));
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

    if (!center.has_value() || blocks.empty()) {
        return center;
    }

    constexpr double kMinSeparationPixels = 28.0;
    constexpr double kStackOffsetPixels = 34.0;

    std::vector<QPointF> blockedCenters;
    blockedCenters.reserve(blocks.size());
    for (const auto& block : blocks) {
        if (block.connectionId.empty()) {
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
        blockedCenters.push_back(transform.map(connectionMarkerCenter(*it)));
    }
    if (blockedCenters.empty()) {
        return center;
    }

    const auto minDistanceToBlocks = [&](const QPointF& candidate) {
        double minDistance = std::numeric_limits<double>::infinity();
        for (const auto& blocked : blockedCenters) {
            minDistance = std::min(minDistance, QLineF(candidate, blocked).length());
        }
        return minDistance;
    };

    const auto baseMinDistance = minDistanceToBlocks(*center);
    if (baseMinDistance >= kMinSeparationPixels) {
        return center;
    }

    const QPointF up = *center + QPointF(0.0, -kStackOffsetPixels);
    const QPointF down = *center + QPointF(0.0, kStackOffsetPixels);
    const auto upDistance = minDistanceToBlocks(up);
    const auto downDistance = minDistanceToBlocks(down);
    return upDistance >= downDistance ? up : down;
}

std::optional<std::size_t> hoveredRouteGuidanceIndex(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::vector<safecrowd::domain::RouteGuidanceDraft>& guidances,
    const std::vector<safecrowd::domain::ConnectionBlockDraft>& blocks,
    const LayoutCanvasTransform& transform,
    const QString& currentFloorId,
    const QPointF& screenPosition) {
    constexpr double kHoverRadiusPixels = 14.0;

    std::optional<std::size_t> closestIndex;
    double closestDistanceSq = kHoverRadiusPixels * kHoverRadiusPixels;

    for (std::size_t index = 0; index < guidances.size(); ++index) {
        const auto& guidance = guidances[index];
        const auto center = routeGuidanceMarkerCenter(layout, guidance, blocks, transform, currentFloorId);
        if (!center.has_value()) {
            continue;
        }

        const auto dx = center->x() - screenPosition.x();
        const auto dy = center->y() - screenPosition.y();
        const auto distanceSq = (dx * dx) + (dy * dy);
        if (distanceSq <= closestDistanceSq) {
            closestDistanceSq = distanceSq;
            closestIndex = index;
        }
    }

    return closestIndex;
}

QString defaultFloorId(const safecrowd::domain::FacilityLayout2D& layout) {
    if (!layout.floors.empty() && !layout.floors.front().id.empty()) {
        return QString::fromStdString(layout.floors.front().id);
    }
    if (!layout.levelId.empty()) {
        return QString::fromStdString(layout.levelId);
    }
    return {};
}

safecrowd::domain::Point2D polygonCenter(const safecrowd::domain::Polygon2D& polygon) {
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

safecrowd::domain::Point2D placementCenter(const std::vector<safecrowd::domain::Point2D>& area) {
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

bool pointInsidePlacementArea(
    const std::vector<safecrowd::domain::Point2D>& area,
    const safecrowd::domain::Point2D& point) {
    return area.size() < 3 || pointInRing(area, point) || std::any_of(area.begin(), area.end(), [&](const auto& vertex) {
        return std::hypot(vertex.x - point.x, vertex.y - point.y) <= kGeometryEpsilon;
    });
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
    if (!pointInsidePlacementArea(area, point) || blocked(point) || overlapsExistingPoint(point, positions)) {
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
        const auto center = placementCenter(area);
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

std::vector<safecrowd::domain::Point2D> fallbackDisplayPositions(const ScenarioCrowdPlacement& placement) {
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

QRectF groupMarkerBounds(const ScenarioCrowdPlacement& placement, const LayoutCanvasTransform& transform) {
    const auto positions = fallbackDisplayPositions(placement);
    if (positions.empty()) {
        return {};
    }

    QRectF bounds;
    for (const auto& worldPoint : positions) {
        const auto point = transform.map(worldPoint);
        const QRectF markerBounds(
            point - QPointF(kOccupantMarkerRadius, kOccupantMarkerRadius),
            QSizeF(kOccupantMarkerRadius * 2.0, kOccupantMarkerRadius * 2.0));
        bounds = bounds.isNull() ? markerBounds : bounds.united(markerBounds);
    }
    return bounds.adjusted(-7.0, -7.0, 7.0, 7.0);
}

void drawOccupantSourceMarker(QPainter& painter, const QPointF& center, const QColor& color) {
    painter.setPen(QPen(color, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(color.red(), color.green(), color.blue(), 36));
    painter.drawEllipse(center, 9.0, 9.0);
    painter.setBrush(color);
    painter.drawEllipse(center, 3.8, 3.8);
    painter.drawLine(center + QPointF(11.0, 0.0), center + QPointF(18.0, 0.0));
    painter.drawLine(center + QPointF(18.0, 0.0), center + QPointF(14.0, -4.0));
    painter.drawLine(center + QPointF(18.0, 0.0), center + QPointF(14.0, 4.0));
}

QString placementIdFromCrowdElementId(const QString& crowdElementId) {
    return crowdElementId.section('/', 0, 0);
}

std::optional<int> occupantIndexFromCrowdElementId(const QString& crowdElementId, const QString& placementId) {
    const auto prefix = QString("%1/occupant-").arg(placementId);
    if (!crowdElementId.startsWith(prefix)) {
        return std::nullopt;
    }

    bool ok = false;
    const auto oneBasedIndex = crowdElementId.mid(prefix.size()).toInt(&ok);
    if (!ok || oneBasedIndex <= 0) {
        return std::nullopt;
    }
    return oneBasedIndex - 1;
}

QString scenarioToolIconResourcePath(const QString& type) {
    if (type == "select") {
        return QStringLiteral(":/tool-icons/scenario-authoring/select.svg");
    }
    if (type == "individual") {
        return QStringLiteral(":/tool-icons/scenario-authoring/add-individual-occupant.svg");
    }
    if (type == "group") {
        return QStringLiteral(":/tool-icons/scenario-authoring/add-occupant-group.svg");
    }
    if (type == "block") {
        return QStringLiteral(":/tool-icons/scenario-authoring/block-door.svg");
    }
    return {};
}

QIcon makeToolIcon(const QString& type, const QColor& color) {
    const auto resourcePath = scenarioToolIconResourcePath(type);
    if (!resourcePath.isEmpty()) {
        const auto icon = makeSvgToolIcon(resourcePath, color, QSize(22, 22));
        if (!icon.isNull()) {
            return icon;
        }
    }

    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (type == "select") {
        QPolygonF pointer;
        pointer << QPointF(14, 10) << QPointF(30, 22) << QPointF(22, 25) << QPointF(18, 35);
        painter.drawPolygon(pointer);
        return QIcon(pixmap);
    }
    if (type == "individual") {
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(22, 15), 5.0, 5.0);
        painter.drawRoundedRect(QRectF(16, 23, 12, 12), 5, 5);
        return QIcon(pixmap);
    }

    if (type == "source") {
        painter.setPen(QPen(color, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        QPainterPath platform;
        platform.moveTo(9.0, 35.0);
        platform.lineTo(30.0, 35.0);
        platform.lineTo(35.0, 40.0);
        platform.lineTo(4.0, 40.0);
        platform.closeSubpath();
        painter.drawPath(platform);

        painter.drawEllipse(QPointF(18, 12), 6.0, 6.0);
        QPainterPath body;
        body.moveTo(10.0, 33.0);
        body.lineTo(10.0, 25.0);
        body.cubicTo(10.0, 19.5, 13.5, 17.0, 18.0, 17.0);
        body.cubicTo(22.5, 17.0, 26.0, 19.5, 26.0, 25.0);
        body.lineTo(26.0, 33.0);
        painter.drawPath(body);

        painter.drawLine(QPointF(32.0, 26.0), QPointF(41.0, 26.0));
        painter.drawLine(QPointF(36.5, 21.5), QPointF(36.5, 30.5));
        return QIcon(pixmap);
    }

    if (type == "block") {
        painter.setPen(QPen(color, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(22, 22), 11.5, 11.5);
        painter.drawLine(QPointF(14.5, 29.5), QPointF(29.5, 14.5));
        return QIcon(pixmap);
    }

    if (type == "guidance") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.save();
        painter.translate(QPointF(22, 22));
        painter.rotate(-25.0);
        painter.translate(QPointF(-22, -22));
        painter.drawRoundedRect(QRectF(18, 8, 10, 20), 3.0, 3.0);
        painter.drawRoundedRect(QRectF(19, 28, 8, 9), 2.5, 2.5);
        painter.restore();

        painter.setPen(QPen(color, 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(32, 10), QPointF(36, 6));
        painter.drawLine(QPointF(34, 15), QPointF(39, 13));
        painter.drawLine(QPointF(29, 7), QPointF(31, 2));
        return QIcon(pixmap);
    }

    if (type == "fire") {
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        QPainterPath flame;
        flame.moveTo(22.0, 34.0);
        flame.cubicTo(13.0, 28.0, 17.0, 17.0, 22.0, 10.0);
        flame.cubicTo(31.0, 17.0, 31.0, 28.0, 22.0, 34.0);
        painter.drawPath(flame);
        painter.setBrush(Qt::white);
        QPainterPath core;
        core.moveTo(22.0, 31.0);
        core.cubicTo(18.0, 27.0, 20.0, 22.0, 23.0, 18.0);
        core.cubicTo(26.0, 22.0, 26.0, 28.0, 22.0, 31.0);
        painter.drawPath(core);
        return QIcon(pixmap);
    }

    if (type == "smoke") {
        painter.setPen(QPen(color, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawArc(QRectF(10, 23, 18, 10), 20 * 16, 220 * 16);
        painter.drawArc(QRectF(18, 17, 17, 10), 20 * 16, 220 * 16);
        painter.drawArc(QRectF(12, 11, 15, 9), 20 * 16, 220 * 16);
        return QIcon(pixmap);
    }

    if (type != "group") {
        return QIcon(pixmap);
    }

    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(17, 15), 4.0, 4.0);
    painter.drawEllipse(QPointF(27, 15), 4.0, 4.0);
    painter.drawEllipse(QPointF(22, 25), 4.0, 4.0);
    painter.drawRoundedRect(QRectF(12, 30, 20, 5), 2.5, 2.5);
    return QIcon(pixmap);
}

double intervalSecondsFrom(int value, const QString& unit) {
    if (unit == "hour") {
        return static_cast<double>(value) * 3600.0;
    }
    if (unit == "min") {
        return static_cast<double>(value) * 60.0;
    }
    return static_cast<double>(value);
}

QStringList intervalUnitOptions() {
    return {"sec", "min", "hour"};
}

class ConnectionBlockScheduleDialog final : public QDialog {
public:
    explicit ConnectionBlockScheduleDialog(
        std::vector<safecrowd::domain::ConnectionBlockIntervalDraft> intervals,
        QWidget* parent = nullptr)
        : QDialog(parent),
          intervals_(std::move(intervals)) {
        setWindowTitle("Block door schedule");
        setModal(true);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(10);

        auto* caption = new QLabel("Add one or more block/unblock intervals.", this);
        caption->setWordWrap(true);
        root->addWidget(caption);

        rowsContainer_ = new QWidget(this);
        rowsLayout_ = new QVBoxLayout(rowsContainer_);
        rowsLayout_->setContentsMargins(0, 0, 0, 0);
        rowsLayout_->setSpacing(8);
        root->addWidget(rowsContainer_);

        auto* addRowButton = new QPushButton("+", this);
        addRowButton->setToolTip("Add interval");
        addRowButton->setFixedSize(36, 32);
        root->addWidget(addRowButton, 0, Qt::AlignLeft);
        connect(addRowButton, &QPushButton::clicked, this, [this]() {
            addRow({});
        });

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            if (!applyFromUi()) {
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, [this]() {
            reject();
        });

        if (intervals_.empty()) {
            addRow({});
        } else {
            for (const auto& interval : intervals_) {
                addRow(interval);
            }
        }
    }

    std::vector<safecrowd::domain::ConnectionBlockIntervalDraft> intervals() const {
        return intervals_;
    }

private:
    struct Row {
        QWidget* container{nullptr};
        QSpinBox* startValue{nullptr};
        QComboBox* startUnit{nullptr};
        QSpinBox* endValue{nullptr};
        QComboBox* endUnit{nullptr};
        QPushButton* removeButton{nullptr};
    };

    static int clampIntSeconds(double seconds) {
        if (!std::isfinite(seconds) || seconds < 0.0) {
            return 0;
        }
        const auto value = static_cast<long long>(std::llround(seconds));
        return static_cast<int>(std::clamp<long long>(value, 0, 1'000'000'000LL));
    }

    void addRow(const safecrowd::domain::ConnectionBlockIntervalDraft& interval) {
        auto* row = new QWidget(rowsContainer_);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        auto* startLabel = new QLabel("Start", row);
        layout->addWidget(startLabel);

        auto* startValue = new QSpinBox(row);
        startValue->setRange(0, 1'000'000'000);
        startValue->setValue(clampIntSeconds(interval.startSeconds));
        layout->addWidget(startValue);

        auto* startUnit = new QComboBox(row);
        startUnit->addItems(intervalUnitOptions());
        startUnit->setCurrentText("sec");
        layout->addWidget(startUnit);

        auto* endLabel = new QLabel("End", row);
        layout->addWidget(endLabel);

        auto* endValue = new QSpinBox(row);
        endValue->setRange(0, 1'000'000'000);
        endValue->setValue(clampIntSeconds(interval.endSeconds));
        layout->addWidget(endValue);

        auto* endUnit = new QComboBox(row);
        endUnit->addItems(intervalUnitOptions());
        endUnit->setCurrentText("sec");
        layout->addWidget(endUnit);

        auto* remove = new QPushButton("-", row);
        remove->setToolTip("Remove interval");
        remove->setFixedSize(32, 32);
        layout->addWidget(remove);

        Row widgets{
            .container = row,
            .startValue = startValue,
            .startUnit = startUnit,
            .endValue = endValue,
            .endUnit = endUnit,
            .removeButton = remove,
        };
        rows_.push_back(widgets);
        rowsLayout_->addWidget(row);

        connect(remove, &QPushButton::clicked, this, [this, row]() {
            rows_.erase(std::remove_if(rows_.begin(), rows_.end(), [&](const Row& r) { return r.container == row; }), rows_.end());
            row->deleteLater();
        });
    }

    bool applyFromUi() {
        std::vector<safecrowd::domain::ConnectionBlockIntervalDraft> intervals;
        intervals.reserve(rows_.size());
        for (const auto& row : rows_) {
            if (row.startValue == nullptr || row.startUnit == nullptr || row.endValue == nullptr || row.endUnit == nullptr) {
                continue;
            }
            const auto startSeconds = intervalSecondsFrom(row.startValue->value(), row.startUnit->currentText());
            const auto endSeconds = intervalSecondsFrom(row.endValue->value(), row.endUnit->currentText());
            if (endSeconds < startSeconds) {
                QMessageBox::warning(this, "Invalid interval", "End time must be greater than or equal to start time.");
                return false;
            }
            intervals.push_back({
                .startSeconds = startSeconds,
                .endSeconds = endSeconds,
            });
        }
        intervals_ = std::move(intervals);
        return true;
    }

    QWidget* rowsContainer_{nullptr};
    QVBoxLayout* rowsLayout_{nullptr};
    std::vector<Row> rows_{};
    std::vector<safecrowd::domain::ConnectionBlockIntervalDraft> intervals_{};
};

class RouteGuidanceSettingsDialog final : public QDialog {
public:
    explicit RouteGuidanceSettingsDialog(
        safecrowd::domain::RouteGuidanceDraft guidance,
        QWidget* parent = nullptr)
        : QDialog(parent),
          guidance_(std::move(guidance)) {
        setWindowTitle("Route guidance settings");
        setModal(true);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(10);

        auto* caption = new QLabel(
            "Route guidance settings.\n"
            "Period defines when the event is active.\n"
            "Parameters control how strongly agents follow the guidance.\n"
            "Target exit is selected by clicking an exit or a door on the canvas.",
            this);
        caption->setWordWrap(true);
        root->addWidget(caption);

        auto* form = new QWidget(this);
        auto* formLayout = new QGridLayout(form);
        formLayout->setContentsMargins(0, 0, 0, 0);
        formLayout->setHorizontalSpacing(10);
        formLayout->setVerticalSpacing(8);
        root->addWidget(form);

        int row = 0;
        periodRowsContainer_ = nullptr;
        alwaysPeriodLabel_ = nullptr;

        {
            auto* title = new QLabel("Period", this);
            title->setStyleSheet("QLabel { font-weight: 600; color: #16202b; }");
            formLayout->addWidget(title, row * 2, 0, Qt::AlignLeft);

            periodRowsContainer_ = new QWidget(this);
            periodRowsLayout_ = new QVBoxLayout(periodRowsContainer_);
            periodRowsLayout_->setContentsMargins(0, 0, 0, 0);
            periodRowsLayout_->setSpacing(6);
            formLayout->addWidget(periodRowsContainer_, row * 2, 1, Qt::AlignLeft);

            alwaysPeriodLabel_ = new QLabel("Always (no periods configured).", this);
            alwaysPeriodLabel_->setStyleSheet("QLabel { color: #4f5d6b; }");
            periodRowsLayout_->addWidget(alwaysPeriodLabel_);

            auto* helpRow = new QWidget(this);
            auto* helpLayout = new QHBoxLayout(helpRow);
            helpLayout->setContentsMargins(0, 0, 0, 0);
            helpLayout->setSpacing(8);

            auto* helpLabel = new QLabel("Active time range for this guidance event.", this);
            helpLabel->setWordWrap(true);
            helpLabel->setStyleSheet("QLabel { color: #4f5d6b; font-size: 12px; }");
            helpLayout->addWidget(helpLabel, 1);

            addPeriodButton_ = new QPushButton("+", this);
            addPeriodButton_->setToolTip("Add a period interval.");
            addPeriodButton_->setStyleSheet(ui::secondaryButtonStyleSheet());
            {
                auto font = addPeriodButton_->font();
                font.setPointSize(std::max(9, font.pointSize() - 1));
                addPeriodButton_->setFont(font);
            }
            helpLayout->addWidget(addPeriodButton_, 0);

            removePeriodButton_ = new QPushButton("-", this);
            removePeriodButton_->setToolTip("Remove the last period interval.");
            removePeriodButton_->setStyleSheet(ui::secondaryButtonStyleSheet());
            {
                auto font = removePeriodButton_->font();
                font.setPointSize(std::max(9, font.pointSize() - 1));
                removePeriodButton_->setFont(font);
            }
            helpLayout->addWidget(removePeriodButton_, 0);

            formLayout->addWidget(helpRow, (row * 2) + 1, 0, 1, 2);

            connect(addPeriodButton_, &QPushButton::clicked, this, [this]() {
                addPeriodRow(std::nullopt);
            });
            connect(removePeriodButton_, &QPushButton::clicked, this, [this]() {
                removeLastPeriodRow();
            });

            // Seed UI rows from existing guidance data.
            if (!guidance_.periods.empty()) {
                for (const auto& period : guidance_.periods) {
                    addPeriodRow(period);
                }
            } else if (guidance_.endSeconds > 0.0 || guidance_.startSeconds > 0.0) {
                addPeriodRow(safecrowd::domain::RouteGuidancePeriodDraft{
                    .startSeconds = guidance_.startSeconds,
                    .endSeconds = guidance_.endSeconds,
                });
            }

            refreshPeriodUiState();
            row++;
        }

        auto* paramsHeader = new QLabel("Parameters", this);
        paramsHeader->setStyleSheet("QLabel { font-weight: 600; color: #16202b; }");
        formLayout->addWidget(paramsHeader, row * 2, 0, 1, 2, Qt::AlignLeft);
        row++;

        baseComplianceRate_ = addField(
            formLayout,
            row++,
            "Base compliance",
            "Baseline compliance (0~1). Target group-average probability of following the guidance.",
            0.0,
            1.0,
            0.01,
            2,
            std::clamp(guidance_.baseComplianceRate, 0.0, 1.0));

        guidanceStrength_ = addField(
            formLayout,
            row++,
            "Guidance strength",
            "Guidance strength (0~1). Examples: signage 0.25 / broadcast 0.55 / staff control 0.85.",
            0.0,
            1.0,
            0.01,
            2,
            std::clamp(guidance_.guidanceStrength, 0.0, 1.0));

        maxDetourMeters_ = addField(
            formLayout,
            row++,
            "Max detour (m)",
            "Max detour tolerance in meters. Larger detours reduce compliance.",
            0.0,
            10'000.0,
            1.0,
            1,
            std::max(0.0, guidance_.maxDetourMeters));

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        root->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            if (!applyFromUi()) {
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, [this]() {
            reject();
        });
    }

    safecrowd::domain::RouteGuidanceDraft guidance() const {
        return guidance_;
    }

private:
    struct PeriodRowWidgets {
        QWidget* root{nullptr};
        QDoubleSpinBox* start{nullptr};
        QDoubleSpinBox* end{nullptr};
        QString startRaw{};
        QString endRaw{};
    };

    void refreshPeriodUiState() {
        const bool hasPeriods = !periodRows_.empty();
        if (alwaysPeriodLabel_ != nullptr) {
            alwaysPeriodLabel_->setVisible(!hasPeriods);
        }
        if (removePeriodButton_ != nullptr) {
            removePeriodButton_->setEnabled(hasPeriods);
        }
    }

    void addPeriodRow(std::optional<safecrowd::domain::RouteGuidancePeriodDraft> period) {
        if (periodRowsContainer_ == nullptr || periodRowsLayout_ == nullptr) {
            return;
        }

        auto row = std::make_unique<PeriodRowWidgets>();
        row->root = new QWidget(this);
        auto* layout = new QHBoxLayout(row->root);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        auto* startLabel = new QLabel("Start", this);
        startLabel->setStyleSheet("QLabel { color: #4f5d6b; }");
        layout->addWidget(startLabel);

        row->start = new QDoubleSpinBox(this);
        row->start->setRange(0.0, 1'000'000.0);
        row->start->setDecimals(1);
        row->start->setSingleStep(0.5);
        row->start->setMinimumWidth(120);
        row->start->setValue(period.has_value() ? std::max(0.0, period->startSeconds) : 0.0);
        layout->addWidget(row->start);

        auto* startUnit = new QLabel("sec", this);
        startUnit->setStyleSheet("QLabel { color: #4f5d6b; }");
        layout->addWidget(startUnit);

        auto* endLabel = new QLabel("End", this);
        endLabel->setStyleSheet("QLabel { color: #4f5d6b; }");
        layout->addWidget(endLabel);

        row->end = new QDoubleSpinBox(this);
        row->end->setRange(0.0, 1'000'000.0);
        row->end->setDecimals(1);
        row->end->setSingleStep(0.5);
        row->end->setMinimumWidth(120);
        const auto seedEnd = period.has_value()
            ? std::max(std::max(0.0, period->startSeconds), std::max(0.0, period->endSeconds))
            : 10.0;
        row->end->setValue(seedEnd);
        layout->addWidget(row->end);

        auto* endUnit = new QLabel("sec", this);
        endUnit->setStyleSheet("QLabel { color: #4f5d6b; }");
        layout->addWidget(endUnit);

        layout->addStretch(1);

        periodRowsLayout_->addWidget(row->root);
        row->startRaw = row->start->text();
        if (auto* edit = row->start->findChild<QLineEdit*>(); edit != nullptr) {
            connect(edit, &QLineEdit::textEdited, this, [rowPtr = row.get()](const QString& text) {
                if (rowPtr != nullptr) {
                    rowPtr->startRaw = text;
                }
            });
        }
        row->endRaw = row->end->text();
        if (auto* edit = row->end->findChild<QLineEdit*>(); edit != nullptr) {
            connect(edit, &QLineEdit::textEdited, this, [rowPtr = row.get()](const QString& text) {
                if (rowPtr != nullptr) {
                    rowPtr->endRaw = text;
                }
            });
        }

        periodRows_.push_back(std::move(row));

        refreshPeriodUiState();
    }

    void removeLastPeriodRow() {
        if (periodRows_.empty()) {
            return;
        }
        auto row = std::move(periodRows_.back());
        periodRows_.pop_back();
        if (row != nullptr && row->root != nullptr) {
            row->root->deleteLater();
        }
        refreshPeriodUiState();
    }

    QDoubleSpinBox* addField(
        QGridLayout* layout,
        int row,
        const QString& label,
        const QString& help,
        double min,
        double max,
        double step,
        int decimals,
        double value) {
        auto* title = new QLabel(label, this);
        title->setStyleSheet("QLabel { color: #16202b; }");
        layout->addWidget(title, row * 2, 0, Qt::AlignLeft);

        auto* spin = new QDoubleSpinBox(this);
        spin->setRange(min, max);
        spin->setDecimals(decimals);
        spin->setSingleStep(step);
        spin->setValue(value);
        spin->setMinimumWidth(140);
        spin->setToolTip(help);
        layout->addWidget(spin, row * 2, 1, Qt::AlignLeft);

        auto* helpLabel = new QLabel(help, this);
        helpLabel->setWordWrap(true);
        helpLabel->setStyleSheet("QLabel { color: #4f5d6b; font-size: 12px; }");
        layout->addWidget(helpLabel, (row * 2) + 1, 0, 1, 2);
        return spin;
    }

    bool applyFromUi() {
        auto validateNonNegative = [&](const QString& rawText, QDoubleSpinBox* spin, QString* rawOut) -> bool {
            if (rawText.isEmpty()) {
                return true;
            }
            bool ok = false;
            const auto typedValue = rawText.toDouble(&ok);
            if (ok && typedValue < 0.0) {
                QMessageBox::information(this, "Invalid value", "Only numbers greater than or equal to 0 can be entered.");
                spin->setValue(0.0);
                if (rawOut != nullptr) {
                    *rawOut = spin->text();
                }
                return false;
            }
            return true;
        };

        guidance_.periods.clear();
        for (const auto& rowPtr : periodRows_) {
            if (rowPtr == nullptr || rowPtr->start == nullptr || rowPtr->end == nullptr) {
                continue;
            }
            if (!validateNonNegative(rowPtr->startRaw, rowPtr->start, &rowPtr->startRaw)) {
                return false;
            }
            if (!validateNonNegative(rowPtr->endRaw, rowPtr->end, &rowPtr->endRaw)) {
                return false;
            }
            const auto start = std::max(0.0, rowPtr->start->value());
            const auto end = std::max(start, std::max(0.0, rowPtr->end->value()));
            guidance_.periods.push_back({.startSeconds = start, .endSeconds = end});
        }

        // Keep legacy scalar fields in sync for older views.
        if (!guidance_.periods.empty()) {
            guidance_.startSeconds = guidance_.periods.front().startSeconds;
            guidance_.endSeconds = guidance_.periods.front().endSeconds;
        } else {
            guidance_.startSeconds = 0.0;
            guidance_.endSeconds = 0.0;
        }

        guidance_.baseComplianceRate = std::clamp(baseComplianceRate_->value(), 0.0, 1.0);
        guidance_.guidanceStrength = std::clamp(guidanceStrength_->value(), 0.0, 1.0);
        guidance_.maxDetourMeters = std::max(0.0, maxDetourMeters_->value());
        return true;
    }

    safecrowd::domain::RouteGuidanceDraft guidance_{};
    QWidget* periodRowsContainer_{nullptr};
    QVBoxLayout* periodRowsLayout_{nullptr};
    QLabel* alwaysPeriodLabel_{nullptr};
    QPushButton* addPeriodButton_{nullptr};
    QPushButton* removePeriodButton_{nullptr};
    std::vector<std::unique_ptr<PeriodRowWidgets>> periodRows_{};
    QDoubleSpinBox* baseComplianceRate_{nullptr};
    QDoubleSpinBox* guidanceStrength_{nullptr};
    QDoubleSpinBox* maxDetourMeters_{nullptr};
};

}  // namespace

ScenarioCanvasWidget::ScenarioCanvasWidget(
    safecrowd::domain::FacilityLayout2D layout,
    QWidget* parent)
    : QWidget(parent),
      layout_(std::move(layout)) {
    currentFloorId_ = defaultFloorId(layout_);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(520, 360);
    setStyleSheet("QWidget { background: #f4f7fb; }");
    QCoreApplication::instance()->installEventFilter(this);
    setupToolbars();
}

ScenarioCanvasWidget::~ScenarioCanvasWidget() {
    if (auto* app = QCoreApplication::instance(); app != nullptr) {
        app->removeEventFilter(this);
    }
}

void ScenarioCanvasWidget::setPlacements(std::vector<ScenarioCrowdPlacement> placements) {
    placements_ = std::move(placements);
    const auto fallbackFloorId = currentFloorId_.isEmpty() ? defaultFloorId(layout_) : currentFloorId_;
    for (auto& placement : placements_) {
        if (placement.floorId.isEmpty()) {
            placement.floorId = fallbackFloorId;
        }
    }
    update();
}

void ScenarioCanvasWidget::setPlacementsChangedHandler(std::function<void(const std::vector<ScenarioCrowdPlacement>&)> handler) {
    placementsChangedHandler_ = std::move(handler);
}

void ScenarioCanvasWidget::setConnectionBlocks(std::vector<safecrowd::domain::ConnectionBlockDraft> blocks) {
    connectionBlocks_ = std::move(blocks);
    update();
}

void ScenarioCanvasWidget::setConnectionBlocksChangedHandler(std::function<void(const std::vector<safecrowd::domain::ConnectionBlockDraft>&)> handler) {
    connectionBlocksChangedHandler_ = std::move(handler);
}

void ScenarioCanvasWidget::setEnvironmentHazards(std::vector<safecrowd::domain::EnvironmentHazardDraft> hazards) {
    environmentHazards_ = std::move(hazards);
    update();
}

void ScenarioCanvasWidget::setEnvironmentHazardsChangedHandler(
    std::function<void(const std::vector<safecrowd::domain::EnvironmentHazardDraft>&)> handler) {
    environmentHazardsChangedHandler_ = std::move(handler);
}

void ScenarioCanvasWidget::setRouteGuidances(std::vector<safecrowd::domain::RouteGuidanceDraft> guidances) {
    routeGuidances_ = std::move(guidances);
    update();
}

void ScenarioCanvasWidget::setRouteGuidancesChangedHandler(
    std::function<void(const std::vector<safecrowd::domain::RouteGuidanceDraft>&)> handler) {
    routeGuidancesChangedHandler_ = std::move(handler);
}

void ScenarioCanvasWidget::setLayoutElementActivatedHandler(std::function<void(const QString&)> handler) {
    layoutElementActivatedHandler_ = std::move(handler);
}

void ScenarioCanvasWidget::setCrowdSelectionChangedHandler(std::function<void(const QString&)> handler) {
    crowdSelectionChangedHandler_ = std::move(handler);
}

void ScenarioCanvasWidget::focusLayoutElement(const QString& elementId) {
    if (elementId.startsWith("floor:")) {
        currentFloorId_ = elementId.mid(QString("floor:").size());
        focusedLayoutElementId_.clear();
        focusedCrowdElementId_.clear();
        focusedPlacementId_.clear();
        selectedPlacementIds_.clear();
        camera_.reset();
        update();
        return;
    }

    selectFloorForElement(elementId);
    focusedLayoutElementId_ = elementId;
    focusedCrowdElementId_.clear();
    focusedPlacementId_.clear();
    selectedPlacementIds_.clear();
    update();
}

void ScenarioCanvasWidget::activateLayoutElement(const QString& elementId) {
    focusLayoutElement(elementId);

    if (toolMode_ == ToolMode::BlockDoor) {
        const auto targetId = elementId.toStdString();
        const auto it = std::find_if(layout_.connections.begin(), layout_.connections.end(), [&](const auto& connection) {
            return connection.id == targetId;
        });
        if (it == layout_.connections.end()) {
            return;
        }
        addConnectionBlockForConnection(*it);
        return;
    }

    if (toolMode_ == ToolMode::FireHazard || toolMode_ == ToolMode::SmokeHazard) {
        const auto targetId = elementId.toStdString();
        const auto it = std::find_if(layout_.zones.begin(), layout_.zones.end(), [&](const auto& zone) {
            return zone.id == targetId;
        });
        if (it == layout_.zones.end()) {
            return;
        }
        addEnvironmentHazardForZone(
            *it,
            polygonCenter(it->area),
            toolMode_ == ToolMode::FireHazard
                ? safecrowd::domain::EnvironmentHazardKind::Fire
                : safecrowd::domain::EnvironmentHazardKind::Smoke);
        return;
    }

    if (toolMode_ == ToolMode::RouteGuidance) {
        const auto targetId = elementId.toStdString();
        const auto it = std::find_if(layout_.zones.begin(), layout_.zones.end(), [&](const auto& zone) {
            return zone.id == targetId;
        });
        if (it != layout_.zones.end() && it->kind == safecrowd::domain::ZoneKind::Exit) {
            addRouteGuidanceForExitZone(*it);
            return;
        }

        const auto connectionIt = std::find_if(layout_.connections.begin(), layout_.connections.end(), [&](const auto& connection) {
            return connection.id == targetId;
        });
        if (connectionIt == layout_.connections.end()) {
            return;
        }
        addRouteGuidanceForConnection(*connectionIt);
    }
}

void ScenarioCanvasWidget::focusPlacement(const QString& placementId) {
    focusedCrowdElementId_ = placementId;
    focusedPlacementId_ = placementId.section('/', 0, 0);
    selectedPlacementIds_ = focusedPlacementId_.isEmpty() ? QStringList{} : QStringList{focusedPlacementId_};
    focusedLayoutElementId_.clear();
    const auto it = std::find_if(placements_.begin(), placements_.end(), [this](const auto& placement) {
        return placement.id == focusedPlacementId_;
    });
    if (it != placements_.end() && !it->floorId.isEmpty() && it->floorId != currentFloorId_) {
        currentFloorId_ = it->floorId;
        camera_.reset();
    }
    update();
}

bool ScenarioCanvasWidget::deleteCrowdElementById(const QString& crowdElementId) {
    return deleteCrowdElement(crowdElementId);
}

bool ScenarioCanvasWidget::deleteConnectionBlockById(const QString& blockId) {
    auto it = std::find_if(connectionBlocks_.begin(), connectionBlocks_.end(), [&](const auto& block) {
        return QString::fromStdString(block.id) == blockId;
    });
    if (it == connectionBlocks_.end()) {
        return false;
    }

    connectionBlocks_.erase(it);
    emitConnectionBlocksChanged();
    update();
    return true;
}

bool ScenarioCanvasWidget::editConnectionBlockScheduleById(const QString& blockId) {
    auto it = std::find_if(connectionBlocks_.begin(), connectionBlocks_.end(), [&](const auto& block) {
        return QString::fromStdString(block.id) == blockId;
    });
    if (it == connectionBlocks_.end()) {
        return false;
    }

    ConnectionBlockScheduleDialog dialog(it->intervals, this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    it->intervals = dialog.intervals();
    emitConnectionBlocksChanged();
    update();
    return true;
}

bool ScenarioCanvasWidget::deleteEnvironmentHazardById(const QString& hazardId) {
    auto it = std::find_if(environmentHazards_.begin(), environmentHazards_.end(), [&](const auto& hazard) {
        return QString::fromStdString(hazard.id) == hazardId;
    });
    if (it == environmentHazards_.end()) {
        return false;
    }

    environmentHazards_.erase(it);
    emitEnvironmentHazardsChanged();
    update();
    return true;
}

bool ScenarioCanvasWidget::deleteRouteGuidanceById(const QString& guidanceId) {
    auto it = std::find_if(routeGuidances_.begin(), routeGuidances_.end(), [&](const auto& guidance) {
        return QString::fromStdString(guidance.id) == guidanceId;
    });
    if (it == routeGuidances_.end()) {
        return false;
    }

    routeGuidances_.erase(it);
    emitRouteGuidancesChanged();
    update();
    return true;
}

bool ScenarioCanvasWidget::editRouteGuidanceById(const QString& guidanceId) {
    auto it = std::find_if(routeGuidances_.begin(), routeGuidances_.end(), [&](const auto& guidance) {
        return QString::fromStdString(guidance.id) == guidanceId;
    });
    if (it == routeGuidances_.end()) {
        return false;
    }

    RouteGuidanceSettingsDialog dialog(*it, this);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    *it = dialog.guidance();
    emitRouteGuidancesChanged();
    update();
    return true;
}

bool ScenarioCanvasWidget::eventFilter(QObject* watched, QEvent* event) {
    (void)watched;
    camera_.handleGlobalKeyEvent(event);
    return QWidget::eventFilter(watched, event);
}

void ScenarioCanvasWidget::keyPressEvent(QKeyEvent* event) {
    if (camera_.handleKeyPress(event)) {
        return;
    }
    QWidget::keyPressEvent(event);
}

void ScenarioCanvasWidget::keyReleaseEvent(QKeyEvent* event) {
    if (camera_.handleKeyRelease(event)) {
        return;
    }
    QWidget::keyReleaseEvent(event);
}

void ScenarioCanvasWidget::leaveEvent(QEvent* event) {
    hoveredConnectionBlockId_.clear();
    hoveredEnvironmentHazardId_.clear();
    hoveredRouteGuidanceId_.clear();
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}

void ScenarioCanvasWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        camera_.reset();
        update();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ScenarioCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (camera_.updatePan(event)) {
        update();
        return;
    }

    if (dragging_) {
        if (!hoveredConnectionBlockId_.isEmpty() || !hoveredEnvironmentHazardId_.isEmpty() || !hoveredRouteGuidanceId_.isEmpty()) {
            hoveredConnectionBlockId_.clear();
            hoveredEnvironmentHazardId_.clear();
            hoveredRouteGuidanceId_.clear();
            QToolTip::hideText();
        }
        dragCurrent_ = event->position();
        update();
        event->accept();
        return;
    }
    if (selectionDragging_) {
        if (!hoveredConnectionBlockId_.isEmpty() || !hoveredEnvironmentHazardId_.isEmpty() || !hoveredRouteGuidanceId_.isEmpty()) {
            hoveredConnectionBlockId_.clear();
            hoveredEnvironmentHazardId_.clear();
            hoveredRouteGuidanceId_.clear();
            QToolTip::hideText();
        }
        selectionDragCurrent_ = event->position();
        update();
        event->accept();
        return;
    }

    if (const auto bounds = collectBounds(); bounds.has_value()) {
        const auto transform = currentTransform(*bounds);
        const auto hoveredGuidance = hoveredRouteGuidanceIndex(
            layout_,
            routeGuidances_,
            connectionBlocks_,
            transform,
            currentFloorId_,
            event->position());
        const auto hoveredHazard = hoveredEnvironmentHazardIndex(
            layout_,
            environmentHazards_,
            transform,
            currentFloorId_,
            event->position());
        const auto hoveredBlock = hoveredConnectionBlockIndex(layout_, connectionBlocks_, transform, currentFloorId_, event->position());

        if (hoveredGuidance.has_value()) {
            const auto& guidance = routeGuidances_[*hoveredGuidance];
            const auto tooltip = formatRouteGuidanceTooltip(guidance);
            const auto hoveredId = QString::fromStdString(guidance.id.empty()
                ? (!guidance.installConnectionId.empty() ? guidance.installConnectionId : guidance.guidedExitZoneId)
                : guidance.id);
            if (hoveredId != hoveredRouteGuidanceId_) {
                hoveredRouteGuidanceId_ = hoveredId;
                hoveredConnectionBlockId_.clear();
                hoveredEnvironmentHazardId_.clear();
                QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
            }
        } else if (hoveredHazard.has_value()) {
            const auto& hazard = environmentHazards_[*hoveredHazard];
            const auto tooltip = formatEnvironmentHazardTooltip(hazard);
            const auto hoveredId = QString::fromStdString(hazard.id);
            if (hoveredId != hoveredEnvironmentHazardId_) {
                hoveredEnvironmentHazardId_ = hoveredId;
                hoveredConnectionBlockId_.clear();
                hoveredRouteGuidanceId_.clear();
                QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
            }
        } else if (hoveredBlock.has_value()) {
            const auto& block = connectionBlocks_[*hoveredBlock];
            const auto tooltip = formatConnectionBlockTooltip(block);
            if (!tooltip.isEmpty()) {
                const auto hoveredId = QString::fromStdString(block.id.empty() ? block.connectionId : block.id);
                if (hoveredId != hoveredConnectionBlockId_) {
                    hoveredConnectionBlockId_ = hoveredId;
                    hoveredEnvironmentHazardId_.clear();
                    hoveredRouteGuidanceId_.clear();
                    QToolTip::showText(event->globalPosition().toPoint(), tooltip, this);
                }
            }
        } else {
            if (!hoveredConnectionBlockId_.isEmpty() || !hoveredEnvironmentHazardId_.isEmpty() || !hoveredRouteGuidanceId_.isEmpty()) {
                hoveredConnectionBlockId_.clear();
                hoveredEnvironmentHazardId_.clear();
                hoveredRouteGuidanceId_.clear();
                QToolTip::hideText();
            }
        }
    }
    QWidget::mouseMoveEvent(event);
}

void ScenarioCanvasWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);

    if (camera_.beginPan(event)) {
        return;
    }

    if (event->button() == Qt::RightButton) {
        if (const auto bounds = collectBounds(); bounds.has_value()) {
            const auto transform = currentTransform(*bounds);
            auto crowdElementId = selectedPlacementAt(event->position(), transform);
            if (crowdElementId.isEmpty()) {
                crowdElementId = placementAt(event->position(), transform, 8.0);
            }
            if (!crowdElementId.isEmpty()) {
                focusPlacement(crowdElementId);
                openCrowdPlacementContextMenu(crowdElementId, event->globalPosition().toPoint());
                event->accept();
                return;
            }
        }

        const auto point = unmapPoint(event->position());
        constexpr double kPickRadiusPixels = 18.0;
        const auto offsetPoint = unmapPoint(event->position() + QPointF(kPickRadiusPixels, 0.0));
        const auto dx = offsetPoint.x - point.x;
        const auto dy = offsetPoint.y - point.y;
        const auto hitTolerance = std::max(1.2, std::hypot(dx, dy));

        if (const auto bounds = collectBounds(); bounds.has_value()) {
            const auto transform = currentTransform(*bounds);
            constexpr double kHitTolerancePixels = 18.0;
            for (const auto& guidance : routeGuidances_) {
                const auto center = routeGuidanceMarkerCenter(
                    layout_,
                    guidance,
                    connectionBlocks_,
                    transform,
                    currentFloorId_);
                if (!center.has_value()) {
                    continue;
                }
                if (QLineF(event->position(), *center).length() <= kHitTolerancePixels) {
                    openRouteGuidanceEditor(QString::fromStdString(guidance.id), event->globalPosition().toPoint());
                    event->accept();
                    return;
                }
            }
        }

        for (const auto& block : connectionBlocks_) {
            if (block.connectionId.empty()) {
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
            const auto halfWidth = std::max(0.0, it->effectiveWidth * 0.5);
            const auto distance = std::max(
                0.0,
                distancePointToSegment(point, it->centerSpan.start, it->centerSpan.end) - halfWidth);
            if (distance <= hitTolerance) {
                openConnectionBlockScheduleEditor(QString::fromStdString(block.id), event->globalPosition().toPoint());
                event->accept();
                return;
            }
        }

        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton || event->position().y() < kTopToolbarHeight + kPropertyPanelHeight) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (toolMode_ == ToolMode::IndividualPlacement) {
        addIndividualPlacement(event->position());
        event->accept();
        return;
    }

    if (toolMode_ == ToolMode::SourcePlacement) {
        addSourcePlacement(event->position());
        event->accept();
        return;
    }

    if (toolMode_ == ToolMode::GroupPlacement) {
        dragging_ = true;
        dragStart_ = event->position();
        dragCurrent_ = dragStart_;
        update();
        event->accept();
        return;
    }

    if (toolMode_ == ToolMode::BlockDoor) {
        addConnectionBlock(event->position());
        event->accept();
        return;
    }

    if (toolMode_ == ToolMode::FireHazard || toolMode_ == ToolMode::SmokeHazard) {
        addEnvironmentHazard(
            event->position(),
            toolMode_ == ToolMode::FireHazard
                ? safecrowd::domain::EnvironmentHazardKind::Fire
                : safecrowd::domain::EnvironmentHazardKind::Smoke);
        event->accept();
        return;
    }

    if (toolMode_ == ToolMode::RouteGuidance) {
        addRouteGuidance(event->position());
        event->accept();
        return;
    }

    if (toolMode_ == ToolMode::Select) {
        selectionDragging_ = true;
        selectionDragStart_ = event->position();
        selectionDragCurrent_ = selectionDragStart_;
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void ScenarioCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (camera_.finishPan(event)) {
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (selectionDragging_) {
        const auto start = selectionDragStart_;
        const auto end = event->position();
        selectionDragging_ = false;
        selectionDragStart_ = {};
        selectionDragCurrent_ = {};

        const auto bounds = collectBounds();
        if (bounds.has_value()) {
            const auto transform = currentTransform(*bounds);
            const auto dragDistance = QLineF(start, end).length();
            if (dragDistance <= kSelectionDragThresholdPixels) {
                selectSingleAt(end, transform);
            } else {
                selectPlacementsInRect(QRectF(start, end).normalized(), transform);
            }
        }
        update();
        event->accept();
        return;
    }

    if (!dragging_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const auto start = dragStart_;
    const auto end = event->position();
    dragging_ = false;
    dragStart_ = {};
    dragCurrent_ = {};
    addGroupPlacement(start, end);
    update();
    event->accept();
}

void ScenarioCanvasWidget::paintEvent(QPaintEvent* event) {
    (void)event;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#f4f7fb"));

    const auto bounds = collectBounds();
    const auto viewport = previewViewport();
    if (!bounds.has_value()) {
        painter.setPen(QColor("#4f5d6b"));
        painter.drawText(rect(), Qt::AlignCenter, "No approved layout available");
        return;
    }
    const auto transform = currentTransform(*bounds);

    drawLayoutCanvasSurface(painter, QRectF(rect()));

    drawFacilityLayoutCanvas(painter, layout_, transform, currentFloorId_.toStdString());
    drawFocusedLayoutElement(painter, transform);

    painter.setPen(Qt::NoPen);
    for (const auto& placement : placements_) {
        if (!currentFloorId_.isEmpty() && !placement.floorId.isEmpty() && placement.floorId != currentFloorId_) {
            continue;
        }
        if (placement.kind == ScenarioCrowdPlacementKind::Source) {
            if (placement.area.empty()) {
                continue;
            }
            drawOccupantSourceMarker(painter, transform.map(placement.area.front()), QColor("#1f5fae"));
            continue;
        }

        if (placement.kind == ScenarioCrowdPlacementKind::Individual) {
            if (placement.area.empty()) {
                continue;
            }
            const auto origin = transform.map(placement.area.front());
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#1f5fae"));
            painter.drawEllipse(origin, kOccupantMarkerRadius, kOccupantMarkerRadius);
            continue;
        }

        if (placement.area.size() >= 4) {
            painter.setBrush(QColor("#1f5fae"));
            painter.setPen(QPen(QColor("#0f4c8f"), 1.2, Qt::SolidLine, Qt::RoundCap));
            for (const auto& worldPoint : fallbackDisplayPositions(placement)) {
                const auto point = transform.map(worldPoint);
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(point, kOccupantMarkerRadius, kOccupantMarkerRadius);
                painter.setPen(QPen(QColor("#0f4c8f"), 1.2, Qt::SolidLine, Qt::RoundCap));
            }
            painter.setPen(Qt::NoPen);
        }
    }
    drawFocusedPlacement(painter, transform);
    drawConnectionBlocks(painter, transform);
    drawEnvironmentHazards(painter, transform);
    drawRouteGuidances(painter, transform);

    if (dragging_ || selectionDragging_) {
        const auto start = dragging_ ? dragStart_ : selectionDragStart_;
        const auto current = dragging_ ? dragCurrent_ : selectionDragCurrent_;
        painter.setPen(QPen(kSelectionHighlightColor, 1.5, Qt::DashLine));
        painter.setBrush(QColor(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 36));
        painter.drawRect(QRectF(start, current).normalized());
    }
}

void ScenarioCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    repositionToolbars();
}

void ScenarioCanvasWidget::wheelEvent(QWheelEvent* event) {
    if (switchFloorByWheel(event)) {
        return;
    }

    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        QWidget::wheelEvent(event);
        return;
    }

    if (camera_.zoomAt(event, *bounds, previewViewport())) {
        update();
        return;
    }

    QWidget::wheelEvent(event);
}

void ScenarioCanvasWidget::drawFocusedLayoutElement(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (focusedLayoutElementId_.isEmpty()) {
        return;
    }

    const QPen highlightPen(kSelectionHighlightColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    const QColor highlightFill(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 42);

    for (const auto& zone : layout_.zones) {
        if (!matchesFloor(zone.floorId, currentFloorId_)) {
            continue;
        }
        if (QString::fromStdString(zone.id) != focusedLayoutElementId_) {
            continue;
        }
        painter.setPen(highlightPen);
        painter.setBrush(highlightFill);
        painter.drawPath(layoutCanvasPolygonPath(zone.area, transform));
        return;
    }

    for (const auto& connection : layout_.connections) {
        if (!matchesFloor(connection.floorId, currentFloorId_)) {
            continue;
        }
        if (QString::fromStdString(connection.id) != focusedLayoutElementId_) {
            continue;
        }
        painter.setPen(highlightPen);
        painter.setBrush(Qt::NoBrush);
        drawLayoutCanvasLine(painter, connection.centerSpan, transform);
        return;
    }

    for (const auto& barrier : layout_.barriers) {
        if (!matchesFloor(barrier.floorId, currentFloorId_)) {
            continue;
        }
        if (QString::fromStdString(barrier.id) != focusedLayoutElementId_) {
            continue;
        }
        painter.setPen(highlightPen);
        painter.setBrush(Qt::NoBrush);
        drawLayoutCanvasPolyline(painter, barrier.geometry, transform);
        return;
    }
}

void ScenarioCanvasWidget::drawFocusedPlacement(QPainter& painter, const LayoutCanvasTransform& transform) const {
    if (focusedPlacementId_.isEmpty() && selectedPlacementIds_.empty()) {
        return;
    }

    painter.setPen(QPen(kSelectionHighlightColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 42));

    for (const auto& placement : placements_) {
        const auto selected = selectedPlacementIds_.contains(placement.id) || placement.id == focusedPlacementId_;
        if (!selected || placement.area.empty()) {
            continue;
        }
        if (!currentFloorId_.isEmpty() && !placement.floorId.isEmpty() && placement.floorId != currentFloorId_) {
            continue;
        }

        if (placement.kind == ScenarioCrowdPlacementKind::Individual || placement.area.size() < 4) {
            const auto center = transform.map(placement.area.front());
            painter.setPen(Qt::NoPen);
            painter.setBrush(kSelectionHighlightColor);
            painter.drawEllipse(center, kOccupantMarkerRadius, kOccupantMarkerRadius);
            painter.setPen(QPen(kSelectionHighlightColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(QColor(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 42));
            continue;
        }

        QPainterPath areaPath;
        if (placement.area.size() >= 3) {
            areaPath.moveTo(transform.map(placement.area.front()));
            for (std::size_t index = 1; index < placement.area.size(); ++index) {
                areaPath.lineTo(transform.map(placement.area[index]));
            }
            areaPath.closeSubpath();
            painter.drawPath(areaPath);
        }

        const auto markerPositions = fallbackDisplayPositions(placement);
        bool occupantIndexOk = false;
        if (focusedCrowdElementId_.startsWith(placement.id + "/occupant-")) {
            const auto suffix = focusedCrowdElementId_.mid(QString("%1/occupant-").arg(placement.id).size());
            const auto parsedIndex = suffix.toInt(&occupantIndexOk);
            for (int index = 0; index < static_cast<int>(markerPositions.size()); ++index) {
                if (!occupantIndexOk || index + 1 != parsedIndex) {
                    continue;
                }
                const auto center = transform.map(markerPositions[static_cast<std::size_t>(index)]);
                painter.setPen(Qt::NoPen);
                painter.setBrush(kSelectionHighlightColor);
                painter.drawEllipse(center, kOccupantMarkerRadius, kOccupantMarkerRadius);
            }
        } else {
            painter.setPen(Qt::NoPen);
            painter.setBrush(kSelectionHighlightColor);
            for (const auto& worldPoint : markerPositions) {
                painter.drawEllipse(transform.map(worldPoint), kOccupantMarkerRadius, kOccupantMarkerRadius);
            }
        }

        painter.setPen(QPen(kSelectionHighlightColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(QColor(kSelectionHighlightColor.red(), kSelectionHighlightColor.green(), kSelectionHighlightColor.blue(), 42));
    }
}

void ScenarioCanvasWidget::drawConnectionBlocks(QPainter& painter, const LayoutCanvasTransform& transform) const {
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor("#c0392b"), 2.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    for (const auto& block : connectionBlocks_) {
        if (block.connectionId.empty()) {
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
        const double r = 10.0;
        painter.drawEllipse(center, r, r);
        painter.drawLine(QPointF(center.x() - 6.5, center.y() + 6.5), QPointF(center.x() + 6.5, center.y() - 6.5));
    }
}

void ScenarioCanvasWidget::drawEnvironmentHazards(QPainter& painter, const LayoutCanvasTransform& transform) const {
    for (const auto& hazard : environmentHazards_) {
        if (!matchesFloor(safecrowd::domain::environmentHazardFloorId(layout_, hazard), currentFloorId_)) {
            continue;
        }

        const auto center = transform.map(hazard.position);
        const QColor fill = hazard.kind == safecrowd::domain::EnvironmentHazardKind::Fire
            ? QColor("#c2410c")
            : QColor("#64748b");
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawEllipse(center, 11.0, 11.0);

        painter.setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        if (hazard.kind == safecrowd::domain::EnvironmentHazardKind::Fire) {
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
}

void ScenarioCanvasWidget::drawRouteGuidances(QPainter& painter, const LayoutCanvasTransform& transform) const {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#1f5fae"));

    for (const auto& guidance : routeGuidances_) {
        const auto center = routeGuidanceMarkerCenter(
            layout_,
            guidance,
            connectionBlocks_,
            transform,
            currentFloorId_);
        if (!center.has_value()) {
            continue;
        }

        const QPointF markerCenter = *center;

        const double r = 10.0;
        painter.setBrush(QColor("#1f5fae"));
        painter.drawEllipse(markerCenter, r, r);

        painter.save();
        painter.translate(markerCenter);
        painter.rotate(-25.0);
        painter.translate(-markerCenter);
        painter.setBrush(Qt::white);
        painter.drawRoundedRect(QRectF(markerCenter.x() - 1.8, markerCenter.y() - 7.0, 3.6, 10.5), 1.4, 1.4);
        painter.drawRoundedRect(QRectF(markerCenter.x() - 1.5, markerCenter.y() + 2.2, 3.0, 5.2), 1.2, 1.2);
        painter.restore();

        painter.setPen(QPen(Qt::white, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(markerCenter.x() + 5.3, markerCenter.y() - 5.0), QPointF(markerCenter.x() + 8.2, markerCenter.y() - 7.7));
        painter.drawLine(QPointF(markerCenter.x() + 6.3, markerCenter.y() - 2.0), QPointF(markerCenter.x() + 9.2, markerCenter.y() - 2.8));
        painter.drawLine(QPointF(markerCenter.x() + 3.7, markerCenter.y() - 7.2), QPointF(markerCenter.x() + 4.8, markerCenter.y() - 9.8));
        painter.setPen(Qt::NoPen);
    }
}

std::optional<LayoutCanvasBounds> ScenarioCanvasWidget::collectBounds() const {
    return collectLayoutCanvasBounds(layout_, currentFloorId_.toStdString());
}

LayoutCanvasTransform ScenarioCanvasWidget::currentTransform(const LayoutCanvasBounds& bounds) const {
    return LayoutCanvasTransform(bounds, previewViewport(), camera_.zoom(), camera_.panOffset());
}

bool ScenarioCanvasWidget::switchFloorByWheel(QWheelEvent* event) {
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
        if (QString::fromStdString(layout_.floors[index].id) == currentFloorId_) {
            currentIndex = static_cast<int>(index);
            break;
        }
    }

    const auto direction = delta > 0 ? 1 : -1;
    const auto nextIndex = std::clamp(
        currentIndex + direction,
        0,
        static_cast<int>(layout_.floors.size() - 1));
    const auto nextFloorId = QString::fromStdString(layout_.floors[static_cast<std::size_t>(nextIndex)].id);
    if (nextIndex != currentIndex && !nextFloorId.isEmpty()) {
        currentFloorId_ = nextFloorId;
        focusedLayoutElementId_.clear();
        focusedCrowdElementId_.clear();
        focusedPlacementId_.clear();
        selectedPlacementIds_.clear();
        dragging_ = false;
        selectionDragging_ = false;
        dragStart_ = {};
        dragCurrent_ = {};
        selectionDragStart_ = {};
        selectionDragCurrent_ = {};
        camera_.reset();
        if (layoutElementActivatedHandler_) {
            layoutElementActivatedHandler_(QString("floor:%1").arg(currentFloorId_));
        }
        if (crowdSelectionChangedHandler_) {
            crowdSelectionChangedHandler_({});
        }
        update();
    }

    event->accept();
    return true;
}

void ScenarioCanvasWidget::selectFloorForElement(const QString& elementId) {
    auto selectFloor = [&](const std::string& floorId) {
        if (floorId.empty()) {
            return;
        }
        const auto floorIdText = QString::fromStdString(floorId);
        if (floorIdText == currentFloorId_) {
            return;
        }
        currentFloorId_ = floorIdText;
        camera_.reset();
    };

    for (const auto& zone : layout_.zones) {
        if (QString::fromStdString(zone.id) == elementId) {
            selectFloor(zone.floorId);
            return;
        }
    }
    for (const auto& connection : layout_.connections) {
        if (QString::fromStdString(connection.id) == elementId) {
            selectFloor(connection.floorId);
            return;
        }
    }
    for (const auto& barrier : layout_.barriers) {
        if (QString::fromStdString(barrier.id) == elementId) {
            selectFloor(barrier.floorId);
            return;
        }
    }
}

safecrowd::domain::Point2D ScenarioCanvasWidget::unmapPoint(const QPointF& point) const {
    const auto bounds = collectBounds();
    if (!bounds.has_value()) {
        return {};
    }
    return currentTransform(*bounds).unmap(point);
}

QRectF ScenarioCanvasWidget::previewViewport() const {
    return QRectF(rect()).adjusted(
        kViewportPadding,
        kTopToolbarHeight + kPropertyPanelHeight + kViewportPadding,
        -kViewportPadding,
        -kViewportPadding);
}

QString ScenarioCanvasWidget::zoneAt(const safecrowd::domain::Point2D& point) const {
    for (const auto& zone : layout_.zones) {
        if (!matchesFloor(zone.floorId, currentFloorId_)) {
            continue;
        }
        if (pointInPolygon(zone.area, point)) {
            return QString::fromStdString(zone.id);
        }
    }
    return {};
}

const safecrowd::domain::Connection2D* ScenarioCanvasWidget::connectionAt(
    const safecrowd::domain::Point2D& point,
    double toleranceWorldUnits) const {
    const safecrowd::domain::Connection2D* best = nullptr;
    double bestDistance = std::max(0.0, toleranceWorldUnits);
    for (const auto& connection : layout_.connections) {
        if (!matchesFloor(connection.floorId, currentFloorId_)) {
            continue;
        }
        const auto distance = distancePointToSegment(point, connection.centerSpan.start, connection.centerSpan.end);
        if (distance <= bestDistance) {
            bestDistance = distance;
            best = &connection;
        }
    }
    return best;
}

const safecrowd::domain::Barrier2D* ScenarioCanvasWidget::barrierAt(
    const safecrowd::domain::Point2D& point,
    double toleranceWorldUnits) const {
    const safecrowd::domain::Barrier2D* best = nullptr;
    double bestDistance = std::max(0.0, toleranceWorldUnits);
    for (const auto& barrier : layout_.barriers) {
        if (!matchesFloor(barrier.floorId, currentFloorId_)) {
            continue;
        }
        const auto& vertices = barrier.geometry.vertices;
        if (vertices.size() < 2) {
            continue;
        }
        for (std::size_t index = 1; index < vertices.size(); ++index) {
            const auto distance = distancePointToSegment(point, vertices[index - 1], vertices[index]);
            if (distance <= bestDistance) {
                bestDistance = distance;
                best = &barrier;
            }
        }
        if (barrier.geometry.closed) {
            const auto distance = distancePointToSegment(point, vertices.back(), vertices.front());
            if (distance <= bestDistance) {
                bestDistance = distance;
                best = &barrier;
            }
        }
    }
    return best;
}

safecrowd::domain::Point2D ScenarioCanvasWidget::connectionCenter(const safecrowd::domain::Connection2D& connection) const {
    return {
        .x = (connection.centerSpan.start.x + connection.centerSpan.end.x) * 0.5,
        .y = (connection.centerSpan.start.y + connection.centerSpan.end.y) * 0.5,
    };
}

QString ScenarioCanvasWidget::placementAt(
    const QPointF& position,
    const LayoutCanvasTransform& transform,
    double pickPadding) const {
    const double pickRadius = kOccupantMarkerRadius + 6.0 + std::max(0.0, pickPadding);
    for (auto it = placements_.rbegin(); it != placements_.rend(); ++it) {
        const auto& placement = *it;
        if (!currentFloorId_.isEmpty() && !placement.floorId.isEmpty() && placement.floorId != currentFloorId_) {
            continue;
        }
        if (placement.area.empty()) {
            continue;
        }
        if (placement.kind == ScenarioCrowdPlacementKind::Individual || placement.area.size() < 4) {
            if (QLineF(position, transform.map(placement.area.front())).length() <= pickRadius) {
                return placement.id;
            }
            continue;
        }

        const auto markers = fallbackDisplayPositions(placement);
        for (int index = 0; index < static_cast<int>(markers.size()); ++index) {
            const auto& worldPoint = markers[static_cast<std::size_t>(index)];
            if (QLineF(position, transform.map(worldPoint)).length() <= pickRadius) {
                return QString("%1/occupant-%2").arg(placement.id).arg(index + 1);
            }
        }

        if (groupMarkerBounds(placement, transform).adjusted(-pickPadding, -pickPadding, pickPadding, pickPadding).contains(position)) {
            return placement.id;
        }
    }
    return {};
}

QString ScenarioCanvasWidget::selectedPlacementAt(const QPointF& position, const LayoutCanvasTransform& transform) const {
    constexpr double kSelectedPickPadding = 14.0;
    const auto focusedPlacementId = placementIdFromCrowdElementId(focusedCrowdElementId_);

    for (auto it = placements_.rbegin(); it != placements_.rend(); ++it) {
        const auto& placement = *it;
        if (!currentFloorId_.isEmpty() && !placement.floorId.isEmpty() && placement.floorId != currentFloorId_) {
            continue;
        }
        if (placement.area.empty()) {
            continue;
        }
        const bool selected = selectedPlacementIds_.contains(placement.id) || placement.id == focusedPlacementId;
        if (!selected) {
            continue;
        }

        const double pickRadius = kOccupantMarkerRadius + 6.0 + kSelectedPickPadding;
        if (placement.kind == ScenarioCrowdPlacementKind::Individual || placement.area.size() < 4) {
            if (QLineF(position, transform.map(placement.area.front())).length() <= pickRadius) {
                return placement.id;
            }
            continue;
        }

        if (const auto focusedOccupantIndex = occupantIndexFromCrowdElementId(focusedCrowdElementId_, placement.id);
            focusedOccupantIndex.has_value()) {
            const auto markers = fallbackDisplayPositions(placement);
            const auto index = *focusedOccupantIndex;
            if (index >= 0 && index < static_cast<int>(markers.size())) {
                if (QLineF(position, transform.map(markers[static_cast<std::size_t>(index)])).length() <= pickRadius) {
                    return focusedCrowdElementId_;
                }
            }
        }

        const auto markers = fallbackDisplayPositions(placement);
        for (int index = 0; index < static_cast<int>(markers.size()); ++index) {
            if (QLineF(position, transform.map(markers[static_cast<std::size_t>(index)])).length() <= pickRadius) {
                return QString("%1/occupant-%2").arg(placement.id).arg(index + 1);
            }
        }

        if (pointInsidePlacementArea(placement.area, transform.unmap(position))) {
            return placement.id;
        }

        if (groupMarkerBounds(placement, transform)
                .adjusted(-kSelectedPickPadding, -kSelectedPickPadding, kSelectedPickPadding, kSelectedPickPadding)
                .contains(position)) {
            return placement.id;
        }
    }

    return {};
}

bool ScenarioCanvasWidget::placementPointBlocked(const safecrowd::domain::Point2D& point) const {
    for (const auto& barrier : layout_.barriers) {
        if (!matchesFloor(barrier.floorId, currentFloorId_)) {
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

bool ScenarioCanvasWidget::placementAreaBlocked(
    const std::vector<safecrowd::domain::Point2D>& area,
    int occupantCount) const {
    (void)occupantCount;
    return area.empty();
}

safecrowd::domain::Point2D ScenarioCanvasWidget::defaultVelocityFrom(const safecrowd::domain::Point2D& point) const {
    std::optional<safecrowd::domain::Point2D> target;
    for (const auto& zone : layout_.zones) {
        if (zone.kind == safecrowd::domain::ZoneKind::Exit) {
            target = polygonCenter(zone.area);
            break;
        }
    }
    if (!target.has_value() && !layout_.zones.empty()) {
        target = polygonCenter(layout_.zones.back().area);
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

QString ScenarioCanvasWidget::nextPlacementId(ScenarioCrowdPlacementKind kind) const {
    const char* prefix = "group";
    if (kind == ScenarioCrowdPlacementKind::Individual) {
        prefix = "individual";
    } else if (kind == ScenarioCrowdPlacementKind::Source) {
        prefix = "source";
    }
    return QString("%1-%2").arg(prefix).arg(static_cast<int>(placements_.size()) + 1);
}

QString ScenarioCanvasWidget::nextConnectionBlockId() const {
    return QString("block-%1").arg(static_cast<int>(connectionBlocks_.size()) + 1);
}

QString ScenarioCanvasWidget::nextEnvironmentHazardId() const {
    for (int index = static_cast<int>(environmentHazards_.size()) + 1;; ++index) {
        const auto candidate = QString("hazard-%1").arg(index);
        const auto exists = std::any_of(environmentHazards_.begin(), environmentHazards_.end(), [&](const auto& hazard) {
            return QString::fromStdString(hazard.id) == candidate;
        });
        if (!exists) {
            return candidate;
        }
    }
}

QString ScenarioCanvasWidget::nextRouteGuidanceId() const {
    return QString("guidance-%1").arg(static_cast<int>(routeGuidances_.size()) + 1);
}

void ScenarioCanvasWidget::addGroupPlacement(const QPointF& start, const QPointF& end) {
    if ((QLineF(start, end).length()) < 8.0) {
        return;
    }

    const auto rect = QRectF(start, end).normalized();
    const auto topLeft = unmapPoint(rect.topLeft());
    const auto topRight = unmapPoint(rect.topRight());
    const auto bottomRight = unmapPoint(rect.bottomRight());
    const auto bottomLeft = unmapPoint(rect.bottomLeft());
    const auto zoneId = zoneAt(topLeft);
    if (zoneId.isEmpty()) {
        return;
    }

    const std::vector<safecrowd::domain::Point2D> area = {topLeft, topRight, bottomRight, bottomLeft};
    const auto id = nextPlacementId(ScenarioCrowdPlacementKind::Group);
    const auto count = groupCountSpinBox_ == nullptr ? 25 : groupCountSpinBox_->value();
    if (placementAreaBlocked(area, count)) {
        return;
    }
    const auto distribution = groupDistributionComboBox_ == nullptr
        ? safecrowd::domain::InitialPlacementDistribution::Uniform
        : static_cast<safecrowd::domain::InitialPlacementDistribution>(groupDistributionComboBox_->currentData().toInt());

    const auto pointOccupiedByExistingPlacement = [this](const safecrowd::domain::Point2D& point) {
        for (const auto& placement : placements_) {
            if (!currentFloorId_.isEmpty() && !placement.floorId.isEmpty() && placement.floorId != currentFloorId_) {
                continue;
            }
            for (const auto& existing : fallbackDisplayPositions(placement)) {
                if (std::hypot(existing.x - point.x, existing.y - point.y) < kOccupantMinSpacing) {
                    return true;
                }
            }
        }
        return false;
    };
    const auto blocked = [this, zoneId, &pointOccupiedByExistingPlacement](const safecrowd::domain::Point2D& point) {
        return zoneAt(point) != zoneId || placementPointBlocked(point) || pointOccupiedByExistingPlacement(point);
    };
    const auto generatedPositions = distribution == safecrowd::domain::InitialPlacementDistribution::Random
        ? generateRandomPlacementPositions(id, area, count, blocked)
        : generateUniformPlacementPositions(area, count, blocked);
    if (static_cast<int>(generatedPositions.size()) < count) {
        QMessageBox::warning(
            this,
            "Cannot place occupant group",
            "The selected region is too small or blocked for the requested occupant count.");
        return;
    }

    placements_.push_back({
        .id = id,
        .name = QString("Group %1").arg(id.section('-', -1)),
        .kind = ScenarioCrowdPlacementKind::Group,
        .zoneId = zoneId,
        .floorId = currentFloorId_,
        .area = area,
        .occupantCount = count,
        .velocity = defaultVelocityFrom(placementCenter(area)),
        .distribution = distribution,
        .generatedPositions = generatedPositions,
    });
    focusedCrowdElementId_ = id;
    focusedPlacementId_ = id;
    selectedPlacementIds_ = QStringList{id};
    focusedLayoutElementId_.clear();
    if (crowdSelectionChangedHandler_) {
        crowdSelectionChangedHandler_(id);
    }
    emitPlacementsChanged();
    update();
}

void ScenarioCanvasWidget::addIndividualPlacement(const QPointF& position) {
    const auto point = unmapPoint(position);
    const auto zoneId = zoneAt(point);
    if (zoneId.isEmpty() || placementPointBlocked(point)) {
        return;
    }

    const auto id = nextPlacementId(ScenarioCrowdPlacementKind::Individual);
    placements_.push_back({
        .id = id,
        .name = QString("Individual %1").arg(id.section('-', -1)),
        .kind = ScenarioCrowdPlacementKind::Individual,
        .zoneId = zoneId,
        .floorId = currentFloorId_,
        .area = {point},
        .occupantCount = 1,
        .velocity = defaultVelocityFrom(point),
    });
    focusedCrowdElementId_ = id;
    focusedPlacementId_ = id;
    selectedPlacementIds_ = QStringList{id};
    focusedLayoutElementId_.clear();
    if (crowdSelectionChangedHandler_) {
        crowdSelectionChangedHandler_(id);
    }
    emitPlacementsChanged();
    update();
}

void ScenarioCanvasWidget::addSourcePlacement(const QPointF& position) {
    const auto point = unmapPoint(position);
    const auto zoneId = zoneAt(point);
    if (zoneId.isEmpty() || placementPointBlocked(point)) {
        return;
    }

    const auto id = nextPlacementId(ScenarioCrowdPlacementKind::Source);
    const auto sourceCount = sourceEmissionCount(
        sourceAgentsPerSpawn_,
        sourceDurationSeconds_,
        sourceIntervalSeconds_);
    placements_.push_back({
        .id = id,
        .name = QString("Source %1").arg(id.section('-', -1)),
        .kind = ScenarioCrowdPlacementKind::Source,
        .zoneId = zoneId,
        .floorId = currentFloorId_,
        .area = {point},
        .occupantCount = sourceCount,
        .velocity = defaultVelocityFrom(point),
        .sourceAgentsPerSpawn = sourceAgentsPerSpawn_,
        .sourceStartSeconds = sourceStartSeconds_,
        .sourceEndSeconds = sourceStartSeconds_ + sourceDurationSeconds_,
        .sourceIntervalSeconds = sourceIntervalSeconds_,
    });
    focusedCrowdElementId_ = id;
    focusedPlacementId_ = id;
    selectedPlacementIds_ = QStringList{id};
    focusedLayoutElementId_.clear();
    if (crowdSelectionChangedHandler_) {
        crowdSelectionChangedHandler_(id);
    }
    emitPlacementsChanged();
    update();
}

void ScenarioCanvasWidget::addConnectionBlock(const QPointF& position) {
    const auto point = unmapPoint(position);
    constexpr double kPickRadiusPixels = 18.0;
    const auto offsetPoint = unmapPoint(position + QPointF(kPickRadiusPixels, 0.0));
    const auto dx = offsetPoint.x - point.x;
    const auto dy = offsetPoint.y - point.y;
    const auto pixelToleranceWorldUnits = std::hypot(dx, dy);

    const auto toleranceWorldUnits = std::max(1.2, pixelToleranceWorldUnits);
    const safecrowd::domain::Connection2D* connection = nullptr;
    double bestDistance = toleranceWorldUnits;
    for (const auto& candidate : layout_.connections) {
        if (!matchesFloor(candidate.floorId, currentFloorId_)) {
            continue;
        }
        if (candidate.kind != safecrowd::domain::ConnectionKind::Doorway
            && candidate.kind != safecrowd::domain::ConnectionKind::Exit) {
            continue;
        }
        const auto halfWidth = std::max(0.0, candidate.effectiveWidth * 0.5);
        const auto distance =
            std::max(0.0, distancePointToSegment(point, candidate.centerSpan.start, candidate.centerSpan.end) - halfWidth);
        if (distance <= bestDistance) {
            bestDistance = distance;
            connection = &candidate;
        }
    }
    if (connection == nullptr) {
        QMessageBox::information(this, "Block door", "This tool can only be used on exits or doors.");
        return;
    }

    addConnectionBlockForConnection(*connection);
}

void ScenarioCanvasWidget::addConnectionBlockForConnection(const safecrowd::domain::Connection2D& connection) {
    if (connection.kind != safecrowd::domain::ConnectionKind::Doorway
        && connection.kind != safecrowd::domain::ConnectionKind::Exit) {
        QMessageBox::information(this, "Block door", "This tool can only be used on exits or doors.");
        return;
    }

    for (const auto& existing : connectionBlocks_) {
        if (existing.connectionId == connection.id) {
            QMessageBox::information(this, "Block door", "This door or exit is already blocked.");
            return;
        }
    }

    safecrowd::domain::ConnectionBlockDraft draft;
    draft.id = nextConnectionBlockId().toStdString();
    draft.connectionId = connection.id;
    connectionBlocks_.push_back(std::move(draft));
    emitConnectionBlocksChanged();
    update();
}

void ScenarioCanvasWidget::addEnvironmentHazard(
    const QPointF& position,
    safecrowd::domain::EnvironmentHazardKind kind) {
    const auto point = unmapPoint(position);
    const auto zoneId = zoneAt(point);
    if (zoneId.isEmpty()) {
        QMessageBox::information(this, "Hazard", "Click inside a zone to place a fire or smoke hazard.");
        return;
    }

    const auto zoneIdStd = zoneId.toStdString();
    const auto it = std::find_if(layout_.zones.begin(), layout_.zones.end(), [&](const auto& zone) {
        return zone.id == zoneIdStd;
    });
    if (it == layout_.zones.end()) {
        return;
    }
    addEnvironmentHazardForZone(*it, point, kind);
}

void ScenarioCanvasWidget::addEnvironmentHazardForZone(
    const safecrowd::domain::Zone2D& zone,
    safecrowd::domain::Point2D position,
    safecrowd::domain::EnvironmentHazardKind kind) {
    if (!matchesFloor(zone.floorId, currentFloorId_)) {
        return;
    }

    if (!pointInPolygon(zone.area, position)) {
        const auto fallbackPosition = representativePointInPolygon(zone.area);
        if (!fallbackPosition.has_value()) {
            QMessageBox::information(this, "Hazard", "Could not find a valid point inside this zone.");
            return;
        }
        position = *fallbackPosition;
    }

    safecrowd::domain::EnvironmentHazardDraft draft;
    draft.id = nextEnvironmentHazardId().toStdString();
    draft.kind = kind;
    draft.name = QString("%1 hazard %2")
        .arg(kind == safecrowd::domain::EnvironmentHazardKind::Fire ? "Fire" : "Smoke")
        .arg(static_cast<int>(environmentHazards_.size()) + 1)
        .toStdString();
    draft.affectedZoneId = zone.id;
    draft.floorId = zone.floorId.empty() ? currentFloorId_.toStdString() : zone.floorId;
    draft.position = position;
    draft.startSeconds = 0.0;
    draft.endSeconds = 60.0;
    draft.severity = safecrowd::domain::ScenarioElementSeverity::Medium;
    environmentHazards_.push_back(std::move(draft));
    emitEnvironmentHazardsChanged();
    update();
}

void ScenarioCanvasWidget::addRouteGuidance(const QPointF& position) {
    const auto point = unmapPoint(position);
    const auto zoneId = zoneAt(point);
    if (!zoneId.isEmpty()) {
        const auto zoneIdStd = zoneId.toStdString();
        const auto it = std::find_if(layout_.zones.begin(), layout_.zones.end(), [&](const auto& zone) {
            return zone.id == zoneIdStd;
        });
        if (it != layout_.zones.end() && it->kind == safecrowd::domain::ZoneKind::Exit) {
            addRouteGuidanceForExitZone(*it);
            return;
        }
        // If the user clicked inside a non-exit zone, still allow installing guidance by selecting a nearby door.
    }

    constexpr double kPickRadiusPixels = 18.0;
    const auto offsetPoint = unmapPoint(position + QPointF(kPickRadiusPixels, 0.0));
    const auto dx = offsetPoint.x - point.x;
    const auto dy = offsetPoint.y - point.y;
    const auto pixelToleranceWorldUnits = std::hypot(dx, dy);
    const auto toleranceWorldUnits = std::max(1.2, pixelToleranceWorldUnits);

    const safecrowd::domain::Connection2D* connection = nullptr;
    double bestDistance = toleranceWorldUnits;
    for (const auto& candidate : layout_.connections) {
        if (!matchesFloor(candidate.floorId, currentFloorId_)) {
            continue;
        }
        if (candidate.kind != safecrowd::domain::ConnectionKind::Doorway
            && candidate.kind != safecrowd::domain::ConnectionKind::Exit) {
            continue;
        }
        const auto halfWidth = std::max(0.0, candidate.effectiveWidth * 0.5);
        const auto distance =
            std::max(0.0, distancePointToSegment(point, candidate.centerSpan.start, candidate.centerSpan.end) - halfWidth);
        if (distance <= bestDistance) {
            bestDistance = distance;
            connection = &candidate;
        }
    }

    if (connection == nullptr) {
        QMessageBox::information(this, "Route guidance", "Click an exit zone or a door to install guidance.");
        return;
    }

    addRouteGuidanceForConnection(*connection);
}

void ScenarioCanvasWidget::addRouteGuidanceForExitZone(const safecrowd::domain::Zone2D& zone) {
    if (zone.kind != safecrowd::domain::ZoneKind::Exit) {
        QMessageBox::information(this, "Route guidance", "This tool can only be used on exit zones.");
        return;
    }

    for (const auto& existing : routeGuidances_) {
        if (existing.installConnectionId.empty() && existing.guidedExitZoneId == zone.id) {
            QMessageBox::information(this, "Route guidance", "Guidance is already installed on this exit.");
            return;
        }
    }

    safecrowd::domain::RouteGuidanceDraft draft;
    draft.id = nextRouteGuidanceId().toStdString();
    draft.startSeconds = 0.0;
    draft.endSeconds = 0.0;
    draft.periods.clear();
    draft.guidedExitZoneId = zone.id;
    draft.installConnectionId.clear();
    draft.baseComplianceRate = 0.5;
    draft.guidanceStrength = 0.55;
    draft.maxDetourMeters = 20.0;
    routeGuidances_.push_back(std::move(draft));
    emitRouteGuidancesChanged();
    update();
}

void ScenarioCanvasWidget::addRouteGuidanceForConnection(const safecrowd::domain::Connection2D& connection) {
    if (connection.kind != safecrowd::domain::ConnectionKind::Doorway
        && connection.kind != safecrowd::domain::ConnectionKind::Exit) {
        QMessageBox::information(this, "Route guidance", "This tool can only be used on exit zones or doors.");
        return;
    }

    for (const auto& existing : routeGuidances_) {
        if (!existing.installConnectionId.empty() && existing.installConnectionId == connection.id) {
            QMessageBox::information(this, "Route guidance", "Guidance is already installed on this door.");
            return;
        }
    }

    const auto exitZoneId = pickNearestExitZoneIdForConnection(layout_, connection);

    safecrowd::domain::RouteGuidanceDraft draft;
    draft.id = nextRouteGuidanceId().toStdString();
    draft.startSeconds = 0.0;
    draft.endSeconds = 0.0;
    draft.periods.clear();
    draft.guidedExitZoneId = exitZoneId;
    draft.installConnectionId = connection.id;
    draft.baseComplianceRate = 0.5;
    draft.guidanceStrength = 0.55;
    draft.maxDetourMeters = 20.0;
    routeGuidances_.push_back(std::move(draft));
    emitRouteGuidancesChanged();
    update();
}

void ScenarioCanvasWidget::selectSingleAt(const QPointF& position, const LayoutCanvasTransform& transform) {
    const auto crowdElementId = placementAt(position, transform);
    if (!crowdElementId.isEmpty()) {
        const auto placementId = crowdElementId.section('/', 0, 0);
        focusedCrowdElementId_ = crowdElementId;
        focusedPlacementId_ = placementId;
        selectedPlacementIds_ = QStringList{placementId};
        focusedLayoutElementId_.clear();
        if (layoutElementActivatedHandler_) {
            layoutElementActivatedHandler_({});
        }
        if (crowdSelectionChangedHandler_) {
            crowdSelectionChangedHandler_(crowdElementId);
        }
        return;
    }

    selectedPlacementIds_.clear();
    focusedCrowdElementId_.clear();
    focusedPlacementId_.clear();
    selectLayoutElementAt(position);
}

void ScenarioCanvasWidget::selectPlacementsInRect(const QRectF& screenRect, const LayoutCanvasTransform& transform) {
    selectedPlacementIds_.clear();
    focusedCrowdElementId_.clear();
    focusedPlacementId_.clear();
    focusedLayoutElementId_.clear();

    for (const auto& placement : placements_) {
        if (!currentFloorId_.isEmpty() && !placement.floorId.isEmpty() && placement.floorId != currentFloorId_) {
            continue;
        }
        if (placement.area.empty()) {
            continue;
        }

        bool selected = false;
        if (placement.kind == ScenarioCrowdPlacementKind::Individual || placement.area.size() < 4) {
            const QRectF markerBounds(
                transform.map(placement.area.front()) - QPointF(kOccupantMarkerRadius, kOccupantMarkerRadius),
                QSizeF(kOccupantMarkerRadius * 2.0, kOccupantMarkerRadius * 2.0));
            selected = screenRect.intersects(markerBounds);
        } else {
            selected = screenRect.intersects(groupMarkerBounds(placement, transform));
        }

        if (selected) {
            selectedPlacementIds_.push_back(placement.id);
        }
    }

    if (!selectedPlacementIds_.empty()) {
        focusedPlacementId_ = selectedPlacementIds_.front();
        focusedCrowdElementId_ = focusedPlacementId_;
    }
    if (layoutElementActivatedHandler_) {
        layoutElementActivatedHandler_({});
    }
    if (crowdSelectionChangedHandler_) {
        crowdSelectionChangedHandler_(focusedPlacementId_);
    }
}

void ScenarioCanvasWidget::selectLayoutElementAt(const QPointF& position) {
    const auto point = unmapPoint(position);
    constexpr double kPickRadiusPixels = 14.0;
    const auto offsetPoint = unmapPoint(position + QPointF(kPickRadiusPixels, 0.0));
    const auto dx = offsetPoint.x - point.x;
    const auto dy = offsetPoint.y - point.y;
    const auto toleranceWorldUnits = std::max(0.35, std::hypot(dx, dy));

    QString selectedId;
    if (const auto* connection = connectionAt(point, toleranceWorldUnits); connection != nullptr) {
        selectedId = QString::fromStdString(connection->id);
    } else if (const auto* barrier = barrierAt(point, toleranceWorldUnits); barrier != nullptr) {
        selectedId = QString::fromStdString(barrier->id);
    } else {
        selectedId = zoneAt(point);
    }

    focusedLayoutElementId_ = selectedId;
    focusedCrowdElementId_.clear();
    focusedPlacementId_.clear();
    selectedPlacementIds_.clear();
    if (layoutElementActivatedHandler_) {
        layoutElementActivatedHandler_(selectedId);
    }
    if (crowdSelectionChangedHandler_) {
        crowdSelectionChangedHandler_({});
    }
    update();
}

void ScenarioCanvasWidget::openConnectionBlockScheduleEditor(const QString& blockId, const QPoint& screenPosition) {
    QMenu menu(this);
    auto* editAction = menu.addAction("Set schedule...");
    auto* deleteAction = menu.addAction("Delete");
    const auto* selected = menu.exec(screenPosition);
    if (selected != editAction && selected != deleteAction) {
        return;
    }

    auto it = std::find_if(connectionBlocks_.begin(), connectionBlocks_.end(), [&](const auto& block) {
        return QString::fromStdString(block.id) == blockId;
    });
    if (it == connectionBlocks_.end()) {
        return;
    }

    if (selected == deleteAction) {
        connectionBlocks_.erase(it);
        emitConnectionBlocksChanged();
        update();
        return;
    }

    ConnectionBlockScheduleDialog dialog(it->intervals, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    it->intervals = dialog.intervals();
    emitConnectionBlocksChanged();
    update();
}

void ScenarioCanvasWidget::openRouteGuidanceEditor(const QString& guidanceId, const QPoint& screenPosition) {
    QMenu menu(this);
    auto* settingsAction = menu.addAction("Settings...");
    auto* deleteAction = menu.addAction("Delete");
    const auto* selected = menu.exec(screenPosition);
    if (selected != settingsAction && selected != deleteAction) {
        return;
    }

    auto it = std::find_if(routeGuidances_.begin(), routeGuidances_.end(), [&](const auto& guidance) {
        return QString::fromStdString(guidance.id) == guidanceId;
    });
    if (it == routeGuidances_.end()) {
        return;
    }

    if (selected == deleteAction) {
        deleteRouteGuidanceById(guidanceId);
        return;
    }

    RouteGuidanceSettingsDialog dialog(*it, this);
    dialog.move(screenPosition);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    *it = dialog.guidance();
    emitRouteGuidancesChanged();
    update();
}

bool ScenarioCanvasWidget::editOccupantSourceById(const QString& sourceId, const QPoint& screenPosition) {
    auto placementIt = std::find_if(placements_.begin(), placements_.end(), [&](const auto& placement) {
        return placement.id == sourceId && placement.kind == ScenarioCrowdPlacementKind::Source;
    });
    if (placementIt == placements_.end()) {
        return false;
    }

    OccupantSourceSettings settings{
        .agentsPerSpawn = std::max(1, placementIt->sourceAgentsPerSpawn),
        .startSeconds = placementIt->sourceStartSeconds,
        .durationSeconds = std::max(0.1, placementIt->sourceEndSeconds - placementIt->sourceStartSeconds),
        .intervalSeconds = std::max(0.1, placementIt->sourceIntervalSeconds),
    };
    if (!editOccupantSourceSettings(this, &settings, screenPosition, "Edit occupant source")) {
        return false;
    }

    placementIt->sourceAgentsPerSpawn = settings.agentsPerSpawn;
    placementIt->sourceStartSeconds = settings.startSeconds;
    placementIt->sourceEndSeconds = settings.startSeconds + settings.durationSeconds;
    placementIt->sourceIntervalSeconds = settings.intervalSeconds;
    placementIt->occupantCount = sourceEmissionCount(
        placementIt->sourceAgentsPerSpawn,
        settings.durationSeconds,
        placementIt->sourceIntervalSeconds);
    emitPlacementsChanged();
    update();
    return true;
}

void ScenarioCanvasWidget::openCrowdPlacementContextMenu(const QString& crowdElementId, const QPoint& screenPosition) {
    const auto placementId = placementIdFromCrowdElementId(crowdElementId);
    const auto placementIt = std::find_if(placements_.begin(), placements_.end(), [&](const auto& placement) {
        return placement.id == placementId;
    });

    QMenu menu(this);
    QAction* settingsAction = nullptr;
    if (placementIt != placements_.end() && placementIt->kind == ScenarioCrowdPlacementKind::Source) {
        settingsAction = menu.addAction("Source settings...");
    }
    auto* deleteAction = menu.addAction("Delete");
    const auto* selectedAction = menu.exec(screenPosition);
    if (selectedAction == settingsAction && settingsAction != nullptr) {
        editOccupantSourceById(placementId, screenPosition);
    } else if (selectedAction == deleteAction) {
        deleteCrowdElement(crowdElementId);
    }
}

bool ScenarioCanvasWidget::deleteCrowdElement(const QString& crowdElementId) {
    const auto placementId = placementIdFromCrowdElementId(crowdElementId);
    if (placementId.isEmpty()) {
        return false;
    }

    auto placementIt = std::find_if(placements_.begin(), placements_.end(), [&](const auto& placement) {
        return placement.id == placementId;
    });
    if (placementIt == placements_.end()) {
        return false;
    }

    if (const auto occupantIndex = occupantIndexFromCrowdElementId(crowdElementId, placementId);
        occupantIndex.has_value() && placementIt->kind == ScenarioCrowdPlacementKind::Group) {
        const auto index = *occupantIndex;
        if (index < 0 || index >= placementIt->occupantCount) {
            return false;
        }

        if (placementIt->occupantCount <= 1) {
            placements_.erase(placementIt);
            focusedCrowdElementId_.clear();
            focusedPlacementId_.clear();
            selectedPlacementIds_.clear();
            if (crowdSelectionChangedHandler_) {
                crowdSelectionChangedHandler_({});
            }
        } else {
            if (!placementIt->generatedPositions.empty()) {
                if (index >= static_cast<int>(placementIt->generatedPositions.size())) {
                    return false;
                }
                placementIt->generatedPositions.erase(placementIt->generatedPositions.begin() + index);
            }
            placementIt->occupantCount -= 1;
            focusedCrowdElementId_ = placementId;
            focusedPlacementId_ = placementId;
            selectedPlacementIds_ = QStringList{placementId};
            if (crowdSelectionChangedHandler_) {
                crowdSelectionChangedHandler_(placementId);
            }
        }

        emitPlacementsChanged();
        update();
        return true;
    }

    const bool deleteSelectedPlacements =
        selectedPlacementIds_.contains(placementId) && selectedPlacementIds_.size() > 1;
    if (deleteSelectedPlacements) {
        placements_.erase(
            std::remove_if(placements_.begin(), placements_.end(), [&](const auto& placement) {
                return selectedPlacementIds_.contains(placement.id);
            }),
            placements_.end());
    } else {
        placements_.erase(placementIt);
    }

    focusedCrowdElementId_.clear();
    focusedPlacementId_.clear();
    selectedPlacementIds_.clear();
    if (crowdSelectionChangedHandler_) {
        crowdSelectionChangedHandler_({});
    }
    emitPlacementsChanged();
    update();
    return true;
}

void ScenarioCanvasWidget::emitPlacementsChanged() {
    if (placementsChangedHandler_) {
        placementsChangedHandler_(placements_);
    }
}

void ScenarioCanvasWidget::emitConnectionBlocksChanged() {
    if (connectionBlocksChangedHandler_) {
        connectionBlocksChangedHandler_(connectionBlocks_);
    }
}

void ScenarioCanvasWidget::emitEnvironmentHazardsChanged() {
    if (environmentHazardsChangedHandler_) {
        environmentHazardsChangedHandler_(environmentHazards_);
    }
}

void ScenarioCanvasWidget::emitRouteGuidancesChanged() {
    if (routeGuidancesChangedHandler_) {
        routeGuidancesChangedHandler_(routeGuidances_);
    }
}

bool ScenarioCanvasWidget::configureSourcePlacementTool(const QPoint& screenPosition) {
    OccupantSourceSettings settings{
        .agentsPerSpawn = sourceAgentsPerSpawn_,
        .startSeconds = sourceStartSeconds_,
        .durationSeconds = sourceDurationSeconds_,
        .intervalSeconds = sourceIntervalSeconds_,
    };
    if (!editOccupantSourceSettings(this, &settings, screenPosition, "Add occupant source")) {
        setToolMode(ToolMode::Select);
        return false;
    }

    sourceAgentsPerSpawn_ = settings.agentsPerSpawn;
    sourceStartSeconds_ = settings.startSeconds;
    sourceDurationSeconds_ = settings.durationSeconds;
    sourceIntervalSeconds_ = settings.intervalSeconds;
    setToolMode(ToolMode::SourcePlacement);
    return true;
}

void ScenarioCanvasWidget::repositionToolbars() {
    if (topToolbar_ != nullptr) {
        topToolbar_->setGeometry(0, 0, width(), kTopToolbarHeight);
        topToolbar_->raise();
    }
    if (propertyPanel_ != nullptr) {
        propertyPanel_->setGeometry(0, kTopToolbarHeight, width(), kPropertyPanelHeight);
        propertyPanel_->raise();
    }
}

void ScenarioCanvasWidget::setToolMode(ToolMode mode) {
    toolMode_ = mode;
    if (selectToolButton_ != nullptr) {
        selectToolButton_->setChecked(mode == ToolMode::Select);
    }
    if (individualToolButton_ != nullptr) {
        individualToolButton_->setChecked(mode == ToolMode::IndividualPlacement);
    }
    if (groupToolButton_ != nullptr) {
        groupToolButton_->setChecked(mode == ToolMode::GroupPlacement);
    }
    if (sourceToolButton_ != nullptr) {
        sourceToolButton_->setChecked(mode == ToolMode::SourcePlacement);
    }
    if (blockDoorToolButton_ != nullptr) {
        blockDoorToolButton_->setChecked(mode == ToolMode::BlockDoor);
    }
    if (fireHazardToolButton_ != nullptr) {
        fireHazardToolButton_->setChecked(mode == ToolMode::FireHazard);
    }
    if (smokeHazardToolButton_ != nullptr) {
        smokeHazardToolButton_->setChecked(mode == ToolMode::SmokeHazard);
    }
    if (routeGuidanceToolButton_ != nullptr) {
        routeGuidanceToolButton_->setChecked(mode == ToolMode::RouteGuidance);
    }
    if (groupCountLabel_ != nullptr) {
        groupCountLabel_->setVisible(mode == ToolMode::GroupPlacement);
    }
    if (groupCountSpinBox_ != nullptr) {
        groupCountSpinBox_->setVisible(mode == ToolMode::GroupPlacement);
    }
    if (groupDistributionLabel_ != nullptr) {
        groupDistributionLabel_->setVisible(mode == ToolMode::GroupPlacement);
    }
    if (groupDistributionComboBox_ != nullptr) {
        groupDistributionComboBox_->setVisible(mode == ToolMode::GroupPlacement);
    }
}

void ScenarioCanvasWidget::setupToolbars() {
    const QString frameStyle =
        "QFrame { background: rgba(255, 255, 255, 245); border: 1px solid #d7e0ea; border-radius: 0px; }"
        "QToolButton { background: transparent; border: 0; border-radius: 0px; }"
        "QToolButton:hover { background: #eef3f8; }"
        "QToolButton:checked { background: #dce9f9; }";

    topToolbar_ = new QFrame(this);
    topToolbar_->setStyleSheet(frameStyle);
    auto* topLayout = new QHBoxLayout(topToolbar_);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    propertyPanel_ = new QFrame(this);
    propertyPanel_->setStyleSheet(
        "QFrame { background: rgba(255, 255, 255, 245); border: 1px solid #d7e0ea; border-radius: 0px; }"
        "QSpinBox, QComboBox { min-height: 24px; padding: 0 8px; border: 1px solid #c9d5e2; border-radius: 0px; background: #ffffff; color: #16202b; }");
    auto* propertyLayout = new QHBoxLayout(propertyPanel_);
    propertyLayout->setContentsMargins(12, 0, 16, 0);
    propertyLayout->setSpacing(12);

    const auto makeButton = [&](const QIcon& icon, const QString& tooltip) {
        auto* button = new QToolButton(topToolbar_);
        button->setCheckable(true);
        button->setIcon(icon);
        button->setIconSize(QSize(22, 22));
        button->setToolTip(tooltip);
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(kToolbarButtonSize, kToolbarButtonSize);
        topLayout->addWidget(button);
        return button;
    };

    selectToolButton_ = makeButton(makeToolIcon("select", QColor("#16202b")), "Select");
    individualToolButton_ = makeButton(makeToolIcon("individual", QColor("#1f5fae")), "Add Individual Occupant");
    groupToolButton_ = makeButton(makeToolIcon("group", QColor("#1f5fae")), "Add Occupant Group");
    sourceToolButton_ = makeButton(makeToolIcon("source", QColor("#1f5fae")), "Add Occupant Source");
    blockDoorToolButton_ = makeButton(makeToolIcon("block", QColor("#c0392b")), "block door");
    fireHazardToolButton_ = makeButton(makeToolIcon("fire", QColor("#c2410c")), "Add Fire Hazard");
    smokeHazardToolButton_ = makeButton(makeToolIcon("smoke", QColor("#64748b")), "Add Smoke Hazard");
    routeGuidanceToolButton_ = makeButton(makeToolIcon("guidance", QColor("#1f5fae")), "Route guidance");
    topLayout->addStretch(1);

    groupCountLabel_ = new QLabel("Group count", propertyPanel_);
    groupCountLabel_->setStyleSheet("QLabel { color: #4f5d6b; background: transparent; border: 0; }");
    propertyLayout->addWidget(groupCountLabel_);
    groupCountSpinBox_ = new QSpinBox(propertyPanel_);
    groupCountSpinBox_->setRange(1, 5000);
    groupCountSpinBox_->setValue(25);
    groupCountSpinBox_->setSuffix(" people");
    propertyLayout->addWidget(groupCountSpinBox_);
    groupDistributionLabel_ = new QLabel("Placement", propertyPanel_);
    groupDistributionLabel_->setStyleSheet("QLabel { color: #4f5d6b; background: transparent; border: 0; }");
    propertyLayout->addWidget(groupDistributionLabel_);
    groupDistributionComboBox_ = new QComboBox(propertyPanel_);
    groupDistributionComboBox_->addItem("Uniform", static_cast<int>(safecrowd::domain::InitialPlacementDistribution::Uniform));
    groupDistributionComboBox_->addItem("Random", static_cast<int>(safecrowd::domain::InitialPlacementDistribution::Random));
    propertyLayout->addWidget(groupDistributionComboBox_);
    propertyLayout->addStretch(1);

    connect(selectToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::Select); });
    connect(individualToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::IndividualPlacement); });
    connect(groupToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::GroupPlacement); });
    connect(sourceToolButton_, &QToolButton::clicked, this, [this]() {
        configureSourcePlacementTool(sourceToolButton_->mapToGlobal(QPoint(0, sourceToolButton_->height())));
    });
    connect(blockDoorToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::BlockDoor); });
    connect(fireHazardToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::FireHazard); });
    connect(smokeHazardToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::SmokeHazard); });
    connect(routeGuidanceToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::RouteGuidance); });

    setToolMode(ToolMode::Select);
    repositionToolbars();
}

}  // namespace safecrowd::application
