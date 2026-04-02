# SafeCrowd UML 설계 해설 - domain-import-module.puml

대상 파일: `uml/domain-import-module.puml`

## 문서 목적
이 문서는 SafeCrowd의 오픈소스 import 스택을 도메인 중심으로 어떻게 배치할지 설명한다.

핵심 의도는 다음 두 가지다.

- `libdxfrw`, `IfcOpenShell`, `Clipper2`, `Boost.Geometry`, `Recast`, `Detour`를 모두 `domain` 안에 가둔다.
- `engine`은 import 라이브러리나 CAD/BIM 타입을 전혀 모르는 상태를 유지한다.

즉, 이 그림은 "도면을 읽는 방법"보다 "도면을 읽은 결과를 어떤 경계에서 끊을지"에 초점을 둔다.

---

## 전체 흐름
전체 흐름은 다음 순서를 따른다.

1. `Import Workflow UI`가 파일 선택과 사용자 검수 흐름을 시작한다.
2. `ImportOrchestrator`가 확장자나 사용자가 고른 모드에 따라 `DxfImportService` 또는 `IfcImportService`를 호출한다.
3. 각 adapter는 원본 포맷의 엔티티와 참조를 `RawImportModel`로 올린다.
4. `GeometryNormalizer`가 포맷 차이를 제거하고 `CanonicalGeometry`를 만든다.
5. `FacilityLayoutBuilder`가 zone, connection, barrier 같은 SafeCrowd 공간 구조를 추론한다.
6. `ImportValidationService`가 끊긴 동선, 누락 출구, 비정상 폭 같은 문제를 진단한다.
7. `ImportArtifactStore`가 원본 trace, 사용자 수정, 재import 메타데이터를 저장한다.
8. `NavigationBakeService`가 `FacilityLayout`을 `BakedNavigationData`로 변환한다.
9. `SimulationSession`이 승인된 layout과 baked nav 데이터를 받아 엔진 리소스로 등록한다.

---

## `ImportOrchestrator`
- 개요: import의 유일한 도메인 진입점이다.
- 목적: application이 importer 구현체와 외부 라이브러리를 몰라도 되게 만든다.
- 유의사항: UI 판단과 file system 접근을 혼합하지 말고, "어떤 importer를 고를지"와 "어떤 domain 결과를 돌려줄지"만 책임지게 하는 편이 좋다.

## `DxfImportService`
- 개요: `libdxfrw`를 이용한 DXF adapter다.
- 목적: DXF entity, layer, unit, block 참조를 SafeCrowd 내부 모델로 끌어오는 역할을 한다.
- 유의사항: DXF 특유의 선/폴리라인 중심 구조는 아직 의미가 약하므로, 여기서 바로 `FacilityLayout`을 만들기보다 `RawImportModel` 단계에 원본 trace를 보존하는 편이 안전하다.

## `IfcImportService`
- 개요: `IfcOpenShell` 기반 IFC adapter다.
- 목적: IFC의 객체 의미와 geometry 참조를 읽어 내부 모델로 변환한다.
- 유의사항: IFC는 의미 정보가 풍부하지만, 시뮬레이션에 바로 넣기에는 과하므로 역시 `RawImportModel`과 `CanonicalGeometry` 단계를 거쳐 단순화해야 한다.

## `RawImportModel`
- 개요: 포맷별 원본 구조를 최대한 잃지 않고 담는 중간 모델이다.
- 목적: 재import, 원본 추적, 사용자 수정 유지, 오류 진단의 기반이 된다.
- 유의사항: 이 단계는 엔진 입력이 아니라 importer와 normalizer 사이 계약이라는 점을 분명히 해야 한다.

## `GeometryNormalizer`
- 개요: `Clipper2`와 `Boost.Geometry`를 사용하는 geometry 정리 계층이다.
- 목적: DXF/IFC 차이를 줄이고, 2D 보행공간 중심의 `CanonicalGeometry`를 만드는 것이다.
- 유의사항: hole 처리, polygon union, self-intersection 정리, 폭 측정, point-in-polygon, spatial query는 이 계층에서 해결하고, 이후 단계에는 가능한 한 단순한 geometry만 넘기는 편이 좋다.

## `CanonicalGeometry`
- 개요: floor, wall, portal, obstacle, vertical link 후보를 모아둔 포맷 중립 geometry 모델이다.
- 목적: importer 차이를 제거하고 layout 추론의 공통 입력으로 쓰기 위해 필요하다.
- 유의사항: 아직 의미가 확정된 것이 아니라 후보 단계이므로, room/exit/stair 판정은 다음 builder 단계에서 수행하는 편이 명확하다.

## `FacilityLayoutBuilder`
- 개요: SafeCrowd 문제영역의 공간 구조를 추론하는 계층이다.
- 목적: `CanonicalGeometry`를 `Zone`, `Connection`, `Barrier`, `Control` 중심의 `FacilityLayout`으로 바꾼다.
- 유의사항: 이 계층은 SafeCrowd 핵심 규칙이 들어가는 곳이므로, 범용 geometry 라이브러리 코드를 engine으로 빼는 것보다 여기서 도메인 의미를 분명히 하는 편이 맞다.

## `FacilityLayout`
- 개요: 시뮬레이션 입력의 기준이 되는 공간 모델이다.
- 목적: import 결과를 검수 가능하고 시나리오에 연결 가능한 구조로 정규화한다.
- 유의사항: zone ID, 연결 관계, 유효 폭, 방향, 계단 여부, 기본 수용량 같은 필드는 여기서 확정되어야 한다.

## `ImportValidationService`
- 개요: import 결과의 문제점을 자동으로 찾는 진단 계층이다.
- 목적: "자동 추출 후 사람이 검수" 흐름을 지원하기 위해 필요하다.
- 유의사항: 누락 출구, 끊긴 walkable 영역, 과도하게 좁은 연결, 고립 구역 같은 문제는 import 성공 여부와 별개로 반드시 따로 보고해야 한다.

## `ImportArtifactStore`
- 개요: import 결과와 재import 메타데이터를 보존하는 저장 계층이다.
- 목적: 원본 trace, 사용자 수정, 매핑 규칙, 검수 이력을 유지하기 위해 필요하다.
- 유의사항: 이 정보는 장기적으로 engine state보다 시나리오 자산에 가까우므로, 엔진 리소스 안에 섞지 않는 편이 낫다.

## `NavigationBakeService`
- 개요: `Recast`와 `Detour`를 이용해 runtime-friendly navigation 데이터를 만드는 계층이다.
- 목적: `FacilityLayout`을 navmesh, path query, spawn-goal anchor 같은 구조로 bake 한다.
- 유의사항: 이 단계는 import의 일부이지만 runtime crowd behavior 자체는 아니다. 그래서 `DetourCrowd`는 이 그림에서 의도적으로 제외했다.

## `BakedNavigationData`
- 개요: 시뮬레이션 시작 직전 엔진에 올릴 수 있는 navigation 리소스 묶음이다.
- 목적: 엔진이 CAD/BIM이나 복잡한 geometry를 모르고도 pathfinding을 수행할 수 있게 만든다.
- 유의사항: 이 모델은 engine의 generic resource slot에 올릴 수 있어야 하므로, 외부 라이브러리의 raw 타입을 그대로 노출하지 않는 편이 좋다.

## `SimulationSession`
- 개요: import 결과를 시뮬레이션 실행과 이어주는 도메인 오케스트레이터다.
- 목적: 승인된 layout과 baked nav 데이터를 엔진 초기화 절차로 연결한다.
- 유의사항: import와 run을 한 객체에 과하게 섞지 말고, 세션은 연결점 역할에 집중하는 편이 깔끔하다.

## `EngineRuntime` / `EngineWorld`
- 개요: 엔진 실행과 리소스 등록의 공통 진입점이다.
- 목적: domain이 import 결과를 리소스로 주입하고 시뮬레이션을 시작할 수 있게 한다.
- 유의사항: 이 경계에서는 외부 SDK 헤더와 타입이 사라져 있어야 한다.

---

## 설계 핵심 요약
- import 관련 오픈소스 스택은 모두 `domain`에 둔다.
- `engine`은 `FacilityLayout`과 `BakedNavigationData` 같은 domain 결과만 소비한다.
- `DetourCrowd`는 runtime behavior 계층이므로 import 모듈 UML에는 넣지 않는다.
- 재import와 사용자 수정 유지까지 고려하면 `ImportArtifactStore` 같은 추적 계층이 필요하다.
