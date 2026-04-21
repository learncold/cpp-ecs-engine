#include "application/ProjectListWidget.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace safecrowd::application {
namespace {

QFont makeListFont() {
    QFont font;
    font.setPointSize(20);
    font.setWeight(QFont::Normal);
    return font;
}

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
    setStyleSheet("#ProjectListWidget { border: 1px solid #767676; background: #ffffff; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(54, 36, 20, 36);
    layout->setSpacing(16);

    if (projects.empty()) {
        auto* emptyLabel = new QLabel("No saved projects", this);
        emptyLabel->setFont(makeListFont());
        emptyLabel->setStyleSheet("QLabel { color: #555555; }");
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
    row->setMinimumHeight(54);
    row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->setCursor(Qt::PointingHandCursor);
    row->setStyleSheet(
        "QPushButton {"
        " border: 0;"
        " background: transparent;"
        " text-align: left;"
        "}"
        "QPushButton:hover { background: #f5f5f5; }");

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(24);

    auto* nameLabel = new QLabel(project.name, row);
    nameLabel->setFont(makeListFont());
    nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    auto* dateLabel = new QLabel(displaySavedAt(project.savedAt), row);
    dateLabel->setFont(makeListFont());
    dateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dateLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    dateLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

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
