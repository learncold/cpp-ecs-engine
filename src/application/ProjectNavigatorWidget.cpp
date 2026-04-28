#include "application/ProjectNavigatorWidget.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "application/ProjectListWidget.h"
#include "application/ProjectNavigatorActions.h"
#include "application/UiStyle.h"

namespace safecrowd::application {

ProjectNavigatorWidget::ProjectNavigatorWidget(const QList<ProjectMetadata>& projects, QWidget* parent)
    : QWidget(parent) {
    setObjectName("ProjectNavigatorWidget");
    setAutoFillBackground(true);
    setStyleSheet("#ProjectNavigatorWidget { background: #f4f7fb; }");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(48, 40, 48, 48);
    rootLayout->setSpacing(28);

    auto* title = new QLabel("SafeCrowd", this);
    title->setFont(ui::font(ui::FontRole::Hero));
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    title->setStyleSheet("QLabel { color: #16202b; }");
    rootLayout->addWidget(title);

    auto* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(24);

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

void ProjectNavigatorWidget::setDeleteProjectHandler(std::function<void(const ProjectMetadata&)> handler) {
    projectList_->setDeleteProjectHandler(std::move(handler));
}

}  // namespace safecrowd::application
