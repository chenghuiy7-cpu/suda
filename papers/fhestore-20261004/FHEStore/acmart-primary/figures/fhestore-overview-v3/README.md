# FHEStore system overview, v3: PCIe and network

The author requested explicit CSD--Host PCIe and Host--Remote HPU network
connections and a fully editable drawio deliverable on 2026-10-04.

`fhestore-overview.drawio` is the primary source. Its modules, cylinder, cloud,
labels, and connectors are native diagrams.net objects. All functional connectors
have actual source/target objects and can be edited or reconnected. The diagram
contains no embedded screenshot, SVG image, or other raster substitute.

## Files

- `fhestore-overview.drawio`: open directly in diagrams.net or draw.io Desktop.
- `build_drawio.py`: reproducible native XML generation.
- `fhestore-overview.svg`, `.pdf`, `.png`: exports rendered from the drawio source.
- `export_drawio.py`: native diagrams.net rendering and vector/preview exports.
- `render-checks.json`: actual native-model and export checks.
- `figure.tex`: paper inclusion, caption, and accessible description.

## Semantics

PCIe carries host task submission and the two ciphertext handoffs between CSD
and Host. Network carries input and result ciphertexts between Host and Remote
FHE Service. Each medium is marked across the corresponding inter-column gap;
the media are transport links rather than processing stages. Ciphertext paths
retain their distinct directions and both traverse the Host relay.

The task runtime submits an operator graph, operator contexts, and data ranges
to the device runtime, which configures existing storage-side operators. The
small Filter -> Encrypt graph represents the submitted logical program.
The storage-side data path is a functional boundary. Storage and Write-back
are system functions; the figure does not imply that they are FPGA kernels.
Record write-back may involve host-assisted read-modify-write, as stated in
the caption. Device buffers, DMA details, and HPU internals remain outside this
overview's scope.

## Provenance and reproduction

This is an editable reconstruction of the author-selected generated reference
`/home/yangchenghui/.codex/generated_images/01a104d2-16af-7740-86b4-595efd656257/exec-74048e8d-75fe-40a7-8ea0-24b8f1c8941c.png`,
with the restrained vector style and corrected functional boundary from v2.
The reference and exact original ImageGen prompt are preserved in v2. The v3
changes and drawio conversion use native shapes; no new image generation is used.
The source evidence and repository revision are documented in v2/README.md.
The v2 assets and manuscript source have not been overwritten.

The exports use the official diagrams.net `viewer-static.min.js`, downloaded from
`https://viewer.diagrams.net/js/viewer-static.min.js`, with Chrome's native
mxCodec/mxImageExport rendering and `rsvg-convert` for PDF and PNG.
The viewer's SHA-256 is recorded in `render-checks.json`; the library is not
needed to open the drawio file in diagrams.net.

```bash
/usr/bin/python3 build_drawio.py --force
/usr/bin/python3 export_drawio.py --viewer-js /tmp/fhestore-drawio-viewer.min.js --force
```

The intended paper export is 178 x 89 mm. The preview is rendered from the
editable source rather than drawn independently.

## Validation

The official viewer decoded and rendered 39 vertices and 14 connected edges,
with no unresolved terminals or unrendered cells. The source contains 55 native
mxCells and no embedded image. The exported SVG has 35 native text elements,
no raster images, and no foreignObject elements. Its explicit plain-text labels
are rendered as SVG text to preserve line breaks in PDF/PNG. The cylinder uses
the viewer's proportional cap size, 0.15.

The final PNG and the 178 mm, 144 dpi paper-size preview were visually inspected
for labels, the cloud contour, arrow endpoints, and the PCIe/Network annotations.
The ordinary 29-unit labels are approximately 8.1 pt at that width. The PDF is
178 x 89 mm and embeds Arimo and Arimo-Bold. An independent semantic reviewer
confirmed the three PCIe-crossing connections, the two network connections,
the host relay, and the functional storage-side boundary.
