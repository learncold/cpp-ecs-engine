# SafeCrowd UML 설계 해설 - application-analysis-workspace.puml

대상 파일: `uml/application-analysis-workspace.puml`

## 문서 목적
이 문서는 SafeCrowd의 실행 후반과 결과 분석 UI를 설명한다. 핵심은 `live playback`과 `persisted artifact 기반 분석`을 한 화면 군 안에서 연결하되, 데이터 출처를 섞지 않는 것이다.

## `Run Control`
- 개요: batch 실행의 시작, 일시정지, 정지를 요청하는 패널이다.
- 목적: application이 엔진 세부 제어 대신 `ScenarioBatchRunner`를 통해 실행을 조정하게 한다.
- 유의사항: comparison이나 export 조건 판단을 여기서 맡지 않는다.

## `Batch Progress`
- 개요: 현재 run 번호, variation 식별자, 반복 실행 진행률을 보여 주는 패널이다.
- 목적: 사용자가 반복 실행과 batch 상태를 실시간으로 추적하게 한다.
- 유의사항: 결과 집계가 끝나기 전과 후의 상태를 구분해서 보여 준다.

## `Live Viewport`
- 개요: 실행 중 runtime snapshot을 재생하는 시각화 영역이다.
- 목적: 현재 움직임과 즉시적인 overlay를 확인하게 한다.
- 유의사항: persisted comparison이나 recommendation evidence를 직접 계산하지 않는다.

## `Heatmap Selector`
- 개요: heatmap과 위험 레이어의 데이터 출처와 표시 종류를 고르는 컨트롤이다.
- 목적: live overlay와 post-run heatmap을 같은 UI affordance로 연결한다.
- 유의사항: 데이터 출처가 `IRenderBridge` 기반인지 `ResultRepository` 기반인지 명확해야 한다.

## `Run Results Panel`
- 개요: 단일 실행 요약을 읽는 첫 결과 패널이다.
- 목적: run 종료 직후의 핵심 지표를 persisted 형태로 먼저 보여 준다.
- 유의사항: live 상태를 그대로 붙이는 패널이 아니라 저장된 `RunResult` 소비자다.

## `Variation Summary`
- 개요: 같은 variation에 속한 반복 실행 집계를 보여 주는 패널이다.
- 목적: deterministic single run과 repeated-run aggregate를 구분한다.
- 유의사항: `Comparison View`로 넘어가기 전에 repeated-run 맥락을 먼저 고정한다.

## `Comparison View`
- 개요: baseline과 alternative variation을 나란히 비교하는 핵심 분석 패널이다.
- 목적: `ScenarioComparison`과 `CumulativeArtifact`를 기준으로 변화량과 근거를 제시한다.
- 유의사항: ad hoc domain delta 계산기로 쓰지 않는다.

## `Recommendation Drawer`
- 개요: 추천 후보, 근거, 시나리오화 액션을 보여 주는 패널이다.
- 목적: 결과 분석 뒤에 이어지는 설명 가능한 운영 대안 검토 흐름을 만든다.
- 유의사항: 추천 입력은 persisted comparison/cumulative artifact에 한정한다.

## `Export Dialog`
- 개요: canonical 결과 번들을 외부 공유용으로 내보내는 대화상자다.
- 목적: comparison과 recommendation이 보는 같은 결과 계약을 export에도 재사용한다.
- 유의사항: 결과가 충분하지 않으면 비활성 상태를 유지한다.

## `ResultRepository` / `ResultAggregator`
- 개요: `ResultRepository`는 run/variation/comparison/cumulative artifact 저장소이고, `ResultAggregator`는 상위 결과를 생성하는 서비스다.
- 목적: 분석 패널들이 같은 persisted 결과 계약을 읽게 한다.
- 유의사항: comparison, export, recommendation의 선행 조건은 이 저장소에 결과가 존재하는지로 판단한다.
