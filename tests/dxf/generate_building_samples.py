"""DXF 임포트 단위 테스트가 참조하는 픽스처 생성기.

`office_suite.dxf` 만 본 스크립트로 결정적으로 재생성됩니다.
시연용 평면 자산은 `assets/demo-layouts/generate_demo_layouts.py` 에 별도로 있습니다.
"""

from __future__ import annotations

from pathlib import Path

import ezdxf
from ezdxf import units


OUTPUT_DIR = Path(__file__).resolve().parent


def ensure_layer(doc: ezdxf.EzDxfDocument, name: str, color: int) -> None:
    if name not in doc.layers:
        doc.layers.add(name, color=color)


def new_document() -> tuple[ezdxf.EzDxfDocument, ezdxf.layouts.Modelspace]:
    doc = ezdxf.new("R2010")
    doc.units = units.M

    ensure_layer(doc, "WALLS", 7)
    ensure_layer(doc, "DOORS", 3)
    ensure_layer(doc, "EXIT", 1)
    ensure_layer(doc, "WINDOWS", 5)
    ensure_layer(doc, "TEXT", 2)
    ensure_layer(doc, "OBSTACLE", 6)

    return doc, doc.modelspace()


def add_space(
    doc: ezdxf.EzDxfDocument,
    msp: ezdxf.layouts.Modelspace,
    name: str,
    x1: float,
    y1: float,
    x2: float,
    y2: float,
    color: int = 4,
) -> None:
    layer = f"SPACE_{name.upper().replace(' ', '_')}"
    ensure_layer(doc, layer, color)

    msp.add_lwpolyline(
        [(x1, y1), (x2, y1), (x2, y2), (x1, y2)],
        close=True,
        dxfattribs={"layer": layer},
    )

    area = abs((x2 - x1) * (y2 - y1))
    msp.add_mtext(
        f"{name.upper()}\\P{area:.1f} m^2",
        dxfattribs={
            "layer": "TEXT",
            "char_height": 0.24,
            "insert": ((x1 + x2) / 2.0 - 0.8, (y1 + y2) / 2.0 + 0.1),
        },
    )


def add_wall(msp: ezdxf.layouts.Modelspace, start: tuple[float, float], end: tuple[float, float]) -> None:
    msp.add_line(start, end, dxfattribs={"layer": "WALLS"})


def add_window(msp: ezdxf.layouts.Modelspace, start: tuple[float, float], end: tuple[float, float]) -> None:
    msp.add_line(start, end, dxfattribs={"layer": "WINDOWS"})


def add_door(
    msp: ezdxf.layouts.Modelspace,
    start: tuple[float, float],
    end: tuple[float, float],
    *,
    swing_center: tuple[float, float] | None = None,
    swing_radius: float | None = None,
    start_angle: float | None = None,
    end_angle: float | None = None,
    layer: str = "DOORS",
) -> None:
    msp.add_line(start, end, dxfattribs={"layer": layer})

    if (
        swing_center is not None
        and swing_radius is not None
        and start_angle is not None
        and end_angle is not None
    ):
        msp.add_arc(
            swing_center,
            swing_radius,
            start_angle,
            end_angle,
            dxfattribs={"layer": layer},
        )


def add_obstacle(msp: ezdxf.layouts.Modelspace, x1: float, y1: float, x2: float, y2: float) -> None:
    msp.add_lwpolyline(
        [(x1, y1), (x2, y1), (x2, y2), (x1, y2)],
        close=True,
        dxfattribs={"layer": "OBSTACLE"},
    )


def add_title(msp: ezdxf.layouts.Modelspace, title: str, subtitle: str) -> None:
    msp.add_mtext(
        f"{title}\\P{subtitle}",
        dxfattribs={"layer": "TEXT", "char_height": 0.28, "insert": (0.2, -1.2)},
    )


def build_office_plan(path: Path) -> None:
    doc, msp = new_document()

    add_title(msp, "OFFICE SUITE SAMPLE", "Small office floor with lobby, meeting room, main office, and service rooms")

    outer = (0.0, 0.0, 14.0, 9.0)
    add_wall(msp, (outer[0], outer[1]), (0.9, 0.0))
    add_wall(msp, (1.9, 0.0), (outer[2], outer[1]))
    add_wall(msp, (outer[2], outer[1]), (outer[2], outer[3]))
    add_wall(msp, (outer[2], outer[3]), (12.4, 9.0))
    add_wall(msp, (11.4, 9.0), (outer[0], outer[3]))
    add_wall(msp, (outer[0], outer[3]), (outer[0], outer[1]))

    add_wall(msp, (3.0, 0.0), (3.0, 4.0))
    add_wall(msp, (3.0, 5.0), (3.0, 9.0))
    add_wall(msp, (9.8, 0.0), (9.8, 3.2))
    add_wall(msp, (9.8, 4.2), (9.8, 6.2))
    add_wall(msp, (9.8, 7.2), (9.8, 9.0))
    add_wall(msp, (3.0, 2.4), (6.0, 2.4))
    add_wall(msp, (7.0, 2.4), (9.8, 2.4))
    add_wall(msp, (9.8, 5.0), (14.0, 5.0))

    add_space(doc, msp, "Lobby", 0.0, 0.0, 3.0, 9.0)
    add_space(doc, msp, "Meeting Room", 3.0, 0.0, 9.8, 2.4)
    add_space(doc, msp, "Main Office", 3.0, 2.4, 9.8, 9.0)
    add_space(doc, msp, "Kitchenette", 9.8, 0.0, 14.0, 5.0)
    add_space(doc, msp, "Storage", 9.8, 5.0, 14.0, 9.0)

    add_door(msp, (0.9, 0.0), (1.9, 0.0), layer="EXIT")
    add_door(msp, (11.4, 9.0), (12.4, 9.0), layer="EXIT")

    add_window(msp, (0.0, 6.0), (0.0, 7.5))
    add_window(msp, (6.0, 9.0), (8.0, 9.0))
    add_window(msp, (14.0, 1.0), (14.0, 2.5))
    add_window(msp, (14.0, 6.0), (14.0, 7.6))

    add_obstacle(msp, 5.0, 4.2, 5.8, 5.8)
    add_obstacle(msp, 10.8, 6.0, 12.5, 7.8)

    doc.saveas(path)


def main() -> None:
    build_office_plan(OUTPUT_DIR / "office_suite.dxf")


if __name__ == "__main__":
    main()
