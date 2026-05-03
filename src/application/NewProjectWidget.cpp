#include "application/NewProjectWidget.h"

#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "application/UiStyle.h"

namespace safecrowd::application {
namespace {

constexpr auto kProjectsRootName = "SafeCrowd Projects";

// String-only; saveProject creates the folder lazily on actual save.
QString defaultProjectsRoot() {
    auto base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    if (base.isEmpty()) {
        return QString();
    }
    return QDir(base).filePath(kProjectsRootName);
}

QString sanitizeFolderName(const QString& name) {
    static const QRegularExpression invalid(R"([\\/:*?"<>|])");
    auto cleaned = name.trimmed();
    cleaned.replace(invalid, "_");
    cleaned = cleaned.simplified();
    return cleaned;
}

bool folderIsAvailableForSuggestion(const QString& path) {
    QDir dir(path);
    if (!dir.exists()) {
        return true;
    }
    const auto entries = dir.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    return entries.isEmpty();
}

QString suggestProjectFolder(const QString& projectName) {
    const auto root = defaultProjectsRoot();
    if (root.isEmpty()) {
        return QString();
    }
    const auto sanitized = sanitizeFolderName(projectName);
    if (sanitized.isEmpty()) {
        return root;
    }
    const auto base = QDir(root).filePath(sanitized);
    if (folderIsAvailableForSuggestion(base)) {
        return base;
    }
    for (int suffix = 2; suffix < 1000; ++suffix) {
        const auto candidate = QDir(root).filePath(QStringLiteral("%1 (%2)").arg(sanitized).arg(suffix));
        if (folderIsAvailableForSuggestion(candidate)) {
            return candidate;
        }
    }
    return base;  // fall back; saveProject will surface a clear error
}

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
    layoutPathEdit_->setPlaceholderText("DXF file");
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
    actionLayout->addWidget(cancelButton);
    actionLayout->addWidget(doneButton);
    cardLayout->addLayout(actionLayout);

    connect(layoutBrowseButton, &QPushButton::clicked, this, [this]() {
        const auto path = QFileDialog::getOpenFileName(this, "Select Layout DXF", QString(), "DXF Files (*.dxf);;All Files (*)");
        if (!path.isEmpty()) {
            layoutPathEdit_->setText(path);
        }
    });

    connect(projectNameEdit_, &QLineEdit::textChanged, this, [this](const QString& name) {
        if (folderEditedByUser_) {
            return;
        }
        const auto suggestion = suggestProjectFolder(name);
        const QSignalBlocker blocker(folderPathEdit_);
        folderPathEdit_->setText(suggestion);
    });

    connect(folderBrowseButton, &QPushButton::clicked, this, [this]() {
        QString startDir = folderPathEdit_->text().trimmed();
        if (startDir.isEmpty()) {
            startDir = defaultProjectsRoot();
        }
        if (!startDir.isEmpty()) {
            QDir candidate(startDir);
            while (!candidate.exists() && !candidate.isRoot() && candidate.cdUp()) {
            }
            if (candidate.exists()) {
                startDir = candidate.absolutePath();
            }
        }

        const auto path = QFileDialog::getExistingDirectory(this, "Select Project Folder", startDir);
        if (!path.isEmpty()) {
            folderEditedByUser_ = true;
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
