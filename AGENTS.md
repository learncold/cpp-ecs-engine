# AGENTS.md

## Project Overview
- SafeCrowd is a Qt-based crowd simulation project.
- Keep the architecture layered: `application -> domain -> engine`.
- The repository is intentionally small and keeps a single top-level `CMakeLists.txt`.

## Build
- Configure: `cmake --preset windows-debug`
- Build: `cmake --build --preset build-debug`
- App target: `safecrowd_app`
- Visual Studio preset output directory: `build/vs2022/windows-debug`

## Source Layout
- All C++ source files live under `src/`.
- Use `src/` as the include root.
- Preferred include style: `#include "application/..."`, `#include "domain/..."`, `#include "engine/..."`

## Architecture Rules
- `engine` must not depend on `domain` or `application`.
- `domain` must not depend on Qt UI code.
- `application` is responsible for wiring UI to domain logic.
- If a viewport needs direct rendering integration, `application -> engine` is allowed.

## Dependency Policy
- Prefer dependencies declared in `vcpkg.json`.
- Use `external/` only for vendored libraries that must live in-tree.
- Do not leave unused third-party code in `external/`.

## Editing Guidelines
- Keep changes minimal and localized.
- Preserve existing naming/style unless there is a clear reason to refactor.
- Update docs when structure or build rules change.

## Docs
- Architecture notes: `docs/프로젝트 구조.md`
- Product overview: `docs/개요서.md`
- Requirements and overview docs are under `docs/`.

## Review Priorities
- Broken build or preset mismatch
- Layer dependency violations
- Qt code leaking into `domain`
- Unused or confusing dependency setup
- Structural changes that are not reflected in docs or CMake
