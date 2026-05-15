#include "application/MainWindow.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>

#include <QMessageBox>

#include "application/LayoutReviewWidget.h"
#include "application/NewProjectWidget.h"
#include "application/ProjectPersistence.h"
#include "application/ProjectNavigatorWidget.h"
#include "application/ScenarioAuthoringWidget.h"
#include "application/ScenarioBatchResultWidget.h"
#include "application/ScenarioResultWidget.h"
#include "application/ScenarioRunWidget.h"
#include "domain/DemoFixtureService.h"
#include "domain/DxfImportService.h"
#include "domain/ImportIssue.h"
#include "domain/ImportOrchestrator.h"
#include "domain/ImportValidationService.h"
#include "domain/SafeCrowdDomain.h"
#include "domain/ScenarioAuthoring.h"

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

ProjectWorkspaceState makeEvacuationScenarioDemoWorkspace() {
    using namespace safecrowd::domain;

    safecrowd::domain::DemoFixtureService fixtureService;
    auto fixture = fixtureService.createSprint1BlockedDoorResultFixture();

    SavedScenarioAuthoringState authoring;
    authoring.scenarios.push_back({
        .draft = fixture.baselineScenario,
        .baseScenarioId = {},
        .stagedForRun = true,
    });
    authoring.scenarios.push_back({
        .draft = fixture.alternativeScenario,
        .baseScenarioId = fixture.baselineScenario.scenarioId,
        .stagedForRun = true,
    });
    authoring.currentScenarioIndex = 1;
    authoring.navigationView = SavedNavigationView::Events;
    authoring.rightPanelMode = SavedRightPanelMode::Scenario;

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::ScenarioResult;
    workspace.authoring = std::move(authoring);
    workspace.runningScenario = fixture.alternativeScenario;
    workspace.runningScenarios = {fixture.baselineScenario, fixture.alternativeScenario};
    workspace.result = SavedScenarioResultState{
        .scenario = fixture.alternativeScenario,
        .frame = std::move(fixture.frame),
        .risk = std::move(fixture.risk),
        .artifacts = std::move(fixture.artifacts),
        .navigationView = SavedResultNavigationView::Bottleneck,
    };
    workspace.batchResult = SavedScenarioBatchResultState{
        .results = {*workspace.result},
        .currentResultIndex = 0,
    };
    return workspace;
}

safecrowd::domain::ImportResult makeBlankImportResult(const QString& projectName) {
    safecrowd::domain::ImportResult result;
    result.layout = safecrowd::domain::FacilityLayout2D{
        .id = projectName.isEmpty() ? std::string{"blank-layout"} : projectName.toStdString(),
        .name = projectName.toStdString(),
        .levelId = "L1",
        .floors = {{
            .id = "L1",
            .label = "Floor 1",
        }},
    };

    safecrowd::domain::ImportValidationService validator;
    result.issues = validator.validate(*result.layout);
    result.reviewStatus = safecrowd::domain::hasBlockingImportIssue(result.issues)
        ? safecrowd::domain::ImportReviewStatus::Pending
        : safecrowd::domain::ImportReviewStatus::NotRequired;
    return result;
}

safecrowd::domain::ImportResult importProjectLayout(const ProjectMetadata& metadata) {
    if (metadata.isBuiltInDemo()) {
        return makeDemoImportResult();
    }
    if (metadata.isBlankLayoutProject()) {
        return makeBlankImportResult(metadata.name);
    }

    safecrowd::domain::DxfImportService importer;
    const safecrowd::domain::ImportRequest importRequest{
        .sourcePath = std::filesystem::path(metadata.layoutPath.toStdWString()),
        .requestedFormat = safecrowd::domain::ImportedFileFormat::Dxf,
        .preserveRawModel = true,
        .runValidation = true,
    };
    return importer.importFile(importRequest);
}

QString zoneLabel(const safecrowd::domain::Zone2D& zone) {
    const auto id = QString::fromStdString(zone.id);
    const auto label = QString::fromStdString(zone.label);
    return label.isEmpty() ? id : QString("%1  -  %2").arg(label, id);
}

const safecrowd::domain::Zone2D* firstStartZone(const safecrowd::domain::FacilityLayout2D& layout) {
    const auto it = std::find_if(layout.zones.begin(), layout.zones.end(), [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Room || zone.kind == safecrowd::domain::ZoneKind::Unknown;
    });
    return it == layout.zones.end() ? nullptr : &(*it);
}

const safecrowd::domain::Zone2D* firstDestinationZone(const safecrowd::domain::FacilityLayout2D& layout) {
    const auto exitIt = std::find_if(layout.zones.begin(), layout.zones.end(), [](const auto& zone) {
        return zone.kind == safecrowd::domain::ZoneKind::Exit;
    });
    if (exitIt != layout.zones.end()) {
        return &(*exitIt);
    }
    return layout.zones.empty() ? nullptr : &layout.zones.back();
}

ScenarioAuthoringWidget::NavigationView navigationViewFromSaved(SavedNavigationView view) {
    switch (view) {
    case SavedNavigationView::Crowd:
        return ScenarioAuthoringWidget::NavigationView::Crowd;
    case SavedNavigationView::Events:
        return ScenarioAuthoringWidget::NavigationView::Events;
    case SavedNavigationView::Layout:
    default:
        return ScenarioAuthoringWidget::NavigationView::Layout;
    }
}

ScenarioAuthoringWidget::RightPanelMode rightPanelModeFromSaved(SavedRightPanelMode mode) {
    switch (mode) {
    case SavedRightPanelMode::None:
        return ScenarioAuthoringWidget::RightPanelMode::None;
    case SavedRightPanelMode::Run:
        return ScenarioAuthoringWidget::RightPanelMode::Run;
    case SavedRightPanelMode::Scenario:
    default:
        return ScenarioAuthoringWidget::RightPanelMode::Scenario;
    }
}

SavedNavigationView savedNavigationViewFromInitial(ScenarioAuthoringWidget::NavigationView view) {
    switch (view) {
    case ScenarioAuthoringWidget::NavigationView::Crowd:
        return SavedNavigationView::Crowd;
    case ScenarioAuthoringWidget::NavigationView::Events:
        return SavedNavigationView::Events;
    case ScenarioAuthoringWidget::NavigationView::Layout:
    default:
        return SavedNavigationView::Layout;
    }
}

SavedRightPanelMode savedRightPanelModeFromInitial(ScenarioAuthoringWidget::RightPanelMode mode) {
    switch (mode) {
    case ScenarioAuthoringWidget::RightPanelMode::None:
        return SavedRightPanelMode::None;
    case ScenarioAuthoringWidget::RightPanelMode::Run:
        return SavedRightPanelMode::Run;
    case ScenarioAuthoringWidget::RightPanelMode::Scenario:
    default:
        return SavedRightPanelMode::Scenario;
    }
}

ScenarioAuthoringWidget::ScenarioState scenarioStateFromSaved(
    const SavedScenarioState& saved,
    const safecrowd::domain::FacilityLayout2D& layout) {
    ScenarioAuthoringWidget::ScenarioState state;
    state.draft = saved.draft;
    state.events = saved.draft.control.events;
    state.baseScenarioId = QString::fromStdString(saved.baseScenarioId);
    state.stagedForRun = saved.stagedForRun;

    if (const auto* startZone = firstStartZone(layout); startZone != nullptr) {
        state.startText = zoneLabel(*startZone);
    }
    if (const auto* destinationZone = firstDestinationZone(layout); destinationZone != nullptr) {
        state.destinationText = zoneLabel(*destinationZone);
    }

    for (const auto& placement : saved.draft.population.initialPlacements) {
        ScenarioCrowdPlacement uiPlacement;
        uiPlacement.id = QString::fromStdString(placement.id);
        uiPlacement.name = uiPlacement.id;
        uiPlacement.kind = (placement.targetAgentCount <= 1 && placement.area.outline.size() <= 1)
            ? ScenarioCrowdPlacementKind::Individual
            : ScenarioCrowdPlacementKind::Group;
        uiPlacement.zoneId = QString::fromStdString(placement.zoneId);
        uiPlacement.floorId = QString::fromStdString(placement.floorId);
        uiPlacement.area = placement.area.outline;
        uiPlacement.occupantCount = static_cast<int>(placement.targetAgentCount);
        uiPlacement.velocity = placement.initialVelocity;
        uiPlacement.distribution = placement.distribution;
        uiPlacement.generatedPositions = placement.explicitPositions;
        state.crowdPlacements.push_back(std::move(uiPlacement));
    }
    for (const auto& source : saved.draft.population.occupantSources) {
        ScenarioCrowdPlacement uiPlacement;
        uiPlacement.id = QString::fromStdString(source.id);
        uiPlacement.name = uiPlacement.id;
        uiPlacement.kind = ScenarioCrowdPlacementKind::Source;
        uiPlacement.zoneId = QString::fromStdString(source.zoneId);
        uiPlacement.floorId = QString::fromStdString(source.floorId);
        uiPlacement.area = {source.position};
        uiPlacement.occupantCount = static_cast<int>(source.targetAgentCount);
        uiPlacement.velocity = source.initialVelocity;
        uiPlacement.sourceAgentsPerSpawn = std::max(1, static_cast<int>(source.agentsPerSpawn));
        uiPlacement.sourceStartSeconds = source.startSeconds;
        uiPlacement.sourceEndSeconds = source.endSeconds;
        uiPlacement.sourceIntervalSeconds = source.spawnIntervalSeconds;
        state.crowdPlacements.push_back(std::move(uiPlacement));
    }

    return state;
}

ScenarioAuthoringWidget::InitialState initialStateFromSaved(
    const SavedScenarioAuthoringState& saved,
    const safecrowd::domain::FacilityLayout2D& layout) {
    ScenarioAuthoringWidget::InitialState initial;
    initial.currentScenarioIndex = saved.currentScenarioIndex;
    initial.navigationView = navigationViewFromSaved(saved.navigationView);
    initial.rightPanelMode = rightPanelModeFromSaved(saved.rightPanelMode);
    initial.scenarios.reserve(saved.scenarios.size());
    for (const auto& scenario : saved.scenarios) {
        initial.scenarios.push_back(scenarioStateFromSaved(scenario, layout));
    }
    if (initial.currentScenarioIndex < 0 || initial.currentScenarioIndex >= static_cast<int>(initial.scenarios.size())) {
        initial.currentScenarioIndex = initial.scenarios.empty() ? -1 : 0;
    }
    return initial;
}

SavedScenarioAuthoringState savedStateFromInitial(const ScenarioAuthoringWidget::InitialState& initial) {
    SavedScenarioAuthoringState saved;
    saved.currentScenarioIndex = initial.currentScenarioIndex;
    saved.navigationView = savedNavigationViewFromInitial(initial.navigationView);
    saved.rightPanelMode = savedRightPanelModeFromInitial(initial.rightPanelMode);
    saved.scenarios.reserve(initial.scenarios.size());
    for (const auto& scenario : initial.scenarios) {
        auto draft = scenario.draft;
        draft.control.events = scenario.events;
        saved.scenarios.push_back({
            .draft = std::move(draft),
            .baseScenarioId = scenario.baseScenarioId.toStdString(),
            .stagedForRun = scenario.stagedForRun,
        });
    }
    return saved;
}

template <typename Widget>
Widget* visibleChild(QWidget* root) {
    if (root == nullptr) {
        return nullptr;
    }

    Widget* match = nullptr;
    if (auto* widget = dynamic_cast<Widget*>(root); widget != nullptr && widget->isVisible()) {
        match = widget;
    }
    const auto children = root->findChildren<QWidget*>();
    for (auto* child : children) {
        if (auto* widget = dynamic_cast<Widget*>(child); widget != nullptr && widget->isVisible()) {
            match = widget;
        }
    }
    return match;
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
            QMessageBox info(QMessageBox::Information,
                             "Delete Project",
                             "The built-in demo project cannot be deleted.",
                             QMessageBox::Ok,
                             this);
            info.setTextFormat(Qt::PlainText);
            info.exec();
            return;
        }

        QMessageBox confirm(QMessageBox::Question,
                            "Delete Project",
                            QString("Move \"%1\" to the recycle bin?\n\n%2\n\n"
                                    "You can restore it from the recycle bin if you change your mind.")
                                .arg(metadata.name, metadata.folderPath),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
        confirm.setTextFormat(Qt::PlainText);
        confirm.setDefaultButton(QMessageBox::No);
        if (confirm.exec() != QMessageBox::Yes) {
            return;
        }

        QString errorMessage;
        if (!ProjectPersistence::deleteProject(metadata, &errorMessage)) {
            QMessageBox warning(QMessageBox::Warning,
                                "Delete Project",
                                errorMessage,
                                QMessageBox::Ok,
                                this);
            warning.setTextFormat(Qt::PlainText);
            warning.exec();
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
    if (request.projectName.isEmpty() || request.folderPath.isEmpty()) {
        QMessageBox::warning(this, "New Project", "Project name and folder are required.");
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

    currentProject_ = metadata;
    hasCurrentProject_ = true;

    auto importResult = importProjectLayout(metadata);
    applySavedReviewState(metadata, &importResult);
    if (!importResult.layout.has_value()) {
        showLayoutReview(metadata, std::move(importResult));
        return;
    }

    ProjectWorkspaceState workspace;
    if (metadata.isBuiltInEvacuationScenarioDemo()) {
        workspace = makeEvacuationScenarioDemoWorkspace();
    } else if (!ProjectPersistence::loadProjectWorkspace(metadata, &workspace)) {
        showLayoutReview(metadata, std::move(importResult));
        return;
    }

    lastApprovedImportResult_ = importResult;
    switch (workspace.activeView) {
    case ProjectWorkspaceView::ScenarioAuthoring:
        if (workspace.authoring.has_value()) {
            showScenarioAuthoring(importResult, initialStateFromSaved(*workspace.authoring, *importResult.layout));
            return;
        }
        break;
    case ProjectWorkspaceView::ScenarioRun:
        if (!workspace.runningScenarios.empty()) {
            showScenarioRun(
                *importResult.layout,
                std::move(workspace.runningScenarios),
                workspace.authoring.has_value()
                    ? std::make_optional(initialStateFromSaved(*workspace.authoring, *importResult.layout))
                    : std::nullopt);
            return;
        }
        if (workspace.runningScenario.has_value()) {
            showScenarioRun(
                *importResult.layout,
                *workspace.runningScenario,
                workspace.authoring.has_value()
                    ? std::make_optional(initialStateFromSaved(*workspace.authoring, *importResult.layout))
                    : std::nullopt);
            return;
        }
        break;
    case ProjectWorkspaceView::ScenarioResult:
        if (workspace.batchResult.has_value() && workspace.batchResult->results.size() > 1) {
            showScenarioBatchResult(
                *importResult.layout,
                workspace.batchResult->results,
                workspace.batchResult->currentResultIndex,
                workspace.authoring.has_value()
                    ? std::make_optional(initialStateFromSaved(*workspace.authoring, *importResult.layout))
                    : std::nullopt);
            return;
        }
        if (workspace.result.has_value()) {
            showScenarioResult(
                *importResult.layout,
                workspace.result->scenario,
                workspace.result->frame,
                workspace.result->risk,
                workspace.result->artifacts,
                workspace.result->navigationView,
                workspace.authoring.has_value()
                    ? std::make_optional(initialStateFromSaved(*workspace.authoring, *importResult.layout))
                    : std::nullopt);
            return;
        }
        break;
    case ProjectWorkspaceView::LayoutReview:
    default:
        break;
    }

    showLayoutReview(metadata, std::move(importResult));
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

    ProjectWorkspaceState workspace;
    workspace.activeView = ProjectWorkspaceView::LayoutReview;

    if (auto* reviewWidget = visibleChild<LayoutReviewWidget>(centralWidget())) {
        if (!ProjectPersistence::saveProjectReview(currentProject_, reviewWidget->currentImportResult(), &errorMessage)) {
            QMessageBox::warning(this, "Save Project", errorMessage);
            return;
        }
    } else if (lastApprovedImportResult_.has_value()) {
        if (!ProjectPersistence::saveProjectReview(currentProject_, *lastApprovedImportResult_, &errorMessage)) {
            QMessageBox::warning(this, "Save Project", errorMessage);
            return;
        }
    }

    auto* authoringWidget = visibleChild<ScenarioAuthoringWidget>(centralWidget());
    auto* batchResultWidget = visibleChild<ScenarioBatchResultWidget>(centralWidget());
    auto* resultWidget = visibleChild<ScenarioResultWidget>(centralWidget());
    auto* runWidget = visibleChild<ScenarioRunWidget>(centralWidget());

    const auto rootWidget = centralWidget();
    const auto widgetDepth = [rootWidget](QWidget* widget) {
        int depth = 0;
        for (auto* current = widget; current != nullptr; current = current->parentWidget()) {
            if (current == rootWidget) {
                return depth;
            }
            ++depth;
        }
        return -1;
    };

    QWidget* activeWorkflowWidget = nullptr;
    int activeWorkflowDepth = -1;
    const auto chooseActiveWorkflowWidget = [&](QWidget* widget) {
        const int depth = widgetDepth(widget);
        if (depth > activeWorkflowDepth) {
            activeWorkflowDepth = depth;
            activeWorkflowWidget = widget;
        }
    };
    chooseActiveWorkflowWidget(authoringWidget);
    chooseActiveWorkflowWidget(batchResultWidget);
    chooseActiveWorkflowWidget(resultWidget);
    chooseActiveWorkflowWidget(runWidget);

    if (activeWorkflowWidget == authoringWidget) {
        workspace.activeView = ProjectWorkspaceView::ScenarioAuthoring;
        workspace.authoring = authoringWidget->currentSavedState();
    } else if (activeWorkflowWidget == batchResultWidget) {
        workspace.activeView = ProjectWorkspaceView::ScenarioResult;
        if (auto authoring = batchResultWidget->returnAuthoringState(); authoring.has_value()) {
            workspace.authoring = savedStateFromInitial(*authoring);
        }
        auto results = batchResultWidget->results();
        if (!results.empty()) {
            const auto index = std::clamp(
                batchResultWidget->currentResultIndex(),
                0,
                static_cast<int>(results.size()) - 1);
            results[static_cast<std::size_t>(index)].navigationView = batchResultWidget->currentSavedNavigationView();
        }
        workspace.batchResult = SavedScenarioBatchResultState{
            .results = std::move(results),
            .currentResultIndex = batchResultWidget->currentResultIndex(),
        };
        if (!workspace.batchResult->results.empty()) {
            const auto index = std::clamp(
                workspace.batchResult->currentResultIndex,
                0,
                static_cast<int>(workspace.batchResult->results.size()) - 1);
            workspace.result = workspace.batchResult->results[static_cast<std::size_t>(index)];
        }
    } else if (activeWorkflowWidget == resultWidget) {
        workspace.activeView = ProjectWorkspaceView::ScenarioResult;
        if (resultWidget->returnAuthoringState().has_value()) {
            workspace.authoring = savedStateFromInitial(*resultWidget->returnAuthoringState());
        }
        workspace.result = SavedScenarioResultState{
            .scenario = resultWidget->scenario(),
            .frame = resultWidget->frame(),
            .risk = resultWidget->risk(),
            .artifacts = resultWidget->artifacts(),
            .navigationView = resultWidget->currentSavedNavigationView(),
        };
    } else if (activeWorkflowWidget == runWidget) {
        if (runWidget->returnAuthoringState().has_value()) {
            workspace.authoring = savedStateFromInitial(*runWidget->returnAuthoringState());
        }
        if (runWidget->hasResultsForSave()) {
            auto results = runWidget->resultsForSave();
            if (!results.empty()) {
                workspace.activeView = ProjectWorkspaceView::ScenarioResult;
                const auto index = std::clamp(
                    runWidget->selectedRunIndex(),
                    0,
                    static_cast<int>(results.size()) - 1);
                workspace.result = results[static_cast<std::size_t>(index)];
                if (results.size() > 1) {
                    workspace.batchResult = SavedScenarioBatchResultState{
                        .results = std::move(results),
                        .currentResultIndex = index,
                    };
                }
            }
        } else {
            workspace.activeView = ProjectWorkspaceView::ScenarioRun;
            workspace.runningScenario = runWidget->scenario();
            workspace.runningScenarios = runWidget->scenarios();
        }
    }

    if (!ProjectPersistence::saveProjectWorkspace(currentProject_, workspace, &errorMessage)) {
        QMessageBox::warning(this, "Save Project", errorMessage);
        return;
    }

    currentProject_ = ProjectPersistence::loadProject(currentProject_.folderPath);
    QMessageBox::information(this, "Save Project", "Project saved.");
}

void MainWindow::showLayoutReview(const ProjectMetadata& metadata) {
    currentProject_ = metadata;
    hasCurrentProject_ = true;
    lastApprovedImportResult_.reset();

    auto importResult = importProjectLayout(metadata);
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

    lastApprovedImportResult_ = importResult;

    setCentralWidget(new ScenarioAuthoringWidget(
        currentProject_.name,
        *importResult.layout,
        [this]() {
            saveCurrentProject();
        },
        [this]() {
            hasCurrentProject_ = false;
            currentProject_ = {};
            showProjectNavigator();
        },
        [this]() {
            if (lastApprovedImportResult_.has_value()) {
                showLayoutReview(currentProject_, *lastApprovedImportResult_);
            } else {
                showLayoutReview(currentProject_);
            }
        },
        this));
}

void MainWindow::showScenarioAuthoring(
    const safecrowd::domain::ImportResult& importResult,
    ScenarioAuthoringWidget::InitialState initialState) {
    if (!importResult.layout.has_value()) {
        QMessageBox::warning(this, "Scenario Authoring", "An approved layout is required before creating a scenario.");
        return;
    }

    lastApprovedImportResult_ = importResult;

    setCentralWidget(new ScenarioAuthoringWidget(
        currentProject_.name,
        *importResult.layout,
        std::move(initialState),
        [this]() {
            saveCurrentProject();
        },
        [this]() {
            hasCurrentProject_ = false;
            currentProject_ = {};
            showProjectNavigator();
        },
        [this]() {
            if (lastApprovedImportResult_.has_value()) {
                showLayoutReview(currentProject_, *lastApprovedImportResult_);
            } else {
                showLayoutReview(currentProject_);
            }
        },
        this));
}

void MainWindow::showScenarioRun(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::ScenarioDraft& scenario,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState) {
    showScenarioRun(
        layout,
        std::vector<safecrowd::domain::ScenarioDraft>{scenario},
        std::move(returnAuthoringState));
}

void MainWindow::showScenarioRun(
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<safecrowd::domain::ScenarioDraft> scenarios,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState) {
    setCentralWidget(new ScenarioRunWidget(
        currentProject_.name,
        layout,
        std::move(scenarios),
        [this]() {
            saveCurrentProject();
        },
        [this]() {
            hasCurrentProject_ = false;
            currentProject_ = {};
            showProjectNavigator();
        },
        [this]() {
            if (lastApprovedImportResult_.has_value()) {
                showLayoutReview(currentProject_, *lastApprovedImportResult_);
            } else {
                showLayoutReview(currentProject_);
            }
        },
        std::move(returnAuthoringState),
        this));
}

void MainWindow::showScenarioBatchResult(
    const safecrowd::domain::FacilityLayout2D& layout,
    std::vector<SavedScenarioResultState> results,
    int currentResultIndex,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState) {
    setCentralWidget(new ScenarioBatchResultWidget(
        currentProject_.name,
        layout,
        std::move(results),
        [this]() {
            saveCurrentProject();
        },
        [this]() {
            hasCurrentProject_ = false;
            currentProject_ = {};
            showProjectNavigator();
        },
        [this]() {
            if (lastApprovedImportResult_.has_value()) {
                showLayoutReview(currentProject_, *lastApprovedImportResult_);
            } else {
                showLayoutReview(currentProject_);
            }
        },
        std::move(returnAuthoringState),
        currentResultIndex,
        this));
}

void MainWindow::showScenarioResult(
    const safecrowd::domain::FacilityLayout2D& layout,
    const safecrowd::domain::ScenarioDraft& scenario,
    const safecrowd::domain::SimulationFrame& frame,
    const safecrowd::domain::ScenarioRiskSnapshot& risk,
    const safecrowd::domain::ScenarioResultArtifacts& artifacts,
    SavedResultNavigationView savedNavigationView,
    std::optional<ScenarioAuthoringWidget::InitialState> returnAuthoringState) {
    setCentralWidget(new ScenarioResultWidget(
        currentProject_.name,
        layout,
        scenario,
        frame,
        risk,
        artifacts,
        [this]() {
            saveCurrentProject();
        },
        [this]() {
            hasCurrentProject_ = false;
            currentProject_ = {};
            showProjectNavigator();
        },
        [this]() {
            if (lastApprovedImportResult_.has_value()) {
                showLayoutReview(currentProject_, *lastApprovedImportResult_);
            } else {
                showLayoutReview(currentProject_);
            }
        },
        savedNavigationView,
        std::move(returnAuthoringState),
        this));
}

}  // namespace safecrowd::application
