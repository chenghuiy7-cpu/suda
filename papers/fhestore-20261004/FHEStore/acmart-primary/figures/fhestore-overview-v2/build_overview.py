#!/usr/bin/env python3
"""Build the editable three-column FHEStore functional overview.

Standard-library Python 3 and rsvg-convert suffice. The PDF is 178 x 89 mm;
the PNG is 3200 x 1600. --force regenerates only the three named outputs.
The selected visual reference is kept separately as design-reference.png.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

NS = "http://www.w3.org/2000/svg"
ET.register_namespace("", NS)
COLORS = {
    "ink": "#283441", "edge": "#687684", "data": "#315b7e",
    "control": "#707982", "csd": "#f3f7fa", "host": "#f7f8fa",
    "box": "#ffffff", "operator": "#e5eef5", "storage": "#edf3f7",
    "cloud": "#f2f6fa",
}


def add(parent, tag, **attrs):
    return ET.SubElement(parent, f"{{{NS}}}{tag}", {
        key.replace("_", "-"): str(value) for key, value in attrs.items()
    })


def text(parent, content, x, y, size=26, weight=400, anchor="middle"):
    node = add(parent, "text", x=x, y=y, font_size=size, font_weight=weight,
               text_anchor=anchor, fill=COLORS["ink"])
    node.text = content
    return node


def rect(parent, ident, x, y, w, h, fill="box", radius=5, width=2):
    return add(parent, "rect", id=ident, x=x, y=y, width=w, height=h,
               rx=radius, fill=COLORS[fill], stroke=COLORS["edge"], stroke_width=width)


def line(parent, ident, points, control=False):
    color = "control" if control else "data"
    path = "M " + " L ".join(f"{x},{y}" for x, y in points)
    attrs = dict(id=ident, d=path, fill="none", stroke=COLORS[color],
                 stroke_width=2.5, marker_end=f"url(#arrow-{color})",
                 stroke_linejoin="round")
    if control:
        attrs["stroke_dasharray"] = "8 6"
    return add(parent, "path", **attrs)


def svg_source():
    svg = ET.Element(f"{{{NS}}}svg", {
        "width": "178mm", "height": "89mm", "viewBox": "0 0 1600 800",
        "version": "1.1", "role": "img", "aria-labelledby": "title desc",
    })
    add(svg, "title", id="title").text = "FHEStore with explicit host-controlled execution and ciphertext relays"
    add(svg, "desc", id="desc").text = (
        "Three columns show FHEStore computational storage, host software, and a "
        "remote FHE service. The host application invokes a task runtime describing "
        "an operator graph, contexts, and memory ranges. Dashed control arrows submit "
        "a task to the device runtime, which configures FPGA operators. The upper "
        "solid data path runs from persistent storage through optional preprocessing "
        "and FPGA encoding/encryption, through the host input ciphertext relay, to "
        "the remote FHE service. Result ciphertexts traverse the lower path in the "
        "opposite direction through the host result ciphertext relay, FPGA "
        "decryption/decoding, and write-back to the same persistent store."
    )
    add(svg, "metadata").text = (
        "Editable SVG reconstructed using design-reference.png; no embedded raster. "
        "Functional overview, 178 x 89 mm. Minimum font: 26 viewBox units, 8.20 pt. "
        "Write-back includes host-assisted record merging where required."
    )
    defs = add(svg, "defs")
    add(defs, "style").text = "text {font-family: Arimo, 'Nimbus Sans', 'DejaVu Sans', sans-serif;}"
    for color in ("data", "control"):
        marker = add(defs, "marker", id=f"arrow-{color}", markerWidth=12,
                     markerHeight=12, viewBox="0 0 12 12", refX=11.8, refY=6,
                     markerUnits="userSpaceOnUse", orient="auto")
        add(marker, "path", d="M 0,0 L 12,6 L 0,12 Z", fill=COLORS[color])
    add(svg, "rect", x=0, y=0, width=1600, height=800, fill="#ffffff")

    regions = add(svg, "g", id="regions")
    rect(regions, "fhestore-boundary", 20, 31, 724, 664, "csd", 10, 1.8)
    rect(regions, "host-boundary", 786, 31, 426, 664, "host", 10, 1.8)
    data_region = rect(regions, "storage-side-data-path-boundary", 34, 407, 696, 276, "csd", 0, 1.5)
    data_region.set("stroke-dasharray", "8 6")
    relay_region = rect(regions, "host-ciphertext-relay-boundary", 806, 407, 388, 276, "host", 0, 1.5)
    relay_region.set("stroke-dasharray", "8 6")
    cloud = (
        "M 1260,500 C 1249,473 1266,443 1296,437 "
        "C 1302,394 1327,376 1356,382 C 1383,354 1435,363 1460,394 "
        "C 1498,385 1533,409 1530,451 C 1572,467 1585,505 1566,537 "
        "C 1586,565 1572,604 1542,615 C 1541,646 1513,670 1476,662 "
        "C 1442,687 1399,684 1376,662 C 1332,672 1298,661 1275,626 "
        "C 1236,618 1228,581 1245,552 C 1219,532 1227,515 1260,500 Z"
    )
    add(regions, "path", id="remote-service-cloud", d=cloud,
        fill=COLORS["cloud"], stroke=COLORS["edge"], stroke_width=2,
        stroke_linejoin="round")

    modules = add(svg, "g", id="modules")
    rect(modules, "host-application", 851, 107, 300, 64)
    rect(modules, "host-task-runtime", 820, 206, 362, 175)
    rect(modules, "device-runtime", 224, 213, 300, 76)
    rect(modules, "preprocess", 280, 465, 172, 70)
    rect(modules, "encode-encrypt", 489, 455, 230, 90, "operator")
    rect(modules, "input-ciphertext-relay", 835, 465, 330, 70)
    rect(modules, "result-ciphertext-relay", 835, 591, 330, 70)
    rect(modules, "decrypt-decode", 489, 581, 230, 90, "operator")
    rect(modules, "write-back", 280, 591, 172, 70)

    store = add(modules, "g", id="persistent-store")
    add(store, "path", d="M 46,487 V 635 C 46,665 230,665 230,635 V 487 Z",
        fill=COLORS["storage"], stroke=COLORS["edge"], stroke_width=2)
    add(store, "ellipse", cx=138, cy=487, rx=92, ry=21,
        fill=COLORS["storage"], stroke=COLORS["edge"], stroke_width=2)

    chip = add(modules, "g", id="hpu-chip", stroke=COLORS["edge"],
               stroke_width=2, fill="none")
    rect(chip, "hpu-icon", 1358, 510, 104, 70, "box", 3)

    connections = add(svg, "g", id="connections")
    line(connections, "application-to-task-runtime", [(1001, 171), (1001, 204)], True)
    line(connections, "submit-task", [(820, 251), (526, 251)], True)
    line(connections, "configure-fpga-operators",
         [(374, 289), (374, 405)], True)
    line(connections, "store-to-preprocess", [(230, 500), (278, 500)])
    line(connections, "preprocess-to-encrypt", [(452, 500), (487, 500)])
    line(connections, "encrypt-to-host-input-relay", [(719, 500), (833, 500)])
    line(connections, "host-input-relay-to-service", [(1165, 500), (1260, 500)])
    line(connections, "service-to-host-result-relay", [(1275, 626), (1167, 626)])
    line(connections, "host-result-relay-to-decrypt", [(835, 626), (721, 626)])
    line(connections, "decrypt-to-write-back", [(489, 626), (454, 626)])
    line(connections, "write-back-to-store", [(280, 626), (232, 626)])

    labels = add(svg, "g", id="editable-labels")
    text(labels, "FHEStore CSD", 45, 74, 34, 700, "start")
    text(labels, "FPGA computational storage", 45, 109, 26, anchor="start")
    text(labels, "Host", 999, 74, 32, 700)
    text(labels, "Application", 1001, 147, 28)
    text(labels, "Task Runtime", 1001, 240, 29, 600)
    text(labels, "Operator graph", 1001, 286)
    text(labels, "Operator contexts", 1001, 321)
    text(labels, "Data ranges", 1001, 356)
    text(labels, "Device Runtime", 374, 261, 27, 600)
    text(labels, "Submit task", 672, 232)
    text(labels, "Configure / execute", 392, 353, anchor="start")
    text(labels, "Storage-side data path", 53, 437, 26, 600, "start")
    text(labels, "Ciphertext relay", 825, 437, 26, 600, "start")
    text(labels, "Persistent", 138, 566)
    text(labels, "data & results", 138, 599)
    text(labels, "Preprocess", 366, 490)
    text(labels, "(optional)", 366, 522)
    text(labels, "Encode & Encrypt", 604, 488)
    text(labels, "FPGA", 604, 527)
    text(labels, "Input ciphertext relay", 1000, 510)
    text(labels, "Result ciphertext relay", 1000, 636)
    text(labels, "Decrypt & Decode", 604, 614)
    text(labels, "FPGA", 604, 653)
    text(labels, "Write-back", 366, 636)
    text(labels, "Remote FHE Service", 1410, 466, 26, 600)
    text(labels, "HPU", 1410, 555, 29)
    text(labels, "Homomorphic", 1410, 619)
    text(labels, "evaluation", 1410, 652)

    legend = add(svg, "g", id="legend")
    line(legend, "legend-data", [(540, 753), (602, 753)])
    text(legend, "Data flow", 617, 762, anchor="start")
    line(legend, "legend-control", [(806, 753), (868, 753)], True)
    text(legend, "Task control", 883, 762, anchor="start")
    ET.indent(svg, "  ")
    return ET.tostring(svg, encoding="unicode", xml_declaration=True) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    folder = Path(__file__).resolve().parent
    outputs = [folder / f"fhestore-overview.{ext}" for ext in ("svg", "pdf", "png")]
    if any(path.exists() for path in outputs) and not args.force:
        parser.error("Outputs already exist; use --force for intentional regeneration")
    renderer = shutil.which("rsvg-convert")
    if not renderer:
        parser.error("rsvg-convert is required")
    outputs[0].write_text(svg_source(), encoding="utf-8")
    subprocess.run([renderer, "-f", "pdf", "-o", str(outputs[1]), str(outputs[0])], check=True)
    subprocess.run([renderer, "-f", "png", "-w", "3200", "-h", "1600",
                    "-o", str(outputs[2]), str(outputs[0])], check=True)
    for path in outputs:
        print(path)


if __name__ == "__main__":
    main()
