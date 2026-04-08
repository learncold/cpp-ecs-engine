# SafeCrowd UML 설계 해설 - domain-result-artifacts.puml

대상 파일: `uml/domain-result-artifacts.puml`

## 문서 목적
이 문서는 SafeCrowd 결과 아티팩트 모델을 설명한다. 핵심은 `run`, `variation`, `comparison`, `cumulative` 단위를 분리해 저장하고, export와 recommendation이 같은 persisted 결과 계약을 읽게 만드는 것이다.

## `PersistedArtifact`
- 개요: 저장소가 관리하는 결과 객체의 공통 상위 개념이다.
- 목적: 결과 아티팩트가 안정된 ID와 schema version을 공유하게 한다.
- 유의사항: 단순 파일 포맷이 아니라 repository가 추적하는 도메인 계약으로 본다.

## `RunResult`
- 개요: 한 번의 실행 결과를 나타내는 최소 저장 단위다.
- 목적: 반복 실행이 있더라도 먼저 단일 run 산출물을 명시적으로 남긴다.
- 유의사항: 상위 summary가 없어도 독립적으로 저장될 수 있어야 한다.

## `VariationSummary`
- 개요: 동일 variation에 속한 여러 run을 집계한 결과다.
- 목적: Monte Carlo 스타일 집계를 단일 deterministic run과 분리한다.
- 유의사항: `RunResult`를 소유권 기반 composition으로 묶지 않는다.

## `ScenarioComparison`
- 개요: baseline과 여러 alternative variation의 비교 결과다.
- 목적: delta를 화면용 임시 계산값이 아니라 저장 가능한 결과 계약으로 유지한다.
- 유의사항: comparison 화면이 열릴 때마다 ad hoc 계산으로 다시 만들지 않는다.

## `CumulativeArtifact`
- 개요: export, 비교, 추천의 공통 입력이 되는 canonical 결과 번들이다.
- 목적: 상위 소비자가 run, summary, comparison을 제각각 조합하지 않게 한다.
- 유의사항: 모든 raw sample을 복제하지 않고 persisted summary와 history를 참조한다.

## `ArtifactIndex`
- 개요: 실제 저장 위치와 export manifest를 가리키는 인덱스다.
- 목적: 결과가 여러 파일로 나뉘어도 상위 서비스가 공통 진입점으로 접근하게 한다.
- 유의사항: 결과 내용을 다시 중복 저장하는 객체로 키우지 않는다.

## `OccupantHistory`
- 개요: 개별 인원 추적 이력이다.
- 목적: 상세 디버깅이나 특정 분석 모드에서 선택적으로 제공한다.
- 유의사항: 기본 결과 파이프라인이 이것에 의존하지 않게 유지한다.
