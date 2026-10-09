# Host-resource figure sources and redraw

Active figures: `host-cpu-time.pdf` and `host-memory.pdf`, with editable-text SVG
exports of the same names. Redraw date: 2026-10-08.

## Numerical source

`source-data.json` is a byte-for-byte snapshot of
`/home/yuanzhihao/suda/CipherStore/samples/figures/fig_encrypt_host_resources.json`.
SHA-256: `6b8e88c1874efcca000a4c1e31e353d5ac7040df47b069fb22f2f2308d96ebdb`.
The original data builder is `build_encrypt_host_resources.py` in that directory.

Each backend/batch summary contains 10 valid invocations. Bars retain the original
arithmetic mean and sample SD of cumulative host CPU time (ms) or peak host RSS
(MiB). All 14 points per figure and their sample counts are unchanged. Resource
labels retain the original zero-decimal rounding; only two CPU-time labels are
raised to avoid paired-text collisions. The underlying values retain full source
precision. The CPU-time baseline uses four workers.

Original per-run CSVs are under
`/home/yuanzhihao/suda/logs/encrypt_resources_qemu4_20261004/{cpu,csd}/resource_raw.csv`.
Their hashes match the source snapshot. All 140 invocations were rechecked:
means agree exactly and recomputed sample SDs agree within 1e-12 (cross-version
floating-point rounding). The rendered SDs use the unchanged source values,
without recomputation or rounding.

## Reproduce

`plot_resources_and_selective.py` draws these two figures and the neighboring
`selective-generation/six-conditions` figure from the local source snapshots.
Use Python with NumPy, Matplotlib and the Tinos font:

```sh
python3 figures/host-resources/plot_resources_and_selective.py
```

For this workspace's system Matplotlib (to avoid an incompatible NumPy package):

```sh
/usr/bin/python3 -I - <<'PY'
import sys, runpy
sys.path = [p for p in sys.path if '/usr/local/lib/python3.10/dist-packages' not in p]
runpy.run_path('figures/host-resources/plot_resources_and_selective.py', run_name='__main__')
PY
```

Run from the `acmart-primary` directory for the commands above; the script itself
resolves data/output paths from its own location. It writes `plot-data-and-qa.json`
containing every plotted mean, SD, count, label and exact source mapping.

## Placement and QA

- Single-column width: 3.337 in; height: 154 pt, below the old approximately 155.5 pt.
- CPU baseline: blue `#4C78A8`; FHEStore: orange `#E58D5C` with `///` hatching.
- Tinos bold: 8.8 pt axis/legend labels, 8.3 pt ticks, 8.2 pt numeric annotations.
- Axis strokes: 0.65 pt; pale horizontal grids: 0.4 pt.
- Axes retain the original units and ranges (CPU time: 0--2800 ms; memory: 0--600 MiB).
- Both vector PDFs embed Tinos Bold; SVG text remains editable. No raster images
  occur in the PDFs. Generator checks all text for clipping at the final size.
- Agent inspection covered all three 200-dpi final-width renders and checked
  readable labels, error bars and hatching. This is artifact QA, not a formal
  venue-compliance or human sign-off claim. The existing one-axes placements are
  retained as requested for this scoped style revision.

Old assets are backed up under `/tmp/fhestore-fig6-8-before` for this session.
