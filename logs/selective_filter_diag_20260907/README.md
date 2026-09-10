# 2026-09-07 selective_filter RTL diagnosis

## Board validation after RX range fix: PASS

The user reran the original 16-record `gt 32` test on the confirmed existing
BOOT (`dc932e2b7fc3a142cbe20d2075fcbfd4b1302a7e54420fda390e4b9021865589`)
with the rebuilt selective host application. Reported results:

- `compute_output_range_bytes=593920`, allocation remains 1576960 bytes.
- `result_bytes=589824`, exactly matching expected physical ciphertext bytes.
- `total_count=16 selected_count=6 decrypted_count=6`.
- `decrypted_prefix=3a2c2d293627` = `[58,44,45,41,54,39]`.
- `host_key_decrypt_checked=yes`, pipeline passed, `test_exit_code=0`.
- FPGA execute call: 10.935 ms; SSD -> SLM: 6.444 ms;
  SLM -> host: 56.576 ms; reported data path: 76.390 ms.

This verifies the selective host RX-range fix for the reported board case,
including ciphertext readback and decryption. It does not validate arbitrary
early termination in the generic DMA driver. The separate standalone LWE
finish-framing RTL fix still requires a new BOOT and board verification.

## Fixes after standalone board test

The board's standalone LWE test returned 98256 instead of 98304 bytes.
Two distinct issues were found and addressed locally:

1. **LWE finish framing:** `forward_done_packet` preserved a 16-byte external
   DMA marker's TKEEP while RX always subtracts and clears 64 bytes. This
   predicts `98304 + 16 - 64 = 98256` and can clear the last 48 payload bytes.
   LWE now sets finish TKEEP/TSTRB to all ones and TLAST to one, preserving
   its status payload. The old code failed the new 16-byte-marker regression;
   the fixed code passes both 16-byte and 64-byte input markers. The user
   subsequently supplied the device log (04:38:15), confirming raw RX
   `bytes=98320`, `payload_bytes=98256`, `finish_bytes=64`, `result_before=0`.
   `MCDMA_FINISH_STRIP` also confirms clearing 64 bytes at offset 98256:
   the last 48 bytes of the expected ciphertext layout plus the 16-byte
   marker. The dump's first six qwords are zero (consistent with native
   layout tail padding); the last two are `0000003a000186a0` and
   `d5ffead205559abc`. This establishes the framing/accounting mismatch,
   but does not constitute a board decryption check.
2. **Selective application RX range:** the host posted its worst-case SLM
   allocation (1576960 bytes) as the receive range. `rte_axi_dma_poll_complete`
   returns the shared IO only at `head->request_end`, set on the last BD
   of each submission; it does not return early on the task finish marker.
   With 48-page batches, 589824 ciphertext bytes fill three submissions.
   The finish marker completes only the first BD of the fourth 48-BD
   submission, leaving the driver waiting for absent data. The application
   now posts `round_up_to_lba(expected_payload + 64)` = **593920 bytes**.
   The fourth submission then contains exactly one BD. SLM allocation
   remains 1576960 bytes. The new sizing log exposes
   `compute_output_range_bytes=593920`.

The host fix is applicable to the current BOOT and requires rebuilding the
selective application only. It relies on the required reference matching the
SSD data; it is not a general driver fix for arbitrary early termination or
unexpected selection counts. The DMA driver itself was not changed.

Validation completed:

- Host application compiled and linked (existing libnvme header warnings).
- 65-byte packed-u8 HPU-native CSim with a 16-byte input marker passed full
  ciphertext/decryption checks.
- Filter -> scalar LWE C++ regression passed.
- New HLS synthesis completed; generated Verilog was copied to the active
  shell RTL directory. Prior changed RTL files were backed up outside the
  synthesis tree in `suda_backups/rtl/lwe_encrypt.before_finish64_20260907`.
- Verilator RTL regression, synchronous context RAM and output backpressure:
  mode 2 emits 98304 payload + 64 finish bytes; mode 3 emits 589824 payload +
  64 finish bytes for `[58,44,45,41,54,39]`. Zero-key/noise ciphertext bodies
  were checked against each plaintext's four radix digits.

See `lwe_finish64_hls.log`, `lwe_finish64_rtl_test.log`, and
`lwe_finish64_rtl.sha256`. HLS reports an estimated Fmax of 298.78 MHz and
unsatisfied loop constraints; these are synthesis results, not routed timing
signoff. No new BOOT.bin was built or installed in this task. The finish
framing change requires a subsequent full FPGA build and board test.

The RTL test source is
`device/operators/hls/lwe_encrypt/test_rtl_finish.cpp`; reproduce after HLS
synthesis using (adjust `RTL_DIR` and use a fresh `BUILD_DIR`):

```bash
verilator --cc --exe --build -Wno-fatal --top-module lwe_encrypt \
  --Mdir "$BUILD_DIR" "$RTL_DIR"/*.v \
  /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/test_rtl_finish.cpp
"$BUILD_DIR/Vlwe_encrypt"
```

The sections below record the earlier isolated-filter investigation.

The reported board execution stalls after `LWE_FINISH_ACCOUNT TX`.
This log accounts for TX completion, not completion of the FPGA graph.
Normal RX completion and RX posting messages are DEBUG-level in mcdma.c;
their absence from the supplied NOTICE log does not establish that RX received
zero bytes.

The context verification reports the expected configurations:

- Filter: `[16,512,4,0,32,1]` (quantity projection, quantity > 32).
- LWE: `[2048,0,3,0,17,1]` (scalar stream, finish-marker termination).
- Software graph: physical slot 0 -> physical slot 2 -> DMA channel 5.

## Reproduced test

Input is the local `tpch_like_512b.bin` whose SHA-256 matches the user dataset:
`8b7197274c1c41b21569550083ac3c152aeeb8c83d287e1809214ecd14b3ed1f`.
`input.hex` contains its 128 little-endian 512-bit beats. The testbench adds
a separate TUSER=0xff finish beat and models a synchronous context read at
byte address 192 (context word 3).

Vivado 2020.2 xsim exercised the exported RTL from
`device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/selective_filter/`:

| Scenario | Result | Cycles | Output stall cycles |
| --- | --- | ---: | ---: |
| TREADY always asserted | PASS | 134 | 0 |
| TREADY asserted once every 1001 cycles | PASS | 7007 | 6996 |

Both runs emitted exactly `[58,44,45,41,54,39]` with TKEEP=1 and
payload TLAST=0, followed by a TUSER=0xff/TLAST=1 status packet with
magic `0x53464c5453544154`, total=16, selected=6, error=0.
Logs and testbench are stored alongside this report.

These tests cover the isolated exported filter RTL, not OperatorController,
AXIS switch, LWE RTL, DMA, routed timing, or the image actually loaded on board.
They do not establish a root cause for the board stall. No functional source
change was made based on this result.

## Reproduce

From this directory, with Vivado 2020.2 tools on PATH:

```bash
xvlog -sv tb.sv ../../device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/selective_filter/*.v
xelab tb -s filter_tb
xsim filter_tb -runall -testplusarg PAUSE=0 -log run_no_stall.log
xsim filter_tb -runall -testplusarg PAUSE=1000 -log run_backpressure.log
```

Check the explicit `PASS RTL` result, not only process exit status. In this
environment xsim initially failed to launch inside the sandbox and succeeded
after permission to run outside it. Icarus did not finish this generated RTL
simulation and was stopped; it is not the basis of the PASS results.

## Outstanding board evidence

The local September 4 rebuild completed its artifact freshness checks with
an accframework DCP of 27,108,234 bytes. Its BOOT.bin SHA-256 is:
`dc932e2b7fc3a142cbe20d2075fcbfd4b1302a7e54420fda390e4b9021865589`.
The user subsequently confirmed that this is the SHA-256 of the actual
board boot image, excluding the previously rejected older artifact as the
explanation for this run.
The build log also reports post-physical-optimization WNS=-0.112 ns and
TNS=-7.581 ns; artifact generation alone is not timing signoff or evidence
that timing caused this stall.

Next evidence needed: whether standalone LWE completes
on that same image, and device-side hlsacc DEBUG logs including RX posting
and completion. A filter-only graph or board ILA capture can then isolate
whether the stall occurs before LWE input or in encryption/output reception.

The existing standalone host application can use the same SSD LBA without
rewriting the dataset: encrypt one byte with `--plaintext-bytes 1 --expect 160`.
The first byte is 0xa0 (the low byte of record id 100000), not quantity 58 at
offset 4. This controls for the LWE/DMA path, although standalone packed-u8
input differs from the filter graph's scalar-stream mode.

For a smaller graph output, keep `--records 16` and change the predicate to
`--predicate gt --threshold 57`: only quantity 58 matches. The existing host
rejects zero-selection predicates before execution, so `gt 255` cannot serve
as a finish-only hardware test with this executable.
