"""Generate Sprint 1 시연용 DXF 평면 자산.

각 평면은 SafeCrowd DXF 임포터의 레이어 규약을 따른다.
- WALLS: 벽 라인 (Wall)
- DOORS: 내부 도어 라인 (Opening)
- EXIT:  외부 출구 라인 (Exit)
- WINDOWS: 창문 라인 (참고용)
- OBSTACLE: 장애물 폴리곤 (Obstacle)
- SPACE_*: 닫힌 폴리라인으로 정의되는 보행 가능 구역 (Walkable)
- TEXT: 라벨/타이틀

3종 평면은 시뮬레이션 결과 지표가 서로 다르게 나오도록 의도적으로 설계되었다.
README.md 의 "시연 강조 지표" 섹션을 참고하라.
"""

from __future__ import annotations

from pathlib import Path

import ezdxf
from ezdxf import units


OUTPUT_DIR = Path(__file__).resolve().parent


def _ensure_layer(doc, name: str, color: int) -> None:
    if name not in doc.layers:
        doc.layers.add(name, color=color)


def _new_document():
    doc = ezdxf.new("R2010")
    doc.units = units.M

    _ensure_layer(doc, "WALLS", 7)
    _ensure_layer(doc, "DOORS", 3)
    _ensure_layer(doc, "EXIT", 1)
    _ensure_layer(doc, "WINDOWS", 5)
    _ensure_layer(doc, "TEXT", 2)
    _ensure_layer(doc, "OBSTACLE", 6)

    return doc, doc.modelspace()


def _add_space(doc, msp, name: str, x1: float, y1: float, x2: float, y2: float, color: int = 4) -> None:
    layer = f"SPACE_{name.upper().replace(' ', '_')}"
    _ensure_layer(doc, layer, color)
    # 닫힌 폴리라인. 외부 벽과 미세하게 어긋나도록 안쪽으로 살짝 들여서 그린다.
    inset = 0.05
    msp.add_lwpolyline(
        [
            (x1 + inset, y1 + inset),
            (x2 - inset, y1 + inset),
            (x2 - inset, y2 - inset),
            (x1 + inset, y2 - inset),
        ],
        close=True,
        dxfattribs={"layer": layer},
    )

    area = abs((x2 - x1) * (y2 - y1))
    msp.add_mtext(
        f"{name.upper()}\\P{area:.1f} m^2",
        dxfattribs={
            "layer": "TEXT",
            "char_height": 0.28,
            "insert": ((x1 + x2) / 2.0 - 1.4, (y1 + y2) / 2.0 + 0.2),
        },
    )


def _add_wall(msp, start, end) -> None:
    msp.add_line(start, end, dxfattribs={"layer": "WALLS"})


def _add_door(msp, start, end) -> None:
    msp.add_line(start, end, dxfattribs={"layer": "DOORS"})


def _add_exit(msp, start, end) -> None:
    msp.add_line(start, end, dxfattribs={"layer": "EXIT"})


def _add_obstacle(msp, x1, y1, x2, y2) -> None:
    msp.add_lwpolyline(
        [(x1, y1), (x2, y1), (x2, y2), (x1, y2)],
        close=True,
        dxfattribs={"layer": "OBSTACLE"},
    )


def _add_title(msp, title: str, subtitle: str) -> None:
    msp.add_mtext(
        f"{title}\\P{subtitle}",
        dxfattribs={"layer": "TEXT", "char_height": 0.34, "insert": (0.2, -1.6)},
    )


# ----------------------------------------------------------------------------
# 1. bottleneck_hall.dxf
# ----------------------------------------------------------------------------
# 의도: 하나의 큰 홀 + 단일 좁은 출구. 출구 폭이 좁아 군중이 정체되며
#       피크 밀도, 고밀도 지속 시간, t90/t95 가 길어진다.
def build_bottleneck_hall(path: Path) -> None:
    doc, msp = _new_document()

    _add_title(
        msp,
        "BOTTLENECK HALL",
        "30x20m 단일 홀 / 1.0m 단일 출구 - 병목 시연",
    )

    # 보행 가능 공간
    _add_space(doc, msp, "Main Hall", 0.0, 0.0, 30.0, 20.0)

    # 외벽 (동쪽 중앙에 1.0m 출구 갭)
    _add_wall(msp, (0.0, 0.0), (30.0, 0.0))         # south
    _add_wall(msp, (0.0, 20.0), (30.0, 20.0))       # north
    _add_wall(msp, (0.0, 0.0), (0.0, 20.0))         # west
    _add_wall(msp, (30.0, 0.0), (30.0, 9.5))        # east-south
    _add_wall(msp, (30.0, 10.5), (30.0, 20.0))      # east-north

    # 단일 출구
    _add_exit(msp, (30.0, 9.5), (30.0, 10.5))

    # 내부 컬럼 (군중 흐름에 약간의 분산/우회 효과)
    _add_obstacle(msp, 9.0, 9.0, 10.0, 11.0)
    _add_obstacle(msp, 19.0, 9.0, 20.0, 11.0)
    _add_obstacle(msp, 14.0, 4.0, 15.0, 6.0)
    _add_obstacle(msp, 14.0, 14.0, 15.0, 16.0)

    doc.saveas(path)


# ----------------------------------------------------------------------------
# 2. multi_exit_concourse.dxf
# ----------------------------------------------------------------------------
# 의도: bottleneck_hall 과 동일 크기·동일 컬럼 배치 + 출구 3개 (동/북/남).
#       동일 군중 시나리오로 비교했을 때 t90·피크 밀도·고밀도 지속이 모두 줄고,
#       ExitUsage 가 3 출구로 분산된다.
def build_multi_exit_concourse(path: Path) -> None:
    doc, msp = _new_document()

    _add_title(
        msp,
        "MULTI-EXIT CONCOURSE",
        "30x20m 단일 홀 / 1.0m 출구 3개 (E/N/S) - 다출구 분산 시연",
    )

    _add_space(doc, msp, "Main Hall", 0.0, 0.0, 30.0, 20.0)

    # 외벽 (출구 갭 3개: 동쪽 중앙, 북쪽 중앙, 남쪽 중앙)
    # 남
    _add_wall(msp, (0.0, 0.0), (14.5, 0.0))
    _add_wall(msp, (15.5, 0.0), (30.0, 0.0))
    # 북
    _add_wall(msp, (0.0, 20.0), (14.5, 20.0))
    _add_wall(msp, (15.5, 20.0), (30.0, 20.0))
    # 서 (출구 없음)
    _add_wall(msp, (0.0, 0.0), (0.0, 20.0))
    # 동
    _add_wall(msp, (30.0, 0.0), (30.0, 9.5))
    _add_wall(msp, (30.0, 10.5), (30.0, 20.0))

    # 출구 3개
    _add_exit(msp, (30.0, 9.5), (30.0, 10.5))     # 동쪽
    _add_exit(msp, (14.5, 20.0), (15.5, 20.0))    # 북쪽
    _add_exit(msp, (14.5, 0.0), (15.5, 0.0))      # 남쪽

    # 동일 컬럼 배치 (병목 평면과 직접 비교가 가능하도록)
    _add_obstacle(msp, 9.0, 9.0, 10.0, 11.0)
    _add_obstacle(msp, 19.0, 9.0, 20.0, 11.0)
    _add_obstacle(msp, 14.0, 4.0, 15.0, 6.0)
    _add_obstacle(msp, 14.0, 14.0, 15.0, 16.0)

    doc.saveas(path)


# ----------------------------------------------------------------------------
# 3. branched_corridor_office.dxf
# ----------------------------------------------------------------------------
# 의도: 사무실 평면. 6개 룸이 중앙 복도에 모이고, 출구 2개가 비대칭으로 배치되어
#       ZoneCompletion 차등 / ExitUsage 비대칭 / 복도 셀 고밀도 지속이 발생한다.
def build_branched_corridor_office(path: Path) -> None:
    doc, msp = _new_document()

    _add_title(
        msp,
        "BRANCHED CORRIDOR OFFICE",
        "36x20m 사무실 / 6 룸 + 중앙 복도 / 비대칭 2 출구 - 현실 시연",
    )

    # 보행 가능 구역
    _add_space(doc, msp, "West Lobby", 0.0, 0.0, 8.0, 20.0)
    _add_space(doc, msp, "Corridor", 8.0, 9.0, 28.0, 11.0)
    _add_space(doc, msp, "Office A", 8.0, 11.0, 14.0, 20.0)
    _add_space(doc, msp, "Office B", 14.0, 11.0, 21.0, 20.0)
    _add_space(doc, msp, "Meeting Room", 21.0, 11.0, 28.0, 20.0)
    _add_space(doc, msp, "Office C", 8.0, 0.0, 14.0, 9.0)
    _add_space(doc, msp, "Office D", 14.0, 0.0, 21.0, 9.0)
    _add_space(doc, msp, "Office E", 21.0, 0.0, 28.0, 9.0)
    _add_space(doc, msp, "East Bay", 28.0, 0.0, 36.0, 20.0)

    # 외벽 (서쪽 중앙 2.0m 출구, 동쪽 남단 2.0m 출구)
    # 서
    _add_wall(msp, (0.0, 0.0), (0.0, 9.0))
    _add_wall(msp, (0.0, 11.0), (0.0, 20.0))
    # 북
    _add_wall(msp, (0.0, 20.0), (36.0, 20.0))
    # 남
    _add_wall(msp, (0.0, 0.0), (36.0, 0.0))
    # 동측 외벽: (36,0)-(36,2) 솔리드, (36,2)-(36,4) 출구 갭, (36,4)-(36,20) 솔리드
    _add_wall(msp, (36.0, 0.0), (36.0, 2.0))
    _add_wall(msp, (36.0, 4.0), (36.0, 20.0))

    # 출구
    _add_exit(msp, (0.0, 9.0), (0.0, 11.0))     # 서측 중앙
    _add_exit(msp, (36.0, 2.0), (36.0, 4.0))    # 동측 남단

    # 내벽: 로비 ↔ 복도 (도어 (8,9)-(8,11) 개방)
    _add_wall(msp, (8.0, 0.0), (8.0, 9.0))
    _add_wall(msp, (8.0, 11.0), (8.0, 20.0))
    _add_door(msp, (8.0, 9.0), (8.0, 11.0))

    # 북측 룸 분리벽
    _add_wall(msp, (14.0, 11.0), (14.0, 20.0))
    _add_wall(msp, (21.0, 11.0), (21.0, 20.0))
    # 남측 룸 분리벽
    _add_wall(msp, (14.0, 0.0), (14.0, 9.0))
    _add_wall(msp, (21.0, 0.0), (21.0, 9.0))

    # 복도 ↔ 북측 룸 (각 1m 도어)
    _add_wall(msp, (8.0, 11.0), (10.5, 11.0))
    _add_door(msp, (10.5, 11.0), (11.5, 11.0))
    _add_wall(msp, (11.5, 11.0), (17.0, 11.0))
    _add_door(msp, (17.0, 11.0), (18.0, 11.0))
    _add_wall(msp, (18.0, 11.0), (24.0, 11.0))
    _add_door(msp, (24.0, 11.0), (25.0, 11.0))
    _add_wall(msp, (25.0, 11.0), (28.0, 11.0))

    # 복도 ↔ 남측 룸 (각 1m 도어)
    _add_wall(msp, (8.0, 9.0), (10.5, 9.0))
    _add_door(msp, (10.5, 9.0), (11.5, 9.0))
    _add_wall(msp, (11.5, 9.0), (17.0, 9.0))
    _add_door(msp, (17.0, 9.0), (18.0, 9.0))
    _add_wall(msp, (18.0, 9.0), (24.0, 9.0))
    _add_door(msp, (24.0, 9.0), (25.0, 9.0))
    _add_wall(msp, (25.0, 9.0), (28.0, 9.0))

    # 복도 ↔ 동측 베이 (도어 (28,9)-(28,11) 개방)
    _add_wall(msp, (28.0, 0.0), (28.0, 9.0))
    _add_wall(msp, (28.0, 11.0), (28.0, 20.0))
    _add_door(msp, (28.0, 9.0), (28.0, 11.0))

    # 장애물 (로비 리셉션, 복도 컬럼, 동쪽 베이 가구)
    _add_obstacle(msp, 2.0, 8.5, 5.5, 11.5)         # 로비 중앙 리셉션 (출구 옆)
    _add_obstacle(msp, 13.0, 9.5, 13.5, 10.5)       # 복도 컬럼
    _add_obstacle(msp, 20.0, 9.5, 20.5, 10.5)       # 복도 컬럼
    _add_obstacle(msp, 32.0, 8.0, 34.5, 12.0)       # 동쪽 베이 가구섬

    doc.saveas(path)


def main() -> None:
    build_bottleneck_hall(OUTPUT_DIR / "bottleneck_hall.dxf")
    build_multi_exit_concourse(OUTPUT_DIR / "multi_exit_concourse.dxf")
    build_branched_corridor_office(OUTPUT_DIR / "branched_corridor_office.dxf")


if __name__ == "__main__":
    main()
