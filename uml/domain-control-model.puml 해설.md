# SafeCrowd UML 설계 해설 - domain-control-model.puml

대상 파일: `uml/domain-control-model.puml`

## 문서 목적
이 문서는 SafeCrowd의 제어 모델을 설명한다. 목표는 운영 이벤트를 단순 플래그 모음으로 두지 않고, `행동`, `트리거`, `occupant tag`, `route choice`, `구역 통제`를 함께 갖는 구조로 정리하는 것이다.

## `ControlPlan`
- 개요: 시나리오 안에서 적용되는 운영 제어의 루트 계약이다.
- 목적: 이벤트, 태그, 행동, route choice binding을 한 곳에서 일관되게 관리한다.
- 유의사항: 제어 관련 필드를 시나리오 전역에 흩뿌리지 않는다.

## `OperationalEvent`
- 개요: 특정 시점이나 상태에서 발동되는 운영 변화 단위다.
- 목적: 출구 개방/폐쇄, 구역 통제, source rate 조정, obstacle activation을 같은 실행 단위로 표현한다.
- 유의사항: 이벤트는 무엇이 바뀌는지만 나타내고, 세부 규칙은 binding과 change 객체가 담당한다.

## `Trigger`
- 개요: 이벤트가 언제 발동되는지 정의하는 조건 구조다.
- 목적: 시간 기반과 상태 기반 발동을 같은 상위 구조 아래에서 관리한다.
- 유의사항: 단순 timestamp 필드로 축소하지 않는다.

## `OccupantTag`, `Behavior`, `BehaviorBinding`
- 개요: 특정 인원 집합에 어떤 행동을 적용할지 정의하는 구조다.
- 목적: 전체 인원 일괄 제어 대신 부분 집합 제어를 가능하게 한다.
- 유의사항: 이벤트가 직접 행동 구현을 품지 않게 분리한다.

## `RouteChoicePolicy` / `PathCostModel`
- 개요: 출구 선택과 reroute에 쓰는 상위 정책과 비용 조합 모델이다.
- 목적: `travel`, `queue`, `remaining route cost`를 명시적으로 조합한다.
- 유의사항: 비용 계산 구현은 engine 경계에 있더라도 어떤 비용을 쓸지는 domain 계약으로 남긴다.

## `ControlZone`
- 개요: `구역 통제`를 표현하기 위한 controllable group 참조다.
- 목적: room/door/connector 묶음 수준의 운영 제어를 layout과 같은 용어로 연결한다.
- 유의사항: 측정 구역과 같은 개념으로 취급하지 않는다.

## `AccessRuleChange`
- 개요: door나 connector 같은 직접 접근 지점의 규칙을 바꾸는 이벤트 효과다.
- 목적: 출구 개방/폐쇄, 일방통행, connector 접근 제한을 표현한다.
- 유의사항: layout 기본 속성과 runtime override를 분리한다.

## `ZoneAccessChange`
- 개요: `ControlZone` 단위 접근 규칙을 바꾸는 이벤트 효과다.
- 목적: 제품 문서의 `구역 통제`, `통제 가능 구역` 요구를 도메인 모델에서 직접 수용한다.
- 유의사항: zone은 named group이므로 geometry 자체를 다시 정의하지 않는다.

## `SourceRateChange`
- 개요: 동적 인원 유입 source의 유입률을 조정하는 이벤트 효과다.
- 목적: source 기반 유입을 운영 계획과 연결한다.
- 유의사항: source 정의와 source 조정 이벤트를 같은 객체로 합치지 않는다.

## `DynamicObstacleActivation`
- 개요: 재사용 가능한 `DynamicObstacleRule`을 켜거나 끄는 이벤트 효과다.
- 목적: layout에 정의된 obstacle rule을 runtime control과 연결한다.
- 유의사항: obstacle rule 자체를 이벤트 내부에 중복 정의하지 않는다.
