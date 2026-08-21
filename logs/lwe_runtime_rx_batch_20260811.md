# LWE 加密 runtime 输出批处理优化记录

日期：2026-08-11

## 问题与证据

当前 LWE 算子的数学核心并不是主要瓶颈。HLS csynth 中 2048 维 mask
循环的 II 为 3，按 4 个 radix block、250MHz 粗略估算，核心计算约为
0.098ms/u8；但板上 `nvme_execute_hlsacc_program()` 的历史中位数为：

- 1B：约 14.199ms；
- 8B：约 72.484ms；
- 128B：历史单次约 1636.552ms。

原 runtime 的 `rx_channel_recv()` 将 `max_cnt` 固定为 1，因此每个 4KB
output SLM page 都要经历一次 AXI DMA 提交、completion、SPDK 回调和下一页
重提交。1 个 u8 的 output SLM 为 17 页，128 个 u8 为 2057 页。

底层 `rte_axi_dma_poll_complete()` 还会为一次多 BD RX 提交中的每个 BD
返回同一个 `spdk_axi_dma_io` 指针。上层若直接批量提交，会重复回收同一
IO 对象，因此此前只能采用逐页 RX。这是本次优化首先修复的生命周期问题。

## 修改内容

1. 在 AXI DMA descriptor 中增加 `request_end`，标记一次 RX 提交的最后一个
   BD。
2. RX poller 仍逐个回收已完成 BD、累计实际传输字节，但只在
   `request_end` 对应的 BD 完成后向 SPDK 返回一次 `spdk_axi_dma_io`。
3. 返回前将整批实际字节数写入 `io->status.transfered_bytes`；最后一批若
   携带 `TUSER=0xff`，现有逻辑继续从整批字节数中扣除 64B 结束 beat，
   保留其前面的密文 payload。
4. `mcdma` 的 compute RX 默认改为每批最多 48 个 4KB page。128B 密文的
   completion/回调次数预计由 2057 次降到 43 次。
5. 高频逐页日志由 NOTICE 降为 DEBUG，避免日志格式化和终端输出进入性能
   热点。
6. RX 的 `vtophys` 或底层提交失败时，立即将已申请的 IO 对象归还 pool，
   避免错误路径泄漏。
7. 增加运行时回退变量 `SUDA_MCDMA_RX_BATCH_PAGES`，有效范围为 1 到 48。
   默认值为 48；设为 1 可恢复旧的逐页提交行为，无需重新编译。

涉及文件：

- `device/platform/software_stack/nf_spdk/dpdk/drivers/bus/axi_dma/rte_axi_dma.h`
- `device/platform/software_stack/nf_spdk/dpdk/drivers/bus/axi_dma/rte_axi_dma.c`
- `device/platform/software_stack/nf_spdk/lib/axi_dma/axi_dma.c`
- `device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c`

## 编译结果

DPDK AArch64 构建和安装通过，SPDK `lib`、`module`、`app/nvmf_tgt` 交叉编译
通过。构建中仍有仓库已有的 warning，本次未出现编译或链接错误。

新 runtime：

```text
device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt.lwe_rx_batch_20260811
SHA-256 7fb019f0540c71f3b953fd3de355afb73ca676ea88321718f9cb51c7ec7d162f
ELF 64-bit LSB executable, ARM aarch64
```

编译前 runtime 备份：

```text
device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt.before_rx_batch_20260811
SHA-256 ab76cf7e2900108d4e5d42f03d4cd8447603d16d2338a2c34fe29d15f825271e
```

`readelf -d` 表明程序未动态依赖 DPDK so，AXI DMA 驱动以静态库链接，
因此 ARM 端只需替换 `nvmf_tgt`。

## 板上验证顺序

1. 备份并替换 ARM 上的 `nvmf_tgt`，重启 ARM runtime 和 QEMU/NVMQ 连接。
2. 默认以 48 页批量启动；启动日志应出现
   `Compute RX batch size: 48 page(s)`。
3. 先跑 1B 正确性，再跑 16B、32B、128B 性能采样。
4. 检查解密结果、`exec_result`、`expected_logical_bytes` 和
   `physical_bytes`，确认批处理没有破坏输出记账。
5. 若异常，以 `SUDA_MCDMA_RX_BATCH_PAGES=1` 启动同一二进制做 A/B 对照；
   必要时恢复旧二进制。

建议先执行：

```bash
BATCH_SIZES="1 16 32 128" WARMUP=3 ITERATIONS=10 INPUT_LBAS=256 \
OUTPUT_CSV=fpga_rx_batch_48.csv ./run_fpga_bench.sh
```

然后在 ARM 端将批量页数设为 1，重启 runtime，在相同环境下执行：

```bash
BATCH_SIZES="1 16 32 128" WARMUP=3 ITERATIONS=10 INPUT_LBAS=256 \
OUTPUT_CSV=fpga_rx_batch_1.csv ./run_fpga_bench.sh
```

## 性能预期与结论边界

CPU 串行基线中位数约为 0.46 到 0.48ms/u8，128B 为 61.482ms。按现有
逐页开销和 HLS 理论核心时间粗略外推，批量 RX 后 128B execute 可能进入
几十毫秒范围，因此“FPGA execute 小于 CPU 加密时间”在中等及较大 batch
上具备可行性。1B 仍可能被固定控制开销主导，不保证快于 CPU。

该估算不能代替板上结果。正式结论应使用同一明文数量、同一密文结构、
相同 warm-up 和至少 10 次样本的中位数/P95，并同时保留正确性验证。当前
HLS PRNG 和噪声采样仍为原型，性能结论只针对现有实现，不代表生产级
tfhe-rs 随机源的最终性能。
