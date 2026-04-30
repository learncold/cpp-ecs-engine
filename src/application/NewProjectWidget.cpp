#include "application/NewProjectWidget.h"

#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "application/UiStyle.h"

namespace safecrowd::application {
namespace {

QPushButton* createOutlinedButton(const QString& text, QWidget* parent) {
    auto* button = new QPushButton(text, parent);
    button->setFont(ui::font(ui::FontRole::Body));
    button->setMinimumSize(132, 46);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet(ui::secondaryButtonStyleSheet());
    return button;
}

QLineEdit* createTextInput(QWidget* parent) {
    auto* input = new QLineEdit(parent);
    input->setFont(ui::font(ui::FontRole::Body));
    input->setMinimumHeight(46);
    input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return input;
}

}  // namespace

NewProjectWidget::NewProjectWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName("NewProjectWidget");
    setStyleSheet("#NewProjectWidget { background: #f4f7fb; }");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(48, 40, 48, 40);
    rootLayout->setSpacing(0);

    auto* card = new QFrame(this);
    card->setStyleSheet(ui::panelStyleSheet());
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(0);
    rootLayout->addWidget(card, 0, Qt::AlignTop);
    rootLayout->addStretch(1);

    auto* title = new QLabel("New Project", this);
    title->setFont(ui::font(ui::FontRole::Hero));
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    cardLayout->addWidget(title);
    cardLayout->addSpacing(28);

    auto* projectNameLabel = new QLabel("Project Name", this);
    projectNameLabel->setFont(ui::font(ui::FontRole::SectionTitle));
    cardLayout->addWidget(projectNameLabel);
    cardLayout->addSpacing(10);

    projectNameEdit_ = createTextInput(this);
    projectNameEdit_->setStyleSheet(ui::textFieldStyleSheet(false));
    projectNameEdit_->setMaximumWidth(760);
    cardLayout->addWidget(projectNameEdit_);
    cardLayout->addSpacing(20);

    auto* layoutLabel = new QLabel("Layout", this);
    layoutLabel->setFont(ui::font(ui::FontRole::SectionTitle));
    cardLayout->addWidget(layoutLabel);
    cardLayout->addSpacing(10);

    auto* layoutRow = new QHBoxLayout();
    layoutRow->setSpacing(12);
    auto* layoutBrowseButton = createOutlinedButton("Browse", this);
    layoutPathEdit_ = createTextInput(this);
    layoutPathEdit_->setPlaceholderText("Select a DXF file using Browse");
    layoutPathEdit_->setReadOnly(true);
    layoutPathEdit_->setStyleSheet(ui::textFieldStyleSheet(true));
    layoutPathEdit_->setMaximumWidth(760);
    layoutRow->addWidget(layoutBrowseButton, 0);
    layoutRow->addWidget(layoutPathEdit_, 1);
    layoutRow->addStretch(1);
    cardLayout->addLayout(layoutRow);
    cardLayout->addSpacing(20);

    auto* folderLabel = new QLabel("Folder", this);
    folderLabel->setFont(ui::font(ui::FontRole::SectionTitle));
    cardLayout->addWidget(folderLabel);
    cardLayout->addSpacing(10);

    auto* folderRow = new QHBoxLayout();
    folderRow->setSpacing(12);
    auto* folderBrowseButton = createOutlinedButton("Browse", this);
    folderPathEdit_ = createTextInput(this);
    folderPathEdit_->setPlaceholderText("Project folder");
    folderPathEdit_->setReadOnly(true);
    folderPathEdit_->setStyleSheet(ui::textFieldStyleSheet(true));
    folderPathEdit_->setMaximumWidth(760);
    folderRow->addWidget(folderBrowseButton, 0);
    folderRow->addWidget(folderPathEdit_, 1);
    folderRow->addStretch(1);
    cardLayout->addLayout(folderRow);
    cardLayout->addSpacing(28);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(12);
    actionLayout->addStretch(1);

    auto* cancelButton = createOutlinedButton("Cancel", this);
    auto* doneButton = createOutlinedButton("Done", this);
    doneButton->setStyleSheet(ui::primaryButtonStyleSheet());
    doneButton->setToolTip("Project name, DXF layout file, and folder are all required");
    actionLayout->addWidget(cancelButton);
    actionLayout->addWidget(doneButton);
    cardLayout->addLayout(actionLayout);

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
