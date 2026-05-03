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


def build_home_plan(path: Path) -> None:
    doc, msp = new_document()

    add_title(msp, "HOME PLAN SAMPLE", "Generated from a plan2dxf-style workflow for SafeCrowd tests")

    outer = (0.0, 0.0, 10.0, 8.0)
    add_wall(msp, (outer[0], outer[1]), (outer[2], outer[1]))
    add_wall(msp, (outer[2], outer[1]), (outer[2], outer[3]))
    add_wall(msp, (outer[2], outer[3]), (outer[0], outer[3]))
    add_wall(msp, (outer[0], outer[3]), (outer[0], outer[1]))

    add_wall(msp, (4.8, 0.0), (4.8, 5.0))
    add_wall(msp, (0.0, 5.0), (10.0, 5.0))
    add_wall(msp, (7.2, 5.0), (7.2, 8.0))

    add_space(doc, msp, "Living Room", 0.1, 0.1, 4.7, 4.9)
    add_space(doc, msp, "Kitchen", 4.9, 0.1, 9.9, 2.9)
    add_space(doc, msp, "Dining", 4.9, 3.1, 9.9, 4.9)
    add_space(doc, msp, "Bedroom", 0.1, 5.1, 7.1, 7.9)
    add_space(doc, msp, "Bath", 7.3, 5.1, 9.9, 7.9)

    add_door(msp, (2.0, 0.0), (2.9, 0.0), swing_center=(2.0, 0.0), swing_radius=0.9, start_angle=0, end_angle=90, layer="EXIT")
    add_door(msp, (4.8, 2.0), (4.8, 2.9), swing_center=(4.8, 2.0), swing_radius=0.9, start_angle=0, end_angle=90)
    add_door(msp, (7.2, 6.2), (7.2, 7.0), swing_center=(7.2, 6.2), swing_radius=0.8, start_angle=180, end_angle=270)

    add_window(msp, (0.0, 6.1), (0.0, 7.2))
    add_window(msp, (8.1, 8.0), (9.3, 8.0))
    add_window(msp, (10.0, 1.0), (10.0, 2.2))

    add_obstacle(msp, 6.2, 1.0, 7.3, 1.8)

    doc.saveas(path)


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


def build_evacuation_complex_large(path: Path) -> None:
    doc, msp = new_document()

    add_title(
        msp,
        "LARGE EVACUATION COMPLEX",
        "Multi-wing civic center floor with wide concourse, large assembly spaces, service wings, and multiple exits",
    )

    outer = (0.0, 0.0, 64.0, 42.0)
    add_wall(msp, (outer[0], outer[1]), (outer[2], outer[1]))
    add_wall(msp, (outer[2], outer[1]), (outer[2], outer[3]))
    add_wall(msp, (outer[2], outer[3]), (outer[0], outer[3]))
    add_wall(msp, (outer[0], outer[3]), (outer[0], outer[1]))

    # Primary structural partitions
    add_wall(msp, (12.0, 0.0), (12.0, 42.0))
    add_wall(msp, (52.0, 0.0), (52.0, 42.0))
    add_wall(msp, (12.0, 16.0), (52.0, 16.0))
    add_wall(msp, (12.0, 26.0), (52.0, 26.0))
    add_wall(msp, (34.0, 26.0), (34.0, 42.0))
    add_wall(msp, (26.0, 0.0), (26.0, 16.0))
    add_wall(msp, (40.0, 0.0), (40.0, 16.0))
    add_wall(msp, (52.0, 12.0), (64.0, 12.0))
    add_wall(msp, (52.0, 30.0), (64.0, 30.0))
    add_wall(msp, (0.0, 12.0), (12.0, 12.0))
    add_wall(msp, (0.0, 30.0), (12.0, 30.0))

    # Walkable zones
    add_space(doc, msp, "West Lobby", 0.1, 12.1, 11.9, 29.9)
    add_space(doc, msp, "Central Concourse", 12.1, 16.1, 51.9, 25.9)
    add_space(doc, msp, "East Lobby", 52.1, 12.1, 63.9, 29.9)
    add_space(doc, msp, "Auditorium", 12.1, 26.1, 33.9, 41.9)
    add_space(doc, msp, "Cafeteria", 34.1, 26.1, 51.9, 41.9)
    add_space(doc, msp, "Training A", 12.1, 0.1, 25.9, 15.9)
    add_space(doc, msp, "Training B", 26.1, 0.1, 39.9, 15.9)
    add_space(doc, msp, "Command Center", 40.1, 0.1, 51.9, 15.9)
    add_space(doc, msp, "West Service", 0.1, 0.1, 11.9, 11.9)
    add_space(doc, msp, "West Breakout", 0.1, 30.1, 11.9, 41.9)
    add_space(doc, msp, "East Storage", 52.1, 0.1, 63.9, 11.9)
    add_space(doc, msp, "East Operations", 52.1, 30.1, 63.9, 41.9)

    # Internal doors from concourse/lobbies to adjacent spaces
    add_door(msp, (12.0, 19.5), (12.0, 22.5), swing_center=(12.0, 19.5), swing_radius=1.0, start_angle=180, end_angle=270)
    add_door(msp, (52.0, 19.5), (52.0, 22.5), swing_center=(52.0, 19.5), swing_radius=1.0, start_angle=0, end_angle=90)
    add_door(msp, (20.0, 26.0), (23.0, 26.0), swing_center=(20.0, 26.0), swing_radius=1.0, start_angle=0, end_angle=90)
    add_door(msp, (42.0, 26.0), (45.0, 26.0), swing_center=(42.0, 26.0), swing_radius=1.0, start_angle=90, end_angle=180)
    add_door(msp, (18.0, 16.0), (21.0, 16.0), swing_center=(18.0, 16.0), swing_radius=1.0, start_angle=270, end_angle=360)
    add_door(msp, (31.0, 16.0), (34.0, 16.0), swing_center=(31.0, 16.0), swing_radius=1.0, start_angle=270, end_angle=360)
    add_door(msp, (45.0, 16.0), (48.0, 16.0), swing_center=(45.0, 16.0), swing_radius=1.0, start_angle=270, end_angle=360)
    add_door(msp, (6.0, 12.0), (8.5, 12.0), swing_center=(6.0, 12.0), swing_radius=0.9, start_angle=270, end_angle=360)
    add_door(msp, (6.0, 30.0), (8.5, 30.0), swing_center=(6.0, 30.0), swing_radius=0.9, start_angle=0, end_angle=90)
    add_door(msp, (57.0, 12.0), (59.5, 12.0), swing_center=(57.0, 12.0), swing_radius=0.9, start_angle=270, end_angle=360)
    add_door(msp, (57.0, 30.0), (59.5, 30.0), swing_center=(57.0, 30.0), swing_radius=0.9, start_angle=0, end_angle=90)

    # External exits around the perimeter
    add_door(msp, (0.0, 18.0), (0.0, 21.0), layer="EXIT")
    add_door(msp, (0.0, 35.0), (0.0, 38.0), layer="EXIT")
    add_door(msp, (18.0, 42.0), (21.0, 42.0), layer="EXIT")
    add_door(msp, (43.0, 42.0), (46.0, 42.0), layer="EXIT")
    add_door(msp, (64.0, 18.0), (64.0, 21.0), layer="EXIT")
    add_door(msp, (64.0, 34.0), (64.0, 37.0), layer="EXIT")
    add_door(msp, (18.0, 0.0), (21.0, 0.0), layer="EXIT")
    add_door(msp, (44.0, 0.0), (47.0, 0.0), layer="EXIT")

    # Windows
    add_window(msp, (0.0, 23.0), (0.0, 27.0))
    add_window(msp, (14.0, 42.0), (17.0, 42.0))
    add_window(msp, (28.0, 42.0), (31.0, 42.0))
    add_window(msp, (36.0, 42.0), (39.0, 42.0))
    add_window(msp, (64.0, 24.0), (64.0, 28.0))
    add_window(msp, (24.0, 0.0), (27.0, 0.0))
    add_window(msp, (48.0, 0.0), (51.0, 0.0))

    # Obstacles / columns / furniture islands
    add_obstacle(msp, 20.0, 19.0, 21.5, 20.5)
    add_obstacle(msp, 30.0, 19.0, 31.5, 20.5)
    add_obstacle(msp, 40.0, 19.0, 41.5, 20.5)
    add_obstacle(msp, 20.0, 21.5, 21.5, 23.0)
    add_obstacle(msp, 30.0, 21.5, 31.5, 23.0)
    add_obstacle(msp, 40.0, 21.5, 41.5, 23.0)
    add_obstacle(msp, 17.0, 31.0, 18.5, 32.5)
    add_obstacle(msp, 24.0, 31.0, 25.5, 32.5)
    add_obstacle(msp, 38.0, 32.0, 40.5, 34.0)
    add_obstacle(msp, 43.0, 6.0, 45.5, 8.0)
    add_obstacle(msp, 55.0, 34.0, 57.0, 36.5)

    doc.saveas(path)


def main() -> None:
    build_home_plan(OUTPUT_DIR / "home_plan.dxf")
    build_office_plan(OUTPUT_DIR / "office_suite.dxf")
    build_evacuation_complex_large(OUTPUT_DIR / "evacuation_complex_large.dxf")


if __name__ == "__main__":
    main()
