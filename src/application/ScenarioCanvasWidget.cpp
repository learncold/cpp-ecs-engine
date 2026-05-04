#include "application/ScenarioCanvasWidget.h"

#include "application/ToolIconResources.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>

#include <QCoreApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

constexpr int kTopToolbarHeight = 44;
constexpr int kPropertyPanelHeight = 42;
constexpr int kToolbarButtonSize = 44;
constexpr double kViewportPadding = 24.0;
constexpr double kDefaultInitialSpeed = 1.2;
constexpr double kOccupantMarkerRadius = 5.0;
constexpr double kOccupantWorldRadius = 0.25;
constexpr double kOccupantMinSpacing = kOccupantWorldRadius * 2.0;
constexpr double kGeometryEpsilon = 1e-9;
constexpr double kSelectionDragThresholdPixels = 4.0;
const QColor kSelectionHighlightColor("#0b3d78");

struct PointBounds {
    double minX{0.0};
    double minY{0.0};
    double maxX{0.0};
    double maxY{0.0};
};

bool matchesFloor(const std::string& elementFloorId, const QString& floorId) {
    return floorId.isEmpty() || elementFloorId.empty() || QString::fromStdString(elementFloorId) == floorId;
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

bool pointInRing(const std::vector<safecrowd::domain::Point2D>& ring, const safecrowd::domain::Point2D& point) {
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

double distancePointToSegment(
    const safecrowd::domain::Point2D& point,
    const safecrowd::domain::Point2D& start,
    const safecrowd::domain::Point2D& end) {
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

    if (type == "block") {
        painter.setPen(QPen(color, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(22, 22), 11.5, 11.5);
        painter.drawLine(QPointF(14.5, 29.5), QPointF(29.5, 14.5));
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

    if (toolMode_ != ToolMode::BlockDoor) {
        return;
    }

    const auto targetId = elementId.toStdString();
    const auto it = std::find_if(layout_.connections.begin(), layout_.connections.end(), [&](const auto& connection) {
        return connection.id == targetId;
    });
    if (it == layout_.connections.end()) {
        return;
    }

    addConnectionBlockForConnection(*it);
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
        dragCurrent_ = event->position();
        update();
        event->accept();
        return;
    }
    if (selectionDragging_) {
        selectionDragCurrent_ = event->position();
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ScenarioCanvasWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);

    if (camera_.beginPan(event)) {
        return;
    }

    if (event->button() == Qt::RightButton) {
        const auto point = unmapPoint(event->position());
        constexpr double kPickRadiusPixels = 18.0;
        const auto offsetPoint = unmapPoint(event->position() + QPointF(kPickRadiusPixels, 0.0));
        const auto dx = offsetPoint.x - point.x;
        const auto dy = offsetPoint.y - point.y;
        const auto hitTolerance = std::max(1.2, std::hypot(dx, dy));
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
        if (pointInRing(zone.area.outline, point)) {
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

QString ScenarioCanvasWidget::placementAt(const QPointF& position, const LayoutCanvasTransform& transform) const {
    constexpr double kPickRadius = kOccupantMarkerRadius + 6.0;
    for (auto it = placements_.rbegin(); it != placements_.rend(); ++it) {
        const auto& placement = *it;
        if (!currentFloorId_.isEmpty() && !placement.floorId.isEmpty() && placement.floorId != currentFloorId_) {
            continue;
        }
        if (placement.area.empty()) {
            continue;
        }
        if (placement.kind == ScenarioCrowdPlacementKind::Individual || placement.area.size() < 4) {
            if (QLineF(position, transform.map(placement.area.front())).length() <= kPickRadius) {
                return placement.id;
            }
            continue;
        }

        const auto markers = fallbackDisplayPositions(placement);
        for (int index = 0; index < static_cast<int>(markers.size()); ++index) {
            const auto& worldPoint = markers[static_cast<std::size_t>(index)];
            if (QLineF(position, transform.map(worldPoint)).length() <= kPickRadius) {
                return QString("%1/occupant-%2").arg(placement.id).arg(index + 1);
            }
        }

        if (groupMarkerBounds(placement, transform).contains(position)) {
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
    const auto prefix = kind == ScenarioCrowdPlacementKind::Individual ? "individual" : "group";
    return QString("%1-%2").arg(prefix).arg(static_cast<int>(placements_.size()) + 1);
}

QString ScenarioCanvasWidget::nextConnectionBlockId() const {
    return QString("block-%1").arg(static_cast<int>(connectionBlocks_.size()) + 1);
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
    if (blockDoorToolButton_ != nullptr) {
        blockDoorToolButton_->setChecked(mode == ToolMode::BlockDoor);
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
    blockDoorToolButton_ = makeButton(makeToolIcon("block", QColor("#c0392b")), "block door");
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
    connect(blockDoorToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::BlockDoor); });

    setToolMode(ToolMode::Select);
    repositionToolbars();
}

}  // namespace safecrowd::application
