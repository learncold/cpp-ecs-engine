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
  - `uml/` for PlantUML diagrams and explanations
  - `.github/` for repository workflow/policy files
  - `external/` for vendored dependencies that must remain in-tree

## Architecture Rules
- `engine` must not depend on `domain` or `application`.
- `domain` must not depend on Qt UI code.
- `application` is responsible for wiring UI to domain logic.
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

## Review Priorities
- Broken build or preset mismatch
- Unit test regression or missing CTest wiring
- Missing tracked source files referenced by `CMakeLists.txt`
- Layer dependency violations
- Qt code leaking into `domain`
- Unused or confusing dependency setup
- Drift between `CONTRIBUTING.md` and `.github/` workflow/template files
