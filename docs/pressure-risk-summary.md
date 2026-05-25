# SafeCrowd 압력 위험 기능 구현 정리

## 개요
이번 압력 위험 기능은 모든 시나리오에서 공통으로 사용할 수 있는 군중 위험 지표이다.
구현은 `engine`은 건드리지 않고 `domain + application` 레이어에서만 진행했다.

관련 핵심 파일:
- `C:\graduation_project\src\domain\ScenarioRiskMetrics.h`
- `C:\graduation_project\src\domain\ScenarioRiskMetricsSystem.cpp`
- `C:\graduation_project\src\domain\ScenarioResultArtifacts.h`
- `C:\graduation_project\src\domain\ScenarioSimulationSystems.cpp`
- `C:\graduation_project\src\application\SimulationCanvasWidget.cpp`
- `C:\graduation_project\src\application\ScenarioResultWidget.cpp`
- `C:\graduation_project\src\application\ScenarioBatchResultWidget.cpp`
- `C:\graduation_project\tests\ScenarioSimulationSystemsTests.cpp`

## 구현한 범위
이번 버전에서 실제로 구현한 내용은 아래와 같다.

- `pressure hotspot` 계산
- `agent compression exposure` 누적
- `critical pressure event` 판정
- 결과 아티팩트에 `pressure summary` 저장
- 결과 화면과 비교 화면에 `pressure` 표시
- 맵 overlay에 `Pressure` 추가

이번 버전에서 넣지 않은 내용은 아래와 같다.

- pressure가 agent movement를 다시 바꾸는 피드백
- 넘어짐, 도미노 붕괴, 군중 전도 전파
- fire/smoke와의 결합 로직
- 프로젝트 저장 후 재오픈 시 pressure summary 복원

## domain 모델
`ScenarioRiskMetrics.h`에 pressure 관련 타입을 추가했습니다.

추가된 주요 타입:
- `ScenarioPressureHotspot`
- `ScenarioPressureAgentMetric`
- `ScenarioCriticalPressureEvent`

`ScenarioRiskSnapshot`에는 아래 필드가 추가됐습니다.

- `pressureExposedAgentCount`
- `criticalPressureAgentCount`
- `pressureHotspots`
- `pressureAgents`
- `criticalPressureEvents`

즉, 기존 risk snapshot이 이제 매 프레임마다 혼잡뿐 아니라 압력 위험까지 같이 들고 있게 된다.

## 판정 기준
현재 상수는 아래처럼 고정되어 있다.

- hotspot 셀 크기: `1.5 m`
- pressure hotspot 최소 score: `1.0`
- critical compression force threshold: `1.0`
- critical exposure threshold: `2.0 sec`
- critical event 최소 critical agent 수: `2`
- critical event 최소 지속시간: `1.0 sec`

개인공간 intrusion 계산에는 기존 시뮬레이션 내부 상수인 `kPersonalSpaceBuffer = 0.08`을 재사용했다.

## pressure hotspot 계산 방식
구현 위치:
- `ScenarioRiskMetricsSystem.cpp`
- `ScenarioSimulationSystems.cpp`

핵심 아이디어:
- 기존 hotspot과 같은 `1.5m x 1.5m` 셀을 사용
- 셀 내부 모든 agent pair를 검사
- 두 사람 사이 거리가 `r1 + r2 + 0.08`보다 작으면 개인공간 침범으로 간주
- 침범 깊이를 `0.08m` 개인공간 버퍼 기준의 `0..1` pair score로 환산해 `pressureScore`에 합산

개념적으로는 아래와 같다.

```text
comfortDistance = agentRadiusA + agentRadiusB + 0.08
if distance < comfortDistance:
    pressureScore += clamp((comfortDistance - distance) / 0.08, 0, 1)
```

pressure hotspot은 다음 조건을 만족해야 보고됩니다.

- intruding pair 존재
- pressureScore >= 1.0

정렬 우선순위:
- `pressureScore`
- `intrudingPairCount`
- `agentCount`

## agent compression exposure 계산 방식
구현 위치:
- `ScenarioRiskMetricsSystem.cpp`

각 active agent에 대해 `compressionForce`를 계산한다.

구성:
- `agent-agent overlap`
- `barrier compression`

현재 force는 아래 두 값을 합친 proxy이다.

- 사람끼리 실제 반경이 겹친 양
- 벽/장애물에 agent 반경이 파고든 양

개념적으로는 아래와 같습니다.

```text
compressionForce = sum(pair overlap) + sum(barrier intrusion)
```

그 다음 exposure는 다음과 같이 누적된다.

- `currentForce >= 1.0`일 때만 `exposureSeconds += deltaSeconds`
- 감쇠는 아직 없음
- agent가 active인 동안 누적값 유지
- evacuate되면 tracking state 제거

현재 snapshot에서 `pressureExposedAgentCount`는 아래 중 하나면 증가한다.

- `exposureSeconds > 0`
- `currentForce >= 1.0`

`critical agent` 조건은 더 강하게 설정했다.

- `currentForce >= 1.0`
- `exposureSeconds >= 2.0`

한때 눌렸던 agent와 지금도 임계 수준으로 눌리는 agent를 구분한다.

## critical pressure event 판정
구현 위치:
- `ScenarioRiskMetricsSystem.cpp`

이벤트는 `cell-based`이다.

조건:
- 같은 셀 안에서 `critical agent >= 2`
- 그 상태가 `1.0초 이상 지속`

이벤트 추적 방식:
- 셀별로 active event state를 들고 감
- 조건을 처음 만족한 시점을 `startedAtSeconds`로 저장
- 유지되면 `durationSeconds`가 증가
- 조건이 깨지면 해당 셀 event state 제거

이벤트 pressure score는 그 셀의 critical/exposed agent들의 `currentForce` 합으로 보고 있습니다.

정렬 우선순위:
- `criticalAgentCount`
- `durationSeconds`
- `pressureScore`

## risk level과의 연결
`ScenarioRiskMetrics.cpp`와 `ScenarioRiskMetricsSystem.cpp`에 반영했습니다.

현재 규칙:
- `Medium`
  - pressure hotspot 1개 이상
  - 또는 critical pressure agent 1명 이상
- `High`
  - sustained critical pressure event 1개 이상

즉 pressure는 이제 completion risk 판단에 직접 들어갑니다.

## snapshot과 peak snapshot
매 프레임 결과는 `snapshot`에 저장되고, 런 전체 최고치는 `peakSnapshot`에 따로 누적됩니다.

보존하는 것:
- `pressureExposedAgentCount`
- `criticalPressureAgentCount`
- 최상위 `pressureHotspots`
- 최상위 `pressureAgents`
- 최상위 `criticalPressureEvents`

중요한 점:
- agent가 나중에 전원 대피해서 현재 snapshot이 비어도
- `peakSnapshot`은 유지됩니다

이건 결과 화면에서 한때 위험했는가를 보려는 목적입니다.

## result artifact 저장
`ScenarioResultArtifacts.h`와 `ScenarioSimulationSystems.cpp`에 `PressureSummary`를 추가했습니다.

저장되는 대표 값:
- `peakPressureScore`
- `peakAtSeconds`
- `peakCell`
- `peakExposedAgentCount`
- `peakCriticalAgentCount`
- `peakCells`
- `peakField`
- `peakHotspots`
- `peakAgents`
- `criticalEvents`

정리하면:
- `risk snapshot`은 현재 프레임 위험
- `pressureSummary`는 런 전체 결과 요약

제한:
- top list는 현재 `5개`까지만 저장
- full time-series는 저장하지 않음
- 대신 peak field, peak hotspot, peak event 중심으로 남김

## UI 반영
### 단일 결과 화면
구현 위치:
- `ScenarioResultWidget.cpp`

추가된 항목:
- `Peak Pressure`
- `Pressure Hotspots`
- `Pressure Events`
- `Critical Pressure`
- `Map overlay -> Pressure`

### batch 비교 화면
구현 위치:
- `ScenarioBatchResultWidget.cpp`

추가된 항목:
- `Pressure` 비교 탭
- 시나리오별 `Peak score`
- `Exposed / Critical`
- `Hotspots`
- `Events`
- `Peak at`

### 캔버스 overlay
구현 위치:
- `SimulationCanvasWidget.cpp`

현재 pressure overlay는:
- peak pressure field를 기반으로 그림
- yellow -> orange -> red 계열 gradient 사용
- density overlay와 분리된 별도 모드

중요:
- 결과 화면 overlay는 실시간 시점별 pressure field가 아니라 `peak field` 기준입니다.

## 테스트 커버리지
`ScenarioSimulationSystemsTests.cpp`에 아래 케이스를 추가했습니다.

검증한 것:
- 조밀한 군집에서는 pressure hotspot이 검출됨
- 같은 셀이어도 느슨한 군집이면 pressure hotspot이 검출되지 않음
- 같은 셀에서 density가 높아도 개인공간 침범이 없으면 pressure field는 비어 있음
- exposure가 시간에 따라 누적됨
- critical agent 판정이 정상 작동함
- sustained critical pressure event가 1초 후에만 생김
- 층이 다르면 hotspot/pressure가 섞이지 않음
- display floor 기준으로 pressure 셀이 잡힘
- agent 대피 후에도 peak pressure metrics가 남음
- result artifacts에 pressure summary가 정상 발행됨
- peak pressure field가 floor/cell 기준으로 누적됨

## 현재 한계
- pressure movement feedback은 시뮬레이션 중 agent 이동에 반영되지만, 결과 화면 overlay는 아직 peak field 기준입니다.
- fire/smoke 리소스를 읽지 않습니다. 완전히 독립입니다.
- threshold는 아직 하드코딩 상수입니다.
- project persistence까지는 아직 안 가서, 저장 후 다시 열었을 때 pressure summary 복원은 후속 작업입니다.
- result view overlay는 peak 기반이라 replay slider와 1:1로 동기화되는 시계열 pressure heatmap은 아닙니다.

## 결론
이번 구현으로 SafeCrowd는 이제 모든 시나리오에서 아래 항목을 일관되게 볼 수 있는 상태가 됐습니다.

- 압력 hotspot
- 개인 압축 노출
- 지속된 critical pressure event
- pressure 결과 요약
- pressure overlay 및 결과 UI 표시
