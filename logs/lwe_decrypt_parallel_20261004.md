# LWE decryption parallel pipeline and hardware timing, 2026-10-04

## Implementation

The decrypt datapath now processes all eight u64 coefficients in a 512-bit
input beat in parallel. Separate framing, address generation, packed key
lookup, coefficient selection, balanced reduction and accumulation stages
replace the eight-cycle beat serializer. Compact-layout beats spanning an LWE
body retain the next block's mask coefficients. Key loading and layout
preparation each take 256 iterations instead of 2048. The context ABI, LWE
parameters, plaintext packing and all three input layouts remain unchanged.

The exported RTL is already synchronized into the shell RTL pool. All 33
exported Verilog files match the RTL used for the final C/RTL cosimulation.
The HLS estimate is 21,723 LUT, 13,124 FF, 0 DSP and 0 BRAM_18K. The previous
estimate was 13,666 LUT, 4,652 FF and 16 BRAM_18K. All hot loops achieved II=1;
estimated clock period is 2.920 ns at a requested 4 ns period and 1.08 ns
uncertainty. These are HLS estimates; final implementation timing and resource
use have not been verified.

## Measured RTL simulation latency

The final eight-transaction C/RTL cosimulation passed. The first three normal
tests match the prior 2026-09-16 test layout, key pattern, plaintext and padding.
Simulation measures ap_start to ap_done with testbench-driven AXIS traffic.

| Case | Previous cycles | New cycles | New latency at 250 MHz | Old/new |
|---|---:|---:|---:|---:|
| HPU-native, 1 u8 | 14,357 | 2,102 | 8.408 us | 6.83x |
| Compact, 65 u8, LBA padding | 535,061 | 67,190 | 268.760 us | 7.96x |
| CPU-padded, 3 u8, until finish | 26,741 | 3,650 | 14.600 us | 7.33x |
| Compact, 1 u8, LBA padding | — | 1,654 | 6.616 us | — |
| Compact, 128 u8, LBA padding | — | 131,702 | 526.808 us | — |

The other three cosimulation transactions check truncation, corrupt padding
and partial TKEEP. The complete 26-case C-model regression also passes.
These speedups compare two RTL implementations, not FPGA versus CPU. No new
board latency or CPU/FPGA acceleration result is claimed.

Saved reports:

- `logs/lwe_decrypt_parallel_cosim_20261004.rpt`
- `logs/lwe_decrypt_parallel_transactions_20261004.rpt`
- HLS project: `device/operators/hls/lwe_decrypt/lwe_decrypt_parallel_final`
- Final cosimulation log:
  `device/operators/hls/lwe_decrypt/hls_parallel_final_cosim_recovered_20261004.log`

## Measurement boundaries

Both encrypt and decrypt slots now have a transparent RTL cycle profiler.
It measures accepted ap_start to the first sampled task-finish TVALID,
including key setup, input starvation and output backpressure. Finish-marker
transfer waiting after that snapshot is excluded. This differs slightly from
the cosimulation ap_done boundary. The profiler also reports input/output
waiting cycles and accepted payload beat counts. Waiting intervals can overlap;
their sum must not be subtracted to claim pure compute time.

The profiler preserves the original first 16 bytes of the 64-byte task marker.
Bytes 16..63 hold a versioned `LWEPF001` footer. ARM clears ordinary markers and
preserves only validated profiling footers. Result lengths still exclude the
marker. Ciphertext payload and layouts do not change. Host apps read metadata
from the existing guard area and reject mismatched output beat counts.

The benchmark reports three distinct boundaries:

- `hw_stream_ms`: device cycle counter at the configured 250 MHz clock.
- `fpga_execute`: synchronous NVMe execute-request latency, including ARM,
  context/stream DMA and completion overhead.
- `fpga_pipeline`: SLM creation, ciphertext upload, program setup, execute and
  plaintext download; excludes parsing and cleanup.

CPU timing still measures the existing warm reference decrypt kernel with
precomputed nonzero key indices and result checking. The benchmark labels
request, hardware-stream and pipeline ratios separately. Pipeline/CPU ratios
are not equal-scope end-to-end comparisons. `--require-hw-profile` rejects
missing device measurements; it never substitutes request latency. Each sweep
records source/key/application SHA-256 and measurement definitions, and resume
checks that configuration matches.

## Validation and deployable artifacts

- Full C-model regression: 26 cases passed.
- C/RTL cosimulation: 8 transactions passed with finite RTL FIFOs.
- Profiler RTL unit test: exact counters, unchanged payload/handshake,
  finish backpressure and three consecutive invocations passed.
- Runtime footer tests: 9,230 payload-offset/IOV-split/preserve cases passed,
  plus malformed metadata validation.
- Benchmark parsing tests: six tests passed, including missing profile,
  differing timing boundaries and invalid correctness results.
- CPU-only benchmark smoke: 1B and 16B passed locally; not a board baseline.
- Both standalone Host applications built successfully.
- ARM libnvmf and nvmf_tgt cross-compilation succeeded using the existing
  aarch64 GCC 10.2 toolchain.
- Vivado shell block-design validation passed. Existing unrelated unused
  host_write_mem_ctrl pin warnings remain. No full place/route was run.

ARM binary:

`device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt`

SHA-256:

`2444e44cf6861dbce41599378a85d327d6cfebda55a65556a1ae48ceea662be8`

Shell decrypt top RTL SHA-256:

`a6b75f5f2a95d98c71cc3193a3d9cf4ac100180011286c3660456008c2c10ec6`

The runtime startup script and its debug/logging modes were not changed.
No Host kernel-driver change is required for profiling. No board runtime or
BOOT.bin was replaced during this work. Final BOOT.bin synthesis remains the
user's tmux build step; inspect final routed timing before deployment.

Source/runtime backup:

`/home/yangchenghui/suda_backups/lwe_decrypt_parallel_20261004/`

Shell RTL backup:

`/home/yangchenghui/suda_backups/rtl/lwe_decrypt_before_logical_20261004_132311.8kAlyP/old_shell_rtl/`

After building and installing the new BOOT.bin and ARM binary, rebuild both
standalone Host apps. Start with small correctness tests, confirm
`hw_profile available=yes`, then run the sweep command in
`host/applications/vscode-lwe-decrypt-offload/README.md`. Large-batch board
performance and the actual CPU acceleration factor remain to be measured.
