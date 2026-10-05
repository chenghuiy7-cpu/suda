# FHEStore overview v4: simplified architecture and work scope

Author request, 2026-10-04: simplify the overall architecture, align the regions,
strengthen outlines, and give the CSD and Host the same color to distinguish
FHEStore's scope from the external remote HPU service.

## Contents and semantics

- CSD: persistent data, device runtime, input preparation, and result recovery.
- Host: task runtime and one ciphertext relay for both directions.
- Remote HPU: one external service boundary and homomorphic evaluation label.
- CSD and Host share blue borders and pale blue fills; Remote HPU uses gray.
  A bracket labeled `FHEStore (this work)` also identifies the two local regions.
- Task submission crosses PCIe from Host to CSD. Both ciphertext directions cross
  PCIe and then the Host--service network. There is no direct CSD--HPU link.
- Input preparation and result recovery are functional stages. Their optional
  preprocessing and write-back details are explained in the caption and text;
  record merging may run on Host. The overview does not classify write-back as
  an FPGA kernel.

The three outer regions share y=30 and height=420. Data rows are horizontal at
y=274 and y=389. The runtime boxes share y=120 and height=58. Operator-graph,
context, buffer, and transport implementation details are removed from the image
and remain in Chapter 3 where needed.

## Editable source and exports

`fhestore-overview.drawio` contains native regions, shapes, text, and connected
edges; it embeds no image. Open it directly in diagrams.net or draw.io Desktop.
The SVG/PDF/PNG exports are rendered from that same source using the official
diagrams.net viewer. `build_drawio.py` and `export_drawio.py` reproduce the outputs.
`figure.tex` provides the figure, caption, accessible description, and label.

From this directory:

```bash
/usr/bin/python3 build_drawio.py --force
/usr/bin/python3 export_drawio.py --viewer-js /tmp/fhestore-drawio-viewer.min.js --force
```

The export script uses local Chrome and rsvg-convert. The viewer library digest
and native rendering checks are recorded in `render-checks.json`.

## Provenance and placement

This revision edits the existing v3 vector/drawio architecture to implement the
author's simplification request. No new image generation is used. The earlier
generated design reference and prompt remain in v2; v3 and the first Chapter 3
draft are preserved. Implementation evidence remains the code-backed evidence
table in `drafts/section3-v1/README.md`, repository HEAD
`d679e93603f0737f8e489195d5aaf4bba7a17c56`, with existing local changes inspected.

Figure type: system framework with logical data and task-control flows. Placement:
two-column-spanning ACM figure, exported at 178 x 61.5 mm, with a 1650 x 570 unit
canvas. The overview's width carries the CSD--Host--service relationship, two
data directions, transport boundaries, task control, and work-scope bracket.
It is about 31% shorter than the v3 export. Ordinary text is 8.25--8.56 pt;
secondary labels are 7.34--7.65 pt. Outer borders are approximately 1.22 pt.
These are design choices, not asserted venue requirements.

## Validation

The official renderer decoded 35 vertices and 11 connected edges, with no
unresolved terminals, unrendered cells, foreignObject labels, or embedded raster
images. Native text is preserved in SVG and PDF; Arimo and Arimo-Bold are embedded
in the PDF. The scope bracket has no arrowhead and is not a data connector.

Agent visual inspection covered the exported figure at 178 mm and 144 dpi and
the compiled two-column Chapter 3 excerpt, checking labels, borders, aligned data
paths, arrow directions, scope identification, and caption consistency. This is
an author-review artifact, not a submission-readiness or human-signoff claim.
