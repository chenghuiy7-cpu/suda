# TFHE encryption IP: monochrome revision

The author selected the black-and-white hardware drawing style of James Victor
Howe, *Practical Lattice-Based Cryptography in Hardware*, Fig. 4.1 (PDF p148,
printed p126), supplied locally as `../../HLS figure paper/thesis.pdf`.

The revision uses aligned gray functional regions and white rectangular modules,
a trapezoid binary-key MUX, circular adders, orthogonal wiring, and bus-width
marks. The input/context region sits beside sampling above arithmetic, with
ciphertext packing on the right. FHEStore's encryption connectivity and symbolic
widths are retained. No DSP or explicit modular-reduction unit is imported from
the reference architecture. Figure 1 and the archived figures remain unchanged.

`tfhe-encryption.svg` is editable text and vector geometry; `tfhe-encryption.pdf`
is the manuscript export; `tfhe-encryption.png` is a 220-dpi review preview.
Run `python generate_figure.py` in this directory to reproduce the SVG/PDF/PNG.
`tfhe-encryption.drawio` is the native diagrams.net version. Its modules, MUX,
adders, labels, and connectors are individually editable; module labels are
grouped with their shapes. Open it in draw.io or diagrams.net. Run
`python build_drawio.py` after regenerating the SVG to reproduce this version.
The native vector renderer revises an existing code-native diagram using the supplied
visual reference. No image-generation model or raster tracing is involved.

The placement is a two-column hardware schematic, 178 × 80.52 mm.
Module and signal labels use 8.5–10 pt text. The figure shows the key gate,
accumulator recurrence, body additions, and mask fan-out as well as packing.
Source-code hashes and the rendering/reference record are in `source-info.json`.

Caption: “HLS architecture for integrated TFHE encoding and encryption.”
