#!/usr/bin/env python3
"""Compact horizontal stacks with both stage shares explicitly annotated.

Mean shares are supplied by the author. Original min/max marks are clipped
from the source PNG and rotated so its percentage axis becomes horizontal.
No numerical uncertainty bounds are inferred or synthesized.
"""
import base64
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
SOURCE = ROOT.parent / "input-preparation-motivation-v1"
facts = json.loads((SOURCE / "source-info.json").read_text())
original = base64.b64encode((SOURCE / "author-upload.png").read_bytes()).decode()
WIDTH, HEIGHT = 242, 133
PLOT_WIDTH, BAR_HEIGHT = 100, 8
BLUE, ORANGE = "#477EAD", "#DF824D"
COMP_LABEL = "#A85125"
operations = ["bitwise_and", "addition", "multiplication"]
labels = ["AND", "Add", "Mul"]
rows = [37, 60, 83]
# Original-image coordinates identify marks, not numerical min/max bounds.
panels = [
    ("cpu_only", [208, 485, 623.5], 26, "(a) CPU-only"),
    ("hpu_accelerated", [944.5, 1221.5, 1360.5], 138, "(b) CPU–HPU"),
]
svg = [f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{WIDTH}pt" height="{HEIGHT}pt" viewBox="0 0 {WIDTH} {HEIGHT}">',
       '<title>Latency shares for CPU-only and CPU–HPU workflows</title>',
       '<desc>Three horizontal 100 percent stacked bars in each of two side-by-side panels show AND, Add, and Mul. Each bar has both mean shares annotated beneath it: preparation at the left in blue and homomorphic computation at the right in dark orange. Both panels use identical host CPU data preparation. Original min/max marks are rotated onto the horizontal percentage axis.</desc>',
       '<defs>',
       f'<pattern id="hatch" width="4" height="4" patternUnits="userSpaceOnUse"><rect width="4" height="4" fill="{BLUE}"/><path d="M-1,1 L1,-1 M0,4 L4,0 M3,5 L5,3" stroke="white" stroke-width="0.55"/></pattern>',
       f'<image id="original" width="1504" height="639" xlink:href="data:image/png;base64,{original}"/>']
for panel, centers, plot_x, title in panels:
    for i, center in enumerate(centers):
        share = facts["input_preparation_percent"][panel][operations[i]]
        source_boundary = 444-359*share/100
        lower, upper = (430, 450) if panel == "cpu_only" else (source_boundary-18, source_boundary+18)
        svg.append(f'<clipPath id="err-{panel}-{i}" clipPathUnits="userSpaceOnUse"><rect x="{center-10}" y="{lower}" width="20" height="{upper-lower}"/></clipPath>')
svg.extend(['</defs>', f'<rect width="{WIDTH}" height="{HEIGHT}" fill="white"/>'])

def text(x, y, value, size=8, fill="#222222", anchor="middle", extra=""):
    svg.append(f'<text x="{x:.3f}" y="{y:.3f}" font-family="Liberation Serif, Times New Roman, serif" font-weight="bold" font-size="{size}" fill="{fill}" text-anchor="{anchor}" {extra}>{value}</text>')

svg.append('<rect x="18" y="1" width="7" height="7" fill="url(#hatch)"/>')
text(29, 8, "Data preparation", anchor="start")
svg.append(f'<rect x="116" y="1" width="7" height="7" fill="{ORANGE}"/>')
text(127, 8, "Homomorphic computation", anchor="start")
for y, label in zip(rows, labels):
    text(20, y+2.8, label, anchor="end")

for panel, centers, plot_x, title in panels:
    text(plot_x+PLOT_WIDTH/2, 23, title, 9)
    for tick in [0, 50, 100]:
        x = plot_x+PLOT_WIDTH*tick/100
        svg.append(f'<path d="M{x:.3f},29 V105" stroke="#DDDDDD" stroke-width="0.45"/>')
        anchor = "start" if tick == 0 else "end" if tick == 100 else "middle"
        text(x, 116, str(tick), anchor=anchor)
    for i, (center, row_y) in enumerate(zip(centers, rows)):
        share = facts["input_preparation_percent"][panel][operations[i]]
        prep_width = PLOT_WIDTH*share/100
        svg.append(f'<rect x="{plot_x}" y="{row_y-BAR_HEIGHT/2}" width="{PLOT_WIDTH}" height="{BAR_HEIGHT}" fill="{ORANGE}"/>')
        svg.append(f'<rect x="{plot_x}" y="{row_y-BAR_HEIGHT/2}" width="{prep_width:.4f}" height="{BAR_HEIGHT}" fill="url(#hatch)"/>')
        # x = plot_x + (444-y_source) * k maps source y=444/85 to 0/100%.
        # The other dimension is only cap styling; it carries no data values.
        k, cap_scale = PLOT_WIDTH/359, 0.25
        tx, ty = plot_x+444*k, row_y-center*cap_scale
        svg.append(f'<g transform="matrix(0 {cap_scale} {-k} 0 {tx} {ty})"><g clip-path="url(#err-{panel}-{i})"><use xlink:href="#original"/></g></g>')
        value = f"{share:.2f}" if panel == "cpu_only" else f"{share:.1f}"
        comp = f"{100-share:.2f}" if panel == "cpu_only" else f"{100-share:.1f}"
        text(plot_x+2, row_y+12, value, fill=BLUE, anchor="start")
        text(plot_x+PLOT_WIDTH-2, row_y+12, comp, fill=COMP_LABEL, anchor="end")
    svg.append(f'<path d="M{plot_x},107 H{plot_x+PLOT_WIDTH}" stroke="#333333" stroke-width="0.75"/>')

text(132, 130, "Latency share (%)", 9)
svg.append('</svg>')
(ROOT/"input-preparation-motivation.svg").write_text("\n".join(svg))
subprocess.run(["rsvg-convert", "--format=pdf", "--output", str(ROOT/"input-preparation-motivation.pdf"), str(ROOT/"input-preparation-motivation.svg")], check=True)
