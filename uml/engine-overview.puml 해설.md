# SafeCrowd UML 설계 해설 - engine-overview.puml

대상 파일: `uml/engine-overview.puml`

## 문서 목적
이 문서는 엔진 레이어 개요 UML에서 보이는 상위 요소들을 설명한다. 이 그림은 엔진 코어를 구현하기 전에 `application`, `domain`, `engine`의 접점을 간단하게 정리한 그림이다.

세부 클래스 구조는 다른 문서에서 다루고, 여기서는 **엔진이 어떤 면을 외부에 노출하는지**를 중심으로 본다.

---

## `safecrowd_app (Qt App)`
- 개요: Qt 기반 사용자 애플리케이션이다.
- 목적: 사용자 입력과 결과 시각화를 담당하고, 도메인 계층을 통해 엔진 사용을 시작한다.
- 유의사항: 엔진 API에 직접 닿는 경우는 viewport bridge 같은 제한된 경로로만 두는 편이 좋다.
- 후속 개선 사항: 실행 상태 모니터링 패널, 프로파일링 뷰, 디버그 오버레이를 추가할 수 있다.

## `safecrowd_domain (Scenario / Simulation / Risk)`
- 개요: 시나리오, 시뮬레이션 규칙, 위험 계산을 포함하는 도메인 레이어다.
- 목적: 엔진 위에서 SafeCrowd 고유의 문제 영역을 구현하는 계층으로 작동한다.
- 유의사항: 도메인은 엔진 API를 사용하되, 엔진 내부 storage 구조에 직접 의존하지 않는 편이 좋다.
- 후속 개선 사항: 시나리오 배치 실행, 분석 파이프라인, 결과 해석 모델을 확장할 수 있다.

## `Engine API Surface`
- 개요: `EngineRuntime`, `EngineWorld`, `WorldQuery`, `WorldResources`, `WorldCommands`, `EngineSystem`을 묶은 외부 노출 면이다.
- 목적: 엔진 외부에서는 이 API만 알면 실행, 조회, 변경 요청을 할 수 있도록 만들기 위해 필요하다.
- 유의사항: 외부 API는 작고 안정적으로 유지해야 하며, 내부 최적화 때문에 자주 바뀌면 상위 계층 결합이 커진다.
- 후속 개선 사항: 읽기/쓰기 분리 facade, 진단용 API, registration helper를 추가할 수 있다.

## `Runtime Core`
- 개요: 시간, 스케줄링, command flush, resource, RNG를 담당하는 엔진 실행 코어다.
- 목적: 시스템이 어떤 순서와 주기로 실행되는지, 변경 요청이 언제 반영되는지를 공통 규칙으로 제공한다.
- 유의사항: 초기 구현에서는 이벤트 시스템이나 스냅샷보다 fixed-step과 phase 경계를 먼저 안정화하는 편이 좋다.
- 후속 개선 사항: 병렬 phase 실행, 성능 계측, 이벤트 기반 트리거, replay 지원을 나중에 검토할 수 있다.

## `ECS Core`
- 개요: 엔티티와 컴포넌트의 밀집 저장과 signature 기반 매칭을 담당하는 저장 코어다.
- 목적: 캐시 친화적인 순회와 단순한 데이터 배치를 통해 엔진의 기본 성능을 확보한다.
- 유의사항: 이 레이어는 범용적이어야 하므로, crowd 전용 타입이나 규칙을 담는 장소가 아니다.
- 후속 개선 사항: query index, archetype 유사 구조, 디버그 검증 도구를 붙일 수 있다.
