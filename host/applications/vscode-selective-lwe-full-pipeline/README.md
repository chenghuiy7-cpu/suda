# Selective LWE full update pipeline

This application verifies one continuous path:

```text
SSD records -> SLM -> FPGA SQL filter -> FPGA LWE encrypt -> Host
-> TCP -> remote HPU u8 ADDS -> TCP -> Host -> SLM -> FPGA LWE decrypt
-> Host read/modify/write -> SSD records -> Host readback verification
```

The filter may compare all integer types supported by SQL metadata v2, but the
projected and updated field must be `u8`, matching the current LWE scalar input.
The same projection offset is used for the final record update.

The device first scans the input SLM in **selection-manifest mode**. It returns
one 64-byte tuple per input row (local index, selected flag, projection value),
a normal 64-byte summary (counts and error status), and a separate SUDA finish
beat. The runtime strips the finish beat; the summary remains readable data.
The receive size is `(record_count + 1) * 64 + 64`, rounded to a 4 KiB range,
so discovery does not require knowing the number of matches.

The Host checks this device-generated manifest and configures the exact
ciphertext receive range from its selected count. It then runs the existing
filter/encrypt graph on **the same input SLM snapshot**. The second pass and
manifest transport add measurable overhead; `selection_stage_ms` reports it.
This preserves the existing DMA completion contract without posting unused RX
descriptors. Output SLM allocation now follows the actual device count rather
than the maximum number of input rows.

`--reference` is optional and only checks the device selection. Execution counts,
row mapping and update indices always come from the FPGA. Update mode captures
the processed input SLM image before freeing it, re-reads source SSD pages before
writing, and rejects a changed source. This comparison does not provide locking
against concurrent writes after the check; use an exclusively owned test range.
Generate mode does not read the original row image back to Host.

Zero matches succeed: encryption/RPC/decryption are skipped. A copy-on-write
update still writes and verifies the unchanged image; an in-place empty task
only verifies the source. The manifest JSON records an empty selection.

By default, pass `--output-ssd-lba` to preserve the source and write a complete
patched copy. `--in-place` explicitly overwrites the source range. Every write
is read back and compared byte for byte.

SSD writes are page based and are not transactional. Use the default separate
destination during initial testing; an I/O or power failure during `--in-place`
can leave a partially updated range.

## Build and local test

```bash
cd /mnt/suda/host/applications/vscode-selective-lwe-full-pipeline
make clean && make -j2
make test
```

## Real TPC-H test

Prepare the first 128 SF=1 `lineitem` rows if needed:

```bash
cd /mnt/suda/host/applications/tpch-lineitem-fpga-test
./run_tpch_filter_test.sh --prepare
```

On the CSD host, run the full path. The source is LBA 65536 and the patched
copy is written to LBA 131072:

```bash
cd /mnt/suda/host/applications/vscode-selective-lwe-full-pipeline
set -o pipefail
./run_tpch_q6_update.sh 2>&1 | tee tpch_q6_full_update.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

The remote HPU server must already be listening. On that server, use the same
startup command as the existing full-pipeline test:

```bash
source "$HOME/.config/suda/hpu-server.env"
"$SUDA_HPU_ROOT/scripts/start_remote_server.sh"
```

The default query uses the TPC-H Q6 predicate and projects `quantity`:

```sql
SELECT quantity FROM lineitem
WHERE shipdate >= 19940101
  AND shipdate < 19950101
  AND discount_bp >= 500
  AND discount_bp <= 700
  AND quantity < 24
```

For the prepared first 128 rows, the selected quantities are
`21,23,13,19,14`. With the default remote scalar `1`, destination records at
zero-based indexes `55,79,81,85,99` must contain `22,24,14,20,15` at byte
offset 25; every other byte remains identical to the source SSD image.

Environment overrides:

```text
TPCH_RECORDS=128
TPCH_SSD_DEVICE=/dev/nvmq0n1
TPCH_SSD_NSID=1
TPCH_SSD_LBA=65536
TPCH_OUTPUT_SSD_LBA=131072
TPCH_LWE_KEY=/mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin
HPU_SERVER=10.16.0.129
HPU_SERVER_PORT=19090
HPU_SCALAR=1
HPU_OPERATION=adds
SLM_READ_CHUNK_BYTES=131072
SLM_WRITE_CHUNK_BYTES=131072
SLM_READ_QUEUE_DEPTH=1
TPCH_SKIP_SSD_PREPARE=0
```

The SLM read and write chunk sizes accept 4 KiB-aligned values from 4096 to
134217728. Host-to-SLM writes now default to 128 KiB. Set both chunk variables
to 134217728 to cover a ciphertext batch of up to 128 MiB with one NVMe command. The
Host builds a chained PRP list and the ARM runtime submits consecutive 1 MiB,
256-IOV multi-BD MCDMA windows.

After one run has written and verified the source image, set
`TPCH_SKIP_SSD_PREPARE=1` for request-size comparisons. This avoids repeating
the ordinary block-device write/readback before every FPGA pipeline run.

Before FPGA decryption, the runner saves the Host-verified remote response and
its expected plaintext to:

```text
testdata/tpch_q6_remote_hpu_native.bin
testdata/tpch_q6_remote_expected_u8.bin
```

These files remain available when FPGA execution times out, so the exact same
five-ciphertext input can be tested with `vscode-lwe-decrypt-offload` after the
device runtime and NVMQ connection have been restarted.

The remote service must support protocol operation `op=3`, the HPU-native
round-trip ADDS operation already used by `vscode-lwe-full-pipeline`.

Small decrypt batches use a 30 ms minimum scheduler response budget. The Host
QDMA request still has its separate 10-second driver timeout; an `EINTR` after
roughly 10 seconds means the device execute request did not complete, rather
than exhaustion of the scheduler budget.

## Unified task interface

`run_task.py` uses Python 3 standard libraries. `tasks/tpch-q6-generate.json`
and `tasks/tpch-q6-update.json` describe the input SSD range, fixed-row schema,
SQL projection/predicate, encryption preset and key reference, and downstream
operation. `tasks/runtime.json` holds devices, bounded batch size, transfer
chunks and read queue depth. Relative paths in a task resolve against that
JSON file's directory. No secret key coefficients are embedded in task JSON.

The Q6 examples use the Q6 selection predicates, project `quantity`, and use +1
for the remote update demonstration; they do not implement the Q6 revenue aggregate.
These examples reuse the prepared **binary 512-byte lineitem records** at LBA
65536. They do not prepare or rewrite the input SSD dataset. Optional reference
checking can be added with:

```json
"validation": {"reference": "../../tpch-lineitem-fpga-test/testdata/tpch_lineitem_512b.bin"}
```

After deploying a manifest-capable BOOT.bin, run:

```bash
cd /mnt/suda/host/applications/vscode-selective-lwe-full-pipeline
make -j2
make test

# Validate metadata and preview the generated commands; no device I/O.
python3 run_task.py --task tasks/tpch-q6-generate.json \
  --runtime tasks/runtime.json --dry-run

# SSD -> FPGA filter/encrypt -> Host artifacts; no SSD writeback.
python3 run_task.py --task tasks/tpch-q6-generate.json \
  --runtime tasks/runtime.json

# Remote HPU +1, FPGA decrypt, then COW SSD write/readback at LBA 131072.
python3 run_task.py --task tasks/tpch-q6-update.json \
  --runtime tasks/runtime.json
```

The current preset is `psi64-big-lwe-u8`: predicate fields can use the existing
signed/unsigned integer types, but the encrypted projection remains u8. Generate
supports CPU or HPU native transport layout; remote update requires HPU native.
A dumped `.lwehls.bin` always uses the portable `LWEHLS01` logical serialization,
including plaintext correctness fixtures, regardless of physical AXIS layout.
The current PRNG/noise remains the prototype used by the earlier experiments.

The current SQL subset compiles comparisons and AND/OR/NOT/parentheses to the
existing v2 predicate descriptors and RPN tokens. It does not implement joins,
aggregates, string parsing, or arbitrary SQL. Metadata changes within that
supported subset do not require further synthesis after manifest mode is deployed.

Batch size defaults to 128 rows and is limited to 1024 by this task interface.
Batch boundaries must be LBA aligned: `batch_records` must be a multiple of
`4096 / gcd(record_bytes, 4096)`. The final batch may be shorter and retains its
last SSD page's padding. Each batch has an explicit global `record_base`; the
sidecar selection file maps ciphertext order to global row indices. Ciphertext
memory is bounded by one batch, independent of total dataset size. For example,
1024 HPU-native u8 projections need 96 MiB of ciphertext payload.

Each execution writes into a unique `task-results/run-*` directory. Successful
batches publish selection sidecars; generation also publishes ciphertext dumps.
`result.json` is published only after every batch succeeds. On failure,
`progress.json` lists completed batches and the failed batch. Multiple SSD batch
writes are not an atomic transaction and completed COW batches are not rolled
back. The JSON interface requires a disjoint COW destination; the existing
native `--in-place` interface remains available explicitly.

Task fields, unknown keys, integer ranges, schema boundaries, source/destination
overlap, transport limits and native SQL compilation are checked before the first
batch. Read/write chunks are selectable from 4 KiB to 128 MiB (4 KiB aligned),
and read queue depth is 1, 2 or 4. The ARM runtime and Host driver must already
support the chosen size; JSON does not alter their capabilities.

## Rebuild the filter hardware

The new Host discovery stage requires new filter RTL. Replacing only the Host
application cannot enable manifest mode on an old BOOT.bin. The ARM runtime and
Host driver need no protocol changes for this feature.

On the development workstation, generate and stage the filter RTL:

```bash
cd /home/yangchenghui/suda/device/operators/hls/selective_filter
set -o pipefail
/opt/Xilinx_2020.2/Vitis_HLS/2020.2/bin/vitis_hls \
  -f run_hls.tcl -tclargs selective_filter csynth \
  2>&1 | tee selective_filter_manifest_csynth.log
hls_rc=${PIPESTATUS[0]}
printf 'hls_exit_code=%s\n' "$hls_rc"
if [ "$hls_rc" -eq 0 ]; then
  ./update_shell_rtl.sh
fi
```

The staging script backs up the old pool outside the synthesis source tree.
Then run your normal full bitstream/BOOT.bin build in tmux:

```bash
cd /home/yangchenghui/suda/device/platform/basic_shell/nf-csd
set -o pipefail
bash build_bd.sh 2>&1 | tee build_bd_selection_manifest.log
build_rc=${PIPESTATUS[0]}
printf 'build_exit_code=%s\n' "$build_rc"
```

The full build checks fresh artifacts and post-route timing. Deploy its BOOT.bin
and reboot before exercising the new device discovery mode. HLS synthesis and
simulation do not replace that final implementation/board validation.

## Local protocol/RTL validation

On the development workstation:

```bash
cd /home/yangchenghui/suda/device/operators/hls/selective_filter
./test_manifest.sh          # C model + Host parser + existing filter/LWE tests
./test_manifest.sh --rtl    # also test synthesized RTL with output backpressure
```

The C-model tests cover partial/all/zero matches, invalid configuration,
truncated input, missing TKEEP bytes, excess LBA padding, and corrupt manifest
counts/row indices. RTL tests cover the original projection path and five
manifest scenarios with backpressure. `make test` in the Host application
covers its local update helper and task planning/execution, including global
batch indices and failure publication, without using hardware.
