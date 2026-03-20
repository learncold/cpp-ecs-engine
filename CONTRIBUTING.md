# SafeCrowd PR Rules

이 저장소는 `application -> domain -> engine` 계층을 유지하는 것을 가장 중요한 규칙으로 둡니다.
PR은 작은 단위로 나누고, 어떤 issue를 해결하는지 분명하게 남겨 주세요.

## 기본 원칙

- 기본 작업 흐름은 `issue -> branch -> PR -> merge`입니다.
- 가능하면 하나의 PR은 하나의 issue만 해결합니다.
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
- 연결된 issue
- 변경이 속한 영역
- 아키텍처 규칙 점검 결과
- 빌드/검증 결과 또는 미실행 사유
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
```

실행하지 못했다면 PR 본문에 이유를 남깁니다.

## 머지 규칙

- `main`에는 직접 push하지 않고 PR로 반영합니다.
- 머지는 squash merge를 기본으로 사용합니다.
- PR 체크가 실패한 상태에서는 머지하지 않습니다.
