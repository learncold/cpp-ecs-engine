# SafeCrowd UML 설계 해설 - application-run-results-workflow.puml

대상 파일: `uml/application-run-results-workflow.puml`

## 문서 목적
이 문서는 application 레이어에서 시나리오 작성, 실행, 결과 확인, 비교, 내보내기, 추천 검토가 어떤 순서로 이어지는지 설명한다. 핵심은 화면이 persisted artifact를 읽고, domain 서비스가 상위 결과 아티팩트를 생성하거나 소비한다는 점이다.

## `Project Workspace`
- 개요: 프로젝트 열기와 저장의 루트 작업 공간이다.
- 목적: authoring과 analysis가 같은 프로젝트 맥락 안에서 이어지게 한다.
- 유의사항: 모든 기능을 한 화면에 몰아넣는 뜻이 아니라 공통 컨텍스트를 공유한다는 의미다.

## `Scenario Editor`
- 개요: 레이아웃, population, control plan을 수정하는 작성 화면이다.
- 목적: baseline과 variation을 같은 authoring 흐름 안에서 관리한다.
- 유의사항: 결과 비교와 추천 근거 계산을 직접 맡지 않는다.

## `Run Control Panel`
- 개요: 실행, 일시정지, 정지, 반복 실행 요청을 담당하는 패널이다.
- 목적: 작성 단계와 실행 단계의 책임을 분리한다.
- 유의사항: 엔진 직접 제어보다 batch orchestration 요청의 진입점으로 본다.

## `Run Results Panel`
- 개요: 단일 run 또는 variation 요약을 먼저 보여 주는 결과 패널이다.
- 목적: 사용자를 바로 비교 화면으로 보내기 전에 저장된 결과를 단계적으로 읽게 한다.
- 유의사항: live engine state가 아니라 `ResultRepository`의 persisted artifact를 읽는다.

## `Comparison View`
- 개요: baseline과 대안을 비교하는 핵심 분석 화면이다.
- 목적: 저장된 comparison/cumulative artifact를 기준으로 delta와 근거를 보여 준다.
- 유의사항: 화면 렌더링 시점의 ad hoc domain 계산기로 쓰지 않는다.

## `ResultRepository`
- 개요: run, variation, comparison, cumulative artifact를 저장하고 다시 여는 서비스다.
- 목적: 모든 결과 화면이 같은 저장 진입점을 사용하게 한다.
- 유의사항: 각 화면이 파일 경로를 직접 관리하지 않게 한다.

## `ResultAggregator`
- 개요: run 결과로부터 variation/comparison/cumulative artifact를 생성하거나 갱신하는 도메인 서비스다.
- 목적: comparison과 recommendation이 같은 저장된 결과 계약을 읽게 한다.
- 유의사항: 비교 화면이 열릴 때마다 delta를 즉석 계산하는 역할로 쓰지 않는다.

## `Recommendation Panel`
- 개요: 추천 후보와 근거를 확인하고 시나리오화하는 화면이다.
- 목적: 추천 기능을 결과 파이프라인 뒤에 두어 근거가 분명한 흐름을 만든다.
- 유의사항: panel이 직접 추천 규칙을 가지지 않고 `AlternativeRecommendationService`를 호출한다.

## `AlternativeRecommendationService`
- 개요: `ScenarioComparison`과 `CumulativeArtifact`를 읽어 운영 대안 후보를 만드는 도메인 서비스다.
- 목적: 추천을 UI 헬퍼가 아니라 도메인 정책 서비스로 유지한다.
- 유의사항: live metric이나 엔진 내부 상태를 직접 읽는 경로를 만들지 않는다.

## `EngineRuntime` / `IRenderBridge`
- 개요: 실행 중 playback과 viewport 동기화를 담당하는 엔진 경계다.
- 목적: application이 직접 ECS를 만지지 않고도 live 상태를 관찰하게 한다.
- 유의사항: 결과 비교, export, recommendation은 runtime와 직접 연결하지 않는다.
