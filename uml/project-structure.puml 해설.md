# SafeCrowd UML 설계 해설 - project-structure.puml

대상 파일: `uml/project-structure.puml`

## 문서 목적
이 문서는 SafeCrowd 전체 계층 구조에서 `application -> domain -> engine` 경계가 어떻게 유지되는지 설명한다. 세부 구현보다 상위 책임과 데이터 흐름을 읽는 용도로 본다.

## `safecrowd_app (Qt App)`
- 개요: Qt 기반 실행 파일과 사용자 인터페이스 진입점이다.
- 목적: 시나리오 편집, 실행 제어, 결과 열람 같은 사용자 상호작용을 담당한다.
- 유의사항: 비교 계산, 추천 규칙, ECS 세부 로직을 UI에 두지 않는다.

## `ScenarioDefinition`
- 개요: SafeCrowd의 authoring aggregate다.
- 목적: `FacilityLayout`, `PopulationSpec`, `EnvironmentState`, `ControlPlan`, `ExecutionConfig`를 한 실행 단위로 묶는다.
- 유의사항: `PopulationProfile`은 상위 입력 계약이 아니라 `PopulationSpec`의 하위 요소로 유지한다.

## `ExecutionConfig`
- 개요: 제한시간, 샘플링 주기, seed, 반복 횟수 같은 실행 계약이다.
- 목적: 같은 시나리오를 어떤 조건으로 반복 실행했는지 재현 가능하게 남긴다.
- 유의사항: 시나리오 내용과 실험 조건을 섞지 않는다.

## `ScenarioCalibrationService`
- 개요: UI 입력을 실행용 내부 파라미터로 보정하는 도메인 계층 서비스다.
- 목적: 사용자 친화적 모델과 엔진 친화적 파라미터 사이를 분리한다.
- 유의사항: 보정 규칙을 엔진이나 UI 쪽으로 흘리지 않는다.

## `SimulationSession`
- 개요: 단일 run 수명주기를 조정하는 도메인 세션이다.
- 목적: 엔진 초기화, 실행, 수집, 종료를 일관된 절차로 묶는다.
- 유의사항: 결과 비교나 추천까지 한 객체에 몰아 넣지 않는다.

## `ScenarioBatchRunner`
- 개요: 기준안, 대안, 반복 실행을 묶어 배치로 실행하는 조정자다.
- 목적: SafeCrowd의 시나리오 비교 흐름을 단일 run 위에 얹는다.
- 유의사항: 화면이 여러 run을 직접 관리하지 않게 한다.

## `ResultAggregator`
- 개요: run 결과로부터 persisted variation/comparison/cumulative artifact를 생성하거나 갱신하는 도메인 서비스다.
- 목적: 결과 화면, 내보내기, 추천이 같은 저장된 결과 계약을 읽게 만든다.
- 유의사항: 화면 렌더링 시점의 ad hoc delta 계산기로 쓰지 않는다.

## `EngineRuntime`
- 개요: 엔진 실행 루프와 수명주기를 관리하는 API 진입점이다.
- 목적: 도메인이 엔진 내부 구현을 몰라도 initialize/play/stop 같은 제어를 수행하게 한다.
- 유의사항: SafeCrowd 전용 위험 규칙을 런타임 자체에 넣지 않는다.

## `EngineWorld`
- 개요: query, resources, commands를 묶은 공유 월드 파사드다.
- 목적: 도메인과 runtime이 raw ECS storage 대신 같은 접근면을 통해 월드에 접근하게 한다.
- 유의사항: 명령은 즉시 반영이 아니라 phase 경계에서 flush된다는 원칙을 유지한다.

## `EngineSystem`
- 개요: 엔진에서 등록되고 스케줄링되는 범용 시스템 계약이다.
- 목적: 도메인 시스템과 엔진 공통 시스템이 같은 실행 규칙을 따르게 한다.
- 유의사항: 시스템 인터페이스는 좁고 명시적인 입력 중심으로 유지한다.

## `Runtime Core`
- 개요: `FrameClock`, `SystemScheduler`, `CommandBuffer`, `ResourceStore`, `DeterministicRng`를 묶은 실행 코어다.
- 목적: 시간 관리, 순서 제어, deferred mutation, 재현성을 엔진 공통 기능으로 제공한다.
- 유의사항: 초기 범위에서는 fixed-step과 deterministic run 계약을 먼저 단단히 잡는다.

## `ECS Core`
- 개요: `EntityRegistry`, `ComponentRegistry`, `PackedComponentStorage` 중심의 저장 계층이다.
- 목적: 엔티티와 컴포넌트를 효율적으로 저장하고 query 기반 순회를 가능하게 한다.
- 유의사항: SafeCrowd 도메인 타입과 Qt 의존성이 들어오지 않게 유지한다.
