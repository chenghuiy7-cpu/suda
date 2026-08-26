# ARM runtime SLM 搬运 IOV 边界修复（2026-08-25）

## 背景

端到端通路当前采用：

- SLM 到 Host：单请求最大 128KB；
- Host 到 SLM：单请求固定 4KB。

检查 ARM `mcdma.c` 后确认，128KB 是 Host 应用当前限制，并不是协议或 QDMA
硬上限。ARM runtime 的 `handc_ctx` 为每个方向保存 64 个 IOV，每个 IOV 当前
描述一个最大 4KB 的 page，因此该实现能完整描述的单请求上限为：

```text
64 pages * 4096 bytes = 262144 bytes = 256KB
```

## 原始问题

SLM write 在获取 Host PRP list 后使用：

```c
for (int i = 1; i <= 128 && read_or_write_length > 0; i++)
```

但 `from_iovecs` 和 `to_iovecs` 均只有 64 项。请求超过 256KB 时，循环会从
下标 64 开始越界写入 `handc_ctx` 的其他字段或相邻内存。

SLM read 的循环虽然只遍历 64 项，但此前没有验证剩余长度。超过 256KB 的
请求可能只构造前 256KB IOV，然后把截断后的搬运当作完整请求继续提交。

这两种行为都不能用作扩大搬运粒度的基础：write 可能破坏内存，read 可能
静默返回不完整数据。

## 修改内容

修改文件：

```text
device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
```

1. 定义统一容量和最大单请求长度：

```c
#define HANDC_IOVEC_CAPACITY 64
#define HANDC_MAX_TRANSFER_BYTES (HANDC_IOVEC_CAPACITY * PAGE_SIZE)
```

2. `handc_ctx` 的两个 IOV 数组统一使用 `HANDC_IOVEC_CAPACITY`，避免数组长度与
   循环边界分别维护。
3. SLM read/write 首次进入 runtime 时验证：
   - SLM namespace 必须能够成功查找；
   - 长度不能为 0；
   - 长度不能超过 256KB。
4. 非法请求设置 NVMe `SPDK_NVME_SC_INVALID_FIELD` 并进入 completion 路径，
   不再启动 DMA，也不会让 Host 一直等待超时。
5. SLM write 的 PRP 展开循环由 `i <= 128` 改为
   `i < HANDC_IOVEC_CAPACITY`。
6. SLM read/write 在 PRP 展开后检查剩余长度。若仍有未描述数据，立即结束
   请求，避免部分搬运后错误地返回成功。
7. 增加中文日志：

```text
SLM_IO_VALIDATE
SLM_WRITE_IOV_BUILD
SLM_READ_IOV_BUILD
```

## 行为边界

修复后 ARM runtime 的当前行为为：

| 单请求长度 | runtime 行为 |
|---:|---|
| 1B 到 256KB | 按 PRP/IOV 正常构造和搬运 |
| 0B | 返回 NVMe INVALID_FIELD |
| 大于 256KB | 返回 NVMe INVALID_FIELD，不提交 DMA |

256KB 是当前静态数据结构允许的上限，不代表已经完成板上稳定性验证。现有
4KB write 和 128KB read 路径的有效 IOV 构造逻辑没有改变。

## 编译与产物

使用项目交叉编译脚本：

```bash
cd /home/yangchenghui/suda/device/platform/software_stack/nf_spdk
bash scripts/arm_cross_compile.sh
```

编译和链接成功。输出仍包含仓库原有 warning，本次修改没有引入编译错误。

```text
build/bin/nvmf_tgt: ELF 64-bit ARM aarch64
SHA-256: 9716bce90fcbebc2308f5861c2e3a3c765e9946dcc9d1e57da6a98055c23bebc
```

保留的命名产物：

```text
build/bin/nvmf_tgt.slm_iov_boundary_20260825
```

## 板上回归顺序

1. 先替换 ARM `nvmf_tgt`，重启 runtime，并重建 QEMU/NVMQ 连接。
2. 使用现有完整通路回归 128B 明文，确认 128KB SLM read 和 4KB SLM write
   仍然通过。
3. 保持 queue depth 为 1，单独建立 Host 到 SLM 再回读校验工具。
4. 依次测试 write 粒度 4KB、16KB、32KB、64KB、128KB，每个粒度校验完整
   payload 和边界数据。
5. 128KB 稳定后再测试 256KB；在完成足够轮次的正确性和 P95/P99 测试前，
   不把 256KB 作为应用默认值。
6. 不同时调整 queue depth，避免把并发问题和单请求大小问题混在一起。

本次只修改并重编 ARM runtime，不需要重新生成 FPGA bitstream 或更换
`BOOT.bin`。

## 板上回归结果

部署 SHA-256 为
`9716bce90fcbebc2308f5861c2e3a3c765e9946dcc9d1e57da6a98055c23bebc`
的 ARM `nvmf_tgt` 后，128B 完整通路回归通过：

```text
SSD -> SLM -> FPGA encrypt -> Host -> TCP -> remote HPU
    -> Host -> decrypt input SLM -> FPGA decrypt
    -> decrypt output SLM -> NVMe Copy -> SSD
```

正确性结果：

```text
input_first_u8=171
remote_operation=adds scalar=1
decrypted_first_u8=172
decrypted_count=128
destination_ssd_readback_checked=yes
remote_hpu_ciphertext_compute=passed
```

本次单次性能数据：

| 阶段 | 延迟 |
|---|---:|
| SSD 到 SLM | 7.209ms |
| FPGA encrypt | 33.187ms |
| SLM 到 Host | 4642.960ms |
| 远端 RPC 往返 | 1207.649ms |
| Host 到 decrypt input SLM | 17885.713ms |
| FPGA decrypt | 19.360ms |
| decrypt output SLM 到 SSD | 8.754ms |
| SSD 回读校验 | 3.529ms |
| 在线端到端 | 23943.226ms |

结果说明此次 ARM IOV 边界修复没有破坏现有 128KB read 和 4KB write 路径。
当前性能瓶颈仍是 Host 到 SLM 的 4KB 同步分块写入，其次是 SLM 到 Host
回读；本次修改属于扩大粒度前的正确性修复，并未改变 Host 应用的实际粒度。

## 128KB Host 到 SLM 验证入口

为单独验证大粒度 Host 到 SLM 写入，完整通路程序增加参数：

```text
--slm-write-chunk-bytes N
```

参数范围为 4096B 到 131072B，必须按 4KB 对齐，默认值仍为 4096B。程序将
打印实际的 `write_chunk_bytes` 和 `write_requests`。对于 128B 明文产生的
12582912B HPU-native 密文：

| 写粒度 | Host 到 SLM 请求数 |
|---:|---:|
| 4KB | 3072 |
| 128KB | 96 |

该改动只影响远端 HPU 结果写入 decrypt input SLM 的分块方式，不改变密文
内容、FPGA 解密逻辑、SLM 到 SSD 路径或远端协议。应用已在 x86 Host 编译
成功；下一步在板上保持单请求串行，验证 128KB 写入的正确性和延迟。

## 128KB Host 到 SLM 首次板上结果

完整通路显式设置 `--slm-write-chunk-bytes 131072` 后，应用提交了 96 个
128KB 请求。ARM 日志中实际出现 96 条写入构造记录，每条均为：

```text
SLM_WRITE_IOV_BUILD PRP展开完成 ... iovcnt=32 bytes=131072
```

这证明 Host PRP 列表已被 runtime 展开为 32 个 4KB IOV，且没有触发长度
上限或 IOV 越界检查。96 个写命令在应用层均返回成功，但随后 FPGA decrypt
执行返回 `-1`，完整通路未通过：

```text
[lwe_full] writing remote HPU result to decrypt SLM chunk_bytes=131072 requests=96
[lwe_full] executing lwe_decrypt on FPGA
nvme_execute_hlsacc_program(decrypt) failed: -1
```

因此 `SLM_WRITE_IOV_BUILD` 不能当作数据完整写入的证明。当前 HANDC 实现对
一个128KB写命令提交一个32-BD RX，并提交32个彼此独立的单BD TX；请求完成
主要由 RX 累计字节数驱动，尚未把全部 TX completion 状态纳入完成门槛。
需要继续核对最后一个写请求的 RX/TX completion 和随后的 decrypt 输入流，
并增加写后回读逐字节校验，区分以下两种情况：

1. decrypt input SLM 内容不完整或乱序；
2. SLM 内容正确，但大粒度搬运后 DMA/HANDC 通道仍有未回收状态，影响下一
   个 FPGA execute 请求。

在上述验证完成前，128KB WRITE 只能视为描述符构造通过，不能作为稳定支持
的写粒度；完整通路默认值继续保持4KB。

## 128KB 首次失败的完整日志复核

对 ARM 与 QEMU 日志按时间对齐后，结论比首次观察更明确：最后一个 128KB
SLM WRITE 的数据搬运 completion 是完整的。ARM 侧记录到一个 131072B RX
completion、32 个状态为 0 且长度为 4096B 的 TX completion，随后请求进入
`END_FETCH_DATA`。因此本次失败不能再简单归因于“最后一个 WRITE 尚未完成”。

随后 decrypt context 也成功搬入并校验：

```text
staged_u32=[128,2048,0,134217728,2]
static_u32=[128,2048,0,134217728,2]
```

decrypt 输入通道从 input SLM 连续提交了 64 批、每批最多 48 个 4KB IOV，
最后一批带 `last_data=1`，累计覆盖完整的 12582912B HPU-native 密文。但在
最后一条 `LWE_TX_SEND` 之后，没有出现 `HLSACC_REQ_CB`；QEMU 约 5 秒后报告
`I/O 0 QID 0 timeout`，继而进入 NVMQ/QDMA controller error recovery。
应用看到的 `nvme_execute_hlsacc_program(decrypt) failed: -1` 是该超时的结果，
不是 128KB WRITE 命令直接返回的错误。

HANDC 搬运使用 MCDMA 物理通道 4，HLS 算子使用物理通道 5 到 7，因此不是
同一个通道的描述符直接复用冲突。当前故障边界为：

```text
96 个 128KB WRITE 返回成功
  -> decrypt context 正确
  -> decrypt 输入 12582912B 全部提交
  -> 没有 decrypt 输出结束与 HLS request callback
  -> Host execute 命令超时
```

现有日志只能证明搬运字节数和 completion 数量正确，不能证明 input SLM 中
每一字节的内容和页顺序正确。为此完整通路程序新增可选诊断参数：

```text
--verify-decrypt-slm-write
```

启用后，程序会在 Host 到 decrypt input SLM 写入完成后，对整个 SLM 进行
回读并与远端 HPU 返回缓冲区逐字节比较；失败时报告首个不一致偏移、期望值
和实际值。该参数默认关闭，不改变正常完整通路。

下一次验证结果的判读方式：

1. 若回读校验失败，故障在 128KB WRITE 的 PRP/IOV 映射、页顺序或数据搬运；
2. 若回读校验通过但 decrypt 仍超时，故障在 WRITE 之后的全局 MCDMA 状态、
   HLS RX 接收或算子完成协议；
3. 若回读校验通过且 decrypt 也恢复，说明增加的回读/等待改变了时序，需要
   继续验证通道 quiescence 和请求完成竞态，不能直接判定问题消失。

## 关闭写后回读后的 20 次稳定性回归

在保持 SLM READ 和 WRITE 粒度均为 128KB、关闭
`--verify-decrypt-slm-write`、保留最终 SSD 回读校验的条件下，完整通路连续
执行 20 次：

```text
PASS=20
FAIL=0
```

每次都满足以下正确性条件：

```text
lwe full SSD-to-remote-HPU-to-SSD pipeline passed
remote HPU ciphertext compute passed
destination_ssd_readback_checked=yes
```

这说明 128KB Host 到 SLM WRITE 在本轮连续测试中没有再次触发 decrypt
execute 超时；此前单次失败未能稳定复现。结合写后全量回读逐字节通过，可将
128KB WRITE 认定为已经通过当前阶段的功能性和初步稳定性验证，但仍需在更长
时间和更多轮次下观察低概率竞态。

20 次关键阶段统计如下：

| 阶段 | 平均(ms) | 中位(ms) | P95(ms) | 最小(ms) | 最大(ms) |
|---|---:|---:|---:|---:|---:|
| FPGA encrypt | 29.107 | 28.784 | 32.180 | 27.177 | 32.466 |
| SLM 到 Host | 4357.431 | 2994.357 | 9397.790 | 2162.336 | 12056.134 |
| Host 到 decrypt input SLM | 3358.554 | 2858.569 | 5516.717 | 1918.285 | 7959.807 |
| FPGA decrypt | 31.461 | 25.799 | 44.828 | 18.648 | 85.800 |
| decrypt output SLM 到 SSD | 9.475 | 8.519 | 16.600 | 3.844 | 18.060 |
| 在线端到端 | 9167.038 | 8567.318 | 13702.313 | 5504.746 | 15891.745 |

原 4KB WRITE 的单次 Host 到 SLM 延迟约为 15240ms；改为 128KB 后，20 次
中位数为 2859ms，约改善 5.3 倍。FPGA encrypt/decrypt 和 SLM 到 SSD 的波动
较小，当前主要性能不稳定性集中在 SLM 到 Host 与 Host 到 SLM 两个大密文
搬运阶段：SLM 到 Host 最大值达到 12056ms，Host 到 SLM 最大值达到 7960ms。
因此后续优化重点应从“是否支持 128KB”转向分析每个 128KB 子请求的长尾、
串行请求调度和 QDMA/NVMQ 往返开销。

本轮原始日志目录：

```text
host/applications/vscode-lwe-full-pipeline/
write128k_stability_20260825_055131/
```

## HANDC 128KB multi-BD DMA 提交优化

### 优化目标

保留 Host 和 SLM 两侧由内存页自然形成的 32 个 4KB IOV，不进行额外的
128KB 连续内存申请或数据拼接；将这 32 个 IOV 作为同一个 DMA 软件请求
一次提交给 AXI MCDMA。底层仍使用 32 个 BD 描述 32 个物理页，但这些 BD
共同组成一个 128KB AXI Stream 数据包：第一个 TX BD 带 SOF，最后一个 TX
BD 带 EOF，并且只在最后一个 BD 完成后向上层返回一次软件 completion。

### 优化前的数据路径

原 `compute_handc_op()` 对 128KB SLM WRITE 的处理方式为：

```text
32个4KB源IOV
  -> 1次32-BD RX提交
  -> 循环执行32次单BD TX提交
  -> 产生32个独立TX软件IO和32次TX completion
  -> RX累计到128KB后立即推进HANDC状态机
```

该实现虽然没有改变总搬运字节数，但把一个 128KB Host 到 SLM 请求拆成了
32 个独立 TX 软件事务。它增加了软件 IO 分配、函数调用、tail descriptor
更新、completion 轮询和日志开销；更重要的是，请求完成只由 RX 字节数驱动，
没有明确等待所有 TX completion，存在通道状态尚未完全回收就推进下一阶段的
可能性。

### 优化后的数据路径

`device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c` 中的
`compute_handc_op()` 已调整为：

```text
32个4KB源IOV + 32个4KB目标IOV
  -> 预先分配1个RX软件IO和1个TX软件IO
  -> 1次RX multi-BD提交（32个BD）
  -> 1次TX multi-BD提交（32个BD）
  -> 1次RX completion + 1次TX completion
  -> TX和RX都完成且字节数都等于128KB
  -> 只向原请求线程发送一次完成消息
```

具体变化如下：

1. TX 调用由循环 32 次
   `spdk_env_axi_dma_tx_channel_send(..., &from_iovecs[i], 1, ...)`
   改为一次
   `spdk_env_axi_dma_tx_channel_send(..., from_iovecs, from_size, ...)`。
2. 32 个 IOV 仍分别对应 32 个 4KB 物理页和 32 个 DMA BD，没有要求这些
   物理页连续，也没有发生 Host CPU memcpy 合并。
3. RX 先于 TX 挂接，避免 AXI Stream 开始发送时没有目标 BD。
4. RX/TX 两侧的软件 IO 在提交前同时分配，降低 RX 已提交后才发现 TX 软件
   IO 不足的风险。
5. 增加 `tx_done`、`rx_done`、`completion_posted` 和 `dma_error` 状态。只有
   TX/RX 双方都完成后才推进 HANDC 状态机，并防止重复发送完成消息。
6. TX 和 RX 都校验 DMA 状态及实际完成字节数；任一侧发生错误或短传输时，
   NVMe 请求以内部错误结束，不再把不完整搬运当成成功。
7. 保持单请求最大 64 个 IOV，即当前 HANDC 最大搬运长度仍为 256KB；本轮
   重点验证值为 32个4KB IOV，即128KB。

底层 `rte_axi_dma_add_buffer()` 原本已经支持单次调用展开多个 IOV：第一个 TX
BD 设置 SOF，最后一个 TX BD 设置 EOF，所有 BD 共用同一个软件 IO，最后一个
BD 由 `request_end` 标记并触发唯一一次软件 completion。因此本轮功能修改
集中在 ARM runtime 的 HANDC 提交和完成逻辑，底层只修正了描述该行为的旧
注释。

### 新增中文诊断日志

128KB 请求上板后应看到以下关键日志，每个请求各出现一次：

```text
HANDC_IOV_SUMMARY 保留4KB IOV并准备multi-BD事务 ... tx_iovcnt=32 rx_iovcnt=32 bytes=131072
HANDC_OP RX_MULTI_BD_SUBMIT ... iovcnt=32 total_rx=131072
HANDC_OP TX_MULTI_BD_SUBMIT ... iovcnt=32 total_tx=131072 packet_count=1
HANDC_RX_CMPL multi-BD接收完成 ... bytes=131072 iovcnt=32 error=0
HANDC_TX_CMPL multi-BD发送完成 ... bytes=131072 iovcnt=32 error=0
HANDC_DMA_DONE TX/RX均已完成 ... tx=131072/131072 rx=131072/131072 error=0
```

验收时不应再看到同一 128KB HANDC 请求对应 32 条 4096B
`HANDC_TX_CMPL`。`packet_count=1` 表示一次软件提交及一个 AXI Stream 包，
不是只有一个硬件 BD。

### 编译产物

ARM AArch64 交叉编译已通过：

```text
device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt.handc_multi_bd_20260825
SHA-256: 413c5f2a28ea7f2b26dffc5f93e02bb67400c86c165978ffa19445c0417766e6
文件大小: 10385608B
```

构建仍会报告仓库中原有的类型、格式和缺少函数声明等 warning，本轮新增代码
没有产生编译错误。`git diff --check` 通过。

### 尚待上板验证与剩余风险

当前状态为“代码完成并通过交叉编译”，尚不能替代真实 Fidus 板卡验证。上板
后应先进行一次带写后回读校验的 128KB 完整通路，再关闭校验连续运行 20 次，
比较 `host_to_slm` 中位数、P95 和最大值。

底层 DMA API 没有提供已提交 RX 请求的取消接口。虽然 runtime 现在会在提交
前同时分配 RX/TX 软件 IO，但如果 RX 已成功挂接后 TX 因描述符环已满而提交
失败，仍无法在当前请求内安全撤销 RX；日志会明确提示需要重启 runtime 恢复。
该异常不是正常空闲通道下的预期路径，后续若要彻底消除，需要给底层 AXI DMA
增加“预留足够数量BD”或“取消已提交请求”的接口。

### 真实板卡首次验证结果

将 SHA-256 为
`413c5f2a28ea7f2b26dffc5f93e02bb67400c86c165978ffa19445c0417766e6`
的 `nvmf_tgt.handc_multi_bd_20260825` 部署到 ARM 后，执行一次 128B 完整
通路测试。该测试需要把 12582912B 远端 HPU 返回密文写入 decrypt input
SLM，因此产生 96 个 128KB SLM WRITE 请求。

ARM 日志统计结果为：

```text
128KB WRITE TX completion: 96
128KB WRITE RX completion: 96
128KB WRITE 双向完成: 96
```

抽查每个请求均满足：

```text
tx_iovcnt=32 rx_iovcnt=32 bytes=131072
RX_MULTI_BD_SUBMIT ... iovcnt=32 total_rx=131072
TX_MULTI_BD_SUBMIT ... iovcnt=32 total_tx=131072 packet_count=1
HANDC_TX_CMPL ... status=0 bytes=131072 iovcnt=32 error=0
HANDC_RX_CMPL ... status=0 bytes=131072 iovcnt=32 error=0
HANDC_DMA_DONE ... tx=131072/131072 rx=131072/131072 error=0
```

因此 ARM runtime 层面的 multi-BD 验收通过：96 个 128KB WRITE 均由 32 个
4KB IOV 组成一次 TX multi-BD 提交，TX/RX completion 数量一一对应，并且
只有双方完成后才推进请求状态机。日志中还可以看到 128KB SLM READ 使用相同
机制，`opc=0x2` 的 TX/RX 均返回 131072B 和 `error=0`。

本节只确认 DMA/HANDC 搬运层通过。完整功能验收仍以 QEMU 应用同时输出
`lwe full ... pipeline passed`、`remote_hpu_ciphertext_compute=passed` 和
`destination_ssd_readback_checked=yes` 为准。

### 首次同口径性能对比

QEMU 应用随后同时输出上述三个完整功能验收标志，因此本次完整端到端功能也
通过。将本次结果与优化前同样启用 128KB READ、128KB WRITE 和
`--verify-decrypt-slm-write` 的单次诊断结果对比：

| 阶段 | 优化前(ms) | multi-BD后(ms) | 变化 |
|---|---:|---:|---:|
| SLM 到 Host | 4254.868 | 1202.314 | 3.54倍，降低71.74% |
| Host 到 decrypt SLM | 2019.966 | 2526.653 | 变慢25.08% |
| decrypt SLM写后回读 | 2169.999 | 1048.899 | 2.07倍，降低51.66% |
| RPC往返 | 1228.134 | 1226.181 | 基本不变 |
| 在线端到端 | 9911.980 | 6240.210 | 1.59倍，降低37.04% |

该结果说明 multi-BD 对 SLM READ 路径产生了明显收益：加密输出回读和诊断性
写后回读都显著缩短。Host 到 SLM WRITE 的单次结果没有改善，说明该阶段仍受
NVMQ命令往返、PRP获取、队列调度和长尾波动影响，不能仅凭减少 TX 软件提交
次数保证每次都更快。

与优化前关闭写后回读的20次基线中位数相比，本次单次 SLM 到 Host 从
3081.984ms降至1202.314ms，Host到SLM从2893.485ms降至2526.653ms。由于
“20次中位数”和“单次结果”统计口径不同，这只能作为趋势证据，不能作为最终
论文数据。下一步应在关闭写后回读的相同条件下重新运行20次，重新计算平均值、
P50、P95和最大值后再给出正式加速比。

### READ收益明显而WRITE单次未改善的原因

首先不能用 `2019.966ms -> 2526.653ms` 两个单次样本判定WRITE发生性能
回归。旧版20次测试中，Host到SLM延迟范围为1918.285ms到7959.807ms，
中位数为2893.485ms；multi-BD后的2526.653ms仍位于旧分布的正常范围内，
并比旧中位数低12.68%。按96个128KB请求折算，旧中位数约为30.14ms/请求，
本次约为26.32ms/请求。

READ与WRITE虽然共用 `compute_handc_op()`，但实际DMA地址方向不同：

| NVMe操作 | MCDMA TX源 | MCDMA RX目标 |
|---|---|---|
| SLM READ，`opc=0x2` | ARM本地SLM物理页 | Host PRP/QDMA地址 |
| SLM WRITE，`opc=0x5` | Host PRP/QDMA地址 | ARM本地SLM物理页 |

multi-BD直接减少的是ARM HANDC内部的软件IO分配、TX提交和completion处理。
READ从本地连续SLM取数，旧实现的32次单BD TX软件开销占比较高，合并后收益
能够直接体现。WRITE则需要从Host PRP/QDMA地址取数，96个128KB命令仍各自
包含一次4KB PRP列表获取、一次NVMe/NVMQ命令往返以及Host到Card方向的数据
传输和调度；这些开销没有因multi-BD消失，因此整体延迟对内部TX提交次数不
那么敏感。

此外，新runtime只有在TX和RX都完成后才返回请求；旧runtime主要由RX累计
字节数驱动完成。新计时口径更严格，包含了TX描述符真正回收完成的等待时间，
旧WRITE时间可能存在少量偏乐观。ARM端大量NOTICE/printf日志也处于应用计时
区间，会给每个同步请求增加波动。

本次READ改善在同一运行中的两条独立READ路径上都出现，可信度更高：主
SLM到Host从4254.868ms降至1202.314ms，写后回读从2169.999ms降至
1048.899ms。WRITE是否获得稳定收益仍需20次同口径测试和逐请求延迟跟踪后
判断，不能依据当前一个样本下结论。

### 关闭写后回读的20次正式验证结果

在关闭 `--verify-decrypt-slm-write` 后，使用与优化前基线完全相同的128B明文、
128KB SLM READ、128KB SLM WRITE和远端HPU ADDS配置连续运行20次。结果为
`passed=20`、`failed=0`，说明multi-BD修改在当前样本中没有引入功能、时序或
通道状态的偶发错误。

这里的“优化前基线”特指2026-08-25 13:52至13:55运行的：

```text
host/applications/vscode-lwe-full-pipeline/
  write128k_stability_20260825_055131/run_01.log ... run_20.log
```

该基线已经在Host应用层使用128KB READ/WRITE请求，但ARM runtime仍把每个
128KB HANDC请求中的32个4KB IOV拆成32次独立TX提交。它不是最早使用4KB
Host请求粒度的原始版本，也不是启用写后回读校验的单次诊断结果。基线20次
原始数据汇总为同目录顶层的`full_pipeline_stages_20runs.csv`和
`full_pipeline_stages_20runs_summary.md`。

逐次数据和自动汇总位于：

```text
host/applications/vscode-lwe-full-pipeline/
  handc_multibd_noverify_20runs_20260825_081058/
    full_pipeline_stages_20runs.csv
    full_pipeline_stages_20runs_summary.md
```

关键阶段与优化前20次基线的正式对比如下：

| 阶段 | 统计量 | 优化前(ms) | multi-BD后(ms) | 加速比 | 延迟降低 |
|---|---|---:|---:|---:|---:|
| SLM到Host | 平均 | 4357.431 | 804.109 | 5.42倍 | 81.55% |
| SLM到Host | 中位 | 3081.984 | 740.968 | 4.16倍 | 75.96% |
| SLM到Host | P95 | 9397.790 | 1095.159 | 8.58倍 | 88.35% |
| Host到SLM | 平均 | 3358.554 | 790.970 | 4.25倍 | 76.45% |
| Host到SLM | 中位 | 2893.485 | 666.818 | 4.34倍 | 76.95% |
| Host到SLM | P95 | 5516.717 | 1284.860 | 4.29倍 | 76.71% |
| 在线端到端 | 平均 | 9167.038 | 3028.049 | 3.03倍 | 66.97% |
| 在线端到端 | 中位 | 8581.990 | 2870.842 | 2.99倍 | 66.55% |
| 在线端到端 | P95 | 13702.313 | 3801.745 | 3.60倍 | 72.25% |

两条本地大密文搬运路径的平均总延迟由
`4357.431 + 3358.554 = 7715.985ms` 降至
`804.109 + 790.970 = 1595.079ms`，合计加速4.84倍，降低79.33%。因此正式
20次结果推翻了前述单次样本中“WRITE没有改善”的临时观感：READ和WRITE都
从multi-BD中获得了稳定收益，单次WRITE变慢属于原路径长尾波动。

FPGA加密平均延迟从29.107ms变为29.127ms，远端RPC平均延迟从1208.061ms
变为1204.879ms，均基本不变。这两个对照项说明端到端改善来自本地
SLM与Host之间的HANDC multi-BD优化，而不是FPGA算法或远端HPU负载发生变化。

优化前，SLM到Host与Host到SLM合计占在线端到端时间84.17%；优化后合计降为
52.68%。在线端到端平均延迟由9.167s降到3.028s，P95由13.702s降到3.802s，
最大值由15.892s降到4.683s，长尾也明显收敛。

优化后的主要瓶颈按平均值为：

1. 远端RPC：1204.879ms，占39.79%。其中远端响应发送564.002ms，HPU准备、
   入队、等待和输出转换合计514.968ms。
2. 加密output SLM到Host：804.109ms，占26.56%。
3. Host到decrypt input SLM：790.970ms，占26.12%。

三项合计2799.958ms，占在线端到端平均延迟92.47%。FPGA加密与解密平均合计
53.658ms，仅占约1.77%，因此下一轮优化不应优先调整算子计算核心，而应继续
降低SLM/Host每个128KB NVMe请求的软件往返和长尾，并优化12MB密文响应的
网络发送路径。RPC子阶段属于RPC整体内部拆分，不能与RPC总时间重复相加。
