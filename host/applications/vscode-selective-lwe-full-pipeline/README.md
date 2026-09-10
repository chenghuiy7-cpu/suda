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

The FPGA filter currently returns values without row identifiers. Therefore,
the application evaluates the same query against `--reference` to obtain the
selected row indexes and exact expected output count. Before writing, it reads
the source SSD pages and compares their record bytes with that reference. A
mismatch aborts the update.

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
```

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
