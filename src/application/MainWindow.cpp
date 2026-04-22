#include "application/MainWindow.h"

#include <filesystem>

#include <QMessageBox>

#include "application/LayoutReviewWidget.h"
#include "application/NewProjectWidget.h"
#include "application/ProjectPersistence.h"
#include "application/ProjectNavigatorWidget.h"
#include "domain/DemoLayouts.h"
#include "domain/DxfImportService.h"
#include "domain/ImportIssue.h"
#include "domain/ImportOrchestrator.h"
#include "domain/ImportValidationService.h"
#include "domain/SafeCrowdDomain.h"

namespace safecrowd::application {
namespace {

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
    showLayoutReview(metadata);
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

    currentProject_ = ProjectPersistence::loadProject(currentProject_.folderPath);
    QMessageBox::information(this, "Save Project", "Project saved.");
}

void MainWindow::showLayoutReview(const ProjectMetadata& metadata) {
    currentProject_ = metadata;
    hasCurrentProject_ = true;

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

    setCentralWidget(new LayoutReviewWidget(metadata.name, importResult, [this]() {
        saveCurrentProject();
    }, this));
}

}  // namespace safecrowd::application
