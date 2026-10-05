#!/usr/bin/env python3
"""Reconstruct the selected FHEStore concept design as editable SVG.

Run with Python 3 (standard library only). rsvg-convert exports a vector PDF
and a 3200 x 1600 review PNG. Outputs have a 178 x 89 mm physical canvas.
Use --force only to regenerate this script's three named outputs.

Design reference: design-reference.png, retained separately beside this script.
The diagram is a functional overview: Write-back denotes the workflow stage,
including host-assisted record merging where that application requires it.
No raster content is included in the reconstructed figure.
"""

import argparse
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET


SVG_NS = "http://www.w3.org/2000/svg"
ET.register_namespace("", SVG_NS)
WIDTH, HEIGHT = 1600, 800
PALETTE = {
    "ink": "#243445",
    "outline": "#33475b",
    "control": "#66727e",
    "region": "#f0f7fc",
    "neutral": "#f5f6f8",
    "storage": "#dfedf9",
    "storage_top": "#eaf4fe",
    "encrypt": "#d9e9fb",
    "blue": "#246ca8",
    "blue_outline": "#285c86",
    "decrypt": "#d9eeeb",
    "teal": "#24837f",
    "cloud": "#f1f7fc",
}


def add(parent, tag, **attrs):
    normalized = {key.replace("_", "-"): str(value) for key, value in attrs.items()}
    return ET.SubElement(parent, f"{{{SVG_NS}}}{tag}", normalized)


def label(parent, text, x, y, size=30, weight=400, anchor="middle", **attrs):
    node = add(parent, "text", x=x, y=y, font_size=size, font_weight=weight,
               text_anchor=anchor, fill=PALETTE["ink"], **attrs)
    node.text = text
    return node


def box(parent, name, x, y, width, height, fill="neutral", stroke="outline", radius=10):
    return add(parent, "rect", id=name, x=x, y=y, width=width, height=height,
               rx=radius, fill=PALETTE[fill], stroke=PALETTE[stroke], stroke_width=3)


def arrow(parent, name, x1, y1, x2, y2, color="outline", width=4, dashed=False):
    attrs = dict(id=name, d=f"M {x1},{y1} L {x2},{y2}",
                 fill="none", stroke=PALETTE[color], stroke_width=width,
                 stroke_linecap="round", marker_end=f"url(#arrow-{color})")
    if dashed:
        attrs["stroke_dasharray"] = "10 9"
    return add(parent, "path", **attrs)


def build_svg():
    root = ET.Element(f"{{{SVG_NS}}}svg", {
        "width": "178mm", "height": "89mm", "viewBox": "0 0 1600 800",
        "version": "1.1", "role": "img", "aria-labelledby": "figure-title figure-description",
    })
    add(root, "title", id="figure-title").text = "FHEStore functional system overview"
    add(root, "desc", id="figure-description").text = (
        "Host application task runtime controls FHEStore through task, graph, and "
        "parameter descriptions. Within FHEStore, persistent data enters optional "
        "preprocessing followed by FPGA encoding and encryption. Input ciphertexts "
        "are transferred through the host to a remote FHE service containing an HPU. "
        "Result ciphertexts return through the host for FPGA decryption and decoding. "
        "Write-back returns recovered results to the same persistent storage. "
        "Solid arrows show data flow; a dashed arrow shows task control."
    )
    add(root, "metadata").text = (
        "Fully editable vector reconstruction of design-reference.png. "
        "Target placement: two-column-spanning, 178 mm x 89 mm. "
        "Minimum text size: 26 SVG units = approximately 8.20 pt at target width. "
        "Functional grouping does not assert that every Write-back operation runs "
        "exclusively on the device. See the companion caption for host-assisted merging."
    )
    defs = add(root, "defs")
    for color in ("outline", "control", "blue", "teal"):
        marker = add(defs, "marker", id=f"arrow-{color}", markerWidth=16,
                     markerHeight=16, viewBox="0 0 16 16", refX=15.5, refY=8,
                     markerUnits="userSpaceOnUse", orient="auto")
        add(marker, "path", d="M 0,0 L 16,8 L 0,16 Z", fill=PALETTE[color])
    add(defs, "style").text = (
        "text {font-family: Arimo, 'Nimbus Sans', 'DejaVu Sans', sans-serif;}"
    )

    add(root, "rect", id="background", x=0, y=0, width=WIDTH, height=HEIGHT, fill="#ffffff")

    # Semantic groups remain individually editable in SVG editors.
    regions = add(root, "g", id="regions")
    box(regions, "fhestore-region", 28, 228, 885, 425, "region", radius=24)
    cloud = (
        "M 1150,430 "
        "C 1138,404 1147,375 1174,359 "
        "C 1160,317 1196,287 1235,287 "
        "C 1248,287 1260,291 1270,297 "
        "C 1297,246 1365,237 1416,270 "
        "C 1442,286 1455,309 1462,331 "
        "C 1506,320 1551,350 1550,393 "
        "C 1590,430 1593,484 1569,520 "
        "C 1580,570 1547,608 1501,604 "
        "C 1459,659 1372,675 1309,643 "
        "C 1287,631 1270,615 1257,594 "
        "C 1226,605 1192,601 1170,588 "
        "C 1138,575 1127,548 1142,521 "
        "C 1119,504 1113,470 1130,445 "
        "C 1135,438 1142,433 1150,430 Z"
    )
    add(regions, "path", id="remote-service-cloud", d=cloud, fill=PALETTE["cloud"],
        stroke=PALETTE["blue_outline"], stroke_width=3.2, stroke_linejoin="round")

    modules = add(root, "g", id="modules-and-icons")
    box(modules, "host-runtime", 370, 35, 355, 96)

    # One shared persistent store, with a recognizable cylinder silhouette.
    store = add(modules, "g", id="persistent-store")
    add(store, "path", d="M 57,399 L 57,588 C 57,623 273,623 273,588 L 273,399 Z",
        fill=PALETTE["storage"], stroke=PALETTE["outline"], stroke_width=3)
    add(store, "ellipse", cx=165, cy=399, rx=108, ry=26,
        fill=PALETTE["storage_top"], stroke=PALETTE["outline"], stroke_width=3)

    box(modules, "preprocess", 342, 389, 193, 82)
    box(modules, "encode-encrypt", 598, 381, 285, 99, "encrypt", "blue_outline")
    box(modules, "decrypt-decode", 598, 546, 285, 95, "decrypt", "teal")
    box(modules, "write-back", 342, 550, 193, 72)

    chip = add(modules, "g", id="hpu-chip", stroke=PALETTE["blue_outline"],
               stroke_width=5, fill="none", stroke_linecap="round")
    for x in (1315, 1353, 1391):
        add(chip, "path", d=f"M {x},415 V 395 M {x},526 V 546")
    for y in (439, 470, 501):
        add(chip, "path", d=f"M 1290,{y} H 1270 M 1416,{y} H 1436")
    add(chip, "rect", x=1290, y=415, width=126, height=111, rx=14,
        fill=PALETTE["storage"], stroke=PALETTE["blue_outline"])

    connectors = add(root, "g", id="data-and-control-connectors")
    arrow(connectors, "task-control", 547.5, 145, 547.5, 226, "control", 3.2, True)
    arrow(connectors, "store-to-preprocess", 273, 430, 340, 430)
    arrow(connectors, "preprocess-to-encrypt", 535, 430, 596, 430)
    arrow(connectors, "input-ciphertext-handoff", 883, 430, 1150, 430, "blue", 5)
    arrow(connectors, "result-ciphertext-handoff", 1170, 588, 885, 588, "teal", 5)
    arrow(connectors, "decrypt-to-write-back", 598, 588, 537, 588)
    arrow(connectors, "write-back-to-store", 342, 588, 275, 588)

    labels = add(root, "g", id="editable-labels")
    label(labels, "Host Application", 547.5, 75, 32, 700)
    label(labels, "Task Runtime", 547.5, 111, 32, 700)
    label(labels, "Task / Graph / Parameters", 570, 190, 27, anchor="start")

    label(labels, "FHEStore", 53, 278, 42, 700, "start")
    label(labels, "FPGA-Based Computational Storage", 53, 314, 28, 600, "start")
    label(labels, "Persistent Data", 165, 506, 27, 700)
    label(labels, "& Results", 165, 543, 27, 700)
    label(labels, "Preprocess", 438.5, 421, 29, 600)
    label(labels, "(optional)", 438.5, 452, 27)
    label(labels, "Encode & Encrypt", 740.5, 423, 30, 700)
    label(labels, "FPGA", 740.5, 462, 28)
    label(labels, "Decrypt & Decode", 740.5, 582, 30, 700)
    label(labels, "FPGA", 740.5, 620, 28)
    label(labels, "Write-back", 438.5, 595, 29, 600)

    label(labels, "Input ciphertexts", 1026, 405, 27)
    label(labels, "Host-mediated", 1032, 500, 26)
    label(labels, "transfer", 1032, 532, 26)
    label(labels, "Result ciphertexts", 1028, 623, 27)
    label(labels, "Remote FHE Service", 1353, 373, 32, 700)
    label(labels, "HPU", 1353, 485, 37, 700)
    label(labels, "Homomorphic Evaluation", 1353, 576, 26)

    legend = add(root, "g", id="legend")
    box(legend, "legend-box", 496, 697, 608, 66, fill="neutral", stroke="control", radius=12)
    arrow(legend, "legend-data-flow", 522, 731, 620, 731, "blue", 4)
    label(legend, "Data flow", 640, 740, 27, anchor="start")
    arrow(legend, "legend-task-control", 798, 731, 896, 731, "control", 3.2, True)
    label(legend, "Task control", 915, 740, 27, anchor="start")

    ET.indent(root, space="  ")
    return ET.tostring(root, encoding="unicode", xml_declaration=True) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true", help="Regenerate this script's existing outputs")
    args = parser.parse_args()
    destination = Path(__file__).resolve().parent
    outputs = [destination / f"fhestore-overview.{suffix}" for suffix in ("svg", "pdf", "png")]
    existing = [str(path) for path in outputs if path.exists()]
    if existing and not args.force:
        parser.error("Refusing to overwrite existing outputs; use --force for intentional regeneration: "
                     + ", ".join(existing))
    renderer = shutil.which("rsvg-convert")
    if renderer is None:
        parser.error("rsvg-convert is required for PDF/PNG export")
    outputs[0].write_text(build_svg(), encoding="utf-8")
    subprocess.run([renderer, "--format", "pdf", "--output", str(outputs[1]), str(outputs[0])], check=True)
    subprocess.run([renderer, "--format", "png", "--width", "3200", "--height", "1600",
                    "--output", str(outputs[2]), str(outputs[0])], check=True)
    for path in outputs:
        print(path)


if __name__ == "__main__":
    main()
