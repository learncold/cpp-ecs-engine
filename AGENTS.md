# AGENTS.md

## Project Overview
- SafeCrowd is an ECS-based crowd simulation and decision-support project with a Qt desktop application.
- Keep the architecture layered: `application -> domain -> engine`.
- Product/architecture documents currently lead the implementation, so verify tracked source files before assuming a module already exists in `src/`.

## Current Repo State
- Declared CMake targets:
  - `ecs_engine`
  - `safecrowd_domain`
  - `safecrowd_app`
- The repository currently includes build configuration, docs, UML diagrams, GitHub workflow/templates, and vendored third-party code under `external/`.
- Source roots are still expected under:
  - `src/application`
  - `src/domain`
  - `src/engine`
- When touching build files, confirm that referenced source files are actually tracked in Git.

## Build
- Configure: `cmake --preset windows-debug`
- Build: `cmake --build --preset build-debug`
- Test: `ctest --preset test-debug`
- Configure without app: `cmake --preset windows-debug-no-app`
- Build engine only: `cmake --build --preset build-engine-debug`
- Build engine + domain: `cmake --build --preset build-engine-domain-debug`
- Build engine + domain + tests without app: `cmake --build --preset build-no-app-debug`
- Test without app: `ctest --preset test-no-app-debug`
- App target: `safecrowd_app`
- UI dependency: Qt6 via `vcpkg.json` (`qtbase`)
- If configure/build fails, check preset/Visual Studio selection and `vcpkg`/Qt availability before assuming the code change caused it.
- PR CI currently validates the engine/domain/test path with `-DSAFECROWD_BUILD_APP=OFF` for fast feedback; keep the full Qt app build healthy locally.

## Source Layout
- All C++ source files live under `src/`.
- Keep each layer's headers and source files directly under `src/<layer>/` unless a clear need for subfolders appears.
- Use `src/` as the include root.
- Preferred includes:
  - `#include "application/..."`
  - `#include "domain/..."`
  - `#include "engine/..."`
- Supporting repository areas:
  - `docs/` for requirements, architecture, and project-management notes
  - `uml/` for historical PlantUML diagrams and explanations; these files are stale and must not be treated as implementation or architecture authority
  - `.github/` for repository workflow/policy files
  - `external/` for vendored dependencies that must remain in-tree

## Source Tree and File Roles
The current `src/` tree is layered as follows. Keep new files in the matching layer and update this section when adding, removing, or renaming source files. This is required even for implementation-only changes that introduce new source/header files.

```text
src/
  application/        Qt desktop app, widgets, view models, persistence adapters, and app-only authoring helpers
  domain/             SafeCrowd domain models, import pipeline, scenario authoring, simulation, and metrics logic
  engine/             Generic ECS runtime primitives, scheduling, storage, entities, and resources
    internal/         Engine-only test/factory accessors that should not be consumed by domain or application code
```

Application layer file roles:
- `IssueCardWidget.h/.cpp`: small Qt card used to show import/layout validation issues.
- `LayoutCanvasRendering.h/.cpp`: shared canvas camera, bounds, transforms, and layout rendering helpers.
- `LayoutCanvasSnapping.h/.cpp`: app-layer snapping helpers for layout canvas editing.
- `LayoutNavigationPanelWidget.h/.cpp`: floor/layout navigation panel for review and editing workflows.
- `LayoutPreviewEditing.h/.cpp`: mutation commands for layout preview editing, including create/delete/floor operations.
- `LayoutPreviewGeometry.h/.cpp`: reusable geometry, floor/id, polygon, barrier, door, stair, and zone-neighbor helpers for layout preview editing.
- `LayoutPreviewWidget.h/.cpp`: Qt canvas widget for layout preview interaction, selection, painting, camera, toolbar, and edit command orchestration.
- `LayoutReviewCodec.h/.cpp`: JSON codec for layout review state, facility layouts, import artifacts, trace refs, and import artifact version handling.
- `LayoutReviewWidget.h/.cpp`: layout review screen that combines preview, navigation, and issue context.
- `main.cpp`: Qt application entry point.
- `MainWindow.h/.cpp`: top-level application window and primary screen wiring.
- `NavigationTreeWidget.h/.cpp`: project/navigation tree UI component.
- `NewProjectRequest.h`: data object for new-project creation input.
- `NewProjectWidget.h/.cpp`: new-project creation UI.
- `ProjectListWidget.h/.cpp`: project list and project selection UI.
- `ProjectMetadata.h`: lightweight project metadata model used by the app shell.
- `ProjectMetadataCodec.h/.cpp`: JSON codec for project metadata and recent-project index entries.
- `ProjectNavigatorActions.h/.cpp`: app navigation action definitions and helpers.
- `ProjectNavigatorWidget.h/.cpp`: project navigation sidebar/widget implementation.
- `ProjectPersistence.h/.cpp`: application persistence adapter for project file I/O, folder safety checks, and codec orchestration.
- `ProjectPersistenceJson.h/.cpp`: low-level JSON helpers shared by persistence codecs.
- `ProjectWorkspaceState.h`: application-level workspace state snapshot.
- `ResultArtifactsCodec.h/.cpp`: JSON codec for simulation frames, risk snapshots, density metrics, and scenario result artifacts.
- `ScenarioAuthoringWidget.h/.cpp`: scenario authoring screen that wires scenario canvas and controls.
- `ScenarioBatchResultWidget.h/.cpp`: UI for displaying batch scenario run results.
- `ScenarioCanvasAuthoringRules.h/.cpp`: app-internal scenario canvas authoring validation and draft creation/move rules.
- `ScenarioCanvasWidget.h/.cpp`: interactive Qt canvas for scenario placements, hazards, blocks, guidance, selection, drag, and painting.
- `ScenarioResultNavigation.h/.cpp`: helpers/widgets for navigating scenario result views.
- `ScenarioResultWidget.h/.cpp`: UI for scenario result summaries and details.
- `ScenarioRunWidget.h/.cpp`: UI for configuring and launching scenario runs.
- `SimulationCanvasWidget.h/.cpp`: Qt canvas for simulation playback and visualization.
- `ToolIconResources.h/.cpp`: helper functions for loading and recoloring tool icons.
- `ToolIcons.qrc`: Qt resource collection for tool icon assets.
- `UiStyle.h/.cpp`: shared app UI tokens and widget styling helpers.
- `WorkspaceStateCodec.h/.cpp`: JSON codec for scenario drafts, saved authoring/result state, workspace state, and workspace version handling.
- `WorkspaceShell.h/.cpp`: main workspace shell composition and navigation host.

Domain layer file roles:
- `AgentComponents.h`: ECS component types for agents and simulation state.
- `AlternativeRecommendationService.h/.cpp`: domain service for ranking or recommending scenario alternatives.
- `CanonicalGeometry.h`: canonical geometry data structures used by import/normalization.
- `CompressionSystem.h/.cpp`: domain simulation system for compression/crowding effects.
- `DemoFixtureService.h/.cpp`: service for creating demo project fixtures.
- `DemoLayouts.h/.cpp`: built-in demo layout definitions.
- `DxfImportService.h/.cpp`: DXF import entry service.
- `FacilityLayout2D.h`: core 2D facility layout model with floors, zones, connections, barriers, and controls.
- `FacilityLayoutBuilder.h/.cpp`: builder utilities that produce `FacilityLayout2D` from normalized/imported geometry.
- `Geometry2D.h`: shared 2D geometry primitives.
- `GeometryNormalizer.h/.cpp`: geometry cleanup and normalization service.
- `GeometryQueries.h/.cpp`: reusable geometry query functions such as point-in-polygon, distances, and walkable clearance.
- `ImportContracts.h`: contracts and DTOs shared across import stages.
- `ImportIssue.h/.cpp`: import issue model and formatting/classification helpers.
- `ImportOrchestrator.h`: high-level import pipeline orchestration interface.
- `ImportResult.h`: import result DTOs.
- `ImportSemanticRules.h/.cpp`: semantic validation and conversion rules for imported geometry.
- `ImportValidationService.h/.cpp`: validation service for import results and layout issues.
- `Metrics.h`: common metric data structures.
- `PopulationSpec.h`: population and initial placement domain models.
- `PressureTuning.h`: pressure/crowding tuning constants or profiles.
- `ProjectRepository.h`: domain-facing project repository abstraction.
- `RawImportModel.h`: raw imported geometry/data model before normalization.
- `SafeCrowdDomain.h/.cpp`: domain facade and cross-service wiring helpers.
- `ScenarioAuthoring.h/.cpp`: scenario draft, control, hazard, guidance, and authoring helper models/functions.
- `ScenarioBatchRunner.h/.cpp`: batch scenario execution service.
- `ScenarioResultArtifacts.h`: scenario result artifact models.
- `ScenarioRiskMetrics.h/.cpp`: risk metric data structures and calculations.
- `ScenarioRiskMetricsSystem.cpp`: ECS-backed risk metric accumulation system implementation.
- `ScenarioSimulationFrame.h`: simulation frame snapshot models.
- `ScenarioSimulationInternal.h/.cpp`: internal scenario simulation helpers shared by simulation systems/runners.
- `ScenarioSimulationMotionSystem.cpp`: agent movement simulation system orchestration, movement integration, constraints, overlap resolution, and clock advancement.
- `ScenarioSimulationRouteGuidance.h/.cpp`: internal route guidance controller, route planning/cache helpers, exit replanning, and hazard-aware route selection support for scenario simulation.
- `ScenarioSimulationRunner.h/.cpp`: scenario simulation runner service.
- `ScenarioSimulationSystems.h/.cpp`: simulation system registration and shared system helpers.
- `ScenarioTemplateCatalog.h`: predefined scenario template catalog models/helpers.

Engine layer file roles:
- `CommandBuffer.h`: deferred ECS command buffer for entity/component mutations.
- `ComponentRegistry.h`: component type registration and metadata.
- `DeterministicRng.h`: deterministic random number utilities.
- `EcsCore.h`: core ECS type aliases and constants.
- `EngineConfig.h`: engine runtime configuration model.
- `EngineRuntime.h/.cpp`: top-level ECS runtime orchestration.
- `EngineState.h`: runtime state container.
- `EngineStats.h`: runtime statistics model.
- `EngineStepContext.h`: per-step context passed to systems.
- `EngineSystem.h`: system interface/base contract.
- `EngineWorld.h`: ECS world container API.
- `Entity.h`: entity identifier model.
- `EntityRegistry.h/.cpp`: entity lifecycle registry.
- `FrameClock.h/.cpp`: deterministic frame timing helper.
- `IComponentStorage.h`: abstract component storage interface.
- `PackedComponentStorage.h`: packed component storage implementation.
- `ResourceStore.h`: typed runtime resource container.
- `SystemDescriptor.h`: system metadata descriptor.
- `SystemScheduler.h/.cpp`: system ordering and update scheduling.
- `TriggerPolicy.h`: system trigger policy model.
- `UpdatePhase.h`: update phase enumeration.
- `WorldQuery.h`: query helpers for ECS world/component access.
- `internal/EngineRuntimeTestAccess.h`: test-only accessors for engine runtime internals.
- `internal/EngineWorldFactory.h`: internal factory helpers for constructing engine worlds.

## Architecture Rules
- `engine` must not depend on `domain` or `application`.
- `domain` must not depend on Qt UI code.
- `application` is responsible for wiring UI to domain logic.
- When implementing application-layer behavior, prefer existing `domain` APIs and models over duplicating business logic in Qt widgets.
- Application code should drive simulation/scenario behavior through domain services/runners that use the engine ECS runtime; do not bypass the ECS structure with parallel UI-owned state machines for simulation logic.
- If a needed behavior is missing below the UI, add it in `domain` and, when appropriate, back it with engine ECS systems/resources/components instead of implementing it only in `application`.
- If a change affects multiple layers, review dependency direction first and keep responsibilities explicit.

## Dependency Policy
- Prefer dependencies declared in `vcpkg.json`.
- Use `external/` only for vendored libraries that must live in-tree.
- `external/glad/` is currently tracked as vendored third-party code.
- Do not leave unused third-party code in `external/`.

## GitHub Workflow
- Use GitHub issue forms for new work items; blank issues are disabled.
- Before starting work, check whether a related GitHub issue already exists.
- If no related issue exists and the current policy does not exempt the work from issue creation, open a new issue in Korean first and then start the implementation.
- When a new task clearly belongs under an existing Epic, add it under that Epic as a native GitHub `sub-issue`.
- After linking the task under its Epic, make sure the relationship is visible from the Project view via `Parent issue` / `Sub-issues progress`.
- If the work falls under the existing docs/policy-only exception, it may proceed without opening a separate issue.
- Changes limited to `src/application/` may proceed without opening a separate issue. A `CMakeLists.txt` update needed only to wire application sources into `safecrowd_app` may be included in this exception.
- Issue types currently supported:
  - `Epic` for larger parent work
  - `Implementation Task` for `Engine` / `Domain` / `Application` / `Build` work
  - `Lightweight Task` for `Docs` / `Chore` / `Analysis` work
- GitHub Project guidance is documented in `docs/process/GitHub Project.md`.
- PR titles must follow `[Area] short summary`.
- Allowed PR areas:
  - `Engine`
  - `Domain`
  - `Application`
  - `Docs`
  - `Build`
  - `Analysis`
  - `Chore`
- PR bodies should follow `.github/PULL_REQUEST_TEMPLATE.md`.
- `main` handling:
  - code/build changes follow the normal `branch -> PR -> merge` flow
  - docs/policy-only changes limited to `docs/`, `uml/`, `AGENTS.md`, `CONTRIBUTING.md`, PR/issue templates, or PR policy workflow files may be pushed directly to `main` by maintainers
  - application-only changes limited to `src/application/` and app target wiring in `CMakeLists.txt` may be pushed directly to `main` by maintainers without a separate issue
  - squash merge remains the intended merge mode for PR-based changes
  - PR checks should stay aligned with `.github/workflows/ci.yml`

## Editing Guidelines
- Keep changes minimal and localized.
- Preserve existing naming/style unless there is a clear reason to refactor.
- Update docs when structure, build rules, or repository workflow changes.
- When adding, removing, or renaming files under `src/`, update `CMakeLists.txt` wiring when needed and update the `Source Tree and File Roles` section in this file before finishing.
- When changing contribution workflow files, keep `CONTRIBUTING.md` and `.github/` files aligned.

## UI Design Guidelines
- Prefer a calm, minimal desktop UI: light neutral surfaces, restrained accent color, and clear visual hierarchy over heavy borders or high-contrast chrome.
- Centralize reusable Qt widget styling in shared application-layer helpers instead of scattering large inline `setStyleSheet()` blocks across many widgets.
- Reuse a small set of design tokens consistently:
  - spacing around 8 / 12 / 16 / 24 / 32 px
  - rounded corners around 12-20 px for cards/panels
  - one primary accent color with muted supporting grays
- Use typography roles consistently:
  - large page title
  - section title
  - body text
  - caption/meta text
- Primary actions should be visually emphasized; secondary actions should stay quieter but still consistent in size, radius, and padding.
- Prefer card/panel composition with generous padding instead of drawing many separator lines.
- Keep dense review/workspace screens readable by separating navigation, canvas, and inspector panels with spacing and surface contrast rather than thick borders.
- Avoid introducing custom UI styling in `domain` or `engine`; all presentation decisions remain in `src/application/`.
- When adding a new screen or widget, align it with the existing shared UI tokens before introducing a new color, radius, or button treatment.

## Docs
- After any changes to the project, ensure the documents remain consistent.
- Architecture notes: `docs/architecture/프로젝트 구조.md`
- Project workflow notes: `docs/process/GitHub Project.md`
- Requirements and overview docs are under `docs/product/`.
- Use `docs/README.md` as the entry point for the document map.
- UML files and their explanatory notes under `uml/` are outdated historical references. Do not use them as the source of truth; verify current tracked source files and current docs instead.

## Review Priorities
- Broken build or preset mismatch
- Unit test regression or missing CTest wiring
- Missing tracked source files referenced by `CMakeLists.txt`
- Layer dependency violations
- Qt code leaking into `domain`
- Unused or confusing dependency setup
- Drift between `CONTRIBUTING.md` and `.github/` workflow/template files
