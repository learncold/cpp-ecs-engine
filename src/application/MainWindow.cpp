#include "application/MainWindow.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <QMessageBox>

#include "application/LayoutReviewWidget.h"
#include "application/NewProjectWidget.h"
#include "application/ProjectPersistence.h"
#include "application/ProjectNavigatorWidget.h"
#include "application/ScenarioAuthoringWidget.h"
#include "domain/DemoFixtureService.h"
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
    safecrowd::domain::DemoFixtureService fixtureService;
    const auto fixture = fixtureService.createSprint1DemoFixture();

    safecrowd::domain::ImportResult result;
    result.layout = fixture.layout;

    safecrowd::domain::ImportValidationService validator;
    result.issues = validator.validate(*result.layout);
    result.reviewStatus = safecrowd::domain::hasBlockingImportIssue(result.issues)
        ? safecrowd::domain::ImportReviewStatus::Pending
        : safecrowd::domain::ImportReviewStatus::NotRequired;
    return result;
}

QString zoneLabel(const safecrowd::domain::Zone2D& zone) {
    const auto id = QString::fromStdString(zone.id);
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty() ? id : QString("%1  -  %2").arg(label, id);
}

const safecrowd::domain::Zone2D* findZone(
    const safecrowd::domain::FacilityLayout2D& layout,
    const std::string& id) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [&](const auto& zone) {
        return zone.id == id;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

ScenarioCrowdPlacement makeUiPlacement(const safecrowd::domain::InitialPlacement2D& placement) {
    ScenarioCrowdPlacement uiPlacement;
    uiPlacement.id = QString::fromStdString(placement.id);
    uiPlacement.name = QStringLiteral("Demo crowd group");
    uiPlacement.kind = (placement.targetAgentCount <= 1 && placement.area.outline.size() <= 1)
        ? ScenarioCrowdPlacementKind::Individual
        : ScenarioCrowdPlacementKind::Group;
    uiPlacement.zoneId = QString::fromStdString(placement.zoneId);
    uiPlacement.area = placement.area.outline;
    uiPlacement.occupantCount = static_cast<int>(placement.targetAgentCount);
    uiPlacement.velocity = placement.initialVelocity;
    return uiPlacement;
}

ScenarioAuthoringWidget::InitialState makeDemoScenarioAuthoringState(
    const safecrowd::domain::FacilityLayout2D& approvedLayout) {
    safecrowd::domain::DemoFixtureService fixtureService;
    const auto fixture = fixtureService.createSprint1DemoFixture();

    ScenarioAuthoringWidget::ScenarioState scenario;
    scenario.draft = fixture.baselineScenario;
    scenario.events = fixture.baselineScenario.control.events;
    scenario.stagedForRun = false;

    if (const auto* startZone = findZone(approvedLayout, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::MainRoomZoneId);
        startZone != nullptr) {
        scenario.startText = zoneLabel(*startZone);
    }
    if (const auto* destinationZone = findZone(approvedLayout, safecrowd::domain::DemoLayouts::Sprint1FacilityIds::ExitZoneId);
        destinationZone != nullptr) {
        scenario.destinationText = zoneLabel(*destinationZone);
    }

    for (const auto& placement : fixture.baselineScenario.population.initialPlacements) {
        scenario.crowdPlacements.push_back(makeUiPlacement(placement));
    }

    ScenarioAuthoringWidget::InitialState state;
    state.scenarios.push_back(std::move(scenario));
    state.currentScenarioIndex = 0;
    state.navigationView = ScenarioAuthoringWidget::NavigationView::Layout;
    state.rightPanelMode = ScenarioAuthoringWidget::RightPanelMode::Scenario;
    return state;
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
    if (currentProject_.isBuiltInDemo()) {
        initialState = makeDemoScenarioAuthoringState(*importResult.layout);
    }
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

    if (currentProject_.isBuiltInDemo() || hasSavedScenarioState) {
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
