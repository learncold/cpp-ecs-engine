#include "application/ProjectNavigatorActions.h"

#include <QFont>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "application/UiStyle.h"

namespace safecrowd::application {
namespace {

QPushButton* createNavigatorButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setFont(ui::font(ui::FontRole::Body));
    button->setMinimumSize(220, 48);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

}  // namespace

ProjectNavigatorActions::ProjectNavigatorActions(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setStyleSheet(
        "#ProjectNavigatorActions {"
        " background: #ffffff;"
        " border: 1px solid #d7e0ea;"
        " border-radius: 20px;"
        "}"
    );
    setObjectName("ProjectNavigatorActions");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);
    layout->setAlignment(Qt::AlignTop);

    auto* newProjectButton = createNavigatorButton("+ New Project", this);
    newProjectButton->setStyleSheet(ui::primaryButtonStyleSheet());
    layout->addWidget(newProjectButton);

    auto* openFolderButton = createNavigatorButton("Open Folder", this);
    openFolderButton->setStyleSheet(ui::secondaryButtonStyleSheet());
    layout->addWidget(openFolderButton);

    layout->addStretch(1);

    connect(newProjectButton, &QPushButton::clicked, this, [this]() {
        if (newProjectHandler_) {
            newProjectHandler_();
        }
    });
}

void ProjectNavigatorActions::setNewProjectHandler(std::function<void()> handler) {
    newProjectHandler_ = std::move(handler);
}

}  // namespace safecrowd::application
