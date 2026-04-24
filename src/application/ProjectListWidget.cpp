#include "application/ProjectListWidget.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
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

void ProjectListWidget::addProjectRow(const ProjectMetadata& project) {
    auto* row = new QPushButton(this);
    row->setMinimumHeight(72);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->setCursor(Qt::PointingHandCursor);
    row->setStyleSheet(ui::ghostRowStyleSheet());

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto* nameLabel = new QLabel(project.name, row);
    nameLabel->setFont(ui::font(ui::FontRole::Body));
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* dateLabel = new QLabel(displaySavedAt(project.savedAt), row);
    dateLabel->setFont(ui::font(ui::FontRole::Caption));
    dateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dateLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    dateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    dateLabel->setStyleSheet(ui::subtleTextStyleSheet());

    layout->addWidget(nameLabel, 1);
    layout->addWidget(dateLabel, 0);

    connect(row, &QPushButton::clicked, this, [this, project]() {
        if (openProjectHandler_) {
            openProjectHandler_(project);
        }
    });

    if (auto* parentLayout = qobject_cast<QVBoxLayout*>(this->layout())) {
        parentLayout->addWidget(row);
    }
}

}  // namespace safecrowd::application
