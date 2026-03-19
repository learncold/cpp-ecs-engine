# SafeCrowd UML 설계 해설 - engine-runtime-core.puml

대상 파일: `uml/engine-runtime-core.puml`

## 문서 목적
이 문서는 엔진 런타임 코어 UML에 등장하는 각 요소를 설명한다. 이 그림은 엔진의 실행 계약, 스케줄링, 지연 변경, 리소스 접근, 재현성 같은 **런타임 규칙**을 정의하는 데 목적이 있다.

이 문서에서의 설명은 초기 엔진 구현 기준이다. 따라서 모든 요소는 "지금 반드시 구현할 것"과 "나중에 확장할 것"을 구분해서 읽는 것이 좋다.

---

## `EngineRuntime`
- 개요: 엔진 실행의 진입점이자 수명주기 관리자다.
- 목적: 초기화, 재생, 일시정지, 정지, 단일 프레임 스텝을 통합된 방식으로 제공한다.
- 유의사항: 직접 domain 규칙을 넣지 말고 orchestration 역할에 집중해야 한다.
- 후속 개선 사항: 상태 전이 로그, pause reason, 디버그 제어 기능을 추가할 수 있다.

## `EngineWorld`
- 개요: query, resources, commands를 묶은 월드 파사드다.
- 목적: 외부 계층이 raw storage를 직접 만지지 않고도 월드에 접근할 수 있게 한다.
- 유의사항: 단순 편의 객체가 아니라 엔진 접근 규칙을 강제하는 경계로 다뤄야 한다.
- 후속 개선 사항: 읽기/쓰기 권한 분리 facade로 확장할 수 있다.

## `WorldQuery`
- 개요: 엔티티와 컴포넌트 읽기용 조회 면이다.
- 목적: 시스템이 필요한 컴포넌트 집합을 안전하게 순회하고 조회하게 한다.
- 유의사항: 초기 구현은 전체 live entity 순회로 시작해도 되지만, query 중 mutation을 허용하면 안 된다.
- 후속 개선 사항: query cache, signature index, iterator 최적화를 추가할 수 있다.

## `WorldResources`
- 개요: 전역 리소스를 읽고 쓰는 접근 면이다.
- 목적: 필드, 설정, 통계, 공유 컨텍스트를 컴포넌트와 구분해서 저장하게 한다.
- 유의사항: 모든 전역 상태를 무분별하게 resource로 밀어 넣으면 결합도가 커진다.
- 후속 개선 사항: 읽기 전용 resource view, immutable resource 구분, lazy initialization 지원을 추가할 수 있다.

## `WorldCommands`
- 개요: 엔티티 생성/삭제와 컴포넌트 변경 요청을 기록하는 변경 요청 면이다.
- 목적: 시스템이 실행 중 구조 변경을 즉시 적용하지 않고 지연 반영하게 만든다.
- 유의사항: `WorldCommands`는 enqueue 전용으로 유지하고, 즉시 flush API를 외부에 쉽게 열지 않는 편이 좋다.
- 후속 개선 사항: batch command, phase-local command channel, 명령 실패 진단 정보를 붙일 수 있다.

## `EngineSystem`
- 개요: 엔진에 등록되어 스케줄러가 호출하는 시스템 인터페이스다.
- 목적: 모든 시스템이 동일한 입력 계약과 실행 모델을 따르도록 한다.
- 유의사항: 전역 싱글턴 접근 대신 `world`와 `stepContext`를 인자로 받는 구조를 유지하는 것이 좋다.
- 후속 개선 사항: lifecycle hook, condition predicate, profiling label을 붙일 수 있다.

## `SystemDescriptor`
- 개요: 시스템의 phase, 순서, 간격, 의존성 제약을 설명하는 메타데이터다.
- 목적: 시스템 실행 순서를 코드 바깥의 선언 정보로 표현하기 위해 필요하다.
- 유의사항: descriptor가 너무 많은 실행 정책을 한꺼번에 품기 시작하면 scheduler가 과도하게 복잡해진다.
- 후속 개선 사항: conflict validation, category grouping, debug print를 추가할 수 있다.

## `EngineConfig`
- 개요: 고정 timestep, catch-up 제한, base seed 같은 런타임 설정 값 모음이다.
- 목적: 엔진 실행 정책을 코드와 분리된 명시적 설정으로 제공한다.
- 유의사항: 실행 중 자주 바뀌는 상태와 초기 설정 값을 섞지 않는 편이 좋다.
- 후속 개선 사항: phase별 예산, profiling 옵션, deterministic strict mode를 추가할 수 있다.

## `EngineStats`
- 개요: 현재 실행 상태와 누적 통계를 나타내는 관찰용 데이터다.
- 목적: UI, 로그, 디버그 도구가 런타임 상태를 읽을 수 있게 한다.
- 유의사항: 통계 수집 비용이 런타임 핵심 경로를 과도하게 방해하지 않게 해야 한다.
- 후속 개선 사항: 시스템별 실행 시간, flush 비용, query 비용 같은 계측치를 확장할 수 있다.

## `EngineStepContext`
- 개요: 현재 프레임/스텝의 문맥 정보를 담는 실행 컨텍스트다.
- 목적: 시스템이 현재 프레임 인덱스, fixed step 인덱스, alpha, run 정보를 명시적으로 받을 수 있게 한다.
- 유의사항: 컨텍스트는 읽기 전용 메타데이터로 유지하는 편이 좋다.
- 후속 개선 사항: phase 정보, delta time 파생값, debug flag를 추가할 수 있다.

## `RunContext`
- 개요: 실행 단위의 run index와 seed 파생 정보를 담는 컨텍스트다.
- 목적: 반복 실행과 재현성 관리에 필요한 실행 단위 식별자를 제공한다.
- 유의사항: 프레임 단위 정보와 실행 단위 정보를 섞지 말고 책임을 분리하는 편이 좋다.
- 후속 개선 사항: batch id, scenario id, replay token 같은 상위 실행 메타데이터를 연결할 수 있다.

## `UpdatePhase`
- 개요: 시스템이 어느 단계에서 실행되는지 나타내는 phase 열거형이다.
- 목적: startup, simulation, render sync를 구분해 안정적인 실행 순서를 만들기 위해 필요하다.
- 유의사항: phase 수를 너무 세분하면 이해는 어려워지고 운영 비용만 커질 수 있다.
- 후속 개선 사항: 필요시 shutdown phase, diagnostics phase 같은 보조 단계를 검토할 수 있다.

## `TriggerPolicy`
- 개요: 시스템이 매 프레임, fixed step, interval 중 어떤 정책으로 실행되는지 나타내는 열거형이다.
- 목적: 모든 시스템을 같은 주기로 돌리지 않고 비용과 의미에 맞게 실행하기 위해 필요하다.
- 유의사항: 초기 버전은 `EveryFrame`, `FixedStep`, `Interval`까지만 유지하는 편이 단순하다.
- 후속 개선 사항: 실제 필요가 생기면 `EventDriven`이나 condition-based trigger를 다시 도입할 수 있다.

## `IRenderBridge`
- 개요: 엔진과 렌더링/뷰포트 계층 사이의 좁은 동기화 인터페이스다.
- 목적: 엔진이 UI 기술을 직접 모르면서도 렌더 동기화 지점을 제공하게 한다.
- 유의사항: Qt 타입이나 렌더러 구현 세부를 이 인터페이스에 직접 새기지 않는 편이 좋다.
- 후속 개선 사항: snapshot 기반 동기화, 더블 버퍼링, 렌더 진단 정보를 추가할 수 있다.

## `FrameClock`
- 개요: 고정 timestep 누적과 catch-up step 계산을 담당하는 시간 관리 요소다.
- 목적: variable frame rate 환경에서도 deterministic한 fixed-step simulation을 유지하게 한다.
- 유의사항: 과도한 catch-up은 프레임 폭주를 부를 수 있으므로 `maxCatchUpSteps` 같은 제한이 중요하다.
- 후속 개선 사항: time scaling, pause-aware time source, drift diagnostics를 추가할 수 있다.

## `SystemScheduler`
- 개요: 시스템 등록과 phase별 실행을 담당하는 스케줄러다.
- 목적: 어떤 시스템이 언제 어떤 순서로 실행되는지 중앙에서 관리한다.
- 유의사항: scheduler는 직접 도메인 규칙을 품는 객체가 아니라 실행 순서를 보장하는 범용 도구여야 한다.
- 후속 개선 사항: descriptor validation, parallel phase, system profiling을 확장할 수 있다.

## `ResourceStore`
- 개요: typed resource를 저장하는 런타임 저장소다.
- 목적: 밀도 필드, 가시성 필드, 전역 설정 같은 공유 상태를 컴포넌트와 분리해 관리한다.
- 유의사항: resource lifetime과 ownership을 명확히 하지 않으면 암묵적 결합이 커질 수 있다.
- 후속 개선 사항: mutable/immutable 분리, debug dump, resource versioning을 붙일 수 있다.

## `CommandBuffer`
- 개요: 지연 반영할 변경 명령을 모아 두는 버퍼다.
- 목적: query 순회 중 구조 변경을 피하고, phase 경계에서 안전하게 mutation을 적용하게 한다.
- 유의사항: 명령 적용 순서와 중복 명령 해석 규칙을 초기부터 명확히 정해두는 편이 좋다.
- 후속 개선 사항: 명령 압축, phase별 버퍼 분리, 실패 보고를 추가할 수 있다.

## `DeterministicRng`
- 개요: seed 기반 반복 가능한 난수 스트림 제공자다.
- 목적: 같은 시나리오와 설정이면 같은 실행을 재현할 수 있게 한다.
- 유의사항: 전역 랜덤 사용을 허용하면 재현성이 쉽게 깨지므로, 스트림 진입점을 통일하는 편이 좋다.
- 후속 개선 사항: system별 stream 분리, jump-ahead, 통계 검증 도구를 추가할 수 있다.

## `EcsCore`
- 개요: 런타임 코어가 읽고 쓰는 ECS 저장 중심부다.
- 목적: 시스템 실행 결과가 최종적으로 반영되는 데이터 저장 위치로 작동한다.
- 유의사항: 런타임 문서에서는 dependency endpoint로만 보이고, 실제 저장 구조 설명은 ECS 코어 문서에서 더 자세히 다뤄야 한다.
- 후속 개선 사항: query index, archetype 유사 구조, 검증 도구를 나중에 붙일 수 있다.
