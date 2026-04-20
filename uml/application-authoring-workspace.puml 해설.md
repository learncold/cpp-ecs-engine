# SafeCrowd UML 설계 해설 - application-authoring-workspace.puml

대상 파일: `uml/application-authoring-workspace.puml`

## 문서 목적
이 문서는 SafeCrowd의 authoring 단계 UI를 설명한다. 목표는 `레이아웃 검토/보정`, `시나리오 생성`, `템플릿 빠른 시작`, `실행 준비 상태 확인`이 어디에서 이뤄지는지 고정하는 것이다.
이 workspace는 독립된 화면 묶음이 아니라 같은 프로젝트 컨텍스트의 앞단이다. 사용자가 여기서 확정한 레이아웃과 시나리오 family는 이후 analysis workspace에서도 같은 `ProjectRepository` 기반 project context로 이어진다.

## `Project Navigator`
- 개요: 프로젝트를 새로 만들고, 다시 열고, 저장하는 authoring 진입점이다.
- 목적: import와 scenario authoring이 항상 같은 workspace 컨텍스트 안에서 시작되게 한다.
- 유의사항: 저장 구현 책임은 `ProjectRepository`에 있다.

## `Import Workflow UI`
- 개요: 파일 선택과 재import 요청을 시작하는 패널이다.
- 목적: 외부 파일 선택과 도메인 import 경계를 분리한다.
- 유의사항: importer 세부 구현 선택을 UI에서 오래 붙잡지 않는다.

## `Issue Review Panel`
- 개요: import 결과의 오류, 경고, 승인 상태를 보여 주는 검토 패널이다.
- 목적: 실행 차단 이슈를 시나리오 편집 단계로 넘기기 전에 먼저 노출한다.
- 유의사항: `실행 불가` 판단은 이 패널과 `Readiness Panel`이 함께 만든다.

## `Layout Canvas + Inspector`
- 개요: 2D topology correction을 위한 보정 화면이다.
- 목적: full CAD 편집기 없이도 door, connection, blocker를 실행 가능한 형태로 고칠 수 있게 한다.
- 유의사항: geometry-heavy editor로 키우지 않고 topology 보정 보조 도구로 유지한다.

## `Scenario Library`
- 개요: baseline, alternative, recommended draft를 모아 보여 주는 목록이다.
- 목적: 비교와 추천 시나리오화가 원본 lineage를 잃지 않게 한다.
- 유의사항: baseline 구분과 추천 초안 구분이 리스트에서 명확해야 한다.

## `Scenario Template Picker`
- 개요: 템플릿 카드와 적용 전제조건을 보여 주는 빠른 시작 패널이다.
- 목적: 비전문가가 어떤 변수부터 손대야 하는지 몰라서 authoring을 중단하지 않게 한다.
- 유의사항: 전제조건 부족 경고를 적용 후가 아니라 적용 전에 보여 준다.

## `Scenario Editor Tabs`
- 개요: population, environment, control, execution 입력을 나눠 편집하는 패널이다.
- 목적: authoring contract를 요구사항 범위에 맞게 분리해서 보여 준다.
- 유의사항: 결과 비교나 추천 계산 책임을 여기로 끌어오지 않는다.

## `Readiness Panel`
- 개요: 필수 입력 누락, 남은 차단 이슈, 실행 가능 여부를 모아 보여 주는 패널이다.
- 목적: run 버튼이 숨은 규칙으로 비활성화되지 않게 한다.
- 유의사항: 누락 필드는 이 패널에서 바로 편집 탭으로 되돌아갈 수 있어야 한다.

## `Variation Diff List`
- 개요: baseline 대비 변경 항목을 기록하는 패널이다.
- 목적: alternative 시나리오의 차이를 비교 화면이 아니라 authoring 단계에서도 투명하게 유지한다.
- 유의사항: control, visibility, inflow, route cost, template source까지 기록 범위에 포함한다.

## `ProjectRepository` / `ScenarioTemplateCatalog`
- 개요: `ProjectRepository`는 workspace 저장 계약이고, `ScenarioTemplateCatalog`는 템플릿 기본값과 전제조건을 반환하는 domain 계약이다.
- 목적: authoring UI가 파일 경로나 템플릿 규칙을 직접 소유하지 않게 한다.
- 유의사항: 두 계약 모두 application의 UI 배치와는 분리된 domain 책임으로 본다. 특히 `ProjectRepository`는 project restore의 정본이고, 후행 analysis 결과 저장소와 책임을 섞지 않는다.
