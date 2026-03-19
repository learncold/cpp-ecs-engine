# SafeCrowd PlantUML 읽는 법

## 목적
이 문서는 `uml/` 폴더의 PlantUML 다이어그램을 렌더링해서 볼 때, 각 요소와 선이 무엇을 뜻하는지 빠르게 이해할 수 있도록 정리한 문서이다.

이 프로젝트에서는 크게 두 종류를 본다.
- 상위 구조를 보여주는 **component/package 중심 그림**
- 엔진 내부 타입 관계를 보여주는 **class diagram**

---

## 1. 박스와 요소 읽는 법

### `package` - 구역을 나누는 선
- 의미: 관련 요소를 묶는 논리적 영역이다.
- 이 프로젝트 예시: `application`, `domain`, `engine`, `engine::api`, `engine::runtime`, `engine::ecs`
- 읽는 법: "이 박스 안의 요소들은 같은 계층 또는 같은 모듈에 속한다"로 보면 된다.

### 회색 사각형 컴포넌트 박스
- 의미: 상위 수준에서 책임 단위를 표현한다.
- 이 프로젝트 예시: `Runtime Core`, `ECS Core`, `safecrowd_app`, `ResultAggregator`
- 읽는 법: 구현 클래스 하나라기보다 "역할 묶음"일 수도 있다.

### `class`
- 의미: 실제 타입 또는 구체 구현 단위를 나타낸다.
- 렌더링에서 보이는 `C`
  - 보통 `Class`를 뜻한다.
- 이 프로젝트 예시: `EngineRuntime`, `EngineWorld`, `FrameClock`, `CommandBuffer`
- 읽는 법: 상태와 함수를 가지는 구체 타입으로 이해하면 된다.

### `interface`
- 의미: 구현보다 계약이 중요한 타입이다.
- 렌더링에서 보이는 `I`
  - 보통 `Interface`를 뜻한다.
- 이 프로젝트 예시: `EngineSystem`, `IRenderBridge`, `IComponentStorage`
- 읽는 법: "이 모양을 만족하는 여러 구현이 올 수 있다"는 뜻으로 보면 된다.

### `enum`
- 의미: 제한된 값 집합을 표현한다.
- 렌더링에서 보이는 `E`
  - 보통 `Enum`을 뜻한다.
- 이 프로젝트 예시: `UpdatePhase`, `TriggerPolicy`
- 읽는 법: 시스템이 가질 수 있는 선택지 목록이라고 보면 된다.

### `note`
- 의미: 설계 의도, 제약, 구현 메모를 붙이는 설명 상자다.
- 이 프로젝트 예시: `WorldCommands`가 즉시 mutation하지 않는다는 설명, `EcsCore`가 재사용 가능한 엔진 코어라는 설명
- 읽는 법: 선과 박스만 보면 놓치기 쉬운 설계 규칙을 보충하는 텍스트다.

---

## 2. 선과 화살표 읽는 법

PlantUML에서 선은 크게 두 가지를 같이 본다.
- **선 종류**: 실선인지 점선인지
- **끝 모양**: 화살표인지, 마름모인지, 삼각형인지

방향은 보통 **왼쪽 요소가 관계를 보내는 쪽**, **오른쪽 요소가 받는 쪽**으로 읽는다.

### `-->`
- 의미: 일반적인 방향성 있는 연관 또는 사용 관계
- 읽는 법: "A가 B를 사용한다", "A가 B로 향한다"
- 예시:
  - `SimulationSession --> EngineRuntime`
  - `EngineWorld --> EcsCore`

### `..>`
- 의미: 점선 의존 관계, 약한 사용 관계, 구현 세부에 가까운 연결
- 읽는 법: "강한 소유는 아니고, 이쪽이 저쪽에 의존한다"
- 예시:
  - `App ..> Api`
  - `WorldCommands ..> EcsCore`

### `*--`
- 렌더링: 채워진 마름모 끝을 가진 선
- 의미:  **합성(composition)** 관계
- 읽는 법: "왼쪽이 오른쪽을 강하게 포함하거나 생명주기를 소유한다"
- 예시:
  - `EngineRuntime *-- FrameClock`
  - `EcsCore *-- EntityRegistry`
- 해석 팁:
  - 보통 오른쪽 요소는 왼쪽 요소 없이 독립적으로 존재하기 어렵다는 뜻으로 읽는다.

### `o--`
- 렌더링: 비워진 마름모 끝을 가진 선
- 의미: **집합(aggregation)** 관계
- 읽는 법: "왼쪽이 오른쪽을 참조하거나 묶어두지만, 생명주기 소유는 합성보다 약하다"
- 예시:
  - `EngineRuntime o-- IRenderBridge`
  - `ComponentRegistry o-- IComponentStorage`
- 해석 팁:
  - "가지고는 있지만, 완전히 내장된 부품이라고 단정하진 않는다" 정도로 이해하면 된다.

### `..|>`
- 렌더링: 점선 + 빈 삼각형
- 의미: **인터페이스 구현(realization)** 관계
- 읽는 법: "왼쪽이 오른쪽 인터페이스를 구현한다"
- 예시:
  - `PackedComponentStorage ..|> IComponentStorage`

### `..>`
- 의미가 같은 표기가 여러 문맥에서 쓰일 수 있음
- 이 프로젝트에서는 주로 "의존" 또는 "간접 연결" 정도로 보면 된다.
- component 그림에서는 약한 의존
- class 그림에서는 deferred mutation path 같은 설명성 연결

---

## 3. 이 프로젝트에서 특히 중요한 관계 해석

### `EngineWorld --> EcsCore`
- 의미: `EngineWorld`가 `EcsCore`로 위임한다.
- 읽는 법: 상위 계층은 raw ECS를 직접 만지지 않고 `EngineWorld`를 통해 접근한다.

### `WorldCommands --> CommandBuffer`
- 의미: 변경 요청은 바로 적용되지 않고 먼저 버퍼에 쌓인다.
- 읽는 법: 시스템 실행 중 구조 변경을 늦춰서 안전하게 적용한다.

### `SystemScheduler --> CommandBuffer`
- 의미: scheduler가 적절한 phase 경계에서 flush를 관리한다.
- 읽는 법: "언제 명령을 실제 반영할지"는 시스템이 아니라 런타임이 결정한다.

### `PackedComponentStorage ..|> IComponentStorage`
- 의미: 타입별 저장소가 공통 storage 인터페이스를 만족한다.
- 읽는 법: `ComponentRegistry`는 구체 타입이 달라도 같은 방식으로 정리 작업을 호출할 수 있다.

### `EngineRuntime *-- ...`
- 의미: 런타임이 프레임 시계, 스케줄러, 커맨드 버퍼, RNG 같은 핵심 실행 요소를 직접 소유한다.
- 읽는 법: 이 요소들은 런타임의 내부 구성품에 가깝다.

---

## 4. 글자와 표기 읽는 팁

### 메서드 앞의 `+`
- 의미: public 멤버를 뜻한다.
- 예시: `+initialize()`, `+query()`

### 박스 안의 여러 줄 텍스트
- 의미: 핵심 책임이나 대표 구성 요소를 요약한 것이다.
- 예시:
  - `Runtime Core (FrameClock / SystemScheduler / CommandBuffer / ResourceStore / DeterministicRng)`
- 읽는 법: "이 박스는 이 요소들을 포함하는 역할 묶음"으로 이해하면 된다.

### 선 옆의 텍스트
- 의미: 관계의 성격을 보충 설명한다.
- 예시:
  - `delegates`
  - `uses facade`
  - `enqueues deferred mutations`
  - `flushes at phase boundaries`
- 읽는 법: 단순 참조인지, 위임인지, 실행 순서 제어인지 구체적으로 알려주는 힌트다.

---

## 5. 이 프로젝트 UML을 읽는 순서 추천

### 1단계: 계층부터 본다
- `project-structure.puml`
- `engine-overview.puml`

먼저 여기서
- `application -> domain -> engine`
- `engine` 안에서 `api / runtime / ecs`가 어떻게 나뉘는지
를 본다.

### 2단계: 런타임 규칙을 본다
- `engine-runtime-core.puml`

여기서
- 시스템이 어떤 인터페이스로 실행되는지
- 명령이 언제 반영되는지
- 리소스와 RNG가 어디에 있는지
를 본다.

### 3단계: 저장 구조를 본다
- `engine-ecs-core.puml`

여기서
- entity 생명주기
- component storage 구조
- query/command가 ECS와 어떻게 연결되는지
를 본다.

---

## 6. 자주 헷갈리는 부분

### 점선이면 항상 약한 관계인가?
- 대체로 그렇지만, 정확히는 "구현보다 의존/계약/설명성 연결에 가깝다"로 보는 편이 좋다.
- 이 프로젝트에서는 점선을 주로 직접 소유가 아닌 관계에 사용한다.

### 채워진 마름모와 비워진 마름모 차이는?
- `*--` 채워진 마름모: 더 강한 소유, 내부 구성품에 가까움
- `o--` 비워진 마름모: 참조하거나 묶고 있지만 더 느슨함

### 화살표 방향은 어떻게 읽나?
- 보통 왼쪽에서 오른쪽으로 읽는다.
- `A --> B`면 "A가 B를 사용/참조/호출한다"로 읽으면 대부분 맞다.

### 노트는 필수로 봐야 하나?
- 그렇다.
- 이 프로젝트 UML에서는 노트에 실제 구현 규칙이 많이 들어 있다.
- 예를 들어 deferred mutation, reusable engine, query cache 후순위 같은 정보는 노트를 봐야 정확히 이해된다.

---

## 7. 한 줄 요약

이 프로젝트 UML은 다음 기준으로 읽으면 된다.
- 박스: 책임 단위
- `C / I / E`: class / interface / enum
- 실선 화살표: 직접 사용/연결
- 점선 화살표: 약한 의존/설명성 연결
- `*--`: 강한 소유
- `o--`: 느슨한 소유
- `..|>`: 인터페이스 구현
- 노트: 설계 의도와 구현 규칙
