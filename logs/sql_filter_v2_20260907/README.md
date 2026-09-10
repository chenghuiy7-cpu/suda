# SQL filter metadata v2 and full FPGA build — 2026-09-07

## Result

The full `nf-csd` FPGA build completed successfully. Vivado generated the
bitstream with zero errors, and Bootgen generated a fresh `BOOT.bin`.

- `BOOT.bin`: `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/shell/virt_one_drive/ready_for_download/fidus/BOOT.bin`
- size: `37518168` bytes
- generated: `2026-09-07 18:38:17 +08:00`
- SHA-256: `4fa9527b44c33c09fd58e84d1508d28bfb7333f5a1200cc44b00a842bab706a6`
- `system.bit` SHA-256: `8354b5f6f4ad8f0c94644728b26d69799f27d161f0009afa4e81c4d2b94779fd`
- full build log: `/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/build_bd_20260907_sql_filter_v2.log`

Final post-route timing was WNS `-0.112 ns`, TNS `-1.978 ns`, WHS
`+0.010 ns`, THS `0`. The design is therefore not timing-clean. The worst
paths are in the existing PCIe/QDMA/OperatorController area, rather than the
new SQL filter. The preceding baseline also had WNS `-0.112 ns` and TNS
`-7.581 ns`.

## Implemented behavior

The filter retains the legacy v1 context and adds runtime metadata v2. The
host compiles the supported SQL subset into one context page and sends it to
the FPGA before row data:

```text
SELECT <field> FROM <name> WHERE <expression>
```

The expression supports `=`, `!=`, `<`, `<=`, `>`, `>=`, `AND`, `OR`,
`NOT`, and parentheses. A schema string gives each field's fixed-width type
and byte offset. Supported types are `u8/u16/u32/u64/i8/i16/i32/i64`.
Metadata v2 supports up to eight predicates and fifteen RPN tokens. Records
must be fixed-width, little-endian, 64-byte aligned, and each referenced
field must remain within one 64-byte stream beat. The current LWE pipeline
requires a `u8` projected field.

The host currently still requires `--reference` to compute the exact selected
count before posting the output DMA range. Removing that dependency needs a
finish-aware variable-length RX/runtime interface.

The build also contains the LWE finish-packet correction: the HLS operator
normalizes the 64-byte completion packet's `keep`, `strb`, and `last` fields.
This is intended to correct the standalone result length from `98256` to
`98304` bytes after the runtime strips the finish packet.

## Verification completed before the FPGA build

- Host SQL compiler unit test: passed.
- Host application build: passed.
- Filter C simulation, legacy compatibility, and filter-to-LWE simulation:
  passed.
- Synthesized RTL test with output backpressure: passed; four input rows,
  two selected values `[17, 24]`, 602 cycles.
- HLS synthesis estimate: 352.40 MHz, 20,381 LUT, 5,317 FF, 0 BRAM, 0 DSP.
- Full Vivado implementation: completed; zero unrouted/failed/partial nets.
- Bitstream and BOOT image generation: completed.

## Board tests after booting the new image

First confirm the running image is the one recorded above. Then rebuild and
run the standalone LWE regression:

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-offload
make -j2
set -o pipefail
./vscode-lwe-encrypt-offload \
  --ssd-nsid 1 --ssd-lba 65536 \
  --plaintext-bytes 1 --expect 160 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --skip-dump --benchmark \
  2>&1 | tee standalone_lwe_finish64_test.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

Expected application result: `result_bytes=98304` and exit code `0`.

Run the SQL metadata v2 pipeline against the existing 16-row TPC-H-like
dataset at LBA 65536:

```bash
cd /mnt/suda/host/applications/vscode-selective-lwe-encrypt-offload
make -j2
set -o pipefail
./vscode-selective-lwe-encrypt-offload \
  --ssd-nsid 1 \
  --ssd-lba 65536 \
  --records 16 \
  --record-bytes 512 \
  --schema 'id:u32@0,quantity:u8@4,order_key:u64@8,price:u64@16' \
  --query 'SELECT quantity FROM lineitem WHERE (quantity >= 40 AND id != 100010) OR id = 100014' \
  --reference testdata/tpch_like_512b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output selective_lwe_sql_v2_ciphertexts.bin \
  --benchmark \
  2>&1 | tee selective_lwe_sql_v2_test.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

For the dataset generated on 2026-09-07, the expected selected count is five
and the expected decrypted prefix is `3a2c2d2927`, corresponding to decimal
quantities `[58, 44, 45, 41, 39]`. The expected physical ciphertext length is
`491520` bytes and the expected process exit code is `0`.
