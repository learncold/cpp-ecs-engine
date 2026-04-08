# SafeCrowd UML 설계 해설 - engine-ecs-core.puml

대상 파일: `uml/engine-ecs-core.puml`

## 문서 목적
이 문서는 ECS 코어 UML에 등장하는 각 요소를 설명한다. 이 그림은 엔진의 데이터 저장 구조와 `EngineWorld`를 통한 접근면을 설명하는 데 목적이 있다.

핵심은 "누가 실제 데이터를 저장하는가"와 "누가 그 저장소에 직접 접근하지 않도록 막는가"를 분리해서 이해하는 것이다.

---

## `EngineWorld`
- 개요: ECS 코어에 접근하는 상위 파사드다.
- 목적: runtime과 상위 계층이 raw registry 대신 동일한 진입점을 쓰도록 만든다.
- 접근 경계: `WorldQuery` 같은 ECS 읽기 면도 직접 생성하지 않고 `EngineWorld::query()`를 통해서만 꺼내 쓴다.
- 유의사항: 편의 객체처럼 보이더라도, 실제로는 계층 경계를 지키는 핵심 요소다.
- 후속 개선 사항: 읽기/쓰기 분리 facade나 phase-aware facade로 나눌 수 있다.

## `WorldQuery`
- 개요: ECS 컴포넌트 조회와 값 접근용 API다.
- 목적: 필요한 컴포넌트 조합을 읽고 개별 엔티티의 컴포넌트 값을 조회하거나 갱신하게 한다.
- 접근 경계: 상위 계층은 raw `EcsCore`를 받아 `WorldQuery`를 직접 만들지 않고, `EngineWorld`가 제공하는 query facade를 사용한다.
- 유의사항: 초기에는 단순 signature 필터로 충분하지만, query 중 엔티티 생성/삭제나 컴포넌트 추가/제거 같은 구조 변경은 허용하지 않는 원칙이 중요하다.
- 후속 개선 사항: cached query, typed iterator, optional access 지원을 확장할 수 있다.

## `WorldCommands`
- 개요: ECS 구조 변경 요청을 표현하는 좁은 명령 면이다.
- 목적: 생성/삭제/추가/제거를 즉시 적용하지 않고 안전한 지점에 반영하도록 만든다.
- 유의사항: 이 레이어에서 바로 storage를 건드리게 열어두면 deferred mutation 설계가 무너진다.
- 후속 개선 사항: 명령 검증, batch API, conflict resolution 정책을 추가할 수 있다.

## `WorldResources`
- 개요: resource 접근 API다.
- 목적: 컴포넌트 저장과는 별개인 전역 상태를 같은 월드 면에서 다루게 한다.
- 유의사항: 이 API는 ECS 코어 그림에 나오지만, 실제 저장 위치는 runtime 계층이라는 점을 혼동하지 않는 것이 중요하다.
- 후속 개선 사항: resource scope, read-only handle, lazy creation을 추가할 수 있다.

## `EcsCore`
- 개요: registry와 storage를 묶는 ECS 저장 코어다.
- 목적: 엔티티 생명주기, 시그니처, 컴포넌트 저장을 한데 관리한다.
- 유의사항: 이 코어는 범용 저장 구조여야 하며 domain 용어를 알면 안 된다.
- 후속 개선 사항: query 보조 인덱스, 메모리 진단, archetype 유사 구조를 검토할 수 있다.

## `EntityRegistry`
- 개요: 엔티티 할당/해제와 signature 추적을 담당하는 레지스트리다.
- 목적: 어떤 entity가 살아 있고 어떤 컴포넌트 구성을 가지는지 관리한다.
- 유의사항: generation 검증과 alive 검사가 약하면 stale handle 버그를 잡기 어려워진다.
- 후속 개선 사항: free list 최적화, 디버그 검증, generation overflow 정책을 추가할 수 있다.

## `ComponentRegistry`
- 개요: 컴포넌트 타입 등록과 타입별 storage 접근을 담당하는 레지스트리다.
- 목적: 타입별 component storage를 중앙에서 관리하고, entity 삭제 시 정리 작업을 연결한다.
- 유의사항: 템플릿 기반 타입 등록 규칙이 흔들리면 type id 일관성이 깨질 수 있다.
- 후속 개선 사항: 정적 등록 도구, reflection 보조, debug type dump를 추가할 수 있다.

## `IComponentStorage`
- 개요: 모든 컴포넌트 저장소가 공통으로 따라야 하는 최소 인터페이스다.
- 목적: 타입이 달라도 entity 삭제 같은 공통 정리 경로를 하나로 묶기 위해 필요하다.
- 유의사항: 공통 인터페이스는 정말 공통인 작업만 담고, 타입별 세부 동작까지 끌어올리지 않는 편이 좋다.
- 후속 개선 사항: debug hooks, storage stats, validation callback을 붙일 수 있다.

## `PackedComponentStorage<T>`
- 개요: remove-by-swap 기반의 밀집 컴포넌트 저장소다.
- 목적: 컴포넌트를 캐시 친화적으로 연속 저장하고 빠르게 순회하게 한다.
- 유의사항: 삭제 시 swap이 일어나므로, 외부에서 storage 내부 인덱스를 안정 참조로 쓰면 안 된다.
- 후속 개선 사항: small buffer 최적화, sparse lookup 개선, debug iterator를 추가할 수 있다.

## `Entity`
- 개요: `index + generation` 조합으로 이루어진 엔티티 핸들이다.
- 목적: 단순 ID 재사용으로 인한 stale reference 문제를 막는다.
- 유의사항: 직렬화나 로그 출력 시에도 index만 보지 말고 generation까지 함께 다루는 편이 안전하다.
- 후속 개선 사항: invalid entity sentinel, compact debug string, hash helper를 추가할 수 있다.

## `ComponentType`
- 개요: 컴포넌트 타입을 식별하는 내부 타입 ID다.
- 목적: signature bitset과 storage 매핑에 사용된다.
- 유의사항: 등록 순서가 바뀌면 의미가 달라질 수 있으므로 type registration 규칙을 고정하는 편이 좋다.
- 후속 개선 사항: compile-time registration, overflow 검증, debug name 매핑을 추가할 수 있다.

## `Signature`
- 개요: entity가 어떤 컴포넌트를 갖는지 나타내는 비트 집합이다.
- 목적: query 매칭과 시스템 대상 선별을 빠르게 수행하게 한다.
- 유의사항: 컴포넌트 수 제한과 bitset 크기 정책은 초기에 명확히 정해야 한다.
- 후속 개선 사항: 동적 비트셋, signature mask cache, query specialization을 붙일 수 있다.
