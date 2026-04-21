#include "application/ProjectNavigatorWidget.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "application/ProjectListWidget.h"
#include "application/ProjectNavigatorActions.h"

namespace safecrowd::application {

ProjectNavigatorWidget::ProjectNavigatorWidget(const QList<ProjectMetadata>& projects, QWidget* parent)
    : QWidget(parent) {
    setObjectName("ProjectNavigatorWidget");
    setAutoFillBackground(true);
    setStyleSheet("#ProjectNavigatorWidget { background: #ffffff; }");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(36, 32, 36, 40);
    rootLayout->setSpacing(54);

    auto* title = new QLabel("SafeCrowd", this);
    QFont titleFont;
    titleFont.setPointSize(38);
    titleFont.setWeight(QFont::Normal);
    title->setFont(titleFont);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    rootLayout->addWidget(title);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(82);

    projectList_ = new ProjectListWidget(projects, this);
    actions_ = new ProjectNavigatorActions(this);

    contentLayout->addWidget(projectList_, 1);
    contentLayout->addWidget(actions_, 0, Qt::AlignTop);

    rootLayout->addLayout(contentLayout, 1);
}

void ProjectNavigatorWidget::setNewProjectHandler(std::function<void()> handler) {
    actions_->setNewProjectHandler(std::move(handler));
}

void ProjectNavigatorWidget::setOpenProjectHandler(std::function<void(const ProjectMetadata&)> handler) {
    projectList_->setOpenProjectHandler(std::move(handler));
}

}  // namespace safecrowd::application
