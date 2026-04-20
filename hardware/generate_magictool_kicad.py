#!/usr/bin/env python3
"""Generate the magicTool KiCad project.

The board is a small Pico 2 W carrier for the firmware GPIO map:
OUT0..OUT3 on GPIO2..GPIO5 and IN0..IN1 on GPIO6..GPIO7.
"""

from __future__ import annotations

import json
import pathlib
import textwrap
import uuid

import pcbnew


ROOT = pathlib.Path(__file__).resolve().parent
PROJECT = "magictool"


def u() -> str:
    return str(uuid.uuid4())


def pro() -> None:
    data = {
        "board": {
            "design_settings": {
                "defaults": {
                    "board_outline_line_width": 0.1,
                    "copper_line_width": 0.2,
                    "copper_text_size_h": 1.5,
                    "copper_text_size_v": 1.5,
                    "copper_text_thickness": 0.3,
                    "other_line_width": 0.15,
                    "silk_line_width": 0.12,
                    "silk_text_size_h": 1.0,
                    "silk_text_size_v": 1.0,
                    "silk_text_thickness": 0.15,
                    "track_width": 0.35,
                    "via_diameter": 0.8,
                    "via_drill": 0.4,
                },
                "diff_pair_dimensions": [],
                "drc_exclusions": [],
                "rules": {},
                "track_widths": [0.35, 0.5],
                "via_dimensions": [{"diameter": 0.8, "drill": 0.4}],
            }
        },
        "boards": [],
        "libraries": {"pinned_footprint_libs": [], "pinned_symbol_libs": []},
        "meta": {"filename": f"{PROJECT}.kicad_pro", "version": 1},
        "net_settings": {
            "classes": [
                {
                    "bus_width": 12,
                    "clearance": 0.2,
                    "diff_pair_gap": 0.25,
                    "diff_pair_via_gap": 0.25,
                    "diff_pair_width": 0.2,
                    "line_style": 0,
                    "microvia_diameter": 0.3,
                    "microvia_drill": 0.1,
                    "name": "Default",
                    "pcb_color": "rgba(0, 0, 0, 0.000)",
                    "schematic_color": "rgba(0, 0, 0, 0.000)",
                    "track_width": 0.35,
                    "via_diameter": 0.8,
                    "via_drill": 0.4,
                    "wire_width": 6,
                }
            ],
            "meta": {"version": 3},
        },
        "pcbnew": {"page_layout_descr_file": ""},
        "sheets": [[u(), ""]],
        "text_variables": {},
    }
    (ROOT / f"{PROJECT}.kicad_pro").write_text(json.dumps(data, indent=2) + "\n")


def symbol(lib_id: str, ref: str, value: str, fp: str, x: float, y: float, rot: int, pins: list[str]) -> str:
    pin_txt = "\n".join(f'\t\t(pin "{p}"\n\t\t\t(uuid "{u()}")\n\t\t)' for p in pins)
    return f"""
\t(symbol
\t\t(lib_id "{lib_id}")
\t\t(at {x:g} {y:g} {rot})
\t\t(unit 1)
\t\t(exclude_from_sim no)
\t\t(in_bom yes)
\t\t(on_board yes)
\t\t(dnp no)
\t\t(uuid "{u()}")
\t\t(property "Reference" "{ref}"
\t\t\t(at {x:g} {y - 3.81:g} 0)
\t\t\t(effects (font (size 1.27 1.27)))
\t\t)
\t\t(property "Value" "{value}"
\t\t\t(at {x:g} {y + 3.81:g} 0)
\t\t\t(effects (font (size 1.27 1.27)))
\t\t)
\t\t(property "Footprint" "{fp}"
\t\t\t(at {x:g} {y:g} 0)
\t\t\t(effects (font (size 1.27 1.27)) (hide yes))
\t\t)
\t\t(property "Datasheet" "~"
\t\t\t(at {x:g} {y:g} 0)
\t\t\t(effects (font (size 1.27 1.27)) (hide yes))
\t\t)
\t\t(property "Description" ""
\t\t\t(at {x:g} {y:g} 0)
\t\t\t(effects (font (size 1.27 1.27)) (hide yes))
\t\t)
{pin_txt}
\t\t(instances
\t\t\t(project "{PROJECT}"
\t\t\t\t(path "/"
\t\t\t\t\t(reference "{ref}")
\t\t\t\t\t(unit 1)
\t\t\t\t)
\t\t\t)
\t\t)
\t)
"""


def extract_lib_symbol(library: str, name: str, embedded_name: str) -> str:
    path = pathlib.Path("/usr/share/kicad/symbols") / f"{library}.kicad_sym"
    text = path.read_text()
    start = text.index(f'(symbol "{name}"')
    depth = 0
    end = start
    for pos in range(start, len(text)):
        if text[pos] == "(":
            depth += 1
        elif text[pos] == ")":
            depth -= 1
            if depth == 0:
                end = pos + 1
                break
    return text[start:end].replace(f'(symbol "{name}"', f'(symbol "{embedded_name}"', 1)


def lib_symbols() -> str:
    entries = [
        extract_lib_symbol("MCU_Module", "RaspberryPi_Pico", "MCU_Module:RaspberryPi_Pico"),
        extract_lib_symbol("Connector_Generic", "Conn_01x08", "Connector_Generic:Conn_01x08"),
        extract_lib_symbol("Connector_Generic", "Conn_01x02", "Connector_Generic:Conn_01x02"),
    ]
    indented = "\n".join(textwrap.indent(entry, "\t\t") for entry in entries)
    return f"\t(lib_symbols\n{indented}\n\t)\n"


def wire(x1: float, y1: float, x2: float, y2: float) -> str:
    return f"""
\t(wire
\t\t(pts (xy {x1:g} {y1:g}) (xy {x2:g} {y2:g}))
\t\t(stroke (width 0) (type default))
\t\t(uuid "{u()}")
\t)
"""


def label(name: str, x: float, y: float, rot: int = 0) -> str:
    return f"""
\t(label "{name}"
\t\t(at {x:g} {y:g} {rot})
\t\t(effects (font (size 1.27 1.27)) (justify left bottom))
\t\t(uuid "{u()}")
\t)
"""


def global_label(name: str, x: float, y: float, rot: int = 0) -> str:
    return f"""
\t(global_label "{name}"
\t\t(shape input)
\t\t(at {x:g} {y:g} {rot})
\t\t(fields_autoplaced yes)
\t\t(effects (font (size 1.27 1.27)) (justify left))
\t\t(uuid "{u()}")
\t\t(property "Intersheetrefs" "${{INTERSHEET_REFS}}" (at {x:g} {y:g} 0)
\t\t\t(effects (font (size 1.27 1.27)) (hide yes))
\t\t)
\t)
"""


def junction(x: float, y: float) -> str:
    return f"""
\t(junction
\t\t(at {x:g} {y:g})
\t\t(diameter 0)
\t\t(color 0 0 0 0)
\t\t(uuid "{u()}")
\t)
"""


def sch() -> None:
    pico_x, pico_y = 120.65, 88.9
    conn_x, conn_y = 50.8, 99.06
    run_x, run_y = 50.8, 128.27
    pico_left_x = pico_x - 22.86
    pico_nets = [
        ("OUT0", pico_y - 10.16),
        ("OUT1", pico_y - 7.62),
        ("OUT2", pico_y - 5.08),
        ("OUT3", pico_y - 2.54),
        ("IN0", pico_y),
        ("IN1", pico_y + 2.54),
    ]
    header_pin_y = [conn_y - 8.89 + i * 2.54 for i in range(8)]
    header_nets = ["OUT0", "OUT1", "OUT2", "OUT3", "IN0", "IN1", "+3V3", "GND"]

    parts = [
        symbol(
            "MCU_Module:RaspberryPi_Pico",
            "A1",
            "Raspberry Pi Pico 2 W",
            "Module:RaspberryPi_Pico_W_SMD_HandSolder",
            pico_x,
            pico_y,
            0,
            [str(i) for i in range(1, 41)],
        ),
        symbol(
            "Connector_Generic:Conn_01x08",
            "J1",
            "magicTool I/O",
            "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical",
            conn_x,
            conn_y,
            0,
            [str(i) for i in range(1, 9)],
        ),
        symbol(
            "Connector_Generic:Conn_01x02",
            "J2",
            "RUN",
            "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",
            run_x,
            run_y,
            0,
            ["1", "2"],
        ),
    ]

    wires = []
    # Short labeled stubs keep the schematic readable while preserving nets.
    for name, y in pico_nets:
        wires += [wire(pico_left_x, y, pico_left_x - 10.16, y), label(name, pico_left_x - 10.16, y)]

    header_pin_x = conn_x - 2.54
    for name, y in zip(header_nets, header_pin_y):
        if name in {"+3V3", "GND"}:
            wires.append(global_label(name, header_pin_x, y, 180))
        else:
            wires += [wire(header_pin_x, y, header_pin_x - 10.16, y), label(name, header_pin_x - 10.16, y)]

    wires += [
        global_label("+3V3", pico_x + 5.08, pico_y + 41.91, 90),
        wire(pico_x + 5.08, pico_y + 38.1, pico_x + 5.08, pico_y + 41.91),
        global_label("GND", pico_x, pico_y - 35.56, 270),
        wire(pico_x, pico_y - 35.56, pico_x, pico_y - 39.37),
    ]

    wires += [
        wire(pico_left_x, pico_y + 22.86, pico_left_x - 10.16, pico_y + 22.86),
        label("RUN", pico_left_x - 10.16, pico_y + 22.86),
        wire(run_x - 2.54, run_y - 1.27, run_x - 12.7, run_y - 1.27),
        label("RUN", run_x - 12.7, run_y - 1.27),
        global_label("GND", run_x - 2.54, run_y + 1.27, 180),
    ]

    texts = """
\t(text "J1 maps directly to firmware GPIOs: OUT0..OUT3 = GPIO2..GPIO5, IN0..IN1 = GPIO6..GPIO7. No external input pulldowns are used; firmware enables internal pulldowns."
\t\t(at 25.4 144.78 0)
\t\t(effects (font (size 1.27 1.27)) (justify left bottom))
\t\t(uuid "{txt_uuid}")
\t)
""".format(txt_uuid=u())

    body = "".join(wires) + "".join(parts) + texts
    content = f"""(kicad_sch
\t(version 20250114)
\t(generator "magictool_kicad_generator")
\t(generator_version "1")
\t(uuid "{u()}")
\t(paper "A4")
\t(title_block
\t\t(title "magicTool Pico Carrier")
\t\t(company "magicTool")
\t\t(comment 1 "Pico 2 / Pico 2 W debug GPIO carrier")
\t)
{lib_symbols()}
{body}
\t(sheet_instances
\t\t(path "/"
\t\t\t(page "1")
\t\t)
\t)
\t(embedded_fonts no)
)
"""
    (ROOT / f"{PROJECT}.kicad_sch").write_text(content)


def mm(v: float) -> int:
    return pcbnew.FromMM(v)


def point(x: float, y: float) -> pcbnew.VECTOR2I:
    return pcbnew.VECTOR2I(mm(x), mm(y))


def load_fp(lib: str, name: str, ref: str, value: str, x: float, y: float, rot_deg: float = 0.0):
    fp = pcbnew.FootprintLoad(f"/usr/share/kicad/footprints/{lib}.pretty", name)
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.SetPosition(point(x, y))
    fp.SetOrientationDegrees(rot_deg)
    return fp


def add_track(board, net, start, end, width=0.35, layer=pcbnew.F_Cu):
    tr = pcbnew.PCB_TRACK(board)
    tr.SetStart(point(*start))
    tr.SetEnd(point(*end))
    tr.SetWidth(mm(width))
    tr.SetLayer(layer)
    tr.SetNet(net)
    board.Add(tr)


def add_polyline(board, net, pts, width=0.35, layer=pcbnew.F_Cu):
    for a, b in zip(pts, pts[1:]):
        add_track(board, net, a, b, width, layer)


def set_text(board, text, x, y, size=1.0, layer=pcbnew.F_SilkS):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(point(x, y))
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(0.15))
    t.SetLayer(layer)
    return t


def pcb() -> None:
    board = pcbnew.BOARD()

    nets = {}
    for name in ["GND", "+3V3", "RUN", "OUT0", "OUT1", "OUT2", "OUT3", "IN0", "IN1"]:
        net = pcbnew.NETINFO_ITEM(board, name)
        board.Add(net)
        nets[name] = net

    fps = []
    pico = load_fp("Module", "RaspberryPi_Pico_W_SMD_HandSolder", "A1", "Raspberry Pi Pico 2 W", 70, 60)
    j1 = load_fp("Connector_PinHeader_2.54mm", "PinHeader_1x08_P2.54mm_Vertical", "J1", "magicTool I/O", 28, 43.49)
    j2 = load_fp("Connector_PinHeader_2.54mm", "PinHeader_1x02_P2.54mm_Vertical", "J2", "RUN", 94, 61.27)
    fps += [pico, j1, j2]

    fps += [
        load_fp("MountingHole", "MountingHole_3.2mm_M3", "H1", "M3", 15, 31),
        load_fp("MountingHole", "MountingHole_3.2mm_M3", "H2", "M3", 100, 31),
        load_fp("MountingHole", "MountingHole_3.2mm_M3", "H3", "M3", 15, 84),
        load_fp("MountingHole", "MountingHole_3.2mm_M3", "H4", "M3", 100, 84),
    ]

    for fp in fps:
        fp.Reference().SetVisible(False)
        fp.Value().SetVisible(False)
        board.Add(fp)

    for pad_num, net_name in {
        "4": "OUT0",
        "5": "OUT1",
        "6": "OUT2",
        "7": "OUT3",
        "9": "IN0",
        "10": "IN1",
        "30": "RUN",
        "36": "+3V3",
        "3": "GND",
        "8": "GND",
        "13": "GND",
        "18": "GND",
        "23": "GND",
        "28": "GND",
        "33": "GND",
        "38": "GND",
    }.items():
        p = pico.FindPadByNumber(pad_num)
        if p:
            p.SetNet(nets[net_name])

    for idx, net_name in enumerate(["OUT0", "OUT1", "OUT2", "OUT3", "IN0", "IN1", "+3V3", "GND"], start=1):
        j1.FindPadByNumber(str(idx)).SetNet(nets[net_name])
    j2.FindPadByNumber("1").SetNet(nets["RUN"])
    j2.FindPadByNumber("2").SetNet(nets["GND"])

    # Board outline.
    for start, end in [((10, 26), (105, 26)), ((105, 26), (105, 89)), ((105, 89), (10, 89)), ((10, 89), (10, 26))]:
        sh = pcbnew.PCB_SHAPE(board)
        sh.SetShape(pcbnew.SHAPE_T_SEGMENT)
        sh.SetStart(point(*start))
        sh.SetEnd(point(*end))
        sh.SetWidth(mm(0.1))
        sh.SetLayer(pcbnew.Edge_Cuts)
        board.Add(sh)

    # Signal routing, aligned with the Pico left-side pads.
    pico_pad = {"OUT0": (60.31, 43.49), "OUT1": (60.31, 46.03), "OUT2": (60.31, 48.57), "OUT3": (60.31, 51.11), "IN0": (60.31, 56.19), "IN1": (60.31, 58.73)}
    header_pad = {"OUT0": (28, 43.49), "OUT1": (28, 46.03), "OUT2": (28, 48.57), "OUT3": (28, 51.11), "IN0": (28, 53.65), "IN1": (28, 56.19), "+3V3": (28, 58.73), "GND": (28, 61.27)}

    for name in ["OUT0", "OUT1", "OUT2", "OUT3"]:
        add_polyline(board, nets[name], [header_pad[name], pico_pad[name]])
    add_polyline(board, nets["IN0"], [header_pad["IN0"], (57, 53.65), (57, 56.19), pico_pad["IN0"]])
    add_polyline(board, nets["IN1"], [header_pad["IN1"], (50, 56.19), (50, 58.73), pico_pad["IN1"]])
    add_polyline(board, nets["RUN"], [(79.69, 61.27), (94, 61.27)])
    add_polyline(board, nets["+3V3"], [(79.69, 46.03), (96, 46.03), (96, 86), (24, 86), (24, 58.73), header_pad["+3V3"]], width=0.5)

    board.Add(set_text(board, "magicTool Pico 2 W carrier", 25, 34, 1.2))
    board.Add(set_text(board, "J1: OUT0 OUT1 OUT2 OUT3", 22, 78, 0.8))
    board.Add(set_text(board, "IN0 IN1 3V3 GND", 22, 81, 0.8))
    board.Add(set_text(board, "Antenna keepout: keep copper clear under Pico W antenna", 57, 33, 0.8, pcbnew.Cmts_User))

    # Top and bottom GND pours.
    for layer in [pcbnew.F_Cu]:
        zone = pcbnew.ZONE(board)
        zone.SetLayer(layer)
        zone.SetNet(nets["GND"])
        outline = zone.Outline()
        outline.NewOutline()
        zone.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        for x, y in [(10.5, 26.5), (104.5, 26.5), (104.5, 88.5), (10.5, 88.5), (10.5, 26.5)]:
            outline.Append(mm(x), mm(y))
        board.Add(zone)

    pcbnew.SaveBoard(str(ROOT / f"{PROJECT}.kicad_pcb"), board)


def readme() -> None:
    (ROOT / "README.md").write_text(
        textwrap.dedent(
            """\
            # magicTool KiCad hardware

            This directory contains a generated KiCad 10 project for a small Raspberry Pi Pico 2 W carrier board matching the firmware pin map.

            ## Electrical interface

            - `A1`: Raspberry Pi Pico 2 W module, using KiCad's Pico W SMD hand-solder footprint.
            - `J1`: 8-pin 2.54 mm debug I/O header.
            - `J1.1..J1.4`: `OUT0..OUT3`, mapped to Pico GPIO `2..5`.
            - `J1.5..J1.6`: `IN0..IN1`, mapped directly to Pico GPIO `6..7`.
            - `J1.7`: Pico `+3V3` output for low-current external references only.
            - `J1.8`: `GND`.
            - Inputs use the firmware's internal pulldowns.
            - `J2`: RUN-to-GND reset header.

            The board assumes USB power and USB data come from the Pico module's onboard connector.

            ## Regeneration

            Run this from the repository root:

            ```bash
            python3 hardware/generate_magictool_kicad.py
            ```

            Then refill zones and run KiCad DRC:

            ```bash
            kicad-cli pcb drc --refill-zones --save-board hardware/magictool.kicad_pcb
            ```
            """
        )
    )


def main() -> None:
    pro()
    sch()
    pcb()
    readme()


if __name__ == "__main__":
    main()
