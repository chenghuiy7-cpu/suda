# Figure 5: Input preparation and HPU computation

## Reproduction

Run `python plot_input_hpu.py` from any directory with Matplotlib and NumPy installed. The script reads the sibling `source-data.json` and `author-confirmation.json` and writes PDF, SVG, PNG, two numeric CSVs, and `qa.json`. The manuscript keeps its existing `input-hpu-two-panels.pdf` asset path. The PNG is only a review preview.

## Source

The snapshot is a byte-for-byte copy of `/home/yuanzhihao/suda/CipherStore/samples/figures/fig_input_hpu_overlay_data.json`, SHA-256 `bb4b7b1e5ca18f5ce999f340f4b0a3f047fef4fb8f8d2017f1823ac9cef27a11`. It contains exact author measurements and log hashes for 70 method/operation/batch groups. No values were inferred from the old PDF. `prepare_original_data.py` preserves the author's original log-to-JSON script for provenance; its repo-relative paths require the original author checkout and it is not needed to reproduce this figure.

Panel (a) shows `100 * mean(input preparation) / mean(HPU execution and synchronization)` for every CPU/FHEStore group. Its error bars are the sample standard deviation of per-run ratios. There are three valid runs for 69 groups and two for FHEStore multiplication at 1024 bytes. All 35 batch positions, 70 bars, and source uncertainties are retained.

Panel (b) averages the seven operation-specific preparation means with equal weights at each batch size. Its error bars are the minimum and maximum of those seven means. The green curve divides the CPU average by the FHEStore average. Its five values remain 2.578591233375942, 2.769051413417686, 2.608705513448269, 2.525980086035516, and 2.4502912869969755; labels use two decimal places.

On 2026-10-08 the author confirmed that the comparison uses the same experimental conditions. The green curve and right axis report **Speedup**, defined as CPU baseline preparation latency divided by FHEStore preparation latency. `author-confirmation.json` records this clarification, which supersedes the historical comparability annotation in the preserved source snapshot. The numeric snapshot, all source measurements, aggregation, and uncertainties are unchanged. Input preparation spans SSD data access through ciphertext availability in host memory; network transfer is excluded. Panel (a) retains each path's HPU computation and synchronization latency as its denominator.

## Appearance and validation

The final canvas is exactly 7.0 by 2.35 inches (504 by 169.2 PDF points), about 6.7 points taller than the old export at the same width. Two side-by-side panels are retained. Every batch label is 8 pt; axis labels are 8.25 pt; group labels, panel labels, and the legend are 8.5 pt. The PDF embeds bold Tinos, a Times-compatible serif. The SVG preserves editable text. CPU bars use `#4C78A8`; FHEStore bars use `#E58D5C` with `///` hatching; the ratio curve uses green `#397D54`. Axes are 0.65 pt with light grey horizontal grids.

The script validates the source means, run counts, ratios, and standard deviations before export; `plotted-values.csv` preserves the exact source fields. `qa.json` records these checks and all visible text bounds. Agent inspection of both the PNG and a 144 dpi PDF rendering confirmed that all labels remain within the canvas without overlap. The original PDF backup is `/tmp/fhestore-figure5-before-20261008/input-hpu-two-panels.pdf`.
