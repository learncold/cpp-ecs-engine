# 시연용 평면 자산 (Demo Layouts)

Sprint 1 시연 흐름(임포트 → 리뷰 → 시뮬레이션 → 결과)에서 결과 지표가 서로 다르게 나오도록 의도적으로 설계된 DXF 도면 모음입니다.

테스트 픽스처가 아니라 **시연 시 직접 임포트해서 보여주기 위한 자산**입니다. 단위 테스트가 참조하는 도면은 `tests/dxf/` 에서 따로 관리합니다.

## 평면 목록

| 파일 | 크기 | 의도 | 시연 강조 결과 지표 |
| --- | --- | --- | --- |
| `bottleneck_hall.dxf` | 30 × 20 m | 큰 단일 홀 + 1.0 m 단일 출구 | `DensitySummary.peakDensity` ↑↑, `highDensityDurationSeconds` ↑, `EvacuationTimingSummary.t90/t95` 길어짐, `ExitUsage` 단일 100 % |
| `multi_exit_concourse.dxf` | 30 × 20 m | 동일 홀 + 1.0 m 출구 3개 (E/N/S) | 동일 군중 시나리오 비교 시 t90·피크 밀도·고밀도 지속 시간 모두 감소, `ExitUsage` 가 3 출구로 분산 |
| `branched_corridor_office.dxf` | 36 × 20 m | 사무실: 6 룸 + 중앙 복도 + 비대칭 2 출구 | `ZoneCompletionMetric` 존별 차등 (원거리 룸 지연), `ExitUsage` 비대칭, 복도 셀의 `peakField` 시각화 임팩트 |

## 시연 시나리오 가이드

`bottleneck_hall ↔ multi_exit_concourse` 는 **같은 홀 크기 · 같은 컬럼 배치**로 설계되어 있어, 동일 군중·동일 시나리오를 양쪽에 적용하면 출구 수만으로 결과가 어떻게 달라지는지 직접 비교할 수 있습니다.

`branched_corridor_office` 는 **현실 평면**에서 어떤 셀이 위험 지점이 되는지 (밀도 필드 시각화) 를 보여주는 역할입니다.

자세한 시연 흐름은 [docs/demo/시연 평면 가이드.md](../../docs/demo/시연%20평면%20가이드.md) 를 참고하세요.

## 재생성

```bash
python assets/demo-layouts/generate_demo_layouts.py
```

DXF 는 결정적으로 재생성됩니다. 디퍼런셜 노이즈가 발생하면 `ezdxf` 버전 차이일 수 있습니다.

## DXF 레이어 규약

생성기는 SafeCrowd DXF 임포터의 레이어 규약을 따릅니다.

| 레이어 | 의미 |
| --- | --- |
| `WALLS` | 벽 라인 → Wall |
| `DOORS` | 내부 도어 라인 → Opening |
| `EXIT` | 외부 출구 라인 → Exit |
| `WINDOWS` | 창문 라인 (참고용) |
| `OBSTACLE` | 장애물 폴리곤 → Obstacle |
| `SPACE_*` | 닫힌 폴리라인 → Walkable Zone |
| `TEXT` | 라벨/타이틀 |
