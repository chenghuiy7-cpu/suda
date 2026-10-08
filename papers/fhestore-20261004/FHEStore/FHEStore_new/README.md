# FHEStore manuscript

The merged manuscript is `acmart-primary/FHEStore_FPGA27.tex`; the compiled
paper is `acmart-primary/FHEStore_FPGA27.pdf`. This version integrates the
ciphertext-production design and experimental evaluation. As of 2026-10-08,
the main text ends on page 10 and the complete PDF has 11 pages.

## Build

From this directory, run:

```sh
cd acmart-primary
latexmk -pdf -interaction=nonstopmode -halt-on-error FHEStore_FPGA27.tex
```

Use a TeX installation with the ACM class dependencies, including `array`,
`flushend`, and the Libertine fonts. The local `acmart.cls`, bibliography
style, BibTeX database, and all eight included graphics are supplied.
The committed PDF was checked against a clean build of these dependencies.

## Figures and revision records

`acmart-primary/figures/` contains the current figure exports, editable SVG
and drawio sources where available, plotting data, and generation scripts.
The required upstream source files for the current generators are retained.
The eight graphics used by LaTeX require no regeneration to compile the paper.

Plot regeneration uses Python, NumPy, Matplotlib, and Tinos fonts; exporting
SVG uses `rsvg-convert`. Drawio export scripts also require Chrome or Chromium
and the diagrams.net viewer JavaScript supplied through `--viewer-js`.
The HPU plotting script reads the committed data snapshot; the original-log
import script refers to separate measurement logs.

Revision and evidence records are in `acmart-primary/notes/`. Table 3 source
data are retained under
`acmart-primary/acmart-primary_eva/figures/encrypt-stage-comparison/`.
Local compilation caches, temporary backups, copied skill packages, external
reference papers, and superseded draft manuscripts are outside this commit.
