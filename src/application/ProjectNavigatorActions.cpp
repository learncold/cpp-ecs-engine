#include "application/ProjectNavigatorActions.h"

#include <QFont>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace safecrowd::application {
namespace {

QPushButton* createNavigatorButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);

    QFont font;
    font.setPointSize(22);
    font.setWeight(QFont::Normal);
    button->setFont(font);

    button->setMinimumSize(214, 54);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(
        "QPushButton {"
        " background: #ffffff;"
        " border: 1px solid #555555;"
        " color: #000000;"
        " padding: 4px 12px;"
        " text-align: center;"
        "}"
        "QPushButton:hover { background: #f4f4f4; }"
        "QPushButton:pressed { background: #e9e9e9; }");

    return button;
}

}  // namespace

ProjectNavigatorActions::ProjectNavigatorActions(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(34);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* newProjectButton = createNavigatorButton("+ New Project", this);
    layout->addWidget(newProjectButton);
    layout->addWidget(createNavigatorButton("Open Folder", this));
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
