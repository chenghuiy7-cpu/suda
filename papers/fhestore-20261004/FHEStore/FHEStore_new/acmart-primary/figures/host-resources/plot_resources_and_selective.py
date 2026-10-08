#!/usr/bin/env python3
"""Redraw three evaluation plots from unchanged aggregate source snapshots.

Requires Python, NumPy, Matplotlib, and a Times-compatible Tinos font.
Run from any directory: python3 plot_resources_and_selective.py
No aggregation, sample exclusion, or statistical redefinition is performed.
"""
from pathlib import Path
import hashlib
import json
import statistics

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties, findfont
from matplotlib.patches import Patch
from matplotlib.ticker import FuncFormatter
import numpy as np

FIGURES = Path(__file__).resolve().parents[1]
RESOURCE_DIR = FIGURES / 'host-resources'
SELECTIVE_DIR = FIGURES / 'selective-generation'
WIDTH_IN = 3.337
CPU = '#4C78A8'
FHESTORE = '#E58D5C'
INK = '#252B32'
AXIS = '#65717C'
GRID = '#DDE3E8'

plt.rcParams.update({
    'font.family': 'serif',
    'font.serif': ['Tinos', 'Times New Roman', 'Liberation Serif', 'DejaVu Serif'],
    'font.weight': 'bold',
    'font.size': 8.5,
    'axes.labelsize': 8.8,
    'axes.labelweight': 'bold',
    'xtick.labelsize': 8.3,
    'ytick.labelsize': 8.3,
    'axes.edgecolor': AXIS,
    'axes.linewidth': 0.65,
    'text.color': INK,
    'xtick.color': INK,
    'ytick.color': INK,
    'xtick.major.width': 0.65,
    'ytick.major.width': 0.65,
    'xtick.major.size': 2.0,
    'ytick.major.size': 2.0,
    'hatch.linewidth': 0.45,
    'pdf.fonttype': 42,
    'svg.fonttype': 'none',
    'svg.hashsalt': 'fhestore-evaluation-20261008',
    'savefig.facecolor': 'white',
})
font_path = findfont(FontProperties(family='Tinos', weight='bold'), fallback_to_default=False)


def source(path):
    raw = path.read_bytes()
    return json.loads(raw), {'file': str(path.relative_to(FIGURES)),
                            'sha256': hashlib.sha256(raw).hexdigest()}


def style(ax, ticks, upper):
    ax.set_ylim(0, upper)
    ax.set_yticks(ticks)
    ax.yaxis.set_major_formatter(FuncFormatter(lambda x, pos: f'{int(x):,}'))
    ax.set_axisbelow(True)
    ax.grid(axis='y', color=GRID, linewidth=0.4)
    ax.tick_params(axis='both', pad=2.5)
    for label in ax.get_xticklabels() + ax.get_yticklabels():
        label.set_fontweight('bold')
    for spine in ax.spines.values():
        spine.set_linewidth(0.65)
        spine.set_color(AXIS)


def legend(fig):
    handles = [Patch(facecolor=CPU, edgecolor=CPU, linewidth=0),
               Patch(facecolor=FHESTORE, edgecolor='white', linewidth=0,
                     hatch='///')]
    fig.legend(handles, ['CPU baseline', 'FHEStore'], ncol=2,
               loc='upper center', bbox_to_anchor=(0.58, 1.005),
               frameon=False, fontsize=8.8, handlelength=1.35,
               handleheight=0.9, handletextpad=0.45, columnspacing=1.3,
               borderaxespad=0.25)


def export(fig, stem):
    fig.canvas.draw()
    renderer = fig.canvas.get_renderer()
    # All figure text is checked in the fixed, final-width canvas.
    clipped = []
    for text in fig.findobj(match=lambda a: isinstance(a, matplotlib.text.Text)):
        if not text.get_visible() or not text.get_text():
            continue
        box = text.get_window_extent(renderer)
        if (box.x0 < -0.5 or box.y0 < -0.5 or
                box.x1 > fig.bbox.width + 0.5 or
                box.y1 > fig.bbox.height + 0.5):
            clipped.append(text.get_text())
        assert text.get_fontweight() in ['bold', 700], text.get_text()
    if clipped:
        raise RuntimeError(f'Clipped text in {stem.name}: {clipped}')
    metadata = {'Creator': 'FHEStore source-data figure generator',
                'CreationDate': None, 'ModDate': None}
    fig.savefig(stem.with_suffix('.pdf'), metadata=metadata)
    fig.savefig(stem.with_suffix('.svg'))
    preview = Path('/tmp/fhestore-fig6-8-redraw')
    preview.mkdir(exist_ok=True)
    fig.savefig(preview / (stem.name + '.png'), dpi=200)
    plt.close(fig)


def resources(data, name, metric, ylabel, upper, ticks):
    fig = plt.figure(figsize=(WIDTH_IN, 154 / 72))
    ax = fig.add_axes([46 / (WIDTH_IN * 72), 32 / 154,
                       190 / (WIDTH_IN * 72), 101 / 154])
    batches = data['batches']
    x = np.arange(len(batches))
    width = 0.34
    labels = {}
    plotted = []
    for backend, color, offset in [('cpu', CPU, -0.18),
                                    ('csd', FHESTORE, 0.18)]:
        rows = [next(r for r in data['points']
                     if r['backend'] == backend and r['batch'] == batch)
                for batch in batches]
        assert all(r['n'] == 10 for r in rows)
        means = [r[metric + '_mean'] for r in rows]
        sds = [r[metric + '_sd'] for r in rows]
        bars = ax.bar(x + offset, means, width, color=color,
                      edgecolor='white' if backend == 'csd' else color,
                      linewidth=0, hatch='///' if backend == 'csd' else None,
                      zorder=3)
        ax.errorbar(x + offset, means, yerr=sds, fmt='none', ecolor=INK,
                    capsize=1.6, elinewidth=0.8, capthick=0.8, zorder=4)
        labels[backend] = []
        for i, (row, bar, mean, sd) in enumerate(zip(rows, bars, means, sds)):
            assert bar.get_height() == mean
            label = f'{mean:.0f}'
            labels[backend].append(ax.annotate(
                label, (x[i] + offset, mean + sd), xytext=(0, 2.5),
                textcoords='offset points', ha='center', va='bottom',
                fontsize=8.2, fontweight='bold', zorder=5))
            bar.set_gid(f"{backend}-batch-{row['batch']}")
            plotted.append({'backend': backend, 'batch': row['batch'],
                            'mean': mean, 'sample_sd': sd, 'n': row['n'],
                            'mean_label': label})
    ax.set_xlim(-0.57, len(batches) - 0.43)
    ax.set_xticks(x)
    ax.set_xticklabels([str(b) for b in batches])
    ax.set_xlabel('Plaintext size (bytes)', labelpad=4.5)
    ax.set_ylabel(ylabel, labelpad=4.3)
    style(ax, ticks, upper)
    legend(fig)
    # Preserve all rounded labels; move a CPU label up only when paired
    # annotations would collide at the final column width.
    fig.canvas.draw()
    renderer = fig.canvas.get_renderer()
    moved = []
    for batch, cpu_label, csd_label in zip(batches, labels['cpu'], labels['csd']):
        a = cpu_label.get_window_extent(renderer).expanded(1.08, 1.15)
        b = csd_label.get_window_extent(renderer).expanded(1.08, 1.15)
        if a.overlaps(b):
            cpu_label.set_position((0, 12.0))
            moved.append(batch)
    export(fig, RESOURCE_DIR / name)
    return {'source_metric': metric, 'units': 'ms' if metric == 'cpu_ms' else 'MiB',
            'height_pt': 154, 'error': 'sample SD', 'points': plotted,
            'raised_cpu_labels': moved}


def selective(data):
    mapping = [('C1', 'TPC-H', 0, '75'), ('C2', 'TPC-H', 0, '50'),
               ('C3', 'TPC-H', 2, 'Q2'), ('C4', 'Intel Lab', 1, '75'),
               ('C5', 'Intel Lab', 1, '50'), ('C6', 'Intel Lab', 2, 'Q4')]
    rows = []
    for condition, dataset, panel, row_id in mapping:
        row = next(r for r in data['panels'][panel]['rows'] if r['id'] == row_id)
        rows.append((condition, dataset, panel, row_id, row))
        for backend in ['cpu', 'fhestore']:
            result = row[backend]
            assert result['n'] == len(result['samples']) == 3
            assert abs(statistics.mean(result['samples']) - result['mean']) < 1e-10
            assert abs(statistics.stdev(result['samples']) - result['sd']) < 1e-10
    fig = plt.figure(figsize=(WIDTH_IN, 137 / 72))
    ax = fig.add_axes([46 / (WIDTH_IN * 72), 32 / 137,
                       190 / (WIDTH_IN * 72), 86 / 137])
    x = np.array([0, 1, 2, 3.6, 4.6, 5.6])
    width = 0.34
    plotted = []
    for backend, color, offset in [('cpu', CPU, -0.18),
                                    ('fhestore', FHESTORE, 0.18)]:
        means = [row[4][backend]['mean'] for row in rows]
        sds = [row[4][backend]['sd'] for row in rows]
        bars = ax.bar(x + offset, means, width, color=color,
                      edgecolor='white' if backend == 'fhestore' else color,
                      linewidth=0, hatch='///' if backend == 'fhestore' else None,
                      zorder=3)
        ax.errorbar(x + offset, means, yerr=sds, fmt='none', ecolor=INK,
                    capsize=1.6, elinewidth=0.8, capthick=0.8, zorder=4)
        for row, bar, mean, sd in zip(rows, bars, means, sds):
            assert bar.get_height() == mean
            bar.set_gid(f'{backend}-{row[0]}')
            plotted.append({'condition': row[0], 'dataset': row[1],
                            'source_panel': row[2], 'source_row': row[3],
                            'backend': backend, 'mean': mean,
                            'sample_sd': sd, 'n': row[4][backend]['n']})
    ax.set_xlim(-0.6, 6.2)
    ax.set_xticks(x)
    ax.set_xticklabels([row[0] for row in rows])
    ax.set_ylabel('Input preparation latency (ms)', labelpad=4.3)
    style(ax, [0, 100, 200, 300, 400, 500], 500)
    legend(fig)
    # Dataset labels retain the original two three-condition groups.
    ax.axvline(2.8, color='#CAD1D8', linewidth=0.45, zorder=0)
    for a, b, label in [(0, 2, 'TPC-H'), (3.6, 5.6, 'Intel Lab')]:
        ax.plot([a - 0.4, b + 0.4], [-0.205, -0.205],
                transform=ax.get_xaxis_transform(), clip_on=False,
                color=AXIS, linewidth=0.5)
        ax.text((a + b) / 2, -0.275, label,
                transform=ax.get_xaxis_transform(), clip_on=False,
                ha='center', va='top', fontsize=8.8, fontweight='bold')
    export(fig, SELECTIVE_DIR / 'six-conditions')
    return {'units': data['units'], 'height_pt': 137,
            'error': data['error'], 'statistic': data['statistic'],
            'points': plotted}


def main():
    resource_data, resource_source = source(RESOURCE_DIR / 'source-data.json')
    selective_data, selective_source = source(SELECTIVE_DIR / 'combined-source-data.json')
    assert len(resource_data['points']) == 14
    report = {'placement': {'width_in': WIDTH_IN, 'single_column': True},
              'font': {'family': 'Tinos', 'bold_file': font_path,
                       'axis_label_pt': 8.8, 'ticks_pt': 8.3,
                       'resource_mean_labels_pt': 8.2},
              'palette': {'CPU baseline': CPU, 'FHEStore': FHESTORE,
                          'FHEStore_hatch': '///'},
              'sources': [resource_source, selective_source]}
    report['host-cpu-time'] = resources(resource_data, 'host-cpu-time', 'cpu_ms',
                                        'Cumulative host CPU time (ms)', 2800,
                                        [0, 500, 1000, 1500, 2000, 2500])
    report['host-memory'] = resources(resource_data, 'host-memory', 'rss',
                                      'Peak host memory (MiB)', 600,
                                      [0, 100, 200, 300, 400, 500, 600])
    report['six-conditions'] = selective(selective_data)
    # Plot records retain source floats and counts exactly, without rounding.
    for name, metric in [('host-cpu-time', 'cpu_ms'), ('host-memory', 'rss')]:
        for point in report[name]['points']:
            row = next(r for r in resource_data['points']
                       if r['backend'] == point['backend'] and r['batch'] == point['batch'])
            assert point['mean'] == row[metric + '_mean']
            assert point['sample_sd'] == row[metric + '_sd']
            assert point['n'] == row['n']
            assert point['mean_label'] == f"{row[metric + '_mean']:.0f}"
    for point in report['six-conditions']['points']:
        row = next(r for r in selective_data['panels'][point['source_panel']]['rows']
                   if r['id'] == point['source_row'])[point['backend']]
        assert point['mean'] == row['mean'] and point['sample_sd'] == row['sd']
        assert point['n'] == row['n']
    report['validation'] = {
        'plotted_means_sample_sds_and_n_equal_source_exactly': True,
        'rounded_resource_labels_unchanged': True,
        'selective_samples_recomputed_in_generator': True,
        'text_clipping_checked_at_final_width': True,
        'all_text_bold': True,
    }
    (RESOURCE_DIR / 'plot-data-and-qa.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Saved vector PDF/SVG for 3 figures; all 40 means/SDs/counts retained.')


if __name__ == '__main__':
    main()
