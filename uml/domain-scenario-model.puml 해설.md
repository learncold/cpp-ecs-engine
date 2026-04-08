# SafeCrowd UML 설계 해설 - domain-scenario-model.puml

대상 파일: `uml/domain-scenario-model.puml`

## 문서 목적
이 문서는 SafeCrowd 시나리오 정의 모델을 설명한다. 핵심은 기존 `ScenarioDefinition`을 유지하되, 내부 입력 계약을 `FacilityLayout`, `PopulationSpec`, `EnvironmentState`, `ControlPlan`, `ExecutionConfig`로 분해해 보여주는 것이다.

## `FacilityLayout`
- 개요: 검토와 보정을 거쳐 실행 가능 상태로 확정된 시설 레이아웃 계약이다.
- 목적: `Room`, `Door`, `Connector`, `ControlZone`, 장애물, 측정 구역까지 포함한 실행 기준을 고정한다.
- 유의사항: raw import geometry가 아니라 실행과 검증에 필요한 토폴로지 수준으로 노출한다.

## `ControlZone`
- 개요: 통제 가능 구역을 나타내는 가벼운 layout 요소다.
- 목적: `구역 통제`, `통제 가능 구역`, 추천안 조정을 room/door/connector 묶음 기준으로 표현한다.
- 유의사항: 별도 geometry 엔티티가 아니라 named member group으로 유지한다.

## `DynamicObstacleRule`
- 개요: 런타임에 활성화될 수 있는 동적 차단 규칙이다.
- 목적: 영속 레이아웃과 실행 중 상태 변화를 분리한다.
- 유의사항: rule 자체는 layout 계약에 남기되, 실제 활성화는 `ControlPlan`과 runtime이 담당한다.

## `ScenarioDefinition`
- 개요: baseline 또는 단일 시나리오의 authoring aggregate다.
- 목적: 상위 구조 문서에서 쓰는 용어를 유지하면서 내부 계약을 명확히 나눈다.
- 유의사항: 모든 저수준 값을 직접 품는 거대한 객체로 만들지 않는다.

## `ScenarioVariation`
- 개요: 기준 시나리오 대비 달라진 부분만 저장하는 variation 단위다.
- 목적: 비교와 반복 실행에서 baseline 대비 delta를 명확히 관리한다.
- 유의사항: baseline 전체 복사본으로 두지 않는다.

## `PopulationSpec`
- 개요: population 입력의 상위 계약이다.
- 목적: `PopulationProfile`, `InitialPlacement`, `DynamicSource`를 같은 루트 아래서 관리한다.
- 유의사항: 문서 전반에서 top-level population contract는 `PopulationSpec`으로 통일한다.

## `PopulationProfile`
- 개요: 이동 특성, 분포 파라미터, connector 사용 제약을 담는 하위 구성요소다.
- 목적: 속도, 가속, 간격, 접근 제약을 프로필 단위로 표현한다.
- 유의사항: 상위 입력 계약으로 승격하지 않고 `PopulationSpec` 안에 둔다.

## `InitialPlacement` / `DynamicSource`
- 개요: 실행 시작 시 배치와 시간 기반 유입을 나누어 표현하는 구조다.
- 목적: 초기 상태와 동적 유입을 같은 값으로 뭉개지 않게 한다.
- 유의사항: 둘을 분리해야 variation diff와 재현성 관리가 단순해진다.

## `ControlPlan`
- 개요: 운영 이벤트, 행동 전환, route choice binding을 묶는 제어 계약이다.
- 목적: 운영 계획을 단순 on/off 플래그가 아니라 실행 가능한 제어 모델로 만든다.
- 유의사항: `ControlZone`과 `DynamicObstacleRule`은 이 계약을 통해 runtime으로 연결된다.

## `ExecutionConfig`
- 개요: 제한시간, 샘플링 주기, base seed, 반복 횟수를 담는 실행 계약이다.
- 목적: 시나리오 내용과 실험 조건을 분리하고 재현성을 유지한다.
- 유의사항: variation과 cumulative 결과가 늘수록 더 명시적으로 보존해야 한다.
