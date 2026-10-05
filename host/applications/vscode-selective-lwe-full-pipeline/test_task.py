#!/usr/bin/env python3
import copy
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from run_task import HERE, batches, load_json, plan


class TaskTests(unittest.TestCase):
    def setUp(self):
        self.task = load_json(HERE / 'tasks/tpch-q6-update.json')
        self.runtime = load_json(HERE / 'tasks/runtime.json')

    def test_partition_preserves_lbas_and_global_indices(self):
        self.task['data']['records'] = 257
        p = plan(self.task, self.runtime)
        b = list(batches(p))
        self.assertEqual([(base, count) for base, count, _ in b], [(0,128),(128,128),(256,1)])
        for i, (base, _, args) in enumerate(b):
            self.assertEqual(args[args.index('--ssd-lba') + 1], str(65536 + i * 16))
            self.assertEqual(args[args.index('--output-ssd-lba') + 1], str(131072 + i * 16))
            self.assertEqual(args[args.index('--record-base') + 1], str(base))
            self.assertNotIn('--reference', args)

    def test_invalid_layout_overlap_alignment_and_unknown_keys(self):
        for modify in (
            lambda t,r: t['data'].update(record_bytes=513),
            lambda t,r: t['downstream']['ssd_result'].update(lba=65540),
            lambda t,r: t['encryption'].update(layout='cpu'),
            lambda t,r: r.update(batch_records=129),
            lambda t,r: r.update(read_chunk_btyes=1),
            lambda t,r: t['data'].update(records=True),
            lambda t,r: t['data']['schema'][1].update(offset=0),
            lambda t,r: t['data']['schema'][1].update(offset=63),
            lambda t,r: r['transport'].update(read_queue_depth=3),
            lambda t,r: r['transport'].update(write_chunk_bytes=134221824),
        ):
            t, r = copy.deepcopy(self.task), copy.deepcopy(self.runtime)
            modify(t,r)
            with self.assertRaises(ValueError): plan(t,r)

    def test_duplicate_json_key_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'bad.json'; p.write_text('{"version":1,"version":2}')
            with self.assertRaises(ValueError): load_json(p)

    def test_generate_cpu_without_reference_or_destination(self):
        self.task['downstream'] = {'mode':'generate','output_dir':'out'}
        self.task['encryption']['layout'] = 'cpu'
        p = plan(self.task, self.runtime)
        self.assertIn('--encrypt-only', p['common'])
        self.assertNotIn('--reference', p['common'])
        self.assertIsNone(p['destination'])

    def test_executor_success_and_partial_failure(self):
        # Fake native executable tests the orchestration boundary only. FPGA
        # output bytes/selection parsing are tested with the actual C model.
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            key = root / 'key'; key.write_bytes(b'\0'*2048)
            executable = root / 'native'
            executable.write_text('''#!/usr/bin/env python3
import json,sys
from pathlib import Path
a=sys.argv[1:]
if '--explain-filter' in a: sys.exit(0)
def v(k): return a[a.index(k)+1]
base=int(v('--record-base')); rows=int(v('--records'))
if Path(__file__).with_name('fail').exists() and base == 128: sys.exit(7)
# First batch is empty, later batches select their last row.
ids=[] if base==0 else [base+rows-1]
Path(v('--selection-output')).write_text(json.dumps(dict(version=1,selection_source='fpga',record_base=base,record_count=rows,selected_count=len(ids),indices=ids,values_u8=[7]*len(ids))))
if '--output' in a: Path(v('--output')).write_bytes(b'LWEHLS01')
''')
            executable.chmod(0o755)
            self.task['data']['records'] = 257
            self.task['encryption']['key_ref'] = str(key)
            self.task['downstream'] = {'mode':'generate','output_dir':str(root / 'results')}
            taskfile = root / 'task.json'; taskfile.write_text(json.dumps(self.task))
            runtimefile = root / 'runtime.json'; runtimefile.write_text(json.dumps(self.runtime))
            cmd = [sys.executable, str(HERE/'run_task.py'), '--task', str(taskfile),
                   '--runtime', str(runtimefile), '--binary', str(executable)]
            dry = subprocess.run(cmd+['--dry-run'], capture_output=True, text=True)
            self.assertEqual(dry.returncode, 0, dry.stderr)
            self.assertFalse((root/'results').exists())
            run = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            result = load_json(next((root/'results').glob('*/result.json')))
            self.assertEqual(result['selected_count'], 2)
            self.assertEqual(len(result['batches']), 3)
            (root/'fail').touch()
            run = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(run.returncode, 1)
            progress = [load_json(p) for p in (root/'results').glob('*/progress.json')]
            failed = next(p for p in progress if p['status']=='failed')
            self.assertEqual(len(failed['batches']), 1)
            self.assertEqual(failed['failed_batch']['record_base'],128)
            self.assertEqual(len(list((root/'results').glob('*/result.json'))), 1)


if __name__ == '__main__':
    unittest.main()
