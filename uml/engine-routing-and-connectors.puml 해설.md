# SafeCrowd UML 설계 해설 - engine-routing-and-connectors.puml

대상 파일: `uml/engine-routing-and-connectors.puml`

## 문서 목적
이 문서는 room, door, connector, control zone, obstacle, path cost, flow measurement가 domain과 engine 경계에서 어떻게 이어지는지를 설명한다. 핵심은 domain 의미 모델을 engine 실행 자원으로 바꾸되, 엔진이 rich domain object에 직접 의존하지 않게 하는 것이다.

## `FacilityLayout`
- 개요: domain에서 확정한 실행용 공간 계약이다.
- 목적: room, door, connector, control zone, 측정 구역 정의를 routing 입력의 기준으로 사용한다.
- 유의사항: 엔진이 raw import geometry를 직접 해석하게 두지 않는다.

## `Connector`
- 개요: 계단, 램프, 에스컬레이터, 보행로를 포괄하는 connector 정의다.
- 목적: connector 종류를 공통 모델로 관리하면서 traversal rule과 modifier만 다르게 적용한다.
- 유의사항: `ConnectorSpec` 같은 별도 이름으로 다시 분기하지 않고 동일 개념으로 유지한다.

## `ControlZone`
- 개요: room/door/connector 묶음을 가리키는 controllable group이다.
- 목적: 측정이 아니라 access override의 대상으로 쓰이는 구역 개념을 고정한다.
- 유의사항: `MeasurementRegionSpec`과 재사용 관계가 아니라 별도 계약으로 유지한다.

## `DynamicObstacleRule`
- 개요: 실행 중 활성화되거나 해제되는 동적 차단 규칙이다.
- 목적: topology 재생성 없이 차단과 penalty를 반영한다.
- 유의사항: 정적 obstruction과 같은 저장/계산 경로로 합치지 않는다.

## `MeasurementRegionSpec`
- 개요: 측정 구역 정의다.
- 목적: heatmap과 region series의 수집 지점을 runtime 전에 고정한다.
- 유의사항: control zone과 결과 시계열을 같은 객체로 보지 않는다.

## `RoutingTopologySnapshot`
- 개요: domain 레이아웃을 엔진이 읽을 수 있는 안정된 routing 스냅샷으로 컴파일한 결과다.
- 목적: 엔진이 layout object graph에 직접 의존하지 않게 한다.
- 유의사항: zone membership과 connector rule seed까지 여기서 정리하고 runtime에 전달한다.

## `ZoneAccessOverrideState`
- 개요: 현재 활성화된 zone 단위 접근 제어 상태를 담는 runtime 자원이다.
- 목적: `ControlZone` 변경을 topology 재구축 없이 path planning에 반영한다.
- 유의사항: 결과 수집용 측정 상태와 섞지 않는다.

## `TravelCostEvaluator`
- 개요: 현재 조건에서 경로 비용을 계산하는 평가기다.
- 목적: topology, dynamic penalties, zone override, route choice weights를 합쳐 실제 cost를 계산한다.
- 유의사항: topology ownership까지 가지지 않게 한다.

## `FlowMeasurementService`
- 개요: door, room, measurement region 단위 샘플을 수집하는 경계 서비스다.
- 목적: 나중에 `DoorHistory`, `RoomHistory`, `MeasurementRegionSeries`로 변환될 원시 샘플을 만든다.
- 유의사항: control zone 상태를 결과 시계열 구조로 직접 해석하지 않는다.
