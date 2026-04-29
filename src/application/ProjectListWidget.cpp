#include "application/ProjectListWidget.h"

#include <QDateTime>
#include <QIcon>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "application/UiStyle.h"

namespace safecrowd::application {
namespace {

QString displaySavedAt(const QString& savedAt) {
    const auto dateTime = QDateTime::fromString(savedAt, Qt::ISODate);
    if (!dateTime.isValid()) {
        return {};
    }
    return QLocale().toString(dateTime, "yyyy-MM-dd AP h:mm");
}

QIcon makeTrashIcon(const QColor& color) {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(QPointF(10, 11), QPointF(22, 11));
    painter.drawLine(QPointF(14, 8), QPointF(18, 8));
    painter.drawLine(QPointF(13, 8), QPointF(19, 8));
    painter.drawRoundedRect(QRectF(11, 13, 10, 13), 2, 2);
    painter.drawLine(QPointF(14, 16), QPointF(14, 23));
    painter.drawLine(QPointF(18, 16), QPointF(18, 23));
    return QIcon(pixmap);
}

}  // namespace

ProjectListWidget::ProjectListWidget(const QList<ProjectMetadata>& projects, QWidget* parent)
    : QFrame(parent) {
    setObjectName("ProjectListWidget");
    setFrameShape(QFrame::StyledPanel);
    setLineWidth(1);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("#ProjectListWidget { border: 1px solid #d7e0ea; border-radius: 20px; background: #ffffff; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(8);

    auto* title = new QLabel("Recent Projects", this);
    title->setFont(ui::font(ui::FontRole::Title));
    layout->addWidget(title);
    layout->addSpacing(10);

    if (projects.empty()) {
        auto* emptyLabel = new QLabel("No saved projects", this);
        emptyLabel->setFont(ui::font(ui::FontRole::Body));
        emptyLabel->setStyleSheet(ui::mutedTextStyleSheet());
        layout->addWidget(emptyLabel);
    } else {
        for (const auto& project : projects) {
            addProjectRow(project);
        }
    }

    layout->addStretch(1);
}

void ProjectListWidget::setOpenProjectHandler(std::function<void(const ProjectMetadata&)> handler) {
    openProjectHandler_ = std::move(handler);
}

void ProjectListWidget::setDeleteProjectHandler(std::function<void(const ProjectMetadata&)> handler) {
    deleteProjectHandler_ = std::move(handler);
}

void ProjectListWidget::addProjectRow(const ProjectMetadata& project) {
    auto* row = new QWidget(this);
    row->setMinimumHeight(72);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* openButton = new QPushButton(row);
    openButton->setMinimumHeight(64);
    openButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    openButton->setCursor(Qt::PointingHandCursor);
    openButton->setStyleSheet(ui::ghostRowStyleSheet());

    auto* openLayout = new QHBoxLayout(openButton);
    openLayout->setContentsMargins(0, 0, 0, 0);
    openLayout->setSpacing(16);

    auto* nameLabel = new QLabel(project.name, openButton);
    nameLabel->setFont(ui::font(ui::FontRole::Body));
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* dateLabel = new QLabel(displaySavedAt(project.savedAt), openButton);
    dateLabel->setFont(ui::font(ui::FontRole::Caption));
    dateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dateLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    dateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    dateLabel->setStyleSheet(ui::subtleTextStyleSheet());

    openLayout->addWidget(nameLabel, 1);
    openLayout->addWidget(dateLabel, 0);
    layout->addWidget(openButton, 1);

    auto* deleteButton = new QPushButton(row);
    deleteButton->setIcon(makeTrashIcon(project.isBuiltInDemo() ? QColor("#9aa8b6") : QColor("#b42318")));
    deleteButton->setIconSize(QSize(24, 24));
    deleteButton->setToolTip(project.isBuiltInDemo() ? "Demo project cannot be deleted" : "Delete project");
    deleteButton->setAccessibleName(deleteButton->toolTip());
    deleteButton->setFixedSize(42, 42);
    deleteButton->setEnabled(!project.isBuiltInDemo());
    deleteButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    layout->addWidget(deleteButton, 0, Qt::AlignVCenter);

    connect(openButton, &QPushButton::clicked, this, [this, project]() {
        if (openProjectHandler_) {
            openProjectHandler_(project);
        }
    });

    connect(deleteButton, &QPushButton::clicked, this, [this, project]() {
        if (deleteProjectHandler_) {
            deleteProjectHandler_(project);
        }
    });

    if (auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout())) {
        parentLayout->addWidget(row);
    }
}

}  // namespace safecrowd::application
