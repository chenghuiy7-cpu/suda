# Compact horizontal bars with both shares

Author-requested AND/Add/Mul labels, a narrower left label region, closer
bar spacing, and numeric labels for both stages. The CPU and HPU panels
remain side by side in one row, at single-column width on page 1. Bars are
8 pt thick, with a 23 pt row pitch (previously 30 pt). All text remains bold
at 8–9 pt in the source. The horizontal axis is **Latency share (%)**.

Both shares appear beneath each bar on one line: preparation in blue on the
left and computation in dark orange on the right. The darker annotation hue
improves contrast on white while preserving the stage colors of the original
bars. CPU pairs are 0.75/99.25, 0.38/99.62, and 0.11/99.89%; HPU pairs are
36.6/63.4, 26.9/73.1, and 17.7/82.3%. Each pair sums to 100%.

`build_figure.py` reads the author's mean preparation shares from
`../input-preparation-motivation-v1/source-info.json`; computation shares
are the complements in the original normalized stacks. It exports a
self-contained editable SVG and the included PDF with `rsvg-convert`.
Run `python3 build_figure.py` from any directory. Bars and labels are vector.
Original min/max marks are image-backed clips rotated onto the horizontal
axis: source y=444/85 maps to 0/100% at the new plot width. No numerical
uncertainty bounds are inferred. The original PNG bytes are unchanged.

Both panels use identical host CPU input preparation, from SSD reads through
encoding and encryption to ciphertexts in host memory. Only homomorphic
computation changes from CPU software to HPU acceleration. Network transfer
is excluded. The surrounding Introduction defines these boundaries and
mean/min–max statistics; the short caption is unchanged. Raw measurements
and repetition counts have not been supplied. Provenance and actual-width
agent visual QA are recorded in `source-info.json`.

The manuscript includes the PDF exported directly from the approved SVG.
After editing the SVG, preserve those edits by exporting it directly rather
than rerunning the generator. From the `acmart-primary` directory:

```sh
rsvg-convert --format=pdf \
  --output=figures/input-preparation-motivation-v5/input-preparation-motivation.pdf \
  figures/input-preparation-motivation-v5/input-preparation-motivation.svg
latexmk -pdf -interaction=nonstopmode -halt-on-error FHEStore_FPGA27.tex
```
