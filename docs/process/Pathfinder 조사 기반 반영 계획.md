# Pathfinder 조사 기반 반영 계획

## 목적
- [Pathfinder 2026.1 군중 대피 시뮬레이션 기능 조사](../references/Pathfinder%202026.1%20군중%20대피%20시뮬레이션%20기능%20조사.md)의 `7. SafeCrowd 우선순위로 다시 정리`를 기준으로 SafeCrowd 산출물 반영 계획을 정리한다.
- 이 문서는 계획 문서다. 여기서 다루는 backlog, UML, issue/project 반영은 별도 작업으로 수행한다.

## 기준 요약
- `MVP 코어`: 공간 계층, 커넥터, 프로필 제약, 출구 선택 비용, 행동/트리거, 동적 유입, 기본 결과 아티팩트, 핵심 결과 레이어
- `1차 확장`: 그룹 이동, queues/targets, generic results viewer, Monte Carlo 고도화, 근접도 분석, 기본 시야/길찾기 저하 반영
- `중기 확장`: elevators, assisted evacuation, smoke/FED/FDS 연동, 고급 노출 후처리
- `연구 후보`: 압사, 낙상, 역방향/교차류의 독립 모델, 추천 최적화 로직
- 우선순위 원칙: Pathfinder처럼 기능 폭을 바로 넓히기보다, 운영 의사결정에 바로 연결되는 최소 코어를 먼저 고정한다.

## 작업 흐름
1. backlog를 현재 우선순위에 맞게 재배열한다.
2. backlog에서 합의된 코어를 기준으로 UML을 보강한다.
3. UML과 backlog를 기준으로 GitHub issue와 Project 항목을 추가하고 연결한다.
4. 실제 구현 이슈는 docs/analysis 이슈와 분리해서 발행한다.

## 위험 정의 문서 통합 방침
- 제품 요구와 사용자 노출 위험 체계는 [위험 정의.md](../product/%EC%9C%84%ED%97%98%20%EC%A0%95%EC%9D%98.md)를 우선 기준으로 삼는다.
- 상세 물리 모델과 연구 근거는 [고급 위험 모델.md](../product/%EA%B3%A0%EA%B8%89%20%EC%9C%84%ED%97%98%20%EB%AA%A8%EB%8D%B8.md)에 둔다.
- 단, [고급 위험 모델.md](../product/%EA%B3%A0%EA%B8%89%20%EC%9C%84%ED%97%98%20%EB%AA%A8%EB%8D%B8.md)는 `Pathfinder 정합 확장`과 `연구 후보`를 함께 담는 참고 문서이며, 두 범주를 같은 우선순위로 취급하지 않는다.
- backlog, UML, issue 제목/본문에서는 `병목/정체/탈출 지연`, `시야 제한 및 길찾기 저하`, `근접도 및 압박 전조`, `운영 갈등`의 4축과 `FacilityLayout/PopulationSpec/EnvironmentState/ControlPlan/ExecutionConfig` 입력 계약을 우선 용어로 사용한다.
- 단, `추천 기능` 자체는 범위에서 빼지 않는다. 별도 문서로 분리하는 것은 추천 엔진의 상세 규칙과 최적화 로직이다.
- 압사, 낙상, 역방향/교차류 독립 모델은 별도 설계 문서가 생기기 전까지 UML/issue 기본 범위에 올리지 않는다.

## 1. Product Backlog 반영 계획

### 목표
- 기존 [Product Backlog.md](../product/Product%20Backlog.md)의 `E1~E7` 구조는 유지한다.
- 단, User Story의 범위, 우선순위, Sprint 배치를 Pathfinder 조사 결과와 맞춘다.

### 반영 원칙
- `Must`는 `7.1. MVP 코어`에 직접 대응하는 기능만 둔다.
- `1차 확장`은 `Should` 또는 후속 Epic 후보로 내린다.
- `중기 확장`은 현재 backlog 본문에 넣더라도 `Later` 성격이 드러나게 분리한다.
- `연구 후보`는 backlog를 당장 수정하지 않더라도, UML/issue 계획에서는 기본 구현 범위처럼 다루지 않는다.
- 추천 기능 `E6`는 유지하되, `E4/E5` 결과 파이프라인이 선행된다는 의존성을 더 명확히 적는다.
- 위험/결과 용어는 가능하면 [위험 정의.md](../product/%EC%9C%84%ED%97%98%20%EC%A0%95%EC%9D%98.md)의 4축과 입력 계약에 맞춘다.

### backlog 보강 대상
| 구분 | 현재 backlog 위치 | 반영 방향 |
| --- | --- | --- |
| 공간 토폴로지 정규화 | `E1` | `Floor/Room/Wall/Door/Obstruction/Obstacle` 구분을 story 수준으로 명시 |
| 커넥터 모델 | `E1`, `E3` | `stairs/ramp/escalator/walkway` 공통 connector + modifier 구조를 acceptance 기준에 반영 |
| 통제 가능 구역 | `E1`, `E2` | `ControlZone`을 named room/door/connectors group으로 두고 구역 통제 구조를 acceptance/UML에 반영 |
| 프로필 제약과 분포 파라미터 | `E2`, `E3` | usable connector restriction, 속도/가속/간격 분포, seed 재현성 보강 |
| 출구 선택 비용 | `E2`, `E3` | 단순 nearest exit가 아니라 이동 시간/대기/잔여 경로 비용 구조를 명시 |
| 행동/트리거/태그 | `E2` | 운영 이벤트를 단순 on/off가 아니라 행동 전환 모델로 구체화 |
| 동적 인원 유입 | `E3` | 초기 배치 외에 source 기반 유입 story 추가 또는 `US-07` 확장 |
| 결과 아티팩트 | `E4`, `E5`, `E7` | `door history`, `room history`, `measurement regions`, persisted `ScenarioComparison`, `cumulative JSON`, 선택적 `occupant history` 명시 |
| 반복 실행 | `E3`, `E5` | run/variation 구조와 Monte Carlo 요약 구분 보강 |
| results viewer | `E5`, `E7` | 1차 확장 범위로 별도 story 후보 분리 |

### Epic별 적용 방향
- `E1 시설 레이아웃 입력 및 검증`
  - 구조 입력 후 결과를 단순 `구역/출구/장애물`이 아니라 토폴로지 계층으로 확정하는 흐름을 강화한다.
  - 수동 보정 기준도 `room-door connectivity`, `정적 obstruction`, `동적 obstacle` 분리 관점으로 다시 쓴다.
- `E2 시나리오 구성 및 운영 계획`
  - 운영 이벤트를 `행동 시퀀스 + 트리거 + occupant tag` 관점으로 확장한다.
  - 대안 시나리오 차이 기록에 `경로 비용`, `행동 전환`, `유입률`이 포함되도록 정리한다.
- `E3 시뮬레이션 실행 및 재현 제어`
  - 인원 배치를 `초기 배치 + 동적 source + variation` 구조로 확장한다.
  - 재현성은 반복 실행 수뿐 아니라 seed, 샘플링 규칙, variation 폴더 구조까지 포함해 적는다.
- `E4 위험 탐지 및 안전 지표`
  - 위험 축은 `병목/정체/탈출 지연`, `시야 제한 및 길찾기 저하`, `근접도 및 압박 전조`, `운영 갈등`의 4축으로 다시 정리한다.
  - 핵심 지표는 `Density`, `Congestion`, `Time to Exit`, `Travel Distance Remaining`과 직접 연결되도록 맞춘다.
  - `근접도 및 압박 전조`는 `MVP hotspot`과 `1차 확장 정량화`를 나눠 적고, `시야/FED`는 기본 visibility와 smoke/FED 연동을 구분한다.
- `E5 결과 시각화 및 비교 분석`
  - 기본 heatmap과 핵심 요약은 `MVP 코어`로 유지한다.
  - generic results viewer, 2D plot, 재내보내기는 별도 확장 story로 분리한다.
  - `Visibility/FED`는 단일 story로 뭉치지 말고 `기본 시야 저하 결과`와 `환경 연동 결과`로 나누는 편이 현재 위험 정의 문서와 더 잘 맞는다.
- `E6 운영 대안 추천`
  - 추천은 유지하되, 입력 근거가 되는 결과 아티팩트가 먼저 안정화되어야 한다는 점을 명시한다.
  - 위험 정의 문서에는 추천 엔진의 상세 규칙을 넣지 않고, 추천 근거와 workflow는 backlog/설계/issue에서 따로 다룬다.
- `E7 프로젝트 저장 및 보고`
  - `scenario/run/variation` 저장 단위와 `cumulative JSON` 또는 canonical run artifact 개념을 보강한다.

### Sprint 재배치 초안
- `Sprint 1`
  - 공간 토폴로지, 커넥터, 기본 실행, 행동/트리거, 동적 유입, 기본 결과 아티팩트
- `Sprint 2`
  - 비교 분석, heatmap, 결과 요약, variation 반복 실행, 기본 저장/내보내기
- `Sprint 3`
  - 추천 기능, 1차 확장 일부, 기본 시야/길찾기 저하, Pathfinder 정합 고급 후처리
- `Later`
  - elevators, assisted evacuation, smoke/FED/FDS, 정교한 노출 후처리
- `별도 설계 이후`
  - 압사, 낙상, 역방향/교차류 독립 모델, 추천 최적화 로직

## 2. UML 설계 계획

### 목표
- 현재 `uml/`의 레이어 구조 UML은 유지한다.
- 단, `MVP 코어`에 필요한 도메인 모델, 제어 모델, 결과 아티팩트 모델이 빠져 있으므로 별도 도면을 추가한다.

### 재사용할 기존 UML
- [project-structure.puml](../../uml/project-structure.puml)
- [engine-overview.puml](../../uml/engine-overview.puml)
- [domain-import-module.puml](../../uml/domain-import-module.puml)

### 추가할 UML 후보
| 파일 | 목적 | 중심 레이어 |
| --- | --- | --- |
| `uml/domain-scenario-model.puml` | `FacilityLayout`, `ControlZone`, `Scenario`, `Variation`, `ExecutionConfig`, `PopulationSpec` 관계 고정 | `domain` |
| `uml/domain-control-model.puml` | `Behavior`, `Trigger`, `OccupantTag`, `OperationalEvent`, `RouteChoicePolicy`, zone-targeted control 관계 정의 | `domain` |
| `uml/engine-routing-and-connectors.puml` | room/door/connectors, `ControlZone`, path cost, dynamic obstacle, flow measurement 구조 정리 | `engine + domain boundary` |
| `uml/domain-result-artifacts.puml` | `RunResult`, `DoorHistory`, `RoomHistory`, `MeasurementRegionSeries`, `OccupantHistory`, `ScenarioComparison`, `CumulativeArtifact` 정리 | `domain` |

### 도면 작성 순서
1. `domain-scenario-model`
2. `domain-control-model`
3. `engine-routing-and-connectors`
4. `domain-result-artifacts`

### 도면별 핵심 질문
- `domain-scenario-model`
  - 시나리오와 variation을 어디서 분리할 것인가
  - 실행 설정과 공간/인구 설정을 어떤 객체 경계로 나눌 것인가
- `domain-control-model`
  - 운영 이벤트를 단순 플래그로 둘지, 행동 전환 객체로 둘지
  - 태그, 트리거, route choice policy를 어떤 관계로 연결할 것인가
- `engine-routing-and-connectors`
  - 정적 obstruction과 동적 obstacle을 어떻게 분리할 것인가
  - connector 공통 모델에 어떤 modifier를 둘 것인가
- `domain-result-artifacts`
  - run 단위, variation 단위, comparison 단위, cumulative 단위를 어떻게 분리할 것인가
  - 상세 occupant history를 항상 둘지 선택적으로 둘지
- `application-run-results-workflow`
  - 작성/실행과 재생/분석을 같은 화면에 둘지 분리할지
  - comparison, export, recommendation이 persisted artifact를 어떻게 읽을지

### UML 산출 규칙
- 새 `.puml` 파일마다 대응 `해설.md`를 같이 만든다.
- 구조 결정이 확정되면 `docs/architecture/프로젝트 구조.md`에 핵심 용어를 반영한다.
- 레이어 경계를 넘는 책임이 생기면 먼저 UML에서 책임을 재조정한 뒤 구현 이슈를 연다.

## 3. Issue 발행 및 Project 셋팅 계획

### 목표
- 조사 메모를 바로 구현으로 넘기지 않고, `문서 정렬 -> 설계 정렬 -> 구현 작업`의 순서로 issue를 분리한다.
- Epic/Task 관계와 Project 필드를 통해 우선순위를 눈에 보이게 관리한다.

### 발행 순서
1. `Docs/Analysis` 성격의 정렬 작업 발행
2. `MVP 코어` 구현 Epic 발행
3. Epic 아래에 영역별 Implementation Task 발행
4. `1차 확장`과 `중기 확장`은 `Later` 중심으로 분리 발행

### 먼저 만들 Lightweight Task 후보
- `Pathfinder 조사 기준으로 Product Backlog 재정렬`
- `Pathfinder 조사 기준으로 UML 보강 계획 확정`
- 필요하면 `Pathfinder 조사 기준으로 GitHub Project 운영 규칙 보강`

### 만들 Epic 후보
- `MVP 코어 구조 정렬`
  - 공간 토폴로지와 connector 모델
  - 프로필/행동/트리거 제어 모델
  - 실행/variation/result artifact 구조
- `1차 확장 기능`
  - group/queue/target/results viewer/Monte Carlo 고도화
- `중기 확장 기능`
  - elevator/assisted evacuation/vision/FED/FDS
- `연구 후보 검토`
  - 압사/낙상/역방향/교차류/추천 최적화는 실제 구현 Epic이 아니라 별도 설계 검토 task로 먼저 분리

### Epic 아래 Implementation Task 예시
- `시설 토폴로지 모델 정리`
- `커넥터 및 경로 비용 모델 정리`
- `프로필 제약 및 행동/트리거 모델 정리`
- `동적 인원 유입과 variation 실행 구조 정리`
- `결과 아티팩트 및 요약 구조 정리`
- `기본 heatmap/비교 분석 구조 정리`

### Parent/Sub-issue 구성 원칙
- 문서 정렬 task는 구현 Epic의 부모가 아니라 독립 `Lightweight Task`로 둔다.
- 구현 task는 반드시 Epic 아래 `sub-issue`로 연결한다.
- `blocked by`는 hard dependency에만 사용한다.
- 한 task가 `Engine + Domain + Application`을 동시에 크게 건드리면 분할을 먼저 검토한다.

### Project 셋팅 계획
- 현재 Project 필드 기준으로 최소 `Title`, `Status`, `Sprint`, `Area`, `Parent issue`, `Sub-issues progress`를 유지한다.
- 가능하면 `Status`에 backlog 상태를 구분할 수 있는 값이 필요하다.
  - 예: `Todo` 또는 `Planned`
- 가능하면 view를 다음처럼 나눈다.
  - `Backlog`
  - `Sprint 1`
  - `Sprint 2`
  - `Later`
  - `Docs/Analysis`
- 구현 Epic은 `Sprint`와 `Area`를 먼저 채우고, 하위 task는 `Parent issue` 연결 후 `blocked by`를 최소한으로 건다.

### Sprint 배치 초안
| 분류 | Sprint |
| --- | --- |
| docs/backlog 정렬 | `Later` 또는 `Docs/Analysis` 뷰 |
| UML 보강 | `Later` 또는 `Docs/Analysis` 뷰 |
| MVP 코어 implementation | `Sprint 1`, `Sprint 2` |
| 1차 확장 | `Sprint 3` 또는 `Later` |
| 중기 확장 | `Later` |
| 연구 후보 설계 | `Docs/Analysis` 또는 별도 `Later` |

## 실행 체크리스트
- [ ] `Product Backlog.md` 반영 범위를 Epic/User Story 수준으로 합의
- [ ] 추가할 UML 파일 목록과 작성 순서 합의
- [ ] issue를 새 Epic으로 열지, 기존 Epic에 편입할지 결정
- [ ] Project `Status`와 view 구성을 backlog 운영에 맞게 점검
- [ ] docs 정렬 이슈와 implementation 이슈를 분리해 발행

## 참고 문서
- [Pathfinder 2026.1 군중 대피 시뮬레이션 기능 조사](../references/Pathfinder%202026.1%20군중%20대피%20시뮬레이션%20기능%20조사.md)
- [Product Backlog.md](../product/Product%20Backlog.md)
- [GitHub Project.md](GitHub%20Project.md)
- [프로젝트 구조.md](../architecture/%ED%94%84%EB%A1%9C%EC%A0%9D%ED%8A%B8%20%EA%B5%AC%EC%A1%B0.md)
