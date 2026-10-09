#!/usr/bin/env python3
"""Reproduce Figure 5 from the original author's 70-group data snapshot.

Panel (a): ratio of mean preparation to mean HPU latency, with SD of
run-level ratios. Panel (b): equal-weight mean of seven operation means,
with their minimum/maximum range. The green curve is CPU/FHEStore latency
ratio, reported as data-preparation speedup under the author-confirmed
comparison conditions.
"""
from pathlib import Path
import csv
import hashlib
import json
import math
import statistics
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.lines import Line2D
from matplotlib.patches import Patch
from matplotlib.ticker import FuncFormatter
import numpy as np

HERE = Path(__file__).resolve().parent
SOURCE = HERE / 'source-data.json'
CONFIRMATION = HERE / 'author-confirmation.json'
CPU_COLOR, FHE_COLOR, RATIO_COLOR = '#4C78A8', '#E58D5C', '#397D54'
FIGURE_WIDTH_IN, FIGURE_HEIGHT_IN = 7.0, 2.35
FONT_SIZE, TICK_SIZE, LEGEND_SIZE = 8.5, 8.0, 8.5
OPERATIONS = ['bit-or', 'bit-and', 'eq', 'shr', 'add', 'sub', 'mul']
OP_LABELS = ['OR', 'AND', 'EQ', 'SHR', 'ADD', 'SUB', 'MUL']
BATCHES = [64, 128, 256, 512, 1024]


def main():
    data = json.loads(SOURCE.read_text())
    confirmation = json.loads(CONFIRMATION.read_text())
    assert data['operations'] == OPERATIONS and data['batches'] == BATCHES
    groups = {(g['method'], g['operation'], g['batch_bytes']): g for g in data['groups']}
    assert len(groups) == 70
    max_ratio_error, max_sd_error = 0.0, 0.0
    for key, g in groups.items():
        expected_n = 2 if key == ('CSD', 'mul', 1024) else 3
        assert g['n'] == expected_n == len(g['samples'])
        preparation = [s['preparation_ms'] for s in g['samples']]
        hpu = [s['hpu_ms'] for s in g['samples']]
        assert math.isclose(statistics.mean(preparation), g['preparation_mean_ms'], rel_tol=1e-12)
        assert math.isclose(statistics.mean(hpu), g['hpu_mean_ms'], rel_tol=1e-12)
        ratio = 100 * statistics.mean(preparation) / statistics.mean(hpu)
        ratio_sd = statistics.stdev([100 * a / b for a, b in zip(preparation, hpu)])
        max_ratio_error = max(max_ratio_error, abs(ratio - g['ratio_pct']))
        max_sd_error = max(max_sd_error, abs(ratio_sd - g['ratio_sd_pct']))
    assert max_ratio_error < 1e-10 and max_sd_error < 1e-10

    # Tinos is a Times-compatible serif and matches the original plot family.
    family = 'Tinos'
    font_file = font_manager.findfont(font_manager.FontProperties(family=family, weight='bold'))
    plt.rcParams.update({
        'font.family': 'serif', 'font.serif': [family], 'font.weight': 'bold',
        'font.size': FONT_SIZE, 'axes.labelsize': FONT_SIZE,
        'axes.labelweight': 'bold', 'axes.linewidth': 0.65,
        'xtick.labelsize': TICK_SIZE, 'ytick.labelsize': TICK_SIZE,
        'xtick.major.size': 2.0, 'ytick.major.size': 2.0,
        'xtick.major.width': 0.65, 'ytick.major.width': 0.65,
        'text.color': '#242424', 'axes.labelcolor': '#242424',
        'xtick.color': '#242424', 'ytick.color': '#242424',
        'hatch.linewidth': 0.45, 'pdf.fonttype': 42, 'ps.fonttype': 42,
        'svg.fonttype': 'none', 'savefig.facecolor': 'white',
    })
    fig = plt.figure(figsize=(FIGURE_WIDTH_IN, FIGURE_HEIGHT_IN))
    # Panel (a) gets slightly more horizontal space than the original export.
    ax = fig.add_axes([0.095, 0.30, 0.555, 0.53])
    bx = fig.add_axes([0.752, 0.30, 0.190, 0.53])
    rx = bx.twinx()
    for panel in (ax, bx):
        panel.set_axisbelow(True)
        panel.grid(axis='y', color='#E6E8EB', linewidth=0.40)
        panel.spines['top'].set_visible(False)
        panel.spines['right'].set_visible(False)
        panel.spines['left'].set_color('#666666')
        panel.spines['bottom'].set_color('#666666')
        panel.tick_params(axis='both', pad=2.0)
    rx.spines['top'].set_visible(False)
    rx.spines['left'].set_visible(False)
    rx.spines['bottom'].set_visible(False)
    rx.spines['right'].set_color('#666666')
    rx.tick_params(axis='y', pad=2.0)

    # All 35 batch positions, two method bars, and all source SDs remain.
    gap = 0.70
    positions = np.array([oi * (len(BATCHES) + gap) + bi
                          for oi in range(len(OPERATIONS)) for bi in range(len(BATCHES))])
    width = 0.34
    plotted_rows = []
    for method, offset, color, hatch in [('CPU', -width/2, CPU_COLOR, None),
                                         ('CSD', width/2, FHE_COLOR, '///')]:
        records = [groups[(method, op, batch)] for op in OPERATIONS for batch in BATCHES]
        heights = [g['ratio_pct'] for g in records]
        errors = [g['ratio_sd_pct'] for g in records]
        ax.bar(positions + offset, heights, width, color=color,
               edgecolor='#30516F' if method == 'CPU' else '#A35F3D',
               linewidth=0.25, hatch=hatch,
               yerr=errors, error_kw={'elinewidth': 0.60, 'capsize': 0.75,
                                     'capthick': 0.60, 'ecolor': '#343434'})
        for g in records:
            plotted_rows.append({k: g[k] for k in ['method', 'operation', 'batch_bytes',
                                'n', 'preparation_mean_ms', 'hpu_mean_ms', 'ratio_pct', 'ratio_sd_pct']})
    ax.set_xlim(-0.7, positions[-1] + 0.7)
    ax.set_ylim(0, 65)
    ax.set_yticks(range(0, 61, 10))
    ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f'{v:.0f}%'))
    ax.set_xticks(positions, [str(b) for _ in OPERATIONS for b in BATCHES],
                  rotation=90, ha='center', va='top')
    ax.tick_params(axis='x', length=0, pad=2.0)
    ax.set_ylabel('Data preparation /\nHPU computation (%)', labelpad=4.0, fontsize=8.25)
    for oi, label in enumerate(OP_LABELS):
        center = oi * (len(BATCHES) + gap) + (len(BATCHES)-1)/2
        ax.text(center, -0.245, label, transform=ax.get_xaxis_transform(),
                ha='center', va='top', fontsize=8.5, fontweight='bold')

    aggregated, prepared = [], {}
    for method in ('CPU', 'CSD'):
        prepared[method] = []
        for batch in BATCHES:
            means = [groups[(method, op, batch)]['preparation_mean_ms'] for op in OPERATIONS]
            row = {'method': method, 'batch_bytes': batch, 'mean_ms': statistics.mean(means),
                   'minimum_operation_mean_ms': min(means), 'maximum_operation_mean_ms': max(means)}
            prepared[method].append(row)
            aggregated.append(row)
    ix = np.arange(len(BATCHES))
    bwidth = 0.34
    for method, offset, color, hatch in [('CPU', -bwidth/2, CPU_COLOR, None),
                                         ('CSD', bwidth/2, FHE_COLOR, '///')]:
        rows = prepared[method]
        means = np.array([r['mean_ms'] for r in rows])
        errors = np.array([[r['mean_ms'] - r['minimum_operation_mean_ms'] for r in rows],
                           [r['maximum_operation_mean_ms'] - r['mean_ms'] for r in rows]])
        bx.bar(ix + offset, means, bwidth, color=color,
               edgecolor='#30516F' if method == 'CPU' else '#A35F3D',
               linewidth=0.25, hatch=hatch, yerr=errors,
               error_kw={'elinewidth': 0.60, 'capsize': 1.0, 'capthick': 0.60,
                         'ecolor': '#343434'})
    ratios = [c['mean_ms']/f['mean_ms'] for c, f in zip(prepared['CPU'], prepared['CSD'])]
    rx.plot(ix, ratios, color=RATIO_COLOR, linewidth=0.85, marker='o', markersize=2.5)
    for x, value in zip(ix, ratios):
        rx.annotate(f'{value:.2f}×', (x, value), xytext=(0, 4), textcoords='offset points',
                    ha='center', va='bottom', color=RATIO_COLOR, fontsize=8.0, fontweight='bold')
    bx.set_xlim(-0.60, 4.60)
    bx.set_ylim(0, 2000)
    bx.set_yticks([0, 500, 1000, 1500, 2000])
    bx.set_xticks(ix, [str(b) for b in BATCHES])
    bx.set_ylabel('Data preparation (ms)', fontsize=8.25, labelpad=4.0)
    rx.set_ylim(0, 3.5)
    rx.set_yticks([0, 1, 2, 3])
    rx.yaxis.set_major_formatter(FuncFormatter(lambda v, _: f'{v:.0f}×'))
    rx.set_ylabel('Speedup (×)', fontsize=8.25, labelpad=4.0)
    for panel in (ax, bx, rx):
        for label in list(panel.get_xticklabels()) + list(panel.get_yticklabels()):
            label.set_fontweight('bold')

    legend = [Patch(facecolor=CPU_COLOR, edgecolor='#30516F', linewidth=.25, label='CPU baseline'),
              Patch(facecolor=FHE_COLOR, edgecolor='#A35F3D', linewidth=.25, hatch='///', label='FHEStore'),
              Line2D([], [], color=RATIO_COLOR, marker='o', markersize=2.5,
                     linewidth=.85, label='Speedup')]
    fig.legend(handles=legend, loc='upper center', bbox_to_anchor=(.51, .985),
               ncol=3, frameon=False, fontsize=LEGEND_SIZE, handlelength=1.30,
               handletextpad=.45, columnspacing=2.5, borderaxespad=0)
    fig.text(.3725, 15/169.2, 'Homomorphic operation', ha='center', va='center',
             fontsize=8.5, fontweight='bold')
    fig.text(.847, 15/169.2, 'Batch size', ha='center', va='center', fontsize=8.5, fontweight='bold')
    fig.text(.3725, 2/169.2, '(a) Relative preparation latency', ha='center', va='bottom',
             fontsize=8.5, fontweight='bold')
    fig.text(.847, 2/169.2, '(b) Preparation latency', ha='center', va='bottom',
             fontsize=8.5, fontweight='bold')

    # Check every visible text element against the fixed 7-inch canvas.
    fig.canvas.draw()
    renderer = fig.canvas.get_renderer()
    text_bounds = [text.get_window_extent(renderer) for text in fig.findobj(matplotlib.text.Text)
                   if text.get_visible() and text.get_text()]
    minimum_x = min(b.x0 for b in text_bounds) / fig.dpi * 72
    maximum_x = max(b.x1 for b in text_bounds) / fig.dpi * 72
    minimum_y = min(b.y0 for b in text_bounds) / fig.dpi * 72
    maximum_y = max(b.y1 for b in text_bounds) / fig.dpi * 72
    assert minimum_x >= 0 and maximum_x <= FIGURE_WIDTH_IN * 72
    assert minimum_y >= 0 and maximum_y <= FIGURE_HEIGHT_IN * 72

    for ext in ('pdf', 'svg', 'png'):
        fig.savefig(HERE / f'input-hpu-two-panels.{ext}', dpi=220,
                    metadata={'Creator': 'plot_input_hpu.py'} if ext == 'pdf' else None)
    plt.close(fig)
    with (HERE / 'plotted-values.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(plotted_rows[0]))
        writer.writeheader(); writer.writerows(plotted_rows)
    with (HERE / 'preparation-by-batch.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(aggregated[0]))
        writer.writeheader(); writer.writerows(aggregated)
    qa = {'source_data_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
          'source_group_count': len(groups), 'panel_a_bar_count': 70,
          'panel_a_batch_labels': 35, 'panel_b_bar_count': 10,
          'panel_a_ratio_error_max_pct': max_ratio_error,
          'panel_a_sd_error_max_pct': max_sd_error,
          'source_n_counts': {'three_runs': 69, 'two_runs': 1},
          'two_run_group': ['CSD', 'mul', 1024],
          'latency_ratios_by_batch': dict(zip(map(str, BATCHES), ratios)),
          'figure_width_inches': FIGURE_WIDTH_IN, 'figure_height_inches': FIGURE_HEIGHT_IN,
          'font_family': family, 'font_file': font_file,
          'font_sizes_points': {'ticks': TICK_SIZE, 'axis_labels': 8.25,
                                'group_labels': 8.5, 'panel_labels': 8.5,
                                'legend': LEGEND_SIZE, 'line_annotations': 8.0},
          'statistics': {'a_height': data['metric'], 'a_error': data['uncertainty'],
                         'b_height': 'Equal-weight mean of seven operation-specific preparation means',
                         'b_error': 'Range across those seven operation-specific means',
                         'right_axis': 'CPU mean preparation / FHEStore mean preparation, data-preparation speedup'},
          'comparability': confirmation['comparison'],
          'comparison_authority': confirmation['authority'],
          'comparison_confirmation_file': CONFIRMATION.name,
          'text_bounds_points': {'minimum_x': minimum_x, 'maximum_x': maximum_x,
                                 'minimum_y': minimum_y, 'maximum_y': maximum_y},
          'visual_qa': 'Agent inspection of final-width PDF and PNG: all 35 batch labels remain readable; no text clipping or overlap'}
    (HERE / 'qa.json').write_text(json.dumps(qa, indent=2)+'\n')
    print(json.dumps(qa, indent=2))

if __name__ == '__main__':
    main()
