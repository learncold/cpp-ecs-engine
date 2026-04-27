#include "application/ScenarioCanvasWidget.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSpinBox>
#include <QToolButton>
#include <QWheelEvent>

namespace safecrowd::application {
namespace {

constexpr int kTopToolbarHeight = 44;
constexpr int kPropertyPanelHeight = 42;
constexpr int kToolbarButtonSize = 44;
constexpr double kViewportPadding = 24.0;

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

QIcon makeToolIcon(const QString& type, const QColor& color) {
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

    painter.setBrush(color);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(17, 15), 4.0, 4.0);
    painter.drawEllipse(QPointF(27, 15), 4.0, 4.0);
    painter.drawEllipse(QPointF(22, 25), 4.0, 4.0);
    painter.drawRoundedRect(QRectF(12, 30, 20, 5), 2.5, 2.5);
    return QIcon(pixmap);
}

}  // namespace

ScenarioCanvasWidget::ScenarioCanvasWidget(
    safecrowd::domain::FacilityLayout2D layout,
    QWidget* parent)
    : QWidget(parent),
      layout_(std::move(layout)) {
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
    update();
}

void ScenarioCanvasWidget::setPlacementsChangedHandler(std::function<void(const std::vector<ScenarioCrowdPlacement>&)> handler) {
    placementsChangedHandler_ = std::move(handler);
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
    QWidget::mouseMoveEvent(event);
}

void ScenarioCanvasWidget::mousePressEvent(QMouseEvent* event) {
    setFocus(Qt::MouseFocusReason);

    if (camera_.beginPan(event)) {
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

    QWidget::mousePressEvent(event);
}

void ScenarioCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (camera_.finishPan(event)) {
        return;
    }

    if (event->button() != Qt::LeftButton || !dragging_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    dragging_ = false;
    addGroupPlacement(dragStart_, event->position());
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

    painter.setPen(QPen(QColor("#d7e0ea"), 1));
    painter.setBrush(QColor("#ffffff"));
    painter.drawRoundedRect(viewport.adjusted(-18, -18, 18, 18), 14, 14);

    drawFacilityLayoutCanvas(painter, layout_, transform);

    painter.setPen(Qt::NoPen);
    for (const auto& placement : placements_) {
        if (placement.kind == ScenarioCrowdPlacementKind::Individual) {
            if (placement.area.empty()) {
                continue;
            }
            painter.setBrush(QColor("#1f5fae"));
            painter.drawEllipse(transform.map(placement.area.front()), 5.0, 5.0);
            continue;
        }

        if (placement.area.size() >= 4) {
            QPainterPath path;
            path.moveTo(transform.map(placement.area.front()));
            for (std::size_t index = 1; index < placement.area.size(); ++index) {
                path.lineTo(transform.map(placement.area[index]));
            }
            path.closeSubpath();
            painter.setPen(QPen(QColor("#1f5fae"), 1.8));
            painter.setBrush(QColor(31, 95, 174, 54));
            painter.drawPath(path);

            painter.setBrush(QColor("#1f5fae"));
            const int markerCount = std::min(20, std::max(1, placement.occupantCount));
            const QRectF rect(
                transform.map(placement.area[0]),
                transform.map(placement.area[2]));
            for (int index = 0; index < markerCount; ++index) {
                const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(markerCount))));
                const int row = index / columns;
                const int column = index % columns;
                const QPointF point(
                    rect.normalized().left() + (column + 0.5) * rect.normalized().width() / columns,
                    rect.normalized().top() + (row + 0.5) * rect.normalized().height() / columns);
                painter.drawEllipse(point, 3.2, 3.2);
            }
        }
    }

    if (dragging_) {
        painter.setPen(QPen(QColor("#1f5fae"), 1.5, Qt::DashLine));
        painter.setBrush(QColor(31, 95, 174, 36));
        painter.drawRect(QRectF(dragStart_, dragCurrent_).normalized());
    }
}

void ScenarioCanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    repositionToolbars();
}

void ScenarioCanvasWidget::wheelEvent(QWheelEvent* event) {
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

std::optional<LayoutCanvasBounds> ScenarioCanvasWidget::collectBounds() const {
    return collectLayoutCanvasBounds(layout_);
}

LayoutCanvasTransform ScenarioCanvasWidget::currentTransform(const LayoutCanvasBounds& bounds) const {
    return LayoutCanvasTransform(bounds, previewViewport(), camera_.zoom(), camera_.panOffset());
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
        if (pointInRing(zone.area.outline, point)) {
            return QString::fromStdString(zone.id);
        }
    }
    return {};
}

QString ScenarioCanvasWidget::nextPlacementId(ScenarioCrowdPlacementKind kind) const {
    const auto prefix = kind == ScenarioCrowdPlacementKind::Individual ? "individual" : "group";
    return QString("%1-%2").arg(prefix).arg(static_cast<int>(placements_.size()) + 1);
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

    const auto id = nextPlacementId(ScenarioCrowdPlacementKind::Group);
    const auto count = groupCountSpinBox_ == nullptr ? 25 : groupCountSpinBox_->value();
    placements_.push_back({
        .id = id,
        .name = QString("Group %1").arg(id.section('-', -1)),
        .kind = ScenarioCrowdPlacementKind::Group,
        .zoneId = zoneId,
        .area = {topLeft, topRight, bottomRight, bottomLeft},
        .occupantCount = count,
    });
    emitPlacementsChanged();
    update();
}

void ScenarioCanvasWidget::addIndividualPlacement(const QPointF& position) {
    const auto point = unmapPoint(position);
    const auto zoneId = zoneAt(point);
    if (zoneId.isEmpty()) {
        return;
    }

    const auto id = nextPlacementId(ScenarioCrowdPlacementKind::Individual);
    placements_.push_back({
        .id = id,
        .name = QString("Individual %1").arg(id.section('-', -1)),
        .kind = ScenarioCrowdPlacementKind::Individual,
        .zoneId = zoneId,
        .area = {point},
        .occupantCount = 1,
    });
    emitPlacementsChanged();
    update();
}

void ScenarioCanvasWidget::emitPlacementsChanged() {
    if (placementsChangedHandler_) {
        placementsChangedHandler_(placements_);
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
    if (groupCountSpinBox_ != nullptr) {
        groupCountSpinBox_->setVisible(mode == ToolMode::GroupPlacement);
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
        "QSpinBox { min-height: 24px; padding: 0 8px; border: 1px solid #c9d5e2; border-radius: 0px; background: #ffffff; color: #16202b; }");
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
    topLayout->addStretch(1);

    auto* countLabel = new QLabel("Group count", propertyPanel_);
    countLabel->setStyleSheet("QLabel { color: #4f5d6b; background: transparent; border: 0; }");
    propertyLayout->addWidget(countLabel);
    groupCountSpinBox_ = new QSpinBox(propertyPanel_);
    groupCountSpinBox_->setRange(1, 5000);
    groupCountSpinBox_->setValue(25);
    groupCountSpinBox_->setSuffix(" people");
    propertyLayout->addWidget(groupCountSpinBox_);
    propertyLayout->addStretch(1);

    connect(selectToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::Select); });
    connect(individualToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::IndividualPlacement); });
    connect(groupToolButton_, &QToolButton::clicked, this, [this]() { setToolMode(ToolMode::GroupPlacement); });

    setToolMode(ToolMode::Select);
    repositionToolbars();
}

}  // namespace safecrowd::application
