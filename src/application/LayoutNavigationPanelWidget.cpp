#include "application/LayoutNavigationPanelWidget.h"

#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "application/UiStyle.h"

namespace safecrowd::application {
namespace {

class ElidedRowButton final : public QPushButton {
public:
    explicit ElidedRowButton(const QString& fullText, QWidget* parent = nullptr)
        : QPushButton(parent),
          fullText_(fullText) {
        setToolTip(fullText_);
        setFont(ui::font(ui::FontRole::Body));
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumWidth(0);
        setStyleSheet(ui::ghostRowStyleSheet());
        updateElidedText();
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QPushButton::resizeEvent(event);
        updateElidedText();
    }

private:
    void updateElidedText() {
        const QFontMetrics metrics(font());
        setText(metrics.elidedText(fullText_, Qt::ElideRight, std::max(24, width() - 34)));
    }

    QString fullText_{};
};

QLabel* createLabel(const QString& text, QWidget* parent, ui::FontRole role = ui::FontRole::Body) {
    auto* label = new QLabel(text, parent);
    label->setFont(ui::font(role));
    label->setWordWrap(true);
    return label;
}

QString zoneLabel(const safecrowd::domain::Zone2D& zone) {
    const auto id = QString::fromStdString(zone.id);
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty() ? id : QString("%1  -  %2").arg(label, id);
}

ElidedRowButton* createRow(
    const QString& text,
    const QString& elementId,
    const std::function<void(const QString&)>& selectElementHandler,
    QWidget* parent) {
    auto* row = new ElidedRowButton(text, parent);
    if (selectElementHandler) {
        QObject::connect(row, &QPushButton::clicked, row, [selectElementHandler, elementId]() {
            selectElementHandler(elementId);
        });
    }
    return row;
}

void addSectionHeader(QVBoxLayout* layout, const QString& header, QWidget* parent) {
    auto* sectionHeader = createLabel(header, parent, ui::FontRole::SectionTitle);
    sectionHeader->setStyleSheet(ui::subtleTextStyleSheet());
    layout->addWidget(sectionHeader);
}

}  // namespace

LayoutNavigationPanelWidget::LayoutNavigationPanelWidget(
    const safecrowd::domain::FacilityLayout2D* facilityLayout,
    std::function<void(const QString&)> selectElementHandler,
    QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* title = createLabel("Layout", this, ui::FontRole::Title);
    layout->addWidget(title);

    if (facilityLayout == nullptr) {
        auto* emptyLabel = createLabel("No recognized layout elements", this);
        emptyLabel->setStyleSheet(ui::mutedTextStyleSheet());
        layout->addWidget(emptyLabel);
        layout->addStretch(1);
        return;
    }

    auto* summary = createLabel(
        QString("%1 zones\n%2 connections\n%3 walls")
            .arg(static_cast<int>(facilityLayout->zones.size()))
            .arg(static_cast<int>(facilityLayout->connections.size()))
            .arg(static_cast<int>(facilityLayout->barriers.size())),
        this);
    summary->setStyleSheet(ui::mutedTextStyleSheet());
    layout->addWidget(summary);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui::polishScrollArea(scrollArea);

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setStyleSheet("QWidget { background: transparent; }");
    auto* sectionsLayout = new QVBoxLayout(scrollContent);
    sectionsLayout->setContentsMargins(0, 0, 14, 0);
    sectionsLayout->setSpacing(10);

    const auto addZoneSection = [&](const QString& header, auto predicate) {
        bool hasRows = false;
        for (const auto& zone : facilityLayout->zones) {
            if (predicate(zone)) {
                hasRows = true;
                break;
            }
        }
        if (!hasRows) {
            return;
        }

        addSectionHeader(sectionsLayout, header, scrollContent);
        for (const auto& zone : facilityLayout->zones) {
            if (!predicate(zone)) {
                continue;
            }
            sectionsLayout->addWidget(createRow(
                zoneLabel(zone),
                QString::fromStdString(zone.id),
                selectElementHandler,
                scrollContent));
        }
    };

    addZoneSection("Rooms", [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Room || zone.kind == safecrowd::domain::ZoneKind::Unknown;
    });
    addZoneSection("Corridors", [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Corridor
            || zone.kind == safecrowd::domain::ZoneKind::Intersection;
    });
    addZoneSection("Exits", [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Exit
            || zone.kind == safecrowd::domain::ZoneKind::Stair;
    });

    if (!facilityLayout->connections.empty()) {
        addSectionHeader(sectionsLayout, "Connections", scrollContent);
        for (const auto& connection : facilityLayout->connections) {
            sectionsLayout->addWidget(createRow(
                QString("%1  ->  %2")
                    .arg(QString::fromStdString(connection.fromZoneId), QString::fromStdString(connection.toZoneId)),
                QString::fromStdString(connection.id),
                selectElementHandler,
                scrollContent));
        }
    }

    if (!facilityLayout->barriers.empty()) {
        addSectionHeader(sectionsLayout, "Walls", scrollContent);
        for (const auto& barrier : facilityLayout->barriers) {
            sectionsLayout->addWidget(createRow(
                QString::fromStdString(barrier.id),
                QString::fromStdString(barrier.id),
                selectElementHandler,
                scrollContent));
        }
    }

    if (sectionsLayout->isEmpty()) {
        auto* emptyLabel = createLabel("No recognized layout elements", scrollContent);
        emptyLabel->setStyleSheet(ui::mutedTextStyleSheet());
        sectionsLayout->addWidget(emptyLabel);
    }

    sectionsLayout->addStretch(1);
    scrollArea->setWidget(scrollContent);
    layout->addWidget(scrollArea, 1);
}

}  // namespace safecrowd::application
