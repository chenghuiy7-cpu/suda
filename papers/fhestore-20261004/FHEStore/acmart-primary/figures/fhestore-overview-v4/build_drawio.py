#!/usr/bin/env python3
"""Simplify the existing native drawio overview; export at 178 x 61.5 mm."""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET

BLUE = "#2C5573"
GRAY = "#5D636B"
BASE = (
    "html=0;whiteSpace=wrap;fontFamily=Arial;fontSize=28;fontColor=#22303C;"
    "strokeColor=#2C5573;strokeWidth=2.8;shadow=0;rounded=0;"
    "align=center;verticalAlign=middle;spacing=4;"
)


class Diagram:
    def __init__(self):
        self.file = ET.Element("mxfile", host="app.diagrams.net", compressed="false", type="device")
        page = ET.SubElement(self.file, "diagram", id="fhestore-overview-v4", name="FHEStore Overview")
        model = ET.SubElement(page, "mxGraphModel", dx="1650", dy="570", grid="1",
                              gridSize="10", guides="1", tooltips="1", connect="1",
                              arrows="1", fold="1", page="1", pageScale="1",
                              pageWidth="1650", pageHeight="570", math="0", shadow="0",
                              background="#FFFFFF")
        self.root = ET.SubElement(model, "root")
        ET.SubElement(self.root, "mxCell", id="0")
        ET.SubElement(self.root, "mxCell", id="1", parent="0")

    def vertex(self, ident, value, x, y, width, height, parent="1", style="", fill="#FFFFFF"):
        cell = ET.SubElement(self.root, "mxCell", id=ident, value=value, parent=parent,
                             vertex="1", style=BASE + f"fillColor={fill};" + style)
        ET.SubElement(cell, "mxGeometry", x=str(x), y=str(y), width=str(width),
                      height=str(height), attrib={"as": "geometry"})
        return cell

    def label(self, ident, value, x, y, width, height, parent="1", bold=False, size=28,
              align="center", color="#22303C"):
        return self.vertex(ident, value, x, y, width, height, parent,
                           f"text;strokeColor=none;fillColor=none;fontSize={size};"
                           f"fontStyle={1 if bold else 0};fontColor={color};align={align};"
                           "spacing=0;connectable=0;")

    def edge(self, ident, source, target, source_port, target_port, control=False,
             arrow=True, points=()):
        sx, sy = source_port
        tx, ty = target_port
        style = (
            "edgeStyle=none;rounded=0;html=0;"
            f"strokeWidth={2.8 if control else 3.8};strokeColor={GRAY if control else BLUE};"
            f"endArrow={'classic' if arrow else 'none'};endFill=1;endSize=12;"
            f"exitX={sx};exitY={sy};exitDx=0;exitDy=0;exitPerimeter=0;"
            f"entryX={tx};entryY={ty};entryDx=0;entryDy=0;entryPerimeter=0;"
        )
        if control:
            style += "dashed=1;dashPattern=7 5;"
        cell = ET.SubElement(self.root, "mxCell", id=ident, value="", edge="1", parent="1",
                             source=source, target=target, style=style)
        geometry = ET.SubElement(cell, "mxGeometry", relative="1", attrib={"as": "geometry"})
        if points:
            array = ET.SubElement(geometry, "Array", attrib={"as": "points"})
            for x, y in points:
                ET.SubElement(array, "mxPoint", x=str(x), y=str(y))

    def save(self, path):
        ET.indent(self.file, space="  ")
        ET.ElementTree(self.file).write(path, encoding="utf-8", xml_declaration=True)


def build():
    d = Diagram()
    d.vertex("page-canvas", "", 0, 0, 1650, 570,
             style="strokeColor=none;connectable=0;selectable=0;")
    # The same outline and fill identify the two parts covered by this work.
    d.vertex("csd-region", "", 25, 30, 600, 420, fill="#F1F6FA",
             style="container=1;strokeWidth=4;")
    d.vertex("host-region", "", 760, 30, 350, 420, fill="#F1F6FA",
             style="container=1;strokeWidth=4;")
    d.vertex("remote-service", "", 1260, 30, 365, 420, fill="#F2F2F2",
             style=f"container=1;strokeColor={GRAY};strokeWidth=4;")
    d.label("csd-title", "FHEStore CSD", 24, 16, 540, 45, "csd-region", True, 34, "left", BLUE)
    d.label("host-title", "Host", 24, 16, 302, 45, "host-region", True, 34, "left", BLUE)
    d.label("remote-title", "Remote HPU", 24, 16, 317, 45, "remote-service", True, 34, "left", GRAY)
    d.label("remote-subtitle", "External FHE service", 24, 64, 317, 35,
            "remote-service", size=27, align="left", color=GRAY)

    # Aligned runtime row: the application is represented by its task runtime.
    d.vertex("device-runtime", "Device runtime", 225, 90, 350, 58, "csd-region")
    d.vertex("host-task-runtime", "Task runtime", 25, 90, 300, 58, "host-region")
    d.edge("runtime-submits-task-pcie", "host-task-runtime", "device-runtime", (0, .5), (1, .5), True)
    d.label("submit-task-label", "Submit task", 630, 99, 125, 32, size=24)
    d.label("pcie-label", "PCIe", 637, 185, 112, 35, bold=True)
    d.label("network-label", "Network", 1117, 185, 136, 35, bold=True)

    # Only the two system functions are expanded; their internal kernels remain in text.
    d.vertex("persistent-store", "Persistent\ndata", 30, 205, 150, 193, "csd-region",
             style="shape=cylinder;size=0.15;")
    d.vertex("input-preparation", "", 225, 205, 350, 78, "csd-region", style="container=1;")
    d.label("input-title", "Input preparation", 8, 9, 334, 30, "input-preparation", True)
    d.label("input-operations", "Encode & Encrypt", 8, 39, 334, 27,
            "input-preparation", size=25)
    d.vertex("result-recovery", "", 225, 320, 350, 78, "csd-region", style="container=1;")
    d.label("result-title", "Result recovery", 8, 9, 334, 30, "result-recovery", True)
    d.label("result-operations", "Decrypt & Decode", 8, 39, 334, 27,
            "result-recovery", size=25)
    d.edge("device-executes-input", "device-runtime", "input-preparation", (.5, 1), (.5, 0), True)
    d.label("execute-label", "Execute", 445, 182, 124, 35, size=26, align="left")

    d.vertex("ciphertext-relay", "", 25, 205, 300, 193, "host-region", style="container=1;")
    d.label("input-ciphertexts", "Input ciphertexts", 8, 22, 284, 34, "ciphertext-relay", size=27)
    d.label("relay-title", "Ciphertext relay", 8, 80, 284, 34, "ciphertext-relay", True)
    d.label("result-ciphertexts", "Result ciphertexts", 8, 137, 284, 34, "ciphertext-relay", size=27)
    d.label("homomorphic-evaluation", "Homomorphic\nevaluation", 24, 275, 317, 70,
            "remote-service", size=30)

    # Exactly horizontal data rows at y=274 and y=389, both routed through Host.
    top_port, bottom_port = 39 / 193, 154 / 193
    d.edge("store-to-input", "persistent-store", "input-preparation", (1, top_port), (0, .5))
    d.edge("input-to-relay-pcie", "input-preparation", "ciphertext-relay", (1, .5), (0, top_port))
    d.edge("relay-to-remote-network", "ciphertext-relay", "remote-service", (1, top_port), (0, 244 / 420))
    d.edge("remote-to-relay-network", "remote-service", "ciphertext-relay", (0, 359 / 420), (1, bottom_port))
    d.edge("relay-to-recovery-pcie", "ciphertext-relay", "result-recovery", (0, bottom_port), (1, .5))
    d.edge("recovery-to-store", "result-recovery", "persistent-store", (0, .5), (1, bottom_port))

    # Redundant scope text keeps the distinction clear in monochrome reproduction.
    for ident, x, y in (("scope-left", 25, 468), ("scope-right", 1110, 468),
                       ("legend-data-start", 630, 545), ("legend-data-end", 702, 545),
                       ("legend-control-start", 947, 545), ("legend-control-end", 1019, 545)):
        d.vertex(ident, "", x, y, 0, 0, style="opacity=0;strokeColor=none;fillColor=none;")
    d.edge("fhestore-scope-bracket", "scope-left", "scope-right", (.5, .5), (.5, .5),
           arrow=False, points=((25, 480), (1110, 480)))
    d.label("fhestore-scope-label", "FHEStore (this work)", 25, 486, 1085, 33,
            bold=True, size=27, color=BLUE)
    d.edge("legend-data-arrow", "legend-data-start", "legend-data-end", (.5, .5), (.5, .5))
    d.edge("legend-control-arrow", "legend-control-start", "legend-control-end", (.5, .5), (.5, .5), True)
    d.label("legend-data-label", "Data flow", 721, 529, 191, 34, size=27, align="left")
    d.label("legend-control-label", "Task control", 1037, 529, 232, 34, size=27, align="left")
    return d


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    output = Path(__file__).resolve().parent / "fhestore-overview.drawio"
    if output.exists() and not args.force:
        parser.error("Output exists; use --force to regenerate")
    d = build()
    cells = d.root.findall("mxCell")
    ids = {cell.get("id") for cell in cells}
    edges = [cell for cell in cells if cell.get("edge") == "1"]
    assert all(e.get("source") in ids and e.get("target") in ids for e in edges)
    assert not any("shape=image" in c.get("style", "") for c in cells)
    d.save(output)
    print(f"{output}\nNative cells: {len(cells)}; connected edges: {len(edges)}; embedded images: 0")


if __name__ == "__main__":
    main()
