#!/usr/bin/env python3
"""Historical CPU and current CSD preparation/HPU ratios (unpaired runs)."""
import csv
import hashlib
import json
import statistics
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
OPS = ('bit-or', 'bit-and', 'eq', 'shr', 'add', 'sub', 'mul')
BATCHES = (64, 128, 256, 512, 1024)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    groups, sources = [], {}
    for batch in BATCHES:
        for method in ('CPU', 'CSD'):
            for op in OPS:
                directory = ROOT / 'logs' / (f'hpu_selected_ops_sweep_20260927/{batch}b'
                    if method == 'CPU' else f'csd_hpu_{batch}_{"eq_sub" if op in ("eq", "sub") else "formal"}_20261006')
                source = directory / ('pipeline_raw.csv' if method == 'CPU' else 'raw.csv')
                sources[str(source.relative_to(ROOT))] = sha(source)
                with source.open() as stream:
                    rows = list(csv.DictReader(stream))
                if method == 'CPU':
                    selected = [r for r in rows if Path(r['output_path']).name == f'{op}_{batch}b.out']
                    prep = [sum(float(r[k]) for k in ('ssd_read_ms', 'cpu_encrypt_ms', 'cpu_native_pack_ms')) for r in selected]
                    hpu = [float(r['remote_hpu_wait_sync_ms']) for r in selected]
                    logs = [directory / 'raw' / f'{op}_{batch}b_iter{i}.log' for i in range(len(selected))]
                    for path in logs:
                        content = path.read_text()
                        assert 'remote_hpu_ciphertext_compute=passed' in content, path
                        assert 'cpu_encrypt_decrypt_checked=yes' in content, path
                else:
                    selected = [r for r in rows if r['operation'] == op and int(r['batch_size']) == batch]
                    assert len({r['iteration'] for r in selected}) == len(selected)
                    prep = [sum(float(r[k]) for k in ('ssd_to_slm_ms', 'fpga_execute_ms', 'slm_to_host_ms')) for r in selected]
                    hpu = [float(r['server_hpu_enqueue_ms']) + float(r['server_hpu_wait_sync_ms']) for r in selected]
                    logs = [directory / f"{op}_measured_{r['iteration']}.log" for r in selected]
                    for path in logs:
                        assert path.with_suffix('.ok').read_text().strip() == sha(path), path
                        assert 'remote_hpu_ciphertext_compute=passed' in path.read_text(), path
                n = 2 if (method, batch, op) == ('CSD', 1024, 'mul') else 3
                assert len(selected) == n, (method, batch, op, len(selected))
                for path in logs:
                    sources[str(path.relative_to(ROOT))] = sha(path)
                ratios = [100 * a / b for a, b in zip(prep, hpu)]
                groups.append(dict(method=method, operation=op, batch_bytes=batch, n=n,
                    preparation_mean_ms=statistics.mean(prep), hpu_mean_ms=statistics.mean(hpu),
                    ratio_pct=100*statistics.mean(prep)/statistics.mean(hpu),
                    ratio_sd_pct=statistics.stdev(ratios),
                    samples=[dict(preparation_ms=a, hpu_ms=b, ratio_pct=r) for a,b,r in zip(prep,hpu,ratios)]))
    data = dict(data_status='ACTUAL_RUN', operations=OPS, batches=BATCHES, groups=groups, sources=sources,
        comparability='Historical CPU (physical x86) and current CSD (QEMU client) are separate experiments with unmatched inputs and transfer paths. Descriptive normalized ratios, not paired speedups.',
        metric='100 * mean(input preparation) / mean(HPU execution and synchronization)',
        uncertainty='Sample SD of per-run ratios; 3 valid measurements except CSD MUL at 1024: 2.',
        boundaries=dict(CPU='SSD read + CPU encryption + HPU-native packing',
            CSD='SSD-to-SLM + FPGA encryption + ciphertext readback to host',
            CPU_HPU='Historical remote_hpu_wait_sync_ms',
            CSD_HPU='server_hpu_enqueue_ms + server_hpu_wait_sync_ms',
            excluded='Network, one-time setup, verification, cleanup, server format conversion and decryption'))
    (OUT / 'fig_input_hpu_overlay_data.json').write_text(json.dumps(data, indent=2)+'\n')
    with (OUT / 'fig_input_hpu_overlay_values.csv').open('w', newline='') as stream:
        fields = ['method','operation','batch_bytes','n','preparation_mean_ms','hpu_mean_ms','ratio_pct','ratio_sd_pct']
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction='ignore')
        writer.writeheader()
        writer.writerows(groups)
    for group in groups:
        print(group['method'],group['operation'],group['batch_bytes'],f"{group['ratio_pct']:.2f}%",f"n={group['n']}")

if __name__ == '__main__':
    main()
