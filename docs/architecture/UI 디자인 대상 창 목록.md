# SafeCrowd UI 디자인 대상 창 목록

이 문서는 현재 앱 구현 방향에 맞춰 UI 화면과 재사용 구조를 정리한다.

현재 기준은 다음 흐름이다.

1. 앱 실행
2. Project Navigator 진입
3. New Project 또는 저장된 프로젝트 선택
4. DXF import
5. Layout Review Workspace 진입
6. 이후 scenario authoring, run, analysis 화면을 같은 workspace shell 안에서 확장

`domain`의 import, validation, simulation 객체는 UI 디자인 대상이 아니다. UI는 `src/application/`에서 Qt Widgets로 구현하고, `domain` 결과를 화면에 표시한다.

## 1. 앱 시작 화면

### 1.1 Project Navigator

앱을 처음 켰을 때 진입하는 화면이다. 예시 프로젝트 목록을 두지 않고, 실제 저장된 프로젝트 목록을 보여준다.

현재 구현 책임:

- 최근 저장 프로젝트 목록 표시
- 새 프로젝트 생성 진입
- 저장된 프로젝트 클릭 시 Layout Review Workspace로 진입
- 프로젝트 목록이 없을 때 빈 상태 표시

필요한 UI 요소:

- `SafeCrowd` 제목
- 저장된 프로젝트 리스트
- 프로젝트명
- 마지막 저장 시각
- `+ New Project` 버튼
- `Open Folder` 버튼

후속 설계 대상:

- 열 수 없는 프로젝트 표시
- 프로젝트 삭제 또는 목록에서 제거
- 프로젝트 검색/정렬
- 프로젝트 상태 표시: review 필요, blocker 있음, ready 등

### 1.2 New Project 화면

새 프로젝트를 만들기 위한 입력 화면이다. 이 화면에서 시뮬레이션 설정까지 받지 않고, 프로젝트 생성과 layout import에 필요한 최소 정보만 받는다.

현재 구현 책임:

- 프로젝트명 입력
- Layout DXF 파일 선택
- 프로젝트 저장 폴더 선택
- `Done` 클릭 시 DXF import 실행
- import 결과로 Layout Review Workspace 진입
- `Cancel` 클릭 시 Project Navigator 복귀

필요한 UI 요소:

- `New Project` 제목
- Project Name 입력 필드
- Layout 파일 선택 버튼
- 선택된 DXF 경로 표시
- Folder 선택 버튼
- 선택된 저장 폴더 경로 표시
- `Cancel`
- `Done`

후속 설계 대상:

- 입력값 validation 메시지
- DXF가 아닌 파일 선택 방지 강화
- 같은 폴더에 기존 프로젝트가 있을 때 덮어쓰기 확인
- import 진행 상태
- import 실패 상세 메시지

## 2. 재사용 Main Workspace 구조

### 2.1 Workspace Shell

프로젝트가 열린 뒤 사용하는 공통 작업 화면 구조다. Layout Review뿐 아니라 향후 scenario authoring, run, analysis도 이 구조를 재사용한다.

현재 구현 책임:

- 상단 프로젝트 메뉴
- 좌측 상태/이슈 패널
- 중앙 canvas
- 우측 review/inspector 패널
- 하단 상태/요약 패널

기본 레이아웃:

- Top Bar
- Left Panel
- Center Canvas
- Right Panel
- Bottom Panel

상단바 현재 동작:

- `Project` 버튼
- `Project > Save Project` 드롭다운 액션
- `Tool` 버튼 자리

저장 동작:

- 프로젝트 폴더에 `safecrowd-project.json` 저장
- 프로젝트 폴더에 `layout.dxf` 복사
- 앱 데이터 경로에 최근 프로젝트 인덱스 저장
- 다음 앱 실행 시 Project Navigator 목록에 표시

후속 설계 대상:

- 저장 상태 표시: saved, unsaved, failed
- 최근 저장 시각 표시
- Project 메뉴 항목 확장: Save As, Close Project, Back to Navigator
- Tool 메뉴의 실제 도구 구성

## 3. Layout Review Workspace

### 3.1 Layout Review 화면

DXF import 직후 진입하는 첫 workspace 화면이다. import 모듈이 파악한 layout geometry와 blocking issue를 검토한다.

현재 구현 책임:

- DXF import 결과 표시
- 중앙 canvas에 import 결과 기반 2D preview 렌더링
- 좌측 패널에 blocking issue 목록 표시
- 우측 패널에 issue count 표시
- 하단 패널에 현재 review/project context 표시

필요한 UI 요소:

- Workspace Shell 상단바
- 좌측 `Blocking` 목록
- 중앙 Layout Preview
- 우측 issue summary
- 하단 review 상태 영역

현재 중앙 preview 표시 대상:

- walkable area / zone polygon
- obstacle polygon
- wall segment
- opening segment
- layout connection
- barrier polyline

현재 좌측 Blocking 목록 표시 대상:

- issue code
- severity
- message
- source id
- target id

후속 설계 대상:

- blocking item 클릭 시 canvas 위치 이동
- 해당 geometry highlight
- warning/info issue 탭
- issue 해결/무시/승인 상태
- Layout approval action
- pan/zoom 가능한 2D CAD형 viewport
- 선택 요소 inspector

### 3.2 Layout Canvas

Layout Review의 중앙 작업 영역이다. 현재는 import 결과를 QPainter로 렌더링하는 미리보기다.

후속 설계 대상:

- 마우스 드래그 pan
- wheel zoom
- fit to view
- layer visibility
- element selection
- geometry hover highlight
- import issue 위치 표시
- DXF layer별 색상/스타일 반영

### 3.3 Blocking / Issue Panel

Layout Review의 좌측 패널이다. import와 validation 결과 중 simulation을 막는 항목을 우선 표시한다.

후속 설계 대상:

- blocker, warning, info 필터
- issue group by source layer/entity
- issue count badge
- issue 상세 drawer
- repair suggestion

## 4. 이후 확장 대상 화면

현재 구현은 Project Navigator, New Project, Layout Review까지를 우선 대상으로 한다. 다음 화면들은 Workspace Shell 안에서 단계적으로 추가한다.

### 4.1 Scenario Authoring

layout review 이후 scenario를 작성하는 화면이다.

필요한 UI 요소:

- Scenario Library
- Scenario Template Picker
- Scenario Editor
- Population 설정
- Environment 설정
- Control 설정
- Execution 설정
- Readiness Panel

### 4.2 Run Workspace

valid scenario를 실행하는 화면이다.

필요한 UI 요소:

- Run / Pause / Resume / Stop
- Batch Progress
- Live Viewport
- Simulation status
- 실행 불가 사유 표시

### 4.3 Analysis Workspace

실행 결과를 검토하고 비교하는 화면이다.

필요한 UI 요소:

- Run Results
- Variation Summary
- Heatmap Selector
- Comparison View
- Recommendation Drawer
- Export Dialog

## 5. 상태별 화면 흐름

### 5.1 No Project

표시 화면:

- Project Navigator

가능한 액션:

- New Project
- 저장된 프로젝트 열기
- 폴더에서 프로젝트 열기

### 5.2 New Project Draft

표시 화면:

- New Project

가능한 액션:

- 프로젝트명 입력
- DXF 선택
- 저장 폴더 선택
- Cancel
- Done

### 5.3 Layout Needs Review

표시 화면:

- Layout Review Workspace

가능한 액션:

- blocking issue 확인
- layout preview 확인
- Project > Save Project

제한:

- blocker가 남아 있으면 simulation run은 비활성화한다.

### 5.4 Layout Ready

후속 구현 상태다.

표시 화면:

- Scenario Authoring Workspace

가능한 액션:

- scenario 생성
- scenario 편집
- run readiness 확인

### 5.5 Scenario Ready

후속 구현 상태다.

표시 화면:

- Run Workspace

가능한 액션:

- simulation 실행
- batch 실행

### 5.6 Results Available

후속 구현 상태다.

표시 화면:

- Analysis Workspace

가능한 액션:

- 결과 확인
- 대안 비교
- 추천 검토
- export

## 6. 현재 우선순위

1. Project Navigator 실제 목록 동작
2. New Project 입력과 DXF import 연결
3. Workspace Shell 재사용 구조 유지
4. Layout Review에서 실제 import 결과 렌더링
5. Blocking issue 목록과 canvas 연동
6. Project 저장/로드 안정화
7. Layout Canvas pan/zoom/selection 추가
8. Scenario Authoring 확장
9. Run Workspace 확장
10. Analysis Workspace 확장

