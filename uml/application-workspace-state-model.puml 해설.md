# SafeCrowd UML 설계 해설 - application-workspace-state-model.puml

대상 파일: `uml/application-workspace-state-model.puml`

## 문서 목적
이 문서는 SafeCrowd application UI의 상태 게이팅 규칙을 설명한다. 핵심은 패널을 단순히 숨기거나 보이는 문제가 아니라, `언제 어떤 화면과 버튼이 활성화되는가`를 요구사항 수준으로 고정하는 것이다.

## `NoProject`
- 개요: 아직 프로젝트가 열리지 않은 초기 상태다.
- 목적: import 또는 프로젝트 다시 열기 전에는 어떤 authoring/analysis 패널도 활성화하지 않는다.
- 유의사항: 최근 프로젝트 목록만 보일 수 있다.

## `LayoutNeedsReview`
- 개요: 레이아웃 import 후 검토나 보정이 남아 있는 상태다.
- 목적: 실행 차단 이슈를 scenario authoring 이전에 분리한다.
- 유의사항: blocking issue가 남아 있으면 run은 비활성 상태를 유지한다.

## `LayoutReady`
- 개요: 레이아웃이 승인되어 authoring을 시작할 수 있는 상태다.
- 목적: scenario draft 편집의 출발점을 만든다.
- 유의사항: 유효한 scenario가 아직 없을 수 있다.

## `ScenarioDraftInvalid`
- 개요: 시나리오 초안은 있지만 필수 입력이 누락되었거나 충돌이 있는 상태다.
- 목적: `Readiness Panel`에서 어떤 입력이 부족한지 드러낸다.
- 유의사항: 저장은 허용할 수 있어도 run은 허용하지 않는다.

## `ScenarioReady`
- 개요: 승인된 레이아웃과 유효한 scenario draft가 결합된 상태다.
- 목적: run 버튼의 기본 활성 상태를 정의한다.
- 유의사항: `approved layout && valid scenario`가 아닌 경우로 되돌아가면 다시 비활성화한다.

## `BatchRunning` / `BatchPaused`
- 개요: batch 실행 중이거나 일시정지된 상태다.
- 목적: 실시간 진행률, 현재 run, 현재 variation을 명시적으로 추적한다.
- 유의사항: 이 상태의 analysis는 live playback 중심이며 persisted comparison은 아직 선행 조건이 충족되지 않을 수 있다.

## `AggregationPending`
- 개요: 실행은 끝났지만 variation/comparison/cumulative artifact 조립이 아직 끝나지 않은 상태다.
- 목적: run 종료와 분석 가능 시점을 분리한다.
- 유의사항: UI는 이 구간에서 결과 요약 일부와 비활성 이유를 함께 보여 주는 편이 안전하다.

## `ResultsAvailable`
- 개요: `RunResult`와 `VariationSummary`가 준비된 상태다.
- 목적: run summary와 repeated-run aggregate를 먼저 여는 기준점을 제공한다.
- 유의사항: export와 recommendation은 아직 더 강한 선행 조건을 요구한다.

## `ComparisonReady`
- 개요: baseline과 1개 이상 alternative의 요약이 준비되어 비교가 가능한 상태다.
- 목적: 비교 화면 진입 게이트를 명시한다.
- 유의사항: scenario family가 바뀌거나 alternative가 사라지면 다시 `ResultsAvailable` 또는 `ScenarioReady`로 되돌아간다.

## `RecommendationReady`
- 개요: `ScenarioComparison`과 `CumulativeArtifact`가 모두 준비되어 추천과 canonical export가 가능한 상태다.
- 목적: 추천, 내보내기, 추천 시나리오화의 진입 조건을 고정한다.
- 유의사항: recommendation과 export를 live runtime 상태에 연결하지 않는다.
