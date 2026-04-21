# SafeCrowd UI 디자인 대상 창 목록

이 문서는 다음 UML을 기준으로 실제 UI 디자인이 필요한 창, 화면, 패널, 다이얼로그를 정리한다.

- `uml/application-authoring-workspace.puml`
- `uml/application-analysis-workspace.puml`
- `uml/application-workspace-state-model.puml`

`ProjectRepository`, `ResultRepository`, `ImportOrchestrator`, `ScenarioBatchRunner`, `SimulationSession`, `ResultAggregator`, `AlternativeRecommendationService`, `EngineRuntime`, `IRenderBridge` 같은 항목은 구현 컴포넌트이며 UI 디자인 대상이 아니다.

## 1. 메인 애플리케이션 창

### 1.1 Main Workspace Window
- 앱의 기본 창이다.
- 프로젝트 상태, 작성(authoring), 실행(run), 분석(analysis) 흐름을 한 화면 구조 안에서 전환하거나 배치한다.
- 상태 모델의 `NoProject`, `LayoutNeedsReview`, `LayoutReady`, `ScenarioDraftInvalid`, `ScenarioReady`, `BatchRunning`, `BatchPaused`, `AggregationPending`, `ResultsAvailable`, `ComparisonReady`, `RecommendationReady` 상태에 따라 패널 활성/비활성이 달라져야 한다.

필요한 디자인 요소:
- 상단 메뉴 또는 툴바
- 현재 프로젝트명과 저장 상태
- 현재 workspace 상태 표시
- 주요 작업 영역 전환
- 닫기/저장/열기 같은 프로젝트 명령
- 실행 가능 여부와 blocker 상태 표시

## 2. 프로젝트 시작/관리 창

### 2.1 Project Navigator
- 새 프로젝트, 기존 프로젝트 열기, 저장, 최근 프로젝트 접근의 진입점이다.
- `Project Save/Open`은 별도 창 이름이 아니라 저장/열기 처리를 담당하는 내부 흐름으로 본다.

필요한 디자인 요소:
- 새 프로젝트 생성
- 프로젝트 열기
- 프로젝트 저장
- 다른 이름으로 저장이 필요하다면 저장 위치 선택
- 최근 프로젝트 목록
- 프로젝트 닫기
- 프로젝트 상태 표시

### 2.2 Project Open Dialog
- 앱 전용 프로젝트 선택 창을 만들 경우 디자인 대상이다.
- OS 파일 다이얼로그만 사용할 경우 별도 상세 UI 디자인은 생략 가능하다.

필요한 디자인 요소:
- 프로젝트 파일 선택
- 최근 프로젝트 바로 열기
- 열 수 없는 프로젝트 오류 표시
- pending review 프로젝트와 approved 프로젝트 상태 구분

### 2.3 Project Save Dialog
- 앱 전용 저장 위치 선택 또는 프로젝트 메타데이터 입력이 필요할 경우 디자인 대상이다.
- 단순 파일 저장만 OS 파일 다이얼로그로 처리한다면 별도 화면 디자인은 최소화할 수 있다.

필요한 디자인 요소:
- 저장 위치 선택
- 프로젝트 이름 입력
- 덮어쓰기 확인
- 저장 실패 오류 표시

## 3. Authoring Workspace 창과 패널

### 3.1 Authoring Workspace
- 레이아웃 import, 검토, 보정, 시나리오 작성이 이루어지는 작업 화면이다.
- `LayoutNeedsReview`, `LayoutReady`, `ScenarioDraftInvalid`, `ScenarioReady` 상태를 주로 다룬다.

필요한 디자인 요소:
- 프로젝트 네비게이터 영역
- import 흐름 진입
- layout canvas
- inspector
- issue review
- scenario library
- scenario editor
- readiness 표시
- variation diff 표시

### 3.2 Import Workflow UI
- 외부 레이아웃 파일을 선택하고 import 또는 reimport를 수행하는 화면 또는 다이얼로그다.

필요한 디자인 요소:
- 파일 선택
- import 옵션
- reimport 옵션
- 변환 규칙 선택
- import 실행
- import 진행 상태
- import 실패/성공 결과 표시

### 3.3 Issue Review Panel
- import 오류, 경고, blocker를 검토하는 패널이다.
- blocker가 남아 있으면 실행이 비활성화되어야 한다.

필요한 디자인 요소:
- 오류/경고 목록
- blocker 여부
- 승인/무시/수정 필요 상태
- 문제 위치로 이동
- layout review 승인 상태
- 남은 blocker 수

### 3.4 Layout Canvas
- import된 공간 구조와 topology를 보고 수정하는 주 작업 화면이다.

필요한 디자인 요소:
- 공간/노드/연결/출구/제어구역 표시
- 선택, 확대/축소, 이동
- 오류 위치 하이라이트
- topology 수정
- live/analysis viewport와 구분되는 authoring 전용 표시

### 3.5 Inspector Panel
- Layout Canvas에서 선택한 요소의 속성을 표시하고 편집하는 패널이다.
- 현재 UML에서는 `Layout Canvas + Inspector`로 묶여 있지만 실제 UI 디자인에서는 별도 패널로 다루는 것이 좋다.

필요한 디자인 요소:
- 선택 요소 속성
- 이름/유형/연결/측정 구역 등 편집 필드
- 유효성 오류 표시
- 변경 저장 또는 적용

### 3.6 Scenario Library
- baseline, alternative, recommended draft를 구분해서 보여주는 시나리오 목록이다.

필요한 디자인 요소:
- 시나리오 패밀리 목록
- baseline 표시
- alternative 목록
- recommended draft 목록
- run-ready 상태
- scenario lineage 표시
- 시나리오 열기/복제/삭제

### 3.7 Scenario Template Picker
- 새 시나리오를 빠르게 만들기 위한 템플릿 선택 화면 또는 다이얼로그다.

필요한 디자인 요소:
- 템플릿 카드
- intended use
- risk axis
- 필요한 layout feature
- prerequisite 충족/미충족 표시
- 적용 불가 사유
- 템플릿 적용

### 3.8 Scenario Editor Tabs
- 선택한 시나리오의 입력값을 편집하는 탭형 화면이다.

필요한 디자인 요소:
- Population 탭
- Environment 탭
- Control 탭
- Execution 탭
- 필수 입력 표시
- invalid edit 상태 표시
- draft 저장

### 3.9 Readiness Panel
- 현재 layout과 scenario가 실행 가능한지 보여주는 패널이다.

필요한 디자인 요소:
- 필수 입력 누락
- blocker 목록
- layout 승인 여부
- scenario validation 결과
- Run 버튼 활성 조건 설명
- 누락 항목으로 이동

### 3.10 Variation Diff List
- baseline 대비 alternative 또는 recommended draft의 변경점을 보여주는 패널이다.

필요한 디자인 요소:
- 변경된 입력 목록
- control event 차이
- route cost assumption 차이
- inflow setting 차이
- visibility condition 차이
- template origin metadata
- baseline으로부터의 변경 추적

## 4. Run/Analysis Workspace 창과 패널

### 4.1 Analysis Workspace
- 실행, 재생, 결과 확인, 비교, 추천, 내보내기를 수행하는 작업 화면이다.
- `BatchRunning`, `BatchPaused`, `AggregationPending`, `ResultsAvailable`, `ComparisonReady`, `RecommendationReady` 상태를 주로 다룬다.

필요한 디자인 요소:
- run control
- batch progress
- live viewport
- heatmap selector
- run results
- variation summary
- comparison
- recommendation
- export

### 4.2 Run Control
- batch 실행을 시작, 일시정지, 재개, 정지하는 제어 영역이다.

필요한 디자인 요소:
- Run
- Pause
- Resume
- Stop
- 실행 불가 상태 표시
- 승인된 layout과 valid scenario 조건 표시
- 현재 실행 상태

### 4.3 Batch Progress
- batch 또는 variation 실행 진행률을 보여주는 패널이다.

필요한 디자인 요소:
- 현재 run 번호
- 현재 variation
- 전체 진행률
- 남은 작업량
- 실패한 run 표시
- aggregation pending 상태 표시

### 4.4 Live Viewport
- 실행 중 runtime playback을 보여주는 viewport다.
- 비교, 추천, 집계 계산을 이 화면에서 직접 추론하지 않아야 한다.

필요한 디자인 요소:
- 시뮬레이션 재생 화면
- frame snapshot 표시
- playback 상태
- live overlay 표시
- 확대/축소/이동
- 실행 중 상호작용 제한

### 4.5 Heatmap Selector
- live overlay와 persisted heatmap layer를 선택하는 컨트롤이다.

필요한 디자인 요소:
- live overlay 선택
- persisted layer 선택
- 데이터 출처 표시
- 레이어 표시/숨김
- 색상 범례
- intensity 범위

### 4.6 Run Results Panel
- 단일 실행 결과 요약을 보여주는 패널이다.
- `ResultsAvailable` 이후 열 수 있다.

필요한 디자인 요소:
- run summary
- 주요 위험 지표
- 시간/구간별 결과
- 관련 artifact 상태
- Variation Summary로 이동

### 4.7 Variation Summary
- 반복 실행 aggregate 결과를 보여주는 패널이다.
- `ResultsAvailable` 이후 열 수 있다.

필요한 디자인 요소:
- variation별 집계
- 평균/최악/분포 지표
- 반복 실행 신뢰도
- baseline comparison 열기
- persisted summary 상태

### 4.8 Comparison View
- baseline과 alternatives를 비교하는 화면이다.
- `ComparisonReady` 이후 활성화된다.

필요한 디자인 요소:
- baseline 선택
- alternative 선택
- 비교 지표 표
- 차이 시각화
- cumulative artifact 표시
- recommendation drawer 열기
- export 진입

### 4.9 Recommendation Drawer
- 추천 근거를 검토하고 추천안을 scenarioize하는 drawer 또는 side panel이다.
- `RecommendationReady` 이후 활성화된다.

필요한 디자인 요소:
- 추천 후보 목록
- evidence 표시
- 추천 근거 artifact 연결
- scenarioize 버튼
- 추천 적용/닫기
- 적용 시 draft 생성 흐름

### 4.10 Export Dialog
- canonical artifact bundle을 내보내는 다이얼로그다.
- 필요한 persisted artifact가 준비된 뒤 활성화된다.

필요한 디자인 요소:
- 내보낼 artifact 선택
- canonical bundle 구성 확인
- 저장 위치 선택
- 파일 형식 선택
- export 진행 상태
- export 성공/실패 표시

## 5. 상태별 UI 활성 규칙

### 5.1 NoProject
- 프로젝트가 열려 있지 않은 상태다.
- Project Navigator, Project Open Dialog, 새 프로젝트 생성 흐름이 중심이다.
- authoring, run, analysis 패널은 비활성 또는 빈 상태를 보여준다.

### 5.2 LayoutNeedsReview
- import된 layout에 검토가 필요하거나 blocker가 남아 있는 상태다.
- Issue Review Panel과 Layout Canvas가 중심이다.
- Run Control은 비활성화되어야 한다.

### 5.3 LayoutReady
- layout이 승인되었지만 실행 가능한 scenario가 아직 선택되지 않은 상태다.
- Scenario Library와 Scenario Template Picker 진입이 중심이다.

### 5.4 ScenarioDraftInvalid
- scenario draft가 있으나 필수 입력이 부족하거나 invalid edit가 있는 상태다.
- Scenario Editor Tabs와 Readiness Panel이 중심이다.
- Run Control은 비활성화되어야 한다.

### 5.5 ScenarioReady
- 승인된 layout과 valid scenario가 모두 준비된 상태다.
- Run Control의 Run이 활성화된다.

### 5.6 BatchRunning
- batch 실행 중인 상태다.
- Run Control, Batch Progress, Live Viewport, Heatmap Selector가 중심이다.
- 편집성 authoring 동작은 제한해야 한다.

### 5.7 BatchPaused
- 실행이 일시정지된 상태다.
- Resume과 Stop이 중심 액션이다.

### 5.8 AggregationPending
- live playback은 끝났지만 결과 artifact 집계가 끝나지 않은 상태다.
- Batch Progress 또는 결과 준비 상태 표시가 필요하다.
- Run Results, Variation Summary, Comparison, Recommendation은 준비된 artifact 수준에 따라 제한된다.

### 5.9 ResultsAvailable
- RunResult와 VariationSummary가 저장된 상태다.
- Run Results Panel과 Variation Summary를 열 수 있다.
- Comparison은 baseline과 alternative summary가 모두 있을 때만 활성화된다.

### 5.10 ComparisonReady
- baseline과 alternative 비교가 가능한 상태다.
- Comparison View가 활성화된다.
- Recommendation은 ScenarioComparison과 CumulativeArtifact가 준비된 뒤 활성화된다.

### 5.11 RecommendationReady
- recommendation 검토와 scenarioize가 가능한 상태다.
- Recommendation Drawer와 Export Dialog가 활성화될 수 있다.

## 6. 최종 디자인 대상 요약

우선 디자인해야 하는 UI 창과 화면은 다음과 같다.

1. Main Workspace Window
2. Project Navigator
3. Project Open Dialog
4. Project Save Dialog
5. Authoring Workspace
6. Import Workflow UI
7. Issue Review Panel
8. Layout Canvas
9. Inspector Panel
10. Scenario Library
11. Scenario Template Picker
12. Scenario Editor Tabs
13. Readiness Panel
14. Variation Diff List
15. Analysis Workspace
16. Run Control
17. Batch Progress
18. Live Viewport
19. Heatmap Selector
20. Run Results Panel
21. Variation Summary
22. Comparison View
23. Recommendation Drawer
24. Export Dialog

이 중 `Project Open Dialog`와 `Project Save Dialog`는 OS 파일 다이얼로그만 사용할 경우 독립 UI 디자인 범위를 줄일 수 있다. 반대로 프로젝트 메타데이터, 최근 프로젝트, pending review 상태, 저장 정책을 앱 안에서 보여줘야 한다면 별도 디자인 대상에 포함한다.
