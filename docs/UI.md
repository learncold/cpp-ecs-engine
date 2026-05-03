# SafeCrowd UI 기준

이 문서는 SafeCrowd 앱에서 사용자가 실제로 만나는 화면, 기능 기준, 후속 UI 개선 대상을 한곳에 정리한다. 세부 작업 단위와 상태 추적은 GitHub Issues와 Project에서 관리하고, 이 문서는 팀원이 "현재 어떤 화면 흐름이 기준인지", "이 버튼이 무엇을 하는지", "다음 Sprint에서 무엇을 보완해야 하는지"를 빠르게 확인하는 기준으로 쓴다.

`domain`의 import, validation, simulation 객체는 UI 디자인 대상이 아니다. UI는 `src/application/`에서 Qt Widgets로 구현하고, `domain` 결과를 화면에 표시한다.

## 1. 앱 전체 흐름

Sprint 1 기준 사용자 흐름은 다음 순서를 따른다.

1. 앱 시작
2. Project Navigator에서 새 프로젝트 생성 또는 최근 프로젝트 열기
3. DXF 파일을 프로젝트 layout으로 import
4. Layout Review Workspace에서 구조와 issue 확인
5. 필요한 경우 layout 수동 보정
6. blocking issue가 없으면 layout 승인
7. Scenario Authoring Workspace에서 baseline 시나리오 작성
8. 보행자 배치와 운영 이벤트 목록 확인
9. 실행 대상으로 stage
10. Run Workspace에서 실행, 일시정지, 재개, 정지
11. 완료 후 Result Summary에서 핵심 결과 확인

Sprint 1 시연에서 가장 중요한 기준은 `도면 불러오기 -> 검토/보정 -> 시나리오 작성 -> 실행`이 끊기지 않는 것이다.

## 2. 공통 UI 구조

### 2.1 Workspace Shell

프로젝트가 열린 뒤 사용하는 공통 작업 화면 구조다. Layout Review, Scenario Authoring, Run, Analysis 화면은 같은 workspace shell 안에서 단계적으로 확장한다.

현재 구현 책임:

- 상단 프로젝트 메뉴
- 좌측 상태/이슈/작성 패널
- 중앙 canvas
- 우측 review/inspector/scenario/run/results 패널
- 하단 상태/요약 패널
- Workspace Shell 옵션에 따라 left rail, left panel, right panel을 숨기거나 표시하며, 숨긴 영역은 폭을 남기지 않는다.
- Project Navigator를 제외한 Workspace Shell 기반 화면은 상단 오른쪽 `Hide Panel` / `Show Panel` 버튼으로 우측 패널을 접고 펼칠 수 있다.

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

저장 위치 정책:

- 기본 위치는 `<사용자 Documents>/SafeCrowd Projects/<프로젝트 이름>`이며, 새 프로젝트 화면에서 프로젝트 이름을 입력하면 폴더 경로가 이 규칙으로 자동 제안된다.
- 자동 제안 경로가 이미 비어 있지 않은 폴더와 충돌하면 ` (2)`, ` (3)` 같은 접미를 붙여 사용 가능한 경로를 찾는다.
- 사용자가 Browse로 직접 고른 경로는 사용자의 의사로 보고, 이후 프로젝트 이름을 바꿔도 자동 갱신하지 않는다.
- 폴더명은 OS 금지 문자(`\\ / : * ? " < > |`)를 `_`로 치환해 정규화한다.
- 저장 시점에 다음 조건을 만족하지 않으면 거부한다.
  - 드라이브 또는 볼륨의 루트가 아니어야 한다.
  - 심볼릭 링크 또는 Windows 정션이 아니어야 한다.
  - 비어 있거나, SafeCrowd 관리 파일(`safecrowd-project.json`, `layout.dxf`, `layout-review.json`, `workspace-state.json`)만 존재해야 한다.
- 최근 프로젝트 인덱스는 `QStandardPaths::AppDataLocation` 아래에 저장한다. `QApplication`의 organization name과 application name을 모두 `SafeCrowd`로 고정하므로, Windows 기준 경로는 `%APPDATA%/SafeCrowd/SafeCrowd/recent-projects.json`이 된다.

## 3. 화면별 기능 기준

### 3.1 Project Navigator

앱을 처음 켰을 때 보이는 시작 화면이다. 예시 프로젝트 목록을 두지 않고, 실제 저장된 프로젝트 목록과 built-in `Demo` 프로젝트를 보여준다.

현재 기능:

- 최근 프로젝트 목록 표시
- built-in `Demo` 프로젝트 표시
- 프로젝트 목록이 없을 때 빈 상태 표시
- `+ New Project`로 새 프로젝트 생성 화면 진입
- 최근 프로젝트 row 클릭 시 해당 프로젝트 열기
- 일반 프로젝트 row의 삭제 버튼으로 프로젝트 삭제
- 삭제 전 확인 다이얼로그 표시
- 삭제 확인 시 실제 프로젝트 폴더 삭제 및 최근 목록 갱신
- built-in `Demo` 프로젝트 삭제 방지

현재 UI 요소:

- `SafeCrowd` 제목
- 저장된 프로젝트 리스트
- 프로젝트명
- 마지막 저장 시각
- `+ New Project` 버튼
- `Open Folder` 버튼

아직 기준 기능이 아닌 것:

- `Open Folder` 버튼은 현재 Sprint 1 핵심 흐름이 아니다.
- 최근 목록에 없는 폴더를 직접 선택해 여는 기능은 별도 작업으로 다룬다.
- 프로젝트 검색, 정렬, 상태 badge는 후속 기능이다.

### 3.2 New Project

새 프로젝트를 만들고 DXF layout을 import하기 위한 화면이다. 이 화면에서 시뮬레이션 설정까지 받지 않고, 프로젝트 생성과 layout import에 필요한 최소 정보만 받는다.

현재 기능:

- 프로젝트명 입력
- layout DXF 파일 선택
- 프로젝트 저장 폴더 선택
- `Done` 클릭 시 프로젝트 메타데이터 저장
- `Done` 클릭 시 DXF를 프로젝트 폴더에 `layout.dxf`로 복사
- 저장 후 Layout Review Workspace로 이동
- `Cancel` 클릭 시 Project Navigator로 복귀
- 프로젝트명, layout 파일, 폴더 중 하나라도 비어 있으면 경고 표시

현재 UI 요소:

- `New Project` 제목
- Project Name 입력 필드
- Layout 파일 선택 버튼
- 선택된 DXF 경로 표시
- Folder 선택 버튼
- 선택된 저장 폴더 경로 표시
- `Cancel`
- `Done`

현재 입력 기준:

- 직접 import 대상은 `.dxf`이다.
- `.dwg`는 직접 import하지 않는다.
- DWG 도면은 외부 도구에서 DXF로 변환한 뒤 사용한다.

### 3.3 Layout Review Workspace

DXF import 결과를 검토하고 시뮬레이션 가능한 layout인지 확인하는 화면이다.

현재 기능:

- DXF import 결과 표시
- 중앙 canvas에 import 결과 기반 2D preview 렌더링
- 좌측 패널에서 issue와 layout 요소 목록 전환
- issue를 `Blocking`, `Warnings`, `Info`로 구분
- issue 클릭 시 관련 대상 focus 및 우측 inspector 상세 표시
- layout 요소 클릭 시 canvas에서 해당 요소 focus
- 우측 패널에 inspector와 approval 상태 표시
- 하단 패널에 현재 review/project context 표시
- blocking issue가 있으면 `Approve Layout` 비활성화
- blocking issue가 없으면 layout 승인 후 Scenario Authoring Workspace로 이동

현재 UI 요소:

- Workspace Shell 상단바
- 좌측 `Blocking` / `Warnings` / `Info` issue 목록
- 좌측 layout element 목록
- 중앙 Layout Preview
- 우측 선택 inspector
- 우측 approval action
- 하단 review 상태 영역

현재 중앙 preview 표시 대상:

- walkable area / zone polygon
- obstacle polygon
- wall segment
- opening segment
- layout connection
- barrier polyline

현재 좌측 issue 목록 표시 대상:

- issue code
- severity
- message
- source id
- target id

현재 수동 보정 기능:

- active floor 선택 및 floor 추가
- room 그리기
- exit 그리기
- wall 그리기
- door/opening 그리기
- stair/ramp 층간 연결 그리기
- ㄷ자형 stair 층간 연결 그리기. 진입 방향 기준 오른쪽 half-lane은 출발 층, 왼쪽 half-lane은 목표 층 geometry로 생성한다.
- connection 또는 barrier 삭제
- undo
- 보정 후 live validation 재실행

### 3.4 Layout Canvas

Layout Review의 중앙 작업 영역이다. 현재는 import 결과와 수동 보정 결과를 QPainter로 렌더링하는 preview와 authoring canvas 역할을 한다.

현재 표시 대상:

- import된 layout geometry
- layout 요소 focus 상태
- active floor에 속한 layout 요소
- 보정 중인 room, exit, wall, door/opening, stair/ramp, ㄷ자형 stair
- 선택적으로 활성화하는 grid mode. `0.1 m`, `0.5 m`, `1.0 m` 간격을 제공하며, 켜진 동안 보정 중인 좌표와 선택 이동을 grid에 맞춘다.
- 공통 canvas surface는 직각 모서리의 white rendering section으로 캔버스 영역 전체를 채운다.
- hotspot overlay는 Result Summary에서 사용한다.

### 3.5 Scenario Authoring Workspace

승인된 layout 위에서 baseline 시나리오를 만들고 실행 준비를 하는 화면이다.

현재 기능:

- Layout Review에서 blocker가 사라진 뒤 `Approve Layout` 클릭 시 진입
- 최초 진입 시 baseline 시나리오 이름 입력
- Workspace Shell 재사용
- 상단바 오른쪽 `Scenario` / `Run` 토글로 오른쪽 패널 내용 전환
- 두 토글을 모두 해제하면 오른쪽 패널을 숨기고 중앙 canvas 확장
- Scenario 패널에서 현재 시나리오 요약 표시
- Scenario switcher로 baseline/alternative 전환
- `New Scenario from Current`로 현재 시나리오를 복제해 alternative 생성
- Run 패널에서 실행 대상으로 staged 된 시나리오 목록 표시
- Layout, Crowd, Events 좌측 탭 제공
- 중앙 canvas에서 보행자 배치 작성
- select, individual occupant placement, rectangular group placement 지원
- 시나리오를 run 대상으로 stage
- 우측 Scenario panel에서 baseline 대비 변경 요약과 readiness 표시

현재 UI 요소:

- 좌측 Layout 요약 및 zone 목록
- 좌측 Crowd 배치/그룹 요약
- 좌측 Events 목록
- Scenario switcher
- New Scenario from Current
- Top-right Scenario / Run panel toggles
- Run staged scenario list
- Run Staged Scenarios button
- Scenario name 입력 팝업
- 승인 layout canvas
- Select tool
- Add Individual Occupant tool
- Add Occupant Group tool
- Add exit closure
- Add staged release
- Readiness Panel

현재 보행자 배치 기능:

- individual occupant placement
- rectangular group placement
- group count 설정
- 배치된 인원을 scenario draft의 `population.initialPlacements`로 반영

현재 약한 부분:

- 운영 이벤트 editor는 최소 구조만 있고 실제 입력 UI가 부족하다.
- readiness panel은 요약 수준이며 실행 불가 사유를 충분히 모아 보여주지 않는다.
- environment와 execution 상세 입력은 아직 제한적이다.
- scenario 저장/로드는 Sprint 1 핵심 시연 범위 밖이지만 후속 보완이 필요하다.

### 3.6 운영 이벤트

운영 이벤트는 앱 실행 버튼을 멈추는 기능이 아니라, 시뮬레이션 안의 현장 조건을 바꾸는 기능이다.

예시:

- 특정 시간 이후 출구 폐쇄
- 특정 구역에서 staged release
- 임시 장애물 활성화
- 안내 방향 또는 접근 가능 대상 변경

Sprint 1 최소 기준:

- 이벤트 목록 표시
- 시간 기반 trigger 입력
- 출구나 연결부 같은 target 선택
- 이벤트를 scenario draft의 control plan에 반영

현재 상태:

- Events 탭과 event draft 구조는 있다.
- 사용자가 직접 이벤트를 추가, 수정, 삭제하는 editor UI는 아직 부족하다.

### 3.7 Run Workspace

stage된 baseline 시나리오를 실제로 실행하고 진행 상태를 보는 화면이다.

현재 기능:

- Scenario Authoring의 Run panel에서 staged baseline scenario 실행
- Workspace Shell 재사용
- Workspace Shell은 navigation 없이 top bar, simulation canvas, right run panel만 사용
- 중앙 live simulation canvas에 agent 위치와 속도 방향 표시
- 우측 Run panel에 실행 상태와 주요 위험 지표 표시
- pause/resume 지원
- stop/reset 지원
- elapsed time 표시
- elapsed time 기준 시간 진행률 표시
- evacuated/total, active agents 표시
- evacuated/total 기준 대피 진행률 표시
- completion risk, stalled agents, hotspot, bottleneck 요약 표시
- run이 complete되면 Result Summary 진입 버튼 활성화
- risk, stalled, hotspot, bottleneck 기준 툴팁 표시

현재 UI 요소:

- Pause / Resume icon button
- Stop icon button
- View Results
- elapsed time
- time progress
- evacuated / total
- evacuation progress
- active agents
- completion risk
- stalled agents
- hotspot count
- worst bottleneck summary

현재 약한 부분:

- run queue는 staged list 수준이다.
- 여러 staged scenario를 순차 실행하는 batch queue는 아직 아니다.
- 실행 전 readiness 조건과 실행 불가 사유 표시가 부족하다.
- batch queue와 persisted result artifact가 아직 연결되어 있지 않다.

### 3.8 Result Summary / Analysis Workspace

실행 완료 후 핵심 결과를 요약해 보여주는 화면이다. 현재는 Sprint 1용 Result Summary가 먼저 구현되어 있고, 비교/추천/내보내기 중심의 전체 Analysis Workspace는 후속 구현 상태다.

현재 기능:

- 완료된 run의 final simulation frame 표시
- Workspace Shell은 left reports panel, simulation canvas, right summary panel 구성 사용
- 상단/우측 summary panel에 completion time, time margin, peak density, worst bottleneck, slowest group 표시
- time margin은 v1에서 `scenario.execution.timeLimitSeconds` 기준 목표 시간 대비 여유로 계산
- slowest group은 v1에서 보행자 배치 그룹별 마지막 완료시간을 표시하며, 별도 취약자 속성 모델이 들어오면 취약자 완료시간 카드로 확장
- 중앙 simulation canvas에서 result overlay 선택 제공: peak density, bottleneck, hotspot, none
- hotspot overlay 표시
- bottleneck overlay 표시
- peak density overlay는 peak 시점의 전체 density field를 사용해 보행 가능 영역 안에 부드러운 heatmap으로 표시
- peak density heatmap 색상 범례 표시
- 누적 대피 곡선 표시
- 하단 graph panel에 Remaining, Exits, Compare 탭 제공
- Exits 탭에 출구별 이용 인원, 비율, 마지막 통과 시각 표시
- Compare 탭은 v1 placeholder이며 baseline/alternative 비교 계산은 후속 Analysis Workspace 범위
- 상세 탭에 Zones, Groups, Criteria 표시
- Zones 탭에 구역별 초기 인원, 대피 인원, 마지막 완료시각 표시
- Groups 탭에 배치 그룹별 초기 인원, 대피 인원, 마지막 완료시각 표시
- Criteria 탭에 고밀도, stalled, bottleneck 계산 기준 표시
- T90/T95 대피 완료 시각 표시
- evacuated count 표시
- elapsed time 표시
- risk level 표시
- stalled count 표시
- 왼쪽 reports panel에 bottleneck 목록 표시
- 왼쪽 reports panel에 hotspot 목록 표시
- bottleneck 목록 클릭 시 canvas focus 표시
- hotspot 목록 클릭 시 canvas focus 표시
- hotspot 색 강도 범례 표시
- 우측 summary panel에서 핵심 metric card와 액션 표시

현재 UI 요소:

- Evacuated
- Time
- Completion
- Time Margin
- Peak Density
- Worst Bottleneck
- Slowest Group
- T90
- T95
- Risk
- Stalled
- Result Reports
- Evacuation Progress
- Remaining / Exits / Compare tabs
- Zones / Groups / Criteria detail tabs
- Bottlenecks
- Hotspots
- Hotspot intensity legend
- Peak Density heatmap legend
- `Run Again`
- `Edit Scenario`
- risk/stalled/hotspot/bottleneck 기준 툴팁
- `Run Again`은 같은 scenario 조건으로 Run Workspace를 새로 열어 시뮬레이션을 다시 시작한다.

현재 약한 부분:

- baseline 대비 alternative 비교는 Sprint 2 범위다.
- persisted result artifact 기반 analysis는 Sprint 2 이후 범위다.
- export는 Sprint 3 범위다.
- ASET/FED/연기 노출 기반 안전여유는 아직 없고, 현재 margin은 시나리오 목표시간 대비 여유다.
- 이동약자/지원 필요자 전용 결과는 아직 없고, 현재는 가장 늦게 완료한 placement group을 표시한다.

## 4. 상태별 화면 흐름

### 4.1 No Project

표시 화면:

- Project Navigator

가능한 액션:

- New Project
- 저장된 프로젝트 열기
- 폴더에서 프로젝트 열기

### 4.2 New Project Draft

표시 화면:

- New Project

가능한 액션:

- 프로젝트명 입력
- DXF 선택
- 저장 폴더 선택
- Cancel
- Done

### 4.3 Layout Needs Review

표시 화면:

- Layout Review Workspace

가능한 액션:

- blocking issue 확인
- layout preview 확인
- layout 수동 보정
- Project > Save Project

제한:

- blocker가 남아 있으면 simulation run은 비활성화한다.

### 4.4 Layout Ready

표시 화면:

- Scenario Authoring Workspace

가능한 액션:

- scenario 생성
- scenario 편집
- run readiness 확인

### 4.5 Scenario Ready

표시 화면:

- Run Workspace

가능한 액션:

- staged baseline simulation 실행
- pause/resume
- stop/reset
- complete 이후 Result Summary 열기

후속 액션:

- batch 실행

### 4.6 Results Available

표시 화면:

- Result Summary
- 후속 Analysis Workspace

가능한 액션:

- 결과 확인

후속 액션:

- 대안 비교
- 추천 검토
- export

## 5. 기능 상태 요약

| 영역 | 현재 상태 | Sprint 1 판단 |
| --- | --- | --- |
| Project Navigator | 최근 프로젝트, 새 프로젝트, 삭제 가능 | 핵심 흐름 가능 |
| Open Folder | 버튼만 있고 동작 없음 | 후순위 |
| New Project | DXF 선택과 프로젝트 생성 가능 | 핵심 흐름 가능 |
| DXF Import | DXF 파싱과 layout 생성 가능 | 핵심 흐름 가능 |
| Layout Review | issue 확인과 승인 가능 | 핵심 흐름 가능 |
| Layout 보정 | 기본 drawing/editing 가능 | 핵심 흐름 가능 |
| Scenario 생성 | baseline 생성과 전환 가능 | 핵심 흐름 가능 |
| Crowd 배치 | 개인/그룹 배치 가능 | 핵심 흐름 가능 |
| 운영 이벤트 | 구조는 있으나 editor 부족 | 보완 필요 |
| Run | 실행, pause/resume, stop 가능 | 핵심 흐름 가능 |
| Result Summary | 기본 결과 요약 가능 | 시연 보조 가능 |

## 6. Sprint별 UI 개선 체크리스트

### 6.1 Sprint 1 보완

- [ ] Project Navigator에서 열 수 없는 프로젝트 상태 표시
- [ ] New Project 입력값 validation 메시지를 화면 안에 표시
- [ ] DXF가 아닌 파일 선택 방지 강화
- [ ] 같은 폴더에 기존 프로젝트가 있을 때 덮어쓰기 확인
- [ ] DXF import 진행 상태 표시
- [ ] import 실패 상세 메시지 또는 전용 화면 제공
- [ ] Layout Review issue 클릭과 canvas focus 연동 강화
- [ ] 선택 요소의 상세 편집 inspector 보강
- [ ] warning/info 승인 또는 무시 상태 표시
- [ ] issue별 repair suggestion 표시
- [ ] 운영 이벤트 추가, 수정, 삭제 editor 보강
- [ ] readiness panel에 실행 불가 사유 목록 표시

### 6.2 Sprint 2 개선

- [ ] 프로젝트 검색/정렬 제공
- [ ] 프로젝트 상태 badge 표시: review 필요, blocker 있음, ready 등
- [ ] Project 메뉴 확장: Save As, Close Project, Back to Navigator
- [ ] 저장 상태 표시: saved, unsaved, failed
- [ ] 최근 저장 시각 표시
- [ ] Layout Canvas pan 제공
- [ ] Layout Canvas wheel zoom 제공
- [ ] Layout Canvas fit to view 제공
- [ ] layer visibility 제공
- [ ] element selection과 geometry hover highlight 제공
- [ ] import issue 위치 표시
- [ ] DXF layer별 색상/스타일 반영
- [ ] reimport 이후 기존 보정 내용 비교
- [ ] Scenario Library 제공
- [ ] Scenario Template Picker 제공
- [ ] Environment 상세 설정 제공
- [ ] Control 이벤트 상세 편집 제공
- [ ] Scenario 저장/로드 연결
- [ ] baseline 대비 alternative 비교 화면 제공
- [ ] persisted result artifact 기반 analysis 연결

### 6.3 Sprint 3 이후 확장

- [ ] 최근 목록에 없는 폴더를 직접 선택해 여는 기능 제공
- [ ] Tool 메뉴의 실제 도구 구성
- [ ] Run Workspace batch progress 제공
- [ ] 여러 staged scenario 순차 실행
- [ ] Run Results 목록 제공
- [ ] Variation Summary 제공
- [ ] Heatmap Selector 제공
- [ ] Comparison View 제공
- [ ] Recommendation Drawer 제공
- [ ] Export Dialog 제공

## 7. 문서 유지 규칙

- 앱 화면의 의미나 사용자 흐름이 바뀌면 이 문서를 갱신한다.
- UI 화면 목록, 기능 기준, 후속 개선 체크리스트는 이 문서만 기준으로 삼는다.
- Sprint별 시연 기준은 `docs/product/Sprint 시연 계획.md`를 따른다.
- 작업 단위, 상태, PR 연결은 GitHub Issues와 Project를 기준으로 관리한다.
- 이 문서는 임시 TODO 목록이 아니라 앱 기능 이해를 위한 기준 문서로 유지한다.
