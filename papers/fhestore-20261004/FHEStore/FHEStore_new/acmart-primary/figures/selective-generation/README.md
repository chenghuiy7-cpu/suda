# Application-directed preprocessing figure

Active figure: `six-conditions.pdf`, with an editable-text `six-conditions.svg`.
The 2026-10-08 redraw changes presentation only.

## Source and condition mapping

`combined-source-data.json` is a byte-for-byte copy of
`/home/yuanzhihao/suda/logs/application-query-preview_20261006/combined-data.json`.
SHA-256: `88dad337d75ec6c698d921fb657b6108cd8c30949bcf6fe5d17099d3902b71d8`.
The original builder and renderer are `build_combined_data.py` and
`six-conditions-render.js` in that source directory.

| Condition | Original panel and row | Dataset |
|---|---|---|
| C1 | panel 0, `75` (Q1) | TPC-H |
| C2 | panel 0, `50` (Q1) | TPC-H |
| C3 | panel 2, `Q2` | TPC-H |
| C4 | panel 1, `75` (Q3) | Intel Lab |
| C5 | panel 1, `50` (Q3) | Intel Lab |
| C6 | panel 2, `Q4` | Intel Lab |

All 12 original means, sample SDs, and counts (3 valid runs per condition/backend)
are preserved exactly. Samples in the source snapshot reproduce the means and
sample SDs within 1e-10. No conditions are pooled, remeasured, or excluded.
The same C1--C3 and C4--C6 dataset grouping is retained. The original plot has no
bar-value annotations, so the redraw does not add them.

## Reproduce and inspect

Use `../host-resources/plot_resources_and_selective.py` and follow the run commands
in the neighboring `host-resources/README.md`. Full plotted values and source-row
mapping are in `../host-resources/plot-data-and-qa.json`.

The figure is 3.337 in wide and 137 pt high, below the old approximately 137.4 pt.
CPU baseline uses blue `#4C78A8`; FHEStore uses orange `#E58D5C` and `///` hatching.
Tinos labels/ticks are bold at 8.3--8.8 pt. The latency axis remains in ms with
its original 0--500 range; errors remain sample SD. Vector PDF/SVG contain no
embedded plot rasters. The agent inspected the final-width render for clipping,
legibility, grouping and visible uncertainty. This style revision preserves the
requested existing one-axes placement; no experimental panels were invented.

Old assets are retained under `/tmp/fhestore-fig6-8-before` for this session.

## 2026-10-08 author-confirmed measurement update

The author confirmed that the updated experimental manuscript uses ten valid
measurements per performance configuration. The manuscript follows this update;
the three-run samples retained in `combined-source-data.json` are historical
source records and remain unchanged. No replacement raw samples are invented.
