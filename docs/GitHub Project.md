# SafeCrowd GitHub Project

## 개요
- Project: `SafeCrowd Implementation`
- URL: `https://github.com/users/learncold/projects/1`
- Repository: `learncold/cpp-ecs-engine`
- 관리 방식:
  - Epic과 Task를 모두 issue로 관리한다.
  - Epic-Task 관계는 GitHub native `Parent issue` / `sub-issue`를 사용한다.
  - 선행 작업 관계는 GitHub native issue dependency `blocked by`를 사용한다.
  - Task 제목은 `Task- short title` 형식을 사용하고, 순서는 제목 번호가 아니라 GitHub issue 번호, Sprint, Parent issue, dependency로 관리한다.

## 현재 필드
- `Title`
- `Status`
  - `In Progress`
  - `Done`
- `Area`
  - `⚙️Engine`
  - `🏃Domain`
  - `🖥️Application`
  - `🔎Analysis`
  - `📄Docs`
- `Sprint`
  - `Sprint 1`
  - `Sprint 2`
  - `Sprint 3`
  - `Later`
- `Parent issue`
- `Sub-issues progress`

## 현재 뷰
- 뷰 수: 1개
- 이름: `View 1`
- 형식: `Table`
- 필터: 없음

권장 표시 컬럼:
- `Title`
- `Status`
- `Sprint`
- `Area`
- `Parent issue`
- `Sub-issues progress`

## 현재 구조
- `Sprint 1`
  - Epic: `#1 EPIC- Engine Foundation`
  - Epic: `#2 EPIC- Sprint 1 Demo Vertical Slice`
  - Task: 각 epic 아래 `Task- ...` sub-issue로 관리
- `Sprint 2`
  - Epic: `#3 EPIC- Product Completion for Sprint 2`
  - Epic: `#4 EPIC- Compare and Presentation Readiness`
  - Task: 각 epic 아래 `Task- ...` sub-issue로 관리
- `Sprint 3`
  - Epic: `#5 EPIC- Finish and Optional Extensions`
  - Task: 각 epic 아래 `Task- ...` sub-issue로 관리

## 메모
- `Docs`, `Chore`, `Analysis`는 `Lightweight Task` form으로 가볍게 등록한다.
- `Engine`, `Domain`, `Application`, `Build`는 `Implementation Task` form으로 범위와 검증 계획까지 남긴다.
- 세부 작업명, 부모-자식 관계, dependency는 GitHub Project와 issue 자체를 기준으로 관리한다.
- 문서 또는 기여 정책만 다루는 PR은 별도 issue 없이 진행할 수 있다.
- Task의 순서는 제목 접두사 뒤 숫자로 관리하지 않는다. 중간 작업이 생기면 새 issue를 추가하고 `Sprint`, `Parent issue`, `blocked by`로 위치를 표현한다.
