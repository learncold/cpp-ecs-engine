# SafeCrowd UML 설계 해설 - application-run-results-workflow.puml

대상 파일: `uml/application-run-results-workflow.puml`

## 문서 목적
이 문서는 SafeCrowd application 레이어의 전체 사용자 여정을 한 장으로 설명한다. 핵심은 `불러오기/검토/보정 -> 시나리오 작성 -> 실행/반복 실행 -> 실시간 확인 -> persisted 결과 분석 -> 추천/내보내기`가 하나의 프로젝트 맥락 안에서 이어지되, 저장 계약과 화면 책임은 분리된다는 점이다.

## `Project Workspace` / `Project Navigator`
- 개요: `Project`, `Authoring`, `Run`, `Analysis`를 묶는 공통 컨텍스트다.
- 목적: 사용자가 프로젝트를 다시 열었을 때 레이아웃, 시나리오 가족, 실행 메타데이터, 결과 인덱스를 같은 맥락에서 이어 가게 한다.
- 유의사항: 공통 맥락을 공유한다고 해서 모든 기능을 하나의 패널에 몰아넣는 뜻은 아니다.

## `Project Save/Open`
- 개요: 프로젝트 수준 저장과 다시 열기 진입점이다.
- 목적: 작업 공간 복원은 `ResultRepository`가 아니라 `ProjectRepository`를 통해 수행되게 한다.
- 유의사항: 결과 아티팩트 조회와 프로젝트 복원을 같은 저장소 책임으로 합치지 않는다.

## `Import Workflow` / `Import Review` / `Layout Correction`
- 개요: 구조 데이터를 불러오고, 차단 이슈를 검토하고, 2D canvas + inspector 기반으로 topology를 보정하는 앞단 흐름이다.
- 목적: 실행 전에 `승인된 레이아웃`이라는 명시적 게이트를 만든다.
- 유의사항: 수동 보정은 full CAD editor가 아니라 topology correction 보조 도구로 유지한다.

## `Scenario Library` / `Template Picker` / `Scenario Editor` / `Readiness Panel`
- 개요: baseline, alternative, recommended draft를 관리하고 템플릿 기반 빠른 시작과 상세 편집을 연결하는 authoring 흐름이다.
- 목적: 비전문가도 템플릿으로 초안을 만들고, 필요한 입력이 누락되면 `Readiness Panel`에서 바로 보게 한다.
- 유의사항: 실행 가능 여부 판단은 run 버튼 주변에 흩어 놓지 않고 `Readiness Panel`에 모은다.

## `Run Queue` / `Run Control Panel` / `Batch Progress`
- 개요: variation 선택, 반복 실행 횟수, seed 계약, 현재 run/variation 진행 상태를 다루는 실행 제어 영역이다.
- 목적: authoring 화면이 배치 실행 세부 상태를 직접 관리하지 않게 한다.
- 유의사항: 실행 제어는 엔진 직접 제어가 아니라 `ScenarioBatchRunner`에 대한 요청으로 본다.

## `Live Viewport` / `Heatmap Overlay`
- 개요: 실행 중 runtime snapshot을 보여 주고, playback 위에 live overlay를 얹는 시각화 영역이다.
- 목적: 실시간 관찰과 종료 후 분석을 같은 캔버스 감각으로 연결한다.
- 유의사항: live overlay와 persisted heatmap은 데이터 출처가 다르므로 같은 계산 경로로 취급하지 않는다.

## `Run Results Panel` / `Variation Summary` / `Comparison View`
- 개요: 단일 run 요약, 반복 실행 집계, baseline 대비 대안 비교를 단계적으로 여는 분석 영역이다.
- 목적: 사용자를 바로 비교 화면으로 보내지 않고, persisted artifact를 읽는 순서를 명확히 유지한다.
- 유의사항: `Comparison View`는 `ScenarioComparison`과 `CumulativeArtifact`를 읽는 소비자이지 ad hoc delta 계산기가 아니다.

## `Recommendation Drawer` / `Export Dialog`
- 개요: 추천 근거 검토와 canonical bundle 내보내기를 담당하는 후행 분석 영역이다.
- 목적: 비교 결과와 누적 아티팩트를 근거로 운영 대안을 설명 가능하게 제시하고, 같은 번들을 외부 공유에도 재사용하게 한다.
- 유의사항: 추천과 내보내기는 run 종료 직후의 live metric이 아니라 저장 완료된 결과 아티팩트를 입력으로 사용한다.

## `ProjectRepository`
- 개요: 레이아웃, 시나리오 가족, run/variation 메타데이터, artifact index를 저장하고 다시 여는 도메인 계약이다.
- 목적: 프로젝트 복원이 결과 저장소 구현에 종속되지 않게 한다.
- 유의사항: project workspace의 진입점이지, raw run summary를 직접 분석하는 저장소가 아니다.

## `ResultRepository` / `ResultAggregator`
- 개요: `ResultRepository`는 persisted 결과를 저장/조회하고, `ResultAggregator`는 run 결과로부터 variation/comparison/cumulative artifact를 생성한다.
- 목적: comparison, export, recommendation이 같은 저장 계약을 읽게 만든다.
- 유의사항: run 화면이 열릴 때마다 도메인 계산을 다시 수행하지 않게 한다.

## `ScenarioTemplateCatalog`
- 개요: 템플릿 카드 목록과 authoring 기본값 번들을 돌려주는 도메인 authoring helper다.
- 목적: application은 카드 배치와 설명 UI만 맡고, 템플릿 기본값 구성은 domain 계약으로 둔다.
- 유의사항: 템플릿 적용 가능 여부 판단도 레이아웃 전제조건과 함께 반환하는 편이 안전하다.

## `AlternativeRecommendationService`
- 개요: `ScenarioComparison`과 `CumulativeArtifact`를 근거로 추천 후보를 생성하고 시나리오화 가능한 변경 묶음을 돌려주는 도메인 서비스다.
- 목적: 추천 규칙을 UI 헬퍼가 아니라 domain 정책으로 유지한다.
- 유의사항: live runtime state를 직접 읽는 경로를 만들지 않는다.
