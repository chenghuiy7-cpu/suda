# lwe_encrypt stream final-signal fix, 2026-06-26

## Goal

The board was restored with the known-good BOOT image whose SHA-256 is:

```text
42435feb0a58c838882f1a96e133a7e9bebf2236a3187f1d275fd996b6775df0
```

`vscode-blowfish-offload` runs on that image, but `vscode-lwe-encrypt-offload`
still stalls at `nvme_execute_hlsacc_program`.

The host-side test was changed to copy 256 4KB LBAs into input SLM, matching the
blowfish-style data movement path, and the stall still happens. This makes the
remaining likely issue the `lwe_encrypt` stream completion behavior in the RTL.

This update intentionally does not generate a full shell bitstream or BOOT.bin.

## HLS source changes

Files changed:

```text
device/operators/hls/lwe_encrypt/lwe_encrypt.cpp
device/operators/hls/lwe_encrypt/lwe_encrypt.hpp
device/operators/hls/lwe_encrypt/test.cpp
device/operators/hls/lwe_encrypt/test_deep.cpp
```

Functional changes:

- Kept the previous blocking `data_in.read()` behavior so the operator waits for
  SUDA runtime TX data after `ap_start`.
- Changed all output stream beats to full 64-byte physical beats:
  - `TKEEP = 0xffffffffffffffff`
  - `TSTRB = 0xffffffffffffffff`
- Changed the final stream beat marker from `TUSER = 0xf0` to `TUSER = 0xff`,
  matching the MCDMA code comment for the real-last signal while preserving the
  high-nibble check used by `tx_rx_channel_poller`.
- Kept `TLAST = 1` on the final beat.
- Added `write_error_packet()` so invalid context/configuration paths emit a
  final error beat instead of returning without output. This should prevent the
  runtime from waiting forever if context is invalid or not loaded as expected.
- Updated testbenches to treat body packets as one logical body word followed by
  zero padding, so dumps remain compact `[mask..., body]` ciphertext words.

## Local source-level validation

Smoke test command:

```bash
g++ -std=c++14 -DUSING_XILINX_STREAM \
  -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include \
  -I/home/yangchenghui/suda/device/shared_components/hls \
  /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt.cpp \
  /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/test.cpp \
  -o /tmp/lwe_encrypt_hls_smoke_test && /tmp/lwe_encrypt_hls_smoke_test
```

Result:

```text
lwe_encrypt HLS smoke test passed. mask_dimension=2048 u8_inputs=1 radix_blocks=4 decrypt_checked=yes
```

Deep test command:

```bash
g++ -std=c++14 -DUSING_XILINX_STREAM \
  -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include \
  -I/home/yangchenghui/suda/device/shared_components/hls \
  /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt.cpp \
  /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/test_deep.cpp \
  -o /tmp/lwe_encrypt_hls_deep_test && /tmp/lwe_encrypt_hls_deep_test
```

Result:

```text
small encoded-input LWE reference passed. dimension=16 ciphertext_words=34 decrypt_checked=yes
HPU Big-LWE clear-input saved-key reference passed. dimension=2048 ciphertext_words=8196 decrypt_checked=yes
loaded saved psi64 Big-LWE secret key. dimension=2048 ones=1015
HPU u8-radix saved-key reference passed. u8_inputs=4 radix_blocks=16 ciphertext_words=32784 decrypt_checked=yes
```

## HLS RTL/IP generation

Command:

```bash
make -C /home/yangchenghui/suda/device/operators lwe_encrypt_hwop
```

Result:

```text
RTL generation completed. IP catalog exported to lwe_encrypt_ip.zip
HLS processing for lwe_encrypt completed successfully.
```

Vitis HLS report:

```text
Report: /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/syn/report/lwe_encrypt_csynth.rpt
Estimated clock: 3.481 ns
Target clock: 4.00 ns
Estimated Fmax: 287.25 MHz
Total FF: 12593
Total LUT: 30515
DSP: 8
BRAM_18K: 0
URAM: 0
```

Note: HLS reports are not a substitute for the final full-shell post-route timing
report. Do not flash a generated BOOT image unless full-shell timing closes.

Generated IP:

```text
/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt_ip.zip
SHA-256: 72963974103fdcb45d9aeb1049df19edcfeb39e5cc657f75a0e814c966fb9e9b
```

Generated Verilog modules:

```text
lwe_encrypt.v
lwe_encrypt_encrypt_encoded_lwe.v
lwe_encrypt_encrypt_one_lwe.v
lwe_encrypt_encrypt_u8_radix.v
lwe_encrypt_flush_output_packet.v
lwe_encrypt_mul_64s_64s_64_5_1.v
lwe_encrypt_regslice_both.v
lwe_encrypt_write_error_packet.v
lwe_encrypt_write_output_word.v
```

The generated Verilog was copied to:

```text
/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt
```

RTL sanity checks:

```text
lwe_encrypt_flush_output_packet.v:
  data_out_TKEEP = 64'd18446744073709551615
  data_out_TSTRB = 64'd18446744073709551615
  data_out_TUSER = final ? 8'd255 : 8'd0

lwe_encrypt_write_error_packet.v:
  data_out_TKEEP = 64'd18446744073709551615
  data_out_TSTRB = 64'd18446744073709551615
  data_out_TLAST = 1
  data_out_TUSER = 8'd255
```

## BOOT/bitstream boundary

No full bitstream build was run in this update.
No BOOT.bin was generated by this update.
No board boot partition was modified by this update.

The existing local file at:

```text
/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/shell/virt_one_drive/ready_for_download/fidus/BOOT.bin
```

still has SHA-256:

```text
867f730098fb86d058ab0660c25fa0a95bde75539242dc39e2d9b6c82a63b6f4
```

This is the previously generated image that should not be flashed blindly.
The known-good board recovery image remains the `42435...` BOOT.bin.

## Next gate before board test

Before flashing any new image:

1. Build the full shell/BOOT only in a controlled run.
2. Check the final post-route timing report.
3. Do not flash if WNS/TNS show timing failure.
4. Keep the known-good `42435...` BOOT.bin available on the TF card or a backup
   machine before any board reboot.

## Host SLM Read Chunking Update

Date: 2026-06-29

After the stream-fix BOOT image let the FPGA program return, the host test
advanced past `nvme_execute_hlsacc_program()` but failed while reading the
output SLM:

```text
[lwe_encrypt] FPGA execution completed; reading output SLM
nvme_slm_read failed: -1
```

The ARM-side SPDK log repeatedly reported:

```text
axi_dma.c:233:spdk_axi_dma_rx_channel_recv: Failed to allocate spdk_axi_dma_io
```

To reduce pressure on the SLM read path and make the LWE host application match
the more conservative read style used by existing SUDA examples, the LWE host
test was changed to read output SLM in 4KB chunks instead of one single
`output_bytes` request.

Modified file:

```text
/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp
```

Build check:

```text
make -C /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload
```

Result: build passed. Only existing libnvme header warnings were emitted.

Follow-up: the first EINTR retry version could hide repeated interrupts or a
blocking ioctl. The host read helper now prints each 4KB output SLM chunk before
issuing `nvme_slm_read()` and caps EINTR retries at 16 per chunk. This makes the
next board run distinguish:

```text
repeated EINTR retries on one chunk
vs.
a blocking ioctl on a specific output SLM offset
```

Build check:

```text
make -C /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload
```

Result: build passed.

## ARM Runtime RX BD Lifetime Experiment

Date: 2026-06-30

The 4KB host-side SLM read chunking did not eliminate the ARM-side error:

```text
axi_dma.c:233:spdk_axi_dma_rx_channel_recv: Failed to allocate spdk_axi_dma_io
```

Code inspection showed that one `rx_channel_recv()` call can submit up to 64
4KB RX BDs, but the lower AXI DMA layer stores the same `spdk_axi_dma_io`
pointer in every BD generated by that call. The DMA poll path then returns
completed RX BDs one by one, which makes the lifetime of that shared software
IO object ambiguous.

To test whether this is the source of the `io_pool` exhaustion, the HLS compute
RX channel was changed to submit only one RX BD per `spdk_axi_dma_io`.
`rx_channel_done()` already re-enters `channel_recv()` while data remains, so
this keeps the control flow intact while making each completion own exactly one
software IO object.

Modified file:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
```

Functional change:

```text
rx_channel_recv():
  max_cnt = 1
  iov_len = min(PAGE_SIZE, remaining_bytes)
  if spdk_axi_dma_rx_channel_recv() fails, clear iovcnt and return the error
```

Expected effect:

```text
One RX completion corresponds to one spdk_axi_dma_io object, reducing stale
completion/reuse ambiguity for large streamed operator outputs such as
lwe_encrypt.
```

Build check:

```text
bash /home/yangchenghui/suda/device/platform/software_stack/nf_spdk/scripts/arm_cross_compile.sh
```

Result: build passed. New ARM binary:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt
ELF 64-bit LSB executable, ARM aarch64
SHA-256: affe5eb4ffcb17c217d172530d65d14cf11e9dffe32aebba694f082692cb4a8c
```

## Host SLM Read EINTR Retry

Date: 2026-06-30

After the ARM runtime RX BD lifetime patch, the LWE host application advanced
to output SLM read but failed on the first 4KB chunk with:

```text
nvme_slm_read failed: ret=-1 mem_id=2147483650 offset=0 length=4096 errno=4 (Interrupted system call)
```

This is an interrupted synchronous ioctl rather than an ARM-side SPDK error.
The host SLM read helper now retries the same chunk when `nvme_slm_read()`
returns with `errno == EINTR`.

Modified file:

```text
/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp
```

Build check:

```text
make -C /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload
```

Result: build passed. Only existing libnvme header warnings were emitted.

## ARM Runtime SLM_READ / HANDC Diagnostics

Date: 2026-06-30

The host application later hung while reading the output SLM after FPGA
execution had completed. QEMU-side logs showed QDMA/NVMQ timeouts and error
recovery, but that looked like a secondary failure after an SLM_READ request
failed to complete.

To locate the first blocking point, diagnostic notice logs were added to the
ARM runtime SLM_READ and handc paths:

```text
SLM_READ enter
SLM_READ handc_submit
HANDC_OP start
HANDC_OP RX_IOV / TX_IOV
HANDC_OP RX_SUBMIT / TX_SUBMIT
HANDC_TX_CMPL
HANDC_RX_CMPL
HANDC_RX_DONE
HANDC_IMPL request_process
SLM_READ END_FETCH_DATA
HLSACC_REQ_CB
```

Modified file:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
```

Build check:

```text
bash /home/yangchenghui/suda/device/platform/software_stack/nf_spdk/scripts/arm_cross_compile.sh
```

Result: build passed. New diagnostic ARM binary:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt
ELF 64-bit LSB executable, ARM aarch64
SHA-256: a570bc8ebbb294d255039c97832e92a344e88392e514ee91329bf65a50bf48f4
```

## ARM Runtime LWE Context 验证日志

Date: 2026-07-07

当前 LWE 上板现象仍然是：

```text
HLSACC_REQ_CB ... result=0
```

如果 LWE 算子真的已经输出了前面的 65792B ciphertext payload，那么
`result` 不应该为 0。因此本轮先验证 `context` 是否正确进入 LWE
算子，而不是继续直接修改 RX/output SLM 逻辑。

修改文件：

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
```

新增两个只读诊断日志：

```text
LWE_CTX_FETCH  准备搬运context ...
LWE_CTX_VERIFY context已搬入 ... staged_u32=[...] static_u32=[...]
```

期望看到：

```text
static_u32=[2048,1,2,0,17]
```

含义：

```text
2048 = Big-LWE mask_dimension
1    = request_count
2    = input_mode, u8 radix
0    = noise_mode, internal prototype noise
17   = noise_bound_log2
```

判断标准：

```text
如果 static_u32 不符合上述值，优先修 context 初始化路径。
如果 static_u32 正确但 result 仍为 0，再回到 RX completion / TUSER 结束包路径。
```

Build check:

```text
bash /home/yangchenghui/suda/device/platform/software_stack/nf_spdk/scripts/arm_cross_compile.sh
```

Result: build passed.

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt
ELF 64-bit LSB executable, ARM aarch64
SHA-256: 0788cebbe159222aa83a24380efeef5d037d817a807c6006fd00db877f804457
```

## ARM Runtime LWE TX/RX Completion 验证日志

Date: 2026-07-07

上板验证已经确认 context 正确：

```text
LWE_CTX_VERIFY ... static_u32=[2048,1,2,0,17]
HLSACC_REQ_CB ... result=0
```

这说明 `result=0` 不是由 context 初始化错误直接导致。下一步需要判断：

```text
1. TX 是否真的把 input SLM 的第一个 64B 明文 beat 送入 LWE 算子；
2. RX 是否已经给 output SLM post 接收 buffer；
3. 第一个 RX completion 是正常 payload，还是 TUSER=0xff 的 done/error 包。
```

修改文件：

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
```

新增只读诊断日志：

```text
LWE_TX_SEND      提交输入流 ...
LWE_RX_RECV_POST 准备接收输出流 ...
LWE_CH_CMPL      TX/RX完成 ...
```

关键判断：

```text
如果 LWE_TX_SEND first_qword 低 8 位不是测试明文 0x3b，先查 input SLM/SSD copy。
如果 LWE_RX_RECV_POST 没出现，说明 output RX 通道没有正确启动。
如果第一条 RX 的 LWE_CH_CMPL 就是 tuser=0xff 且 bytes 很小/为 0，说明 LWE 算子没有吐 payload，可能是硬件 error/done 路径。
如果第一条 RX 是 bytes=4096 且 tuser=0，然后 result 仍为 0，说明 runtime 记账路径有 bug。
```

Build check:

```text
bash /home/yangchenghui/suda/device/platform/software_stack/nf_spdk/scripts/arm_cross_compile.sh
```

Result: build passed.

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt
ELF 64-bit LSB executable, ARM aarch64
SHA-256: 60b8067a8806374ad61f96770c918b973241aaa4d2e624c6229a99a80570e86f
```

## Host LWE Offload Test Program Rebuild

Date: 2026-07-06

本次只重新编译 QEMU 侧 host 测试程序，不需要重新生成 bitstream。

确认点：

```text
/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp
kSlmReadChunkBytes = 4096
kOutputDonePacketBytes = 64
output_slm_bytes = round_up_to_lba(cipher_physical_bytes + done_packet)
```

目的：

```text
避免 encrypt-count=1 时一次读取 128KB output SLM。
当前 1 个 u8 的预期输出为 4 个 radix Big-LWE ciphertext：
4 * 16448B = 65792B
再加 64B done packet，共 65856B，按 4KB 对齐后为 69632B。
```

Build command:

```text
cd /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload
make clean && make
```

Build result:

```text
PASS
SHA-256: cd6047d303956765ce07dfbef23685c8b61593fcccba7715e4d06c5f018061dc
```

Follow-up board test:

```text
[lwe_encrypt] sizing input_bytes=1048576 compute_input_range_bytes=4096 cipher_physical_bytes=65792 output_slm_bytes=69632 read_chunk_bytes=4096
[lwe_encrypt] FPGA execution completed; reading output SLM
[lwe_encrypt] reading output SLM chunk offset=0 length=4096
[lwe_encrypt] nvme_slm_read EINTR retry 1/16 at offset=0
```

结论：

```text
128KB output SLM 过读不是当前卡死的根因。
当前卡点已经收敛到 compute 完成后的第一条 4KB SLM_READ 是否能到达并完成。
下一步需要用独立 SLM 读写自检区分：
1. 普通 4KB SLM_READ 是否健康；
2. 只有 LWE compute 之后的 output SLM_READ 才失败。
```

Follow-up control tests:

```text
vscode-slmcopy-test:
  data 0 1 2
  Finish only slm read,time used 0.019354s copy times64
  Data check
  data 0 1 2

vscode-blowfish-offload:
  Finish blowfish encryption,time used 0.565648 s
```

结论：

```text
普通 4KB SLM_WRITE/SLM_READ 是健康的；
Blowfish 的 “compute + output SLM read” 完整路径也是健康的；
当前问题是 LWE 算子和 SUDA runtime 在 compute 输出/结束包/通道回收上的交互问题。
```

## LWE Verilog 重新生成并更新到算子池

日期：2026-07-02

本次目标：

```text
将“payload 数据包”和“任务结束标志包”分离后的 lwe_encrypt HLS 逻辑重新生成 RTL，
并更新到 basic_shell 的算子池目录，供后续 build_bd.sh 重新生成 BOOT.bin。
```

执行的 HLS 生成命令：

```bash
cd /home/yangchenghui/suda/device/operators
source /opt/Xilinx_2020.2/Vivado/2020.2/settings64.sh
make lwe_encrypt_hwop 2>&1 | tee /home/yangchenghui/suda/logs/lwe_encrypt_hwop_20260702.log
```

生成结果：

```text
/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt_ip.zip
SHA-256: 93df9bb6213b995e1a3a1d9618c6f9f3ac9ee0985ff49775cb721bfb6ca399cd
```

注意事项：

```text
Vitis HLS 第一次 export_design 时遇到 Vivado core_revision 数值过大的 bad lexical cast；
toolset 脚本自动使用安全 core revision 重试，第二次 IP pack 成功。
最终日志显示：RTL generation completed. IP catalog exported to lwe_encrypt_ip.zip
```

算子池更新：

```text
目标目录：
/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt

更新前备份：
/home/yangchenghui/suda/backups/lwe_encrypt_verilog_20260702_before_finish_packet_fix

Verilog hash 清单：
/home/yangchenghui/suda/logs/lwe_encrypt_verilog_pool_20260702.sha256
```

本次算子池中的 Verilog 文件共 11 个，其中新增了结束包相关模块：

```text
lwe_encrypt_forward_done_packet.v
lwe_encrypt_write_synthetic_done_packet.v
```

核对结果：

```text
lwe_encrypt.v 已实例化 forward_done_packet 和 write_synthetic_done_packet，
说明结束包协议修复版已经进入算子池 RTL。
```

下一步：

```bash
cd /home/yangchenghui/suda/device/platform/basic_shell/nf-csd
source /opt/Xilinx_2020.2/Vivado/2020.2/settings64.sh
bash build_bd.sh 2>&1 | tee build_bd_260702_lwe_fix.log
```

## LWE 算子结束包协议修复

日期：2026-07-02

现象：

```text
HLSACC_REQ_CB ... result=0
```

在 `nvmf_tgt` 已经回退到 `a570bc8e...` 后，`4af4` 版本引入的
`HLS_RX_LAST ... result=64` 回归已经消失，但 LWE 执行结果仍然没有统计到
有效输出字节数。

根据 SUDA 文档：

```text
正常数据包 TUSER=0；
任务结束包是一个无效包，TUSER=0xff；
结束包不应该承载有效 payload，而应该被算子透传到下一级；
输出目的空间需要比最大有效输出多一个 page。
```

原来的 `lwe_encrypt` 把最后一个有效密文 body 包同时标记为
`TUSER=0xff/TLAST=1`。这与 SUDA 的结束包语义不一致，MCDMA 可能会把这个包当
成“无效结束包”而不是有效密文 payload，从而导致 `result=0` 或异常的短结果。

修改文件：

```text
/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt.cpp
/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt.hpp
/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/test.cpp
/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/test_deep.cpp
```

修改内容：

```text
1. 所有真实 LWE 密文 payload 包固定为 TUSER=0、TLAST=0。
2. 不再把最后一个 body 包标记为 TUSER=0xff。
3. 算子处理完 request_count 个输入后，继续读取并丢弃上游 padding 数据，
   直到读到 TUSER=0xff 的 SUDA 结束包。
4. 读到结束包后，将该结束包原样透传到 data_out。
5. 测试激励改为“正常输入包 + 单独 TUSER=0xff 结束包”。
6. 测试读取输出时，遇到 TUSER=0xff 只作为结束标志，不再把它解析为密文。
```

验证：

```text
make -f /home/yangchenghui/suda/device/operators/scripts/toolset.mk \
  TARGET=lwe_encrypt MODE=csim run_hls
```

结果：

```text
lwe_encrypt HLS smoke test passed.
CSIM done with 0 errors.
```

额外本地深度回归：

```text
g++ -std=c++14 -DUSING_XILINX_STREAM ... test_deep.cpp lwe_encrypt.cpp
/tmp/lwe_encrypt_test_deep
```

结果：

```text
small encoded-input LWE reference passed
HPU Big-LWE clear-input saved-key reference passed
HPU u8-radix saved-key reference passed
```

下一步：

```text
需要重新执行 HLS RTL 生成、更新算子池 Verilog、重新生成 BOOT.bin 上板。
上板后期望 HLSACC_REQ_CB 的 result 变为有效密文 payload 字节数：
encrypt-count=1 时应接近 65792 bytes，而不是 0 或 64。
```

Expected use:

```text
Replace only nvmf_tgt on ARM and rerun the same LWE host test. This does not
require rebuilding or replacing BOOT.bin.
```

## Host Output SLM 128KB Readback Experiment

Date: 2026-07-01

A clean LWE-only ARM log showed that the FPGA execution reached
`HLSACC_REQ_CB ... result=0`, but no `SLM_READ enter` appeared afterward. The
QEMU side then reported an NVMQ/QDMA I/O timeout while the host program was
issuing the first output SLM read at offset 0.

Because the known-working blowfish application reads output SLM data in 128KB
chunks, the LWE host application was changed as a host-only experiment to match
that transfer shape:

```text
physical LWE output for encrypt-count=1: 69632 bytes
allocated output SLM/readback buffer:     131072 bytes
nvme_slm_read chunk size:                 131072 bytes
verification data:                        first physical LWE bytes only
```

Modified file:

```text
/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp
```

Build check:

```text
make -C /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload
```

Result: build passed. The generated host binary is:

```text
/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload
SHA-256: 7a0f39fd9b047d0a60fc8168828a2139327117eeeeb3b8e481fd52379175002e
```

Expected next ARM diagnostic signature:

```text
SLM_READ enter ... start=0 len=131072
```

If this appears and completes, the earlier failure is tied to the small 4KB
SLM_READ shape. If it still does not appear, the issue is before ARM receives
the output SLM read request.

## QEMU NVMQ Diagnostic Instrumentation

Date: 2026-07-01

The LWE-only ARM runtime log showed:

```text
HANDC_OP start ... opc=0x1 fsm=FETCH_DATA
HANDC_RX_CMPL ... status=0 bytes=4096
HANDC_TX_CMPL ... status=0 bytes=4096
HLSACC_REQ_CB ... result=0
```

This means the `lwe_encrypt` FPGA compute request completes on the ARM/SPDK
side. However, no `SLM_READ enter` line appears afterward, so the output SLM
read request is not reaching the ARM runtime.

Added QEMU-side nvmq diagnostics with the `NVMQ_DIAG` prefix to track
interesting NVMe commands through:

```text
QUEUE_RQ
H2C_SUBMIT / H2C_SUBMIT_FAIL
H2C_DONE
C2H_POST / C2H_POST_FAIL
C2H_CQE
TIMEOUT
```

Modified files:

```text
/home/yangchenghui/suda/host/drivers/nvmq/qdma.c
/home/yangchenghui/suda/host/drivers/nvmq/trace.h
```

Build check:

```text
make EXTRA_CFLAGS='-DTRACE_INCLUDE_PATH=/home/yangchenghui/suda/host/drivers/nvmq'
```

Result: build passed. New diagnostic kernel module:

```text
/home/yangchenghui/suda/host/drivers/nvmq/nvmq.ko
ELF 64-bit LSB relocatable, x86-64
SHA-256: 6dfed7a224fc27e6d4be96316b14af60004760d8e19cd74a5558b948cadc8547
```

Expected QEMU diagnostic signature for the current LWE output read:

```text
NVMQ_DIAG ... opc=0x02 nsid=0x80000002 ... cdw12=0x20000
```

`cdw12=0x20000` corresponds to the current 128KB output SLM readback chunk.

## ARM Runtime RX Last Completion Fix

Date: 2026-07-01

The QEMU-side comparison showed that Blowfish output SLM reads complete with:

```text
QUEUE_RQ -> C2H_POST -> H2C_SUBMIT -> H2C_DONE -> C2H_CQE
```

For LWE, the output SLM read reaches:

```text
QUEUE_RQ -> C2H_POST -> H2C_SUBMIT
```

but no `C2H_CQE` is returned, and the request later times out. This means the
LWE compute request completes, but the following SLM read sees a dirty/broken
QDMA/MCDMA state.

The likely runtime bug was in `tx_rx_channel_poller()` for compute RX channels:
when RX completion had `tuser=0xff`, the code treated it as the final signal and
broke out before accounting `transfered_bytes` or decrementing `iovcnt`. LWE
uses TLAST/TUSER to end a short expanded output stream, so its final completion
can still carry real payload bytes. Not accounting this final completion can
leave channel bookkeeping inconsistent when the request is freed.

Modified file:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
```

Runtime changes:

```text
- Count RX-last completion bytes before marking the channel finished.
- Decrement RX iovcnt for the RX-last completion.
- Add an HLS_RX_LAST notice log for this path.
- Reset reg_for_next_send in tx_rx_channel_release().
```

Build check:

```text
bash /home/yangchenghui/suda/device/platform/software_stack/nf_spdk/scripts/arm_cross_compile.sh
```

Result: build passed. New ARM diagnostic/runtime binary:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt
ELF 64-bit LSB executable, ARM aarch64
SHA-256: 4af4c811d74c1d0e353a8dce373235e058f23e095e47f26a9b55d73b8e396675
```

Expected ARM-side signature during the next LWE run:

```text
HLS_RX_LAST channel=... bytes=... total=... iovcnt=...
HLSACC_REQ_CB ... result=69632
```

If this fix is correct, the following QEMU-side output SLM read should then gain
the missing `NVMQ_DIAG C2H_CQE ... opc=0x02 ...` line.

## ARM Runtime HLSACC Free-Drain Fix

Date: 2026-07-01

After the RX-last accounting fix, the host can still reach:

```text
[lwe_encrypt] FPGA execution completed; reading output SLM
[lwe_encrypt] reading output SLM chunk offset=0 length=131072
[lwe_encrypt] nvme_slm_read EINTR retry 1/16 at offset=0
```

This means the LWE execute completion is returned to the host, but the first
output SLM read can still stall. Blowfish does not reproduce the failure because
its input and output stream sizes are symmetric, while LWE expands one 64-byte
input beat into 1028 64-byte output beats for one u8 radix encryption. This
hits a short-output/TLAST/free boundary in the runtime.

The next suspected race is in `hlsacccompute_req_callback()`:

```text
spdk_hlsacccompute_free_request(..., true)
send execute completion back to host
```

`spdk_hlsacccompute_free_request(..., true)` sends `FORCE_FREE_OPS`, but the old
path did not wait for that command's hardware completion before the host was
allowed to issue the following SLM_READ. The new fix polls the HLSACC CQ for a
short drain window after sending `FORCE_FREE_OPS` and before completing the
host-side execute request.

Modified file:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
```

New runtime log marker:

```text
HLSACC_FREE_DRAIN mcdma_req=... polls=...
```

Build result:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt
ELF 64-bit LSB executable, ARM aarch64
SHA-256: 1744bc76b042f66aa6907c12b2260fc70d401d568026d7f825ee8bd6ba34d222
```

Expected next-test markers on the ARM side:

```text
HLS_RX_LAST ...
HLSACC_REQ_CB ... result=65792
HLSACC_FREE_DRAIN ... polls=...
SLM_READ enter ... len=131072
```

If `SLM_READ enter` does not appear after `HLSACC_FREE_DRAIN`, then the request
is stuck before reaching the ARM runtime. If it appears but no `HANDC_*`
completion follows, the remaining bug is in the SLM_READ hand-copy DMA path.

## Host Input Range Page Alignment Fix

Date: 2026-07-01

The latest ARM log showed:

```text
ERROR! DATA USED IS BIGGER THAN NEEDED!
HLS_RX_LAST channel=5 bytes=64 total=64 iovcnt=0
HLSACC_REQ_CB ... result=64
```

For one u8 radix encryption, the expected LWE output is:

```text
4 radix blocks * 257 64-byte beats = 65792 bytes
```

So `result=64` is not a successful LWE ciphertext. The `DATA USED IS BIGGER`
error is from the HLSACC TX/input channel accounting path. The host program had
set the input memory range length to the exact compute stream size:

```text
encrypt_count=1 -> compute_stream_bytes=64
```

That is too small for this runtime path, which accounts DMA work at page-ish
granularity. The fix keeps the logical compute stream size at 64 bytes, but
rounds the memory range exposed to HLSACC up to one 4KB page.

Modified file:

```text
/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp
```

Behavior change:

```text
ranges[0].payload.length = round_up(encrypt_count * 64, 4096)
```

The LWE operator still stops after `request_count=encrypt_count`, so the extra
input range bytes are only runtime padding.

Build result:

```text
/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload
ELF 64-bit LSB pie executable, x86-64
SHA-256: 9f866ed20462a091b26b03eee4627ddfb365d6572741b333c7f9069ef0f19820
```

## ARM Runtime Range/Channel Diagnostic Logs

Date: 2026-07-01

The host binary hash was confirmed to be the page-aligned-input build:

```text
9f866ed20462a091b26b03eee4627ddfb365d6572741b333c7f9069ef0f19820
```

However the ARM log still showed:

```text
ERROR! DATA USED IS BIGGER THAN NEEDED!
HLS_RX_LAST ... total=64
HLSACC_REQ_CB ... result=64
```

This means either the ARM runtime still receives a 64-byte input range, or the
TX DMA completion reports more bytes than the range length. Added notice-level
diagnostics to distinguish those cases.

New ARM log markers:

```text
MRANGE_CREATE ... idx=0 ... length=... quick_len=...
EXEC_TX_OB ... len=... cur_used=...
EXEC_RX_OB ... len=... cur_used=...
ERROR! DATA USED IS BIGGER THAN NEEDED! channel=... transfered=... cur_used=... iov_len=...
```

Build result:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt
ELF 64-bit LSB executable, ARM aarch64
SHA-256: 42d13bcd7812da8d062ee8b255688f1141a0432da5e9f28acf08fecbf74c7dfe
```

Expected interpretation:

```text
MRANGE_CREATE idx=0 length=4096 and EXEC_TX_OB len=4096
```

If the error still says `transfered > iov_len`, the bug is in the TX completion
accounting path. If `length` or `EXEC_TX_OB len` is still 64, the bug is in host
range creation or command decoding.

## ARM Runtime Source Rollback: 4af4 -> a570

Date: 2026-07-01

The ARM board was confirmed to be running:

```text
4af4c811d74c1d0e353a8dce373235e058f23e095e47f26a9b55d73b8e396675  nvmf_tgt
```

The available ARM backup was:

```text
a570bc8ebbb294d255039c97832e92a344e88392e514ee91329bf65a50bf48f4  nvmf_tgt.bak.20260701-091641
```

The local source was rolled back from the post-`a570` runtime experiments. The
following `mcdma.c` changes were removed:

```text
- 4af4 RX-last/output readback experiment:
  - HLS_RX_LAST notice log
  - RX-last transfered_bytes accounting before channel finish
  - RX-last iovcnt decrement
  - reg_for_next_send reset in tx_rx_channel_release()

- 1744 free-drain experiment:
  - HLSACC_FREE_DRAIN polling after spdk_hlsacccompute_free_request()

- 42d13 range/channel diagnostics:
  - MRANGE_CREATE notice log
  - EXEC_TX_OB / EXEC_RX_OB notice logs
  - expanded DATA USED IS BIGGER THAN NEEDED notice payload
```

Kept from the `a570` source stage:

```text
- SLM_READ / HANDC notice diagnostics
- HLSACC_REQ_CB notice diagnostic
- Earlier RX BD lifetime patch from affe5...
```

The intended source baseline after this rollback is therefore closest to
`a570bc8e...`, not `4af4c811...`, `1744bc76...`, or `42d13bcd...`.

Build check after rollback:

```text
bash /home/yangchenghui/suda/device/platform/software_stack/nf_spdk/scripts/arm_cross_compile.sh
```

Result: build passed. The regenerated ARM binary exactly matches the intended
backup hash:

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt
ELF 64-bit LSB executable, ARM aarch64
SHA-256: a570bc8ebbb294d255039c97832e92a344e88392e514ee91329bf65a50bf48f4
```

## LWE RX 结束包内容诊断

Date: 2026-07-07

背景：

```text
LWE 执行命令能够返回，但 HLSACC_REQ_CB result=0。
QEMU 侧随后读 output SLM 第一个 4KB 时超时。
ARM 侧日志显示 LWE RX 方向第一条 completion 就是 bytes=64, tuser=0xff。
```

判断：

```text
现在需要区分这个 64B tuser=0xff 包到底是：
1. LWE 算子 write_error_packet 主动输出的错误包；
2. SUDA 普通 done marker；
3. slot/连接/bitstream 异常导致的非预期结束包。
```

修改：

```text
在 device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c 的
tx_rx_channel_poller() 中，RX completion 遇到 tuser 高 4 bit 为 0xf 时，
从 io->iovs[0].iov_base 拷贝最多 64B，并按 8 个 little-endian qword 打印：

LWE_RX_LAST_DUMP 收到RX结束包 ... qword=[...]
```

说明：

```text
该修改只增加诊断日志，不改变 payload 统计、cur_used、is_last 或 callback 行为。
如果 qword[0] 等于 LWE error magic，则说明算子内部主动走了错误路径；
如果是普通 done marker，则说明算子没有输出 ciphertext payload 就结束。
```

Build result:

```text
bash /home/yangchenghui/suda/device/platform/software_stack/nf_spdk/scripts/arm_cross_compile.sh
```

结果：编译通过，只有既有 warning。生成的 ARM 二进制：

```text
/home/yangchenghui/suda/device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt
ELF 64-bit LSB executable, ARM aarch64
SHA-256: b1663fb16a6951c32b488c2a274bd70dae6b43f5be6dad917c79cfe53f66b2eb
```

Host 侧保护：

```text
在 host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp 中，
nvme_execute_hlsacc_program() 返回后打印 result_bytes。
如果 result_bytes 小于期望的 ciphertext payload 字节数，则直接停止，
不再继续读 output SLM，避免 result=0 时触发 QEMU nvmq 的 SLM read timeout。
```

Build result:

```text
make -C /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload
```

结果：编译通过，只有 libnvme 头文件中的既有 warning。
