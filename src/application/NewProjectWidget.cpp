#include "application/NewProjectWidget.h"

#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace safecrowd::application {
namespace {

QFont makeFont(int pointSize) {
    QFont font;
    font.setPointSize(pointSize);
    font.setWeight(QFont::Normal);
    return font;
}

QPushButton* createOutlinedButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setFont(makeFont(22));
    button->setMinimumSize(196, 50);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(
        "QPushButton {"
        " background: #ffffff;"
        " border: 1px solid #555555;"
        " color: #000000;"
        " padding: 4px 18px;"
        "}"
        "QPushButton:hover { background: #f4f4f4; }"
        "QPushButton:pressed { background: #e9e9e9; }");
    return button;
}

QLineEdit* createTextInput(QWidget* parent) {
    auto* input = new QLineEdit(parent);
    input->setFont(makeFont(18));
    input->setMinimumHeight(42);
    input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    input->setStyleSheet(
        "QLineEdit {"
        " border: 1px solid #555555;"
        " padding: 4px 8px;"
        " background: #ffffff;"
        "}");
    return input;
}

}  // namespace

NewProjectWidget::NewProjectWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName("NewProjectWidget");
    setStyleSheet("#NewProjectWidget { background: #ffffff; }");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(36, 34, 22, 22);
    rootLayout->setSpacing(0);

    auto* title = new QLabel("New Project", this);
    title->setFont(makeFont(38));
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    rootLayout->addWidget(title);
    rootLayout->addSpacing(34);

    auto* projectNameLabel = new QLabel("Project Name", this);
    projectNameLabel->setFont(makeFont(22));
    rootLayout->addWidget(projectNameLabel);
    rootLayout->addSpacing(22);

    projectNameEdit_ = createTextInput(this);
    projectNameEdit_->setMaximumWidth(760);
    rootLayout->addWidget(projectNameEdit_);
    rootLayout->addSpacing(28);

    auto* layoutLabel = new QLabel("Layout", this);
    layoutLabel->setFont(makeFont(22));
    rootLayout->addWidget(layoutLabel);
    rootLayout->addSpacing(20);

    auto* layoutRow = new QHBoxLayout();
    layoutRow->setSpacing(14);
    auto* layoutBrowseButton = createOutlinedButton("Browse", this);
    layoutPathEdit_ = createTextInput(this);
    layoutPathEdit_->setPlaceholderText("DXF file");
    layoutPathEdit_->setReadOnly(true);
    layoutPathEdit_->setMaximumWidth(760);
    layoutRow->addWidget(layoutBrowseButton, 0);
    layoutRow->addWidget(layoutPathEdit_, 1);
    layoutRow->addStretch(1);
    rootLayout->addLayout(layoutRow);
    rootLayout->addSpacing(42);

    auto* folderLabel = new QLabel("Folder", this);
    folderLabel->setFont(makeFont(22));
    rootLayout->addWidget(folderLabel);
    rootLayout->addSpacing(20);

    auto* folderRow = new QHBoxLayout();
    folderRow->setSpacing(14);
    auto* folderBrowseButton = createOutlinedButton("Browse", this);
    folderPathEdit_ = createTextInput(this);
    folderPathEdit_->setPlaceholderText("Project folder");
    folderPathEdit_->setReadOnly(true);
    folderPathEdit_->setMaximumWidth(760);
    folderRow->addWidget(folderBrowseButton, 0);
    folderRow->addWidget(folderPathEdit_, 1);
    folderRow->addStretch(1);
    rootLayout->addLayout(folderRow);

    rootLayout->addStretch(1);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(50);
    actionLayout->addStretch(1);

    auto* cancelButton = createOutlinedButton("Cancel", this);
    auto* doneButton = createOutlinedButton("Done", this);
    actionLayout->addWidget(cancelButton);
    actionLayout->addWidget(doneButton);
    rootLayout->addLayout(actionLayout);

    connect(layoutBrowseButton, &QPushButton::clicked, this, [this]() {
        const auto path = QFileDialog::getOpenFileName(this, "Select Layout DXF", QString(), "DXF Files (*.dxf);;All Files (*)");
        if (!path.isEmpty()) {
            layoutPathEdit_->setText(path);
        }
    });

    connect(folderBrowseButton, &QPushButton::clicked, this, [this]() {
        const auto path = QFileDialog::getExistingDirectory(this, "Select Project Folder");
        if (!path.isEmpty()) {
            folderPathEdit_->setText(path);
        }
    });

    connect(cancelButton, &QPushButton::clicked, this, [this]() {
        if (cancelHandler_) {
            cancelHandler_();
        }
    });

    connect(doneButton, &QPushButton::clicked, this, [this]() {
        if (doneHandler_) {
            doneHandler_(request());
        }
    });
}

void NewProjectWidget::setDoneHandler(std::function<void(const NewProjectRequest&)> handler) {
    doneHandler_ = std::move(handler);
}

void NewProjectWidget::setCancelHandler(std::function<void()> handler) {
    cancelHandler_ = std::move(handler);
}

NewProjectRequest NewProjectWidget::request() const {
    return {
        .projectName = projectNameEdit_->text().trimmed(),
        .layoutPath = layoutPathEdit_->text().trimmed(),
        .folderPath = folderPathEdit_->text().trimmed(),
    };
}

}  // namespace safecrowd::application
