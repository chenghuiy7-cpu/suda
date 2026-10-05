#!/usr/bin/env python3
"""Build a native, uncompressed, fully editable diagrams.net figure.

Canvas: 1800 x 900 units, intended to export at 178 x 89 mm.
29-unit labels become 8.13 pt at that width. Every diagram component is
an mxCell, and every connector has native source/target terminals.
No SVG/PNG or external image is embedded. The figure uses PCIe for the
CSD--Host links and Network for the Host--remote-service links.
"""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET


COLORS = {
    "text": "#283441", "edge": "#687684", "data": "#315B7E",
    "control": "#707982", "csd": "#F3F7FA", "host": "#F7F8FA",
    "operator": "#E5EEF5", "band": "#F1F2F4", "white": "#FFFFFF",
}
BASE = (
    "html=0;whiteSpace=wrap;fontFamily=Arial;fontSize=29;fontColor=#283441;"
    "strokeColor=#687684;strokeWidth=2;shadow=0;rounded=0;"
    "align=center;verticalAlign=middle;spacing=4;"
)


class Diagram:
    def __init__(self):
        self.file = ET.Element("mxfile", host="app.diagrams.net", compressed="false", type="device")
        page = ET.SubElement(self.file, "diagram", id="fhestore-overview-v3", name="FHEStore Overview")
        model = ET.SubElement(page, "mxGraphModel", dx="1800", dy="900", grid="1",
                              gridSize="10", guides="1", tooltips="1", connect="1",
                              arrows="1", fold="1", page="1", pageScale="1",
                              pageWidth="1800", pageHeight="900", math="0", shadow="0",
                              background="#FFFFFF")
        self.root = ET.SubElement(model, "root")
        ET.SubElement(self.root, "mxCell", id="0")
        ET.SubElement(self.root, "mxCell", id="1", parent="0")

    def vertex(self, ident, value, x, y, width, height, parent="1", style="", fill="white"):
        cell = ET.SubElement(self.root, "mxCell", id=ident, value=value, parent=parent,
                             vertex="1", style=BASE + f"fillColor={COLORS[fill]};" + style)
        ET.SubElement(cell, "mxGeometry", x=str(x), y=str(y), width=str(width),
                      height=str(height), attrib={"as": "geometry"})
        return cell

    def label(self, ident, value, x, y, width, height, parent="1", bold=False, size=29,
              align="center"):
        style = (f"text;strokeColor=none;fillColor=none;fontSize={size};fontStyle={1 if bold else 0};"
                 f"align={align};spacing=0;connectable=0;")
        return self.vertex(ident, value, x, y, width, height, parent, style)

    def edge(self, ident, source, target, source_port, target_port, control=False, parent="1"):
        sx, sy = source_port
        tx, ty = target_port
        color = COLORS["control" if control else "data"]
        style = (
            "edgeStyle=none;rounded=0;html=0;strokeWidth=2.2;"
            f"strokeColor={color};endArrow=classic;endFill=1;endSize=12;"
            f"exitX={sx};exitY={sy};exitDx=0;exitDy=0;exitPerimeter=0;"
            f"entryX={tx};entryY={ty};entryDx=0;entryDy=0;entryPerimeter=0;"
        )
        if control:
            style += "dashed=1;dashPattern=8 6;"
        cell = ET.SubElement(self.root, "mxCell", id=ident, value="", edge="1", parent=parent,
                             source=source, target=target, style=style)
        ET.SubElement(cell, "mxGeometry", relative="1", attrib={"as": "geometry"})

    def save(self, path):
        ET.indent(self.file, space="  ")
        ET.ElementTree(self.file).write(path, encoding="utf-8", xml_declaration=True)


def build():
    d = Diagram()
    d.vertex("page-canvas", "", 0, 0, 1800, 900,
             style="strokeColor=none;connectable=0;selectable=0;")

    # Shared interconnect bands are annotations, never execution modules.
    d.vertex("pcie-interconnect-band", "", 782, 198, 82, 584, fill="band",
             style="strokeColor=none;connectable=0;")
    d.vertex("network-interconnect-band", "", 1343, 198, 82, 584, fill="band",
             style="strokeColor=none;connectable=0;")
    d.label("pcie-link-label", "PCIe", 765, 151, 115, 44)
    d.label("network-link-label", "Network", 1325, 151, 115, 44)

    d.vertex("csd-region", "", 20, 32, 745, 772, fill="csd", style="container=1;")
    d.label("csd-region-title", "FHEStore CSD", 25, 23, 500, 48, "csd-region", True, 36, "left")
    d.label("csd-region-subtitle", "FPGA computational storage", 25, 72, 660, 40,
            "csd-region", size=29, align="left")
    d.vertex("host-region", "", 880, 32, 445, 772, fill="host", style="container=1;")
    d.label("host-region-title", "Host", 20, 23, 405, 48, "host-region", True, 36)

    d.vertex("host-application", "Application", 64, 80, 317, 60, "host-region")
    d.vertex("host-task-runtime", "Task Runtime", 21, 161, 403, 244, "host-region",
             style="container=1;verticalAlign=top;spacingTop=12;fontSize=31;fontStyle=1;")
    d.label("runtime-operator-graph-label", "Operator graph", 22, 54, 359, 38, "host-task-runtime")
    d.vertex("logical-filter", "Filter", 69, 101, 118, 53, "host-task-runtime")
    d.vertex("logical-encrypt", "Encrypt", 226, 101, 124, 53, "host-task-runtime")
    d.label("runtime-contexts-label", "Operator contexts", 20, 163, 363, 36, "host-task-runtime")
    d.label("runtime-ranges-label", "Data ranges", 20, 203, 363, 36, "host-task-runtime")
    d.vertex("device-runtime", "Device Runtime", 215, 178, 315, 86, "csd-region",
             style="fontStyle=1;")

    # The group covers the functional storage-side path, not FPGA-only execution.
    d.vertex("storage-side-data-path", "Storage-side data path", 14, 427, 715, 330,
             "csd-region", fill="csd",
             style="container=1;dashed=1;dashPattern=8 6;strokeWidth=1.5;"
                   "align=left;verticalAlign=top;spacingLeft=18;spacingTop=8;fontStyle=1;")
    d.vertex("persistent-store", "Persistent data\n& results", 11, 66, 203, 239,
             "storage-side-data-path", fill="host", style="shape=cylinder;size=0.15;")
    d.vertex("preprocess", "Preprocess\n(optional)", 252, 59, 174, 84,
             "storage-side-data-path")
    d.vertex("encode-encrypt", "Encode & Encrypt\nFPGA", 463, 52, 249, 98,
             "storage-side-data-path", fill="operator")
    d.vertex("decrypt-decode", "Decrypt & Decode\nFPGA", 463, 220, 249, 98,
             "storage-side-data-path", fill="operator")
    d.vertex("write-back", "Write-back", 252, 233, 174, 72, "storage-side-data-path")

    d.vertex("ciphertext-relay-group", "Ciphertext relay", 14, 427, 417, 330, "host-region",
             fill="host", style="container=1;dashed=1;dashPattern=8 6;strokeWidth=1.5;"
                                "align=left;verticalAlign=top;spacingLeft=18;spacingTop=8;fontStyle=1;")
    d.vertex("input-ciphertext-relay", "Input ciphertext relay", 22, 65, 377, 72,
             "ciphertext-relay-group")
    d.vertex("result-ciphertext-relay", "Result ciphertext relay", 22, 233, 377, 72,
             "ciphertext-relay-group")

    # Native cloud ports (.25,.25) and (.13,.77) lie on its actual contour.
    cloud_height = 168 / (.77 - .25)
    cloud_top = 560 - .25 * cloud_height
    d.vertex("remote-service", "", 1440, cloud_top, 340, cloud_height, fill="csd",
             style="shape=cloud;container=1;")
    d.label("remote-service-title", "Remote FHE\nService", 65, 60, 235, 66,
            "remote-service", True, 29)
    d.vertex("remote-hpu", "HPU", 131, 136, 104, 60, "remote-service")
    d.label("homomorphic-evaluation-label", "Homomorphic\nevaluation", 51, 198, 264, 74,
            "remote-service")

    # Main control edges: host application -> host runtime -> CSD runtime -> path.
    d.edge("application-invokes-runtime", "host-application", "host-task-runtime", (.5, 1), (.5, 0), True)
    d.edge("runtime-submits-task", "host-task-runtime", "device-runtime",
           (0, (253 - 193) / 244), (1, .5), True)
    d.edge("device-configures-execution", "device-runtime", "storage-side-data-path",
           (.5, 1), ((392.5 - 34) / 715, 0), True)
    d.label("submit-task-label", "Submit task", 560, 211, 205, 36)
    d.label("configure-execute-label", "Configure / execute", 410, 355, 300, 40, align="left")
    d.edge("logical-filter-to-encrypt", "logical-filter", "logical-encrypt", (1, .5), (0, .5),
           parent="host-task-runtime")

    # The two ciphertext paths explicitly cross PCIe and Network in order.
    d.edge("store-to-preprocess", "persistent-store", "preprocess", (1, (560 - 525) / 239), (0, .5))
    d.edge("preprocess-to-encrypt", "preprocess", "encode-encrypt", (1, .5), (0, .5))
    d.edge("encrypt-to-host-relay-pcie", "encode-encrypt", "input-ciphertext-relay", (1, .5), (0, .5))
    d.edge("host-input-relay-to-service-network", "input-ciphertext-relay", "remote-service", (1, .5), (.25, .25))
    d.edge("service-to-host-result-relay-network", "remote-service", "result-ciphertext-relay", (.13, .77), (1, .5))
    d.edge("host-result-relay-to-decrypt-pcie", "result-ciphertext-relay", "decrypt-decode", (0, .5), (1, .5))
    d.edge("decrypt-to-write-back", "decrypt-decode", "write-back", (0, .5), (1, .5))
    d.edge("write-back-to-store", "write-back", "persistent-store", (0, .5), (1, (728 - 525) / 239))

    # Native endpoints also keep legend arrows directly editable.
    for ident, x in (("legend-data-start", 570), ("legend-data-end", 660),
                     ("legend-control-start", 938), ("legend-control-end", 1028)):
        d.vertex(ident, "", x, 854, 1, 1, style="opacity=0;strokeColor=none;fillColor=none;")
    d.edge("legend-data-arrow", "legend-data-start", "legend-data-end", (.5, .5), (.5, .5))
    d.edge("legend-control-arrow", "legend-control-start", "legend-control-end", (.5, .5), (.5, .5), True)
    d.label("legend-data-label", "Data flow", 684, 835, 215, 42, align="left")
    d.label("legend-control-label", "Task control", 1053, 835, 255, 42, align="left")
    return d


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true", help="Intentionally regenerate this script's drawio file")
    args = parser.parse_args()
    output = Path(__file__).resolve().parent / "fhestore-overview.drawio"
    if output.exists() and not args.force:
        parser.error("Refusing to overwrite existing output; use --force to regenerate")
    diagram = build()
    cells = diagram.root.findall("mxCell")
    ids = {cell.attrib["id"] for cell in cells}
    edges = [cell for cell in cells if cell.get("edge") == "1"]
    assert all(edge.get("source") in ids and edge.get("target") in ids for edge in edges)
    assert not any("data:image" in cell.get("style", "") or "shape=image" in cell.get("style", "") for cell in cells)
    diagram.save(output)
    print(output)
    print(f"Native cells: {len(cells)}; connected edges: {len(edges)}; embedded images: 0")


if __name__ == "__main__":
    main()
