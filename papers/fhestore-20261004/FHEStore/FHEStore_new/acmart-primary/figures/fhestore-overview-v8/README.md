# Figure 1: author-selected production-only revision

This native reconstruction uses the **v6 author-selected appearance** and the
latest explicit geometry and editing instructions. It preserves the original
light CSD/Host/Remote Server regions, rectangle components, storage cylinder,
transfer-area dashed box, colors, shadows, and physical PCIe connections.
It does not use the v7 redesign.

The authorized changes are:

- Extend `configure / execute` from Device Runtime to the Operator Pool top
  edge and start its `complete` control at that same component.
- Rename `Ciphertext Relay` to `Ciphertext Transfer`.
- Remove the three reverse blue data arrows: Operator Pool to persistent
  data, Host to CSD, and Remote Server to Host.
- Center the remaining forward production stream on y = 270.

All application/task, device/operator, and remote software/accelerator
submission/completion controls are retained. The remote accelerator remains
the external ciphertext consumer.

## Editable source and exports

- `fhestore-overview.drawio`: native editable diagrams.net component cells
  and source/target-connected edges; no embedded image or SVG wrapper.
- `fhestore-overview.svg`, `fhestore-overview.pdf`: vector exports of that
  exact native drawio model, rendered by the official local diagrams.net viewer.
- `fhestore-overview.png`: 3600 × 1223 px review export.
- `final-width-preview.png`: faithful 178 mm placement preview at approximately
  96 dpi, 673 × 229 px.
- `build_drawio.py`: source model and provenance generator.
- `export_drawio.py`: local official-viewer export, adapted from the existing
  Figure 2 v4 exporter.
- `source-info.json`: baseline SHA256, approved changes, palette, dimensions,
  and all scientific/physical connection endpoints.
- `render-checks.json`, `qa.json`: actual renderer checks and inspection notes.

Rebuild from the project directory:

```bash
python3 figures/fhestore-overview-v8/build_drawio.py
python3 figures/fhestore-overview-v8/export_drawio.py \
  --viewer-js /tmp/fhestore-encryption-drawio-20261005/viewer-static.min.js \
  --force
```

The exporter uses the already cached official viewer, local Chrome, and
`rsvg-convert`. In the present environment Chrome rendering requires sandbox
escalation; it does not download any dependency. Source canvas: 1089 × 370;
publication placement: **178 × 60.48 mm**. Native component text is 20–24
canvas units (about 9.3–11.1 pt at placement); the original-style small control
labels are 14 units (about 6.5 pt), and legend text is 16 units (about 7.4 pt).

## Actual validation

The official viewer rendered 23 vertices, 15 edges, and 24 SVG text elements,
with **zero** unresolved edges, unrendered cells, raster elements, or
foreignObject text elements. The PDF is 504.567 × 171.432 pt and embeds Arimo
regular and bold fonts. The agent inspected the actual large PNG and the
faithful placement preview against the v6 baseline and authorized endpoint
list: all control directions remain, all three forward data segments remain,
the reverse data segments are absent, labels are visible, and the extended
controls terminate on the Operator Pool rather than the container boundary.
Agent inspection is not human sign-off.

No image model was used in this revision. No main manuscript or other figure
was edited. The root task owns the manuscript inclusion and caption update.
