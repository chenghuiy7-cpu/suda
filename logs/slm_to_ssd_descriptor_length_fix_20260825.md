# SLM 到 SSD 直写通路排障与整改全过程（2026-08-25）

## 目标与最终结论

本次整改的目标不是简单让测试程序忽略错误，而是实现并验证下面这条真实数据通路：

```text
FPGA decrypt -> decrypt output SLM -> NVMe Copy -> SSD
```

解密后的明文不应先回读 Host 再由 Host 写 SSD。Host readback 只作为可关闭的正确性检查，不属于正式写盘数据路径。

最终确认并修复了两个前后相继的问题：

1. format 4 Copy 的 range 描述符实际只有 32B，但 ARM runtime 按 4096B 等待，状态机停在 `FETCH_DATA`。
2. 解密的 128B payload 与 64B `TUSER=0xff` finish beat 被 AXI DMA 连续写入 output SLM；虽然 runtime 只统计了 128B，finish beat 仍会随整页 Copy 进入 SSD padding。

最终 128B 完整通路通过，输入首字节 `171` 经远端 HPU 加 1 后解密为 `172`，目标 SSD readback 与 padding 均校验通过。

## 现象

完整通路在 FPGA 解密成功后卡在 `nvme_copy(SLM->SSD)`：

```text
[lwe_full] copying decrypt output SLM directly to SSD nsid=1 lba=131072
nvme_copy(SLM->SSD) failed: ret=-1 errno=4 (Interrupted system call)
```

ARM runtime 同一请求的关键日志为：

```text
HLSACC_REQ_CB ... result=128
HANDC_OP start ... opc=0x19 fsm=FETCH_DATA
HANDC_OP RX_IOV[0] ... len=4096
HANDC_OP TX_IOV[0] ... len=32
HANDC_RX_CMPL ... bytes=32 cur_rx=32 total_rx=4096 fsm=FETCH_DATA
HANDC_TX_CMPL ... bytes=32
```

这说明 FPGA 解密已经正确完成并输出 128 字节；失败发生在随后 Copy 命令的 source range 描述符搬运阶段，不是解密算子故障。

## 完整排障与整改时间线

### 第 1 步：先划定故障边界

Host 最初报告的是 `nvme_copy(SLM->SSD) failed: errno=EINTR`。`EINTR` 只能说明等待中的系统调用被中断，不能直接说明 FPGA 解密、SLM、QDMA 或 SSD 中哪一层出错，因此没有把 `EINTR` 本身当作根因。

ARM 日志先出现：

```text
HLSACC_REQ_CB ... request_id=2 result=128
```

由此确认：

- 解密算子已经收到 HPU-native 密文；
- FPGA 解密执行已经结束；
- runtime 得到了 128B 有效明文结果；
- 故障发生在解密完成之后的 SLM 到 SSD Copy 阶段。

这一步排除了“解密算子卡死”“密钥没有加载”“HPU 返回格式错误”等方向。

### 第 2 步：识别 Copy 命令及其状态机位置

紧接 `HLSACC_REQ_CB` 后，ARM 日志出现：

```text
HANDC_OP start ... opc=0x19 fsm=FETCH_DATA
```

`opc=0x19` 对应 NVMe Copy。Host 使用 format 4 字节范围描述符，先把 source range descriptor 从 Host 搬到 ARM runtime，再由 runtime 解析 SLM namespace、源偏移和字节数，最后构造内部 NVMe WRITE 写入目标 SSD。

因此正常状态机应为：

```text
FETCH_DATA(range descriptor)
  -> END_FETCH_DATA
  -> 解析 source range
  -> SLM_TO_SSD_COPY 开始
  -> 内部 NVMe WRITE
  -> SLM_TO_SSD_COPY 完成
  -> Host completion
```

当时日志只有 `FETCH_DATA`，没有 `HANDC_RX_DONE` 和 `SLM_TO_SSD_COPY 开始`，说明请求尚未进入真正的 SSD WRITE 阶段。

### 第 3 步：用收发长度证明第一次根因

同一个 Copy 请求的日志为：

```text
HANDC_OP RX_IOV[0] ... len=4096
HANDC_OP TX_IOV[0] ... len=32
HANDC_RX_CMPL ... bytes=32 cur_rx=32 total_rx=4096
```

Host 只发送了一个 `spdk_nvme_mc_source_range`。源码中的静态断言确认该结构正好为 32B。QDMA 实际完成 32B 是正确行为，而 runtime 的目标 IOV 被固定成 4096B，导致 `compute_handc_op()` 把 `total_rx_bytes` 算成 4096。

状态机完成条件要求 `cur_rx == total_rx`，实际得到：

```text
cur_rx = 32
total_rx = 4096
```

所以 `HANDC_RX_DONE` 永远不会触发，请求没有 completion，Host 最终才表现为 `EINTR`。program deactivate/unload 的 880 是前一请求未结束后的清理失败，不是独立根因。

### 第 4 步：第一次修改只修描述符长度

没有修改 Host 命令格式，也没有修改 FPGA bitstream。ARM runtime 改为从结构体大小计算描述符长度，并让 source/destination IOV 使用同一长度：

```c
uint32_t descriptor_bytes = sizeof(struct spdk_nvme_mc_source_range) * nr;
ctx->from_iovecs[0].iov_len = descriptor_bytes;
ctx->to_iovecs[0].iov_len = descriptor_bytes;
```

同时增加 `SLM_TO_SSD_DESC_FETCH` 日志。第一次修复的中间版 ARM 二进制为：

```text
SHA-256 44093970df9ebb087ee996d19ab3b929bf0e719aa0ac643b1287c5b76cc7a1f8
```

### 第 5 步：第一次复测证明 Copy 已修通，并暴露第二个问题

第一次修复上板后，Host 不再报 `nvme_copy()` 的 `EINTR`，而是继续执行到：

```text
[lwe_full] reading destination SSD for optional verification
Destination SSD padding is not zero at byte 128: 0x01
```

能够进入 SSD readback 本身就是重要证据：

- 32B descriptor 已经搬运完成；
- Copy 状态机已经退出 `FETCH_DATA`；
- 内部 NVMe WRITE 已经返回；
- SLM 到 SSD 的直写通路已经建立。

此时的问题从“Copy 不完成”变成“Copy 内容的 padding 不干净”，不能继续归因于描述符长度。

### 第 6 步：解释 byte 128 的来源

解密 output SLM 在运行算子前已经整页清零，因此正常情况下 128B 明文之后应保持为零。ARM 的算子 RX 日志显示最后一次 completion 为：

```text
RX bytes=192 tuser=0xff
payload_bytes=128
finish_bytes=64
result=128
```

数据布局实际为：

```text
output SLM [0, 128)   : 解密明文 payload
output SLM [128, 192) : 64B TUSER=0xff finish beat
output SLM [192, 4096): 初始化留下的零
```

runtime 的记账逻辑已经正确执行 `192 - 64 = 128`，所以 `request->result=128` 没有问题。但 AXI DMA 在软件记账前已经把完整 192B 写入 output SLM，单纯修正 `result` 不会自动删除物理内存中的 finish beat。

### 第 7 步：拒绝只放宽 Host 校验

可以让 Host 只比较前 128B、忽略 padding，但这样 64B 协议控制包仍会被写进 SSD。这会掩盖数据布局问题，也不符合“SSD 中只保存明文和零填充”的目标。

也可以在 Host 发起 Copy 前额外提交一次 SLM fill 清理尾部，但会增加一条 Host 控制命令，并把协议清理责任推给每个应用程序。

最终选择在 ARM runtime 的 RX completion 层处理，因为 runtime 最清楚 `TUSER=0xff` 和 finish beat 长度，所有算子都可以统一受益。

### 第 8 步：第二次修改清除物理 finish beat

新增 `mcdma_clear_rx_finish_beat()`：

1. 仅在 RX 且 `TUSER=0xff` 时执行；
2. 用 `completion_bytes - finish_bytes` 计算 finish beat 起点；
3. 按 completion 的 IOV 数组逐段定位，支持 finish beat 跨 IOV/页边界；
4. 只把最后 64B finish beat 清零；
5. payload 的内容和 `request->result` 均不改变；
6. 增加 `MCDMA_FINISH_STRIP` 中文成功/失败日志。

本次 128B 解密对应的清理范围为：

```text
output SLM [128, 192) = 0
```

最终 ARM 二进制为：

```text
SHA-256 21d0ae30c267aacc8089875b679b1a7bac6a3f632174ecd15e124cf2d4925c62
```

### 第 9 步：最终验收

最终复测要求同时满足：

```text
lwe full SSD-to-remote-HPU-to-SSD pipeline passed
remote HPU ciphertext compute passed
destination_ssd_write_path=decrypt_output_SLM->NVMe_Copy->SSD
destination_ssd_readback_checked=yes
fpga_decrypt_checked=yes
```

实际复测全部满足。输入首字节 `0xab (171)`，远端 HPU 执行标量加 1，目标 SSD 解密结果首字节为 `0xac (172)`；128B 明文前缀与预期一致，byte 128 到 4095 的 padding 全零。

### 第 10 步：修改范围与回滚边界

本次只修改 ARM runtime：

```text
device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
```

未修改：

- LWE encrypt/decrypt HLS 与 FPGA bitstream；
- Host NVMQ/QDMA 驱动；
- QEMU 配置；
- 远端 HPU TCP 服务协议；
- Host 完整通路的数据格式。

因此部署时只需要替换 ARM `nvmf_tgt`。两个修改前备份分别对应描述符修复前和 finish strip 修复前，可独立用于代码对比和回滚。

## 根因

Host 为 format 4 Memory Copy 提交一个 `spdk_nvme_mc_source_range`，该结构由静态断言确定为 32 字节。

ARM `mcdma.c` 原先把源 IOV 长度设置为实际描述符长度 `32 * nr`，但把目标 IOV 长度固定为一页 `4096` 字节。`compute_handc_op()` 根据目标 IOV 累加 `total_rx_bytes`，因此 runtime 等待 4096 字节；QDMA 实际只传来 32 字节，得到 `cur_rx=32 < total_rx=4096`，不会触发 `HANDC_RX_DONE`，状态机永久停留在 `FETCH_DATA`。

Host 最终因请求没有 completion 而收到 `EINTR`。随后出现的 program deactivate/unload 880 是未完成请求导致的清理失败，属于次生现象。

## 修改

文件：`device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c`

format 4 Copy 的源、目标 IOV 均使用实际描述符字节数：

```c
uint32_t descriptor_bytes = sizeof(struct spdk_nvme_mc_source_range) * nr;
ctx->from_iovecs[0].iov_len = descriptor_bytes;
ctx->to_iovecs[0].iov_len = descriptor_bytes;
```

增加中文诊断日志 `SLM_TO_SSD_DESC_FETCH`，记录 range 数量、实际字节数和两端物理地址。

修改前源码备份：

```text
backups/slm_to_ssd_desc_len_pre_20260825/mcdma.c
SHA-256 bcc1a960f8c08af6bfb98ac77e60ec60e693f172d73e28b3a4848af7fa4cdbc8
```

## 编译结果

仅增量重编 ARM SPDK `lib/nvmf` 和 `app/nvmf_tgt`，未修改 FPGA 比特流、Host NVMQ 或 QEMU 配置。

```text
build/bin/nvmf_tgt: ELF 64-bit ARM aarch64
SHA-256 21d0ae30c267aacc8089875b679b1a7bac6a3f632174ecd15e124cf2d4925c62
```

## 第二阶段：RX 结束包污染 SSD padding

32 字节描述符修复上板后，`nvme_copy(SLM->SSD)` 已经完成，目标 SSD 也可以读回，但严格校验报告：

```text
Destination SSD padding is not zero at byte 128: 0x01
```

对应的 decrypt RX completion 为 `192B = 128B payload + 64B TUSER=0xff finish beat`。runtime 已经只将前 128B 计入 `request->result`，但 AXI DMA 在软件记账前已把完整 192B 写进 output SLM。因此 byte 128 开始的 64B 是协议结束包，而不是解密明文；直接把整个 4KB SLM 页 Copy 到 SSD 时，该结束包也被带入 SSD。

修复是在 RX completion 识别 `TUSER=0xff` 后，根据本次 completion 的 IOV 布局，把最后 64B finish beat 原位清零。该操作不修改前面的 payload，也不要求 Host 回读或重新封装明文。新增日志：

```text
MCDMA_FINISH_STRIP RX结束包已从输出SLM清零 ... payload_offset=128 bytes=64
```

第二阶段修改前源码备份：

```text
backups/rx_finish_strip_pre_20260825/mcdma.c
SHA-256 69aaf3e5959a79751e8e1cd00e78ee268732d08159e69b64b8168c838a439785
```

## 上板验证预期

修复后描述符阶段应出现：

```text
SLM_TO_SSD_DESC_FETCH ... ranges=1 bytes=32
HANDC_OP RX_IOV[0] ... len=32
HANDC_OP TX_IOV[0] ... len=32
HANDC_RX_CMPL ... bytes=32 cur_rx=32 total_rx=32
HANDC_RX_DONE ...
MCDMA_FINISH_STRIP RX结束包已从输出SLM清零 ... payload_offset=128 bytes=64
SLM_TO_SSD_COPY 开始 ...
SLM_TO_SSD_COPY 完成 ... status=0
```

Host 应完成 SLM 到 SSD Copy，并通过目标 SSD readback 校验。若描述符阶段通过但后续仍失败，应依据 `SLM_TO_SSD_COPY 开始/完成` 日志继续定位内部 NVMe WRITE 阶段，不能再归因于本次 32/4096 字节长度不一致。

## 上板验证结果

使用 SHA-256 为 `21d0ae30c267aacc8089875b679b1a7bac6a3f632174ecd15e124cf2d4925c62` 的 `nvmf_tgt`，128B 完整通路测试通过：

```text
SSD -> SLM -> FPGA encrypt -> Host memory -> TCP -> remote HPU
    -> Host memory -> decrypt input SLM -> FPGA decrypt
    -> decrypt output SLM -> NVMe Copy -> SSD
```

正确性结果：

```text
输入首字节：0xab (171)
远端 HPU 操作：每个 u8 加 1
解密首字节：0xac (172)
解密数量：128
destination_ssd_readback_checked=yes
remote_hpu_ciphertext_compute=passed
fpga_decrypt_checked=yes
```

SSD padding 严格校验通过，证明 RX finish beat 已从 output SLM 清除；SLM 到 SSD Copy 也已完成，证明 32B range 描述符死等问题已修复。

单次 benchmark 的主要结果：

| 阶段 | 延迟 | 在线端到端占比 |
|---|---:|---:|
| decrypt Host -> SLM | 15240.311 ms | 71.068% |
| encrypt output SLM -> Host | 4731.303 ms | 22.063% |
| 远端 RPC round trip | 1234.284 ms | 5.756% |
| FPGA encrypt + decrypt execute | 52.032 ms | 0.243% |
| 完整在线端到端 | 21444.705 ms | 100% |

当前主要性能瓶颈仍是约 12MiB HPU-native 密文在 Host 与 SLM 之间的串行搬运，尤其是解密前的 Host 到 SLM 写入。该结果是单次运行数据，正式性能结论需要多次 warm-up 后报告中位数和 P95。
