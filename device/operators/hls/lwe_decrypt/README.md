# LWE decrypt implementation and device timing

The static context ABI and three layouts are unchanged: `0=compact`,
`1=hpu-native`, `2=cpu-padded`. Each u8 still uses four 2048-dimensional LWE
blocks. Layout 0 bodies can cross 512-bit beats. TKEEP, per-block CPU padding,
known-count LBA padding, task markers and draining after errors remain checked.

The 2026-10-04 implementation consumes **eight u64 coefficients per beat**:

```
AXIS reader / TKEEP prefixes
  -> beat framing (position/count recurrence only)
  -> parallel layout addresses
  -> two packed key-byte reads
  -> eight coefficient selections / validation
  -> balanced prefix/suffix adder trees
  -> dot accumulator -> radix decoder -> plaintext packer
```

Compact beats may contain the body of one block and mask coefficients from the
next. The two sums preserve that boundary; no per-block padding conversion is
required. HPU key permutation is prepared before streaming. Eight key banks
load eight bits at a time; loading and preparation each use 256 loop iterations.
Small internal FIFOs explicitly use SRLs to avoid wide, shallow BRAM mappings.

All hot loops achieved II=1 at a requested 4ns clock; estimated period is
2.920ns. This is an HLS estimate, not a guarantee of final routed timing.
The latest HLS estimate is 21,723 LUT, 13,124 FF, 0 DSP and 0 BRAM_18K.

## Verified RTL cycle measurements

Eight C/RTL cosimulation transactions passed. The input layout, key pattern,
expected plaintext and transfer padding of the first three normal tests match
the prior 2026-09-16 tests. Hardware simulation boundary is ap_start to ap_done,
with testbench-driven inputs/outputs; it excludes ARM, NVMe and Host transfers.

| Test | Previous cycles | New cycles | New time at 250MHz | Previous/new |
|---|---:|---:|---:|---:|
| HPU-native 1B | 14,357 | 2,102 | 8.408 us | 6.83x |
| Compact 65B, LBA padding | 535,061 | 67,190 | 268.760 us | 7.96x |
| CPU-padded 3B, until finish | 26,741 | 3,650 | 14.600 us | 7.33x |
| Compact 1B, LBA padding | — | 1,654 | 6.616 us | — |
| Compact 128B, LBA padding | — | 131,702 | 526.808 us | — |

These factors compare old and new RTL, **not FPGA versus CPU**. Board measurements
and final BOOT.bin routing/timing checks are still required. Records and report
locations are in `logs/lwe_decrypt_parallel_20261004.md`.

## Device cycle counters for encryption and decryption

The shell instantiates `lwe_axis_profile` in both operator slots. It forwards
payload and ready/valid without buffering. A clocked counter measures accepted
ap_start to the first sampled task-finish TVALID. The interval includes local
key setup, input waiting and output stalls, and excludes the final finish-marker
transfer wait. It is not a loop-count estimate or a compute-only measurement.
The separate RTL-cosim ap_done measurement above has a slightly different end.

The existing 64-byte finish marker carries a versioned 48-byte footer:

| Byte offset in finish marker | u64 field |
|---:|---|
| 0..15 | Original task/error words; ARM continues to clear these |
| 16 | `LWEPF001` magic |
| 24 | Wall-clock cycles |
| 32 | Input ready while no valid data, before input finish |
| 40 | Output valid while not ready, before finish snapshot |
| 48 | Accepted input payload beats |
| 56 | Accepted output payload beats |

Input and output stall counts may overlap; do not subtract their sum and call
the remainder pure computation. Clock conversion is 250MHz, the current PL1
operator clock configured in `fidus/mpsoc.tcl`. A clock change requires updating
`device/shared_components/lwe_hw_profile.h` and recording the new configuration.

ARM preserves only a validated footer and clears all ordinary finish markers.
Payload/result byte accounting is unchanged. The footer stays in the existing
finish guard area immediately after the 64-byte-rounded physical payload.
Standalone encrypt/decrypt applications print `hw_profile available=yes` and
`stream_ms`, input/output waits and beat counts. Old BOOT.bin or old runtime
prints `available=no`; no hardware timing is inferred from request duration.

## Rebuild and test

HLS (from this directory):

```bash
LWE_DECRYPT_HLS_PROJECT=lwe_decrypt_parallel_check LWE_DECRYPT_TEST_SMOKE=1 \
  /opt/Xilinx_2020.2/Vitis_HLS/2020.2/bin/vitis_hls \
  -f run_hls.tcl -tclargs lwe_decrypt cosim
/opt/Xilinx_2020.2/Vitis_HLS/2020.2/bin/vitis_hls \
  -f run_hls.tcl -tclargs lwe_decrypt rtl_gen
bash update_shell_rtl.sh
```

The checked RTL has already been exported and synchronized. Complete BOOT.bin
building remains a separate `bash build_bd.sh` step. The ARM `nvmf_tgt` binary
must also be replaced to preserve timing footers. No Host driver or encryption
HLS algorithm change is needed for these counters.
