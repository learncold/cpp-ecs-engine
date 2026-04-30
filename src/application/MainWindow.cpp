#include "application/MainWindow.h"

#include <filesystem>

#include <QMessageBox>

#include "application/LayoutReviewWidget.h"
#include "application/NewProjectWidget.h"
#include "application/ProjectPersistence.h"
#include "application/ProjectNavigatorWidget.h"
#include "application/ScenarioAuthoringWidget.h"
#include "domain/DemoLayouts.h"
#include "domain/DxfImportService.h"
#include "domain/ImportIssue.h"
#include "domain/ImportOrchestrator.h"
#include "domain/ImportValidationService.h"
#include "domain/SafeCrowdDomain.h"

namespace safecrowd::application {
namespace {

void applySavedReviewState(const ProjectMetadata& metadata, safecrowd::domain::ImportResult* importResult) {
    if (metadata.isBuiltInDemo() || importResult == nullptr) {
        return;
    }

    ProjectPersistence::loadProjectReview(metadata, importResult);
}

safecrowd::domain::ImportResult makeDemoImportResult() {
    safecrowd::domain::ImportResult result;
    result.layout = safecrowd::domain::DemoLayouts::demoFacility();

    safecrowd::domain::ImportValidationService validator;
    result.issues = validator.validate(*result.layout);
    result.reviewStatus = safecrowd::domain::hasBlockingImportIssue(result.issues)
        ? safecrowd::domain::ImportReviewStatus::Pending
        : safecrowd::domain::ImportReviewStatus::NotRequired;
    return result;
}

}  // namespace

MainWindow::MainWindow(safecrowd::domain::SafeCrowdDomain& domain, QWidget* parent)
    : QMainWindow(parent),
      domain_(domain) {
    setWindowTitle("SafeCrowd");
    resize(1280, 720);
    setMinimumSize(960, 540);
    showProjectNavigator();
}

void MainWindow::showProjectNavigator() {
    auto* navigator = new ProjectNavigatorWidget(ProjectPersistence::loadRecentProjects(), this);
    navigator->setNewProjectHandler([this]() {
        showNewProject();
    });
    navigator->setOpenProjectHandler([this](const ProjectMetadata& metadata) {
        openProject(metadata);
    });
    navigator->setDeleteProjectHandler([this](const ProjectMetadata& metadata) {
        if (metadata.isBuiltInDemo()) {
            QMessageBox::information(this, "Delete Project", "The built-in demo project cannot be deleted.");
            return;
        }

        const auto choice = QMessageBox::question(
            this,
            "Delete Project",
            QString("Delete \"%1\" and its project folder?\n\n%2")
                .arg(metadata.name, metadata.folderPath),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (choice != QMessageBox::Yes) {
            return;
        }

        QString errorMessage;
        if (!ProjectPersistence::deleteProject(metadata, &errorMessage)) {
            QMessageBox::warning(this, "Delete Project", errorMessage);
            return;
        }

        showProjectNavigator();
    });
    setCentralWidget(navigator);
}

void MainWindow::showNewProject() {
    auto* newProject = new NewProjectWidget(this);
    newProject->setCancelHandler([this]() {
        showProjectNavigator();
    });
    newProject->setDoneHandler([this](const NewProjectRequest& request) {
        createProject(request);
    });
    setCentralWidget(newProject);
}

void MainWindow::createProject(const NewProjectRequest& request) {
    if (request.projectName.isEmpty() || request.layoutPath.isEmpty() || request.folderPath.isEmpty()) {
        QMessageBox::warning(this, "New Project", "Project name, layout file, and folder are required.");
        return;
    }

    (void)domain_;

    ProjectMetadata metadata{
        .name = request.projectName,
        .folderPath = request.folderPath,
        .layoutPath = request.layoutPath,
    };

    QString errorMessage;
    if (!ProjectPersistence::saveProject(metadata, &errorMessage)) {
        QMessageBox::warning(this, "New Project", errorMessage);
        return;
    }

    const auto savedMetadata = ProjectPersistence::loadProject(metadata.folderPath);
    showLayoutReview(savedMetadata.isValid() ? savedMetadata : metadata);
}

void MainWindow::openProject(const ProjectMetadata& metadata) {
    if (!metadata.isValid()) {
        QMessageBox::warning(this, "Open Project", "The selected project is missing required metadata.");
        return;
    }

    showLayoutReview(metadata);
}

void MainWindow::saveCurrentProject() {
    if (!hasCurrentProject_) {
        QMessageBox::warning(this, "Save Project", "No project is currently open.");
        return;
    }

    if (currentProject_.isBuiltInDemo()) {
        QMessageBox::information(this, "Save Project", "Built-in demo projects do not need to be saved.");
        return;
    }

    QString errorMessage;
    if (!ProjectPersistence::saveProject(currentProject_, &errorMessage)) {
        QMessageBox::warning(this, "Save Project", errorMessage);
        return;
    }

    if (auto* reviewWidget = dynamic_cast<LayoutReviewWidget*>(centralWidget())) {
        if (!ProjectPersistence::saveProjectReview(currentProject_, reviewWidget->currentImportResult(), &errorMessage)) {
            QMessageBox::warning(this, "Save Project", errorMessage);
            return;
        }
    } else if (auto* authoringWidget = dynamic_cast<ScenarioAuthoringWidget*>(centralWidget())) {
        if (!ProjectPersistence::saveScenarioAuthoringState(currentProject_, authoringWidget->currentState(), &errorMessage)) {
            QMessageBox::warning(this, "Save Project", errorMessage);
            return;
        }
    }

    currentProject_ = ProjectPersistence::loadProject(currentProject_.folderPath);
    QMessageBox::information(this, "Save Project", "Project saved.");
}

void MainWindow::showLayoutReview(const ProjectMetadata& metadata) {
    currentProject_ = metadata;
    hasCurrentProject_ = true;
    lastApprovedImportResult_.reset();

    auto importResult = metadata.isBuiltInDemo()
        ? makeDemoImportResult()
        : [&metadata]() {
            safecrowd::domain::DxfImportService importer;
            const safecrowd::domain::ImportRequest importRequest{
                .sourcePath = std::filesystem::path(metadata.layoutPath.toStdWString()),
                .requestedFormat = safecrowd::domain::ImportedFileFormat::Dxf,
                .preserveRawModel = true,
                .runValidation = true,
            };
            return importer.importFile(importRequest);
        }();

    applySavedReviewState(metadata, &importResult);

    showLayoutReview(metadata, std::move(importResult));
}

void MainWindow::showLayoutReview(const ProjectMetadata& metadata, safecrowd::domain::ImportResult importResult) {
    currentProject_ = metadata;
    hasCurrentProject_ = true;

    setCentralWidget(new LayoutReviewWidget(
        metadata.name,
        importResult,
        [this]() {
            saveCurrentProject();
        },
        [this]() {
            hasCurrentProject_ = false;
            currentProject_ = {};
            showProjectNavigator();
        },
        [this](const safecrowd::domain::ImportResult& approvedImportResult) {
            if (!currentProject_.isBuiltInDemo()) {
                QString errorMessage;
                if (!ProjectPersistence::saveProjectReview(currentProject_, approvedImportResult, &errorMessage)) {
                    QMessageBox::warning(this, "Approve Layout", errorMessage);
                    return;
                }
            }
            lastApprovedImportResult_ = approvedImportResult;
            showScenarioAuthoring(approvedImportResult);
        },
        this));
}

void MainWindow::showScenarioAuthoring(const safecrowd::domain::ImportResult& importResult) {
    if (!importResult.layout.has_value()) {
        QMessageBox::warning(this, "Scenario Authoring", "An approved layout is required before creating a scenario.");
        return;
    }

    ScenarioAuthoringWidget::InitialState initialState;
    const bool hasSavedScenarioState = ProjectPersistence::loadScenarioAuthoringState(
        currentProject_,
        *importResult.layout,
        &initialState);
    lastApprovedImportResult_ = importResult;

    auto saveHandler = [this]() {
        saveCurrentProject();
    };
    auto openProjectHandler = [this]() {
        hasCurrentProject_ = false;
        currentProject_ = {};
        showProjectNavigator();
    };
    auto backToLayoutReviewHandler = [this]() {
        if (lastApprovedImportResult_.has_value()) {
            showLayoutReview(currentProject_, *lastApprovedImportResult_);
        } else {
            showLayoutReview(currentProject_);
        }
    };

    if (hasSavedScenarioState) {
        setCentralWidget(new ScenarioAuthoringWidget(
            currentProject_.name,
            *importResult.layout,
            std::move(initialState),
            saveHandler,
            openProjectHandler,
            backToLayoutReviewHandler,
            this));
        return;
    }

    setCentralWidget(new ScenarioAuthoringWidget(
        currentProject_.name,
        *importResult.layout,
        saveHandler,
        openProjectHandler,
        backToLayoutReviewHandler,
        this));
}

}  // namespace safecrowd::application
