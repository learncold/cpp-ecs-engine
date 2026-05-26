# SafeCrowd Contribution Rules

이 저장소는 `application -> domain -> engine` 계층을 유지하는 것을 가장 중요한 규칙으로 둡니다.
PR은 작은 단위로 나누고, 변경 의도와 검증 결과를 분명하게 남겨 주세요.

## 기본 원칙

- 기본 작업 흐름은 `branch -> PR -> merge`입니다.
- GitHub issue 확인, 생성, 연결은 개발 시작 조건이 아닙니다.
- `docs/`, `uml/`, `AGENTS.md`, `CONTRIBUTING.md`, PR template, PR 정책 워크플로처럼 문서 또는 기여 정책만 다루는 변경은 유지보수자가 `main`에 직접 commit/push할 수 있습니다.
- `src/application/`에 한정된 UI/앱 조립 변경은 유지보수자가 PR 또는 직접 push로 반영할 수 있습니다. 새 application 파일을 앱 타깃에 연결하기 위한 `CMakeLists.txt` 변경도 이 범위에 포함할 수 있습니다.
- 문서/정책 변경과 application-only 범위를 벗어난 코드/빌드 변경이 섞이면 일반 PR 흐름으로 진행합니다.
- 서로 다른 계층을 동시에 크게 건드리는 PR은 피합니다.
- 구조, 빌드, 의존성 규칙이 바뀌면 관련 문서를 함께 업데이트합니다.

## PR 제목 규칙

PR 제목은 아래 형식을 따릅니다.

```text
[Area] short summary
```

허용되는 `Area` 값:

- `Engine`
- `Domain`
- `Application`
- `Docs`
- `Build`
- `Analysis`
- `Chore`

예시:

```text
[Engine] implement generation-safe entity registry
[Docs] clarify build presets and architecture notes
```

## PR 본문 규칙

모든 PR은 아래 내용을 포함해야 합니다.

- 변경 요약
- 변경이 속한 영역
- 아키텍처 규칙 점검 결과
- 빌드/테스트 검증 결과 또는 미실행 사유
- 남은 리스크나 후속 작업

## 아키텍처 체크

PR 작성 시 아래 항목을 항상 점검합니다.

- `engine`에 `domain` 또는 `application` 의존성을 추가하지 않았는가
- `domain`에 Qt UI 코드를 추가하지 않았는가
- `application`이 UI와 도메인 조립 책임을 벗어나지 않았는가
- include 경로가 `src/` 루트 기준(`application/...`, `domain/...`, `engine/...`)을 따르는가

## 검증 규칙

가능하면 아래 명령으로 검증합니다.

```powershell
cmake --preset windows-debug
cmake --build --preset build-debug
ctest --preset test-debug
```

Application layer를 제외한 빠른 검증이 필요하면 아래 프리셋을 사용합니다.

```powershell
cmake --preset windows-debug-no-app
cmake --build --preset build-engine-debug
cmake --build --preset build-engine-domain-debug
cmake --build --preset build-no-app-debug
ctest --preset test-no-app-debug
```

- `windows-debug` / `windows-release` 첫 configure는 `vcpkg`가 `qtbase`를 설치하는 동안 몇 분 이상 걸릴 수 있습니다.
- 현재 manifest는 `qtbase` 기본 feature를 끄고 `widgets`만 요청하므로, 불필요한 SQL/PostgreSQL 플러그인까지 끌어오지 않도록 유지합니다.

로컬 import stack 검증이 필요하면 기본 경로를 유지한 채 아래 옵션을 추가합니다.

```powershell
cmake --preset windows-debug-no-app `
  -DSAFECROWD_ENABLE_IMPORT_STACK=ON `
  -DSAFECROWD_IMPORT_STACK_ROOT=C:/sdk/import-stack `
  -DSAFECROWD_IFCOPENSHELL_ROOT=C:/sdk/IfcOpenShell
cmake --build --preset build-engine-domain-debug
```

- `SAFECROWD_ENABLE_IMPORT_STACK`는 기본값이 `OFF`이며, CI와 Sprint 1 기본 경로는 그대로 유지합니다.
- `SAFECROWD_IMPORT_STACK_ROOT`는 공통 prefix 용도이고, `SAFECROWD_LIBDXFRW_ROOT`, `SAFECROWD_IFCOPENSHELL_ROOT`, `SAFECROWD_CLIPPER2_ROOT`, `SAFECROWD_BOOST_ROOT`, `SAFECROWD_RECAST_ROOT`로 개별 경로를 덮어쓸 수 있습니다.
- 현재 smoke check는 `libdxfrw`, `IfcOpenShell` parser 헤더, `Clipper2`, `Boost.Geometry`, `Recast`, `Detour`의 include/lib 경로를 configure 단계에서 확인합니다.

실행하지 못했다면 PR 본문에 이유를 남깁니다.

## 머지 규칙

- 코드/빌드 변경은 `main`에 직접 push하지 않고 PR로 반영합니다. 단, application-only 변경은 유지보수자가 PR 또는 직접 push로 반영할 수 있습니다.
- `docs/`, `uml/`, `AGENTS.md`, `CONTRIBUTING.md`, PR template, PR 정책 워크플로처럼 문서/정책만 다루는 변경은 유지보수자가 `main`에 직접 push할 수 있습니다.
- 머지는 squash merge를 기본으로 사용합니다.
- PR 체크가 실패한 상태에서는 머지하지 않습니다.
