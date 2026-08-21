# LWE 加密算子性能基准支持记录

日期：2026-08-10

## 本次修改

1. 为 `vscode-lwe-encrypt-offload` 增加 FPGA 分阶段计时。
2. 增加 `--benchmark`，输出机器可读的 `BENCH_FPGA_CSV` 记录，并减少逐个 4KB SLM read 日志。
3. 增加 `--skip-dump`，允许完成 SLM 回读和 Host 正确性检查后不写密文文件，从而将落盘时间排除在加密基准之外。
4. 增加 `run_fpga_bench.sh`，按多个批量大小执行 warm-up 和重复采样。
5. 在 tfhe-rs 增加 `hpu_lwe_encrypt_cpu_bench`，使用已保存的 psi64 KS32 ClientKey 测量同构的 `u8 -> 4 radix Big-LWE` 软件加密。
6. 增加 `summarize_benchmark.py`，按中位数计算核心、数据通路和单次完整流程的性能比值，并输出 P95。
7. FPGA CSV 记录 `input_lbas`，用于区分已验证的 256-LBA 数据搬运配置和理论最小的 1-LBA 配置。

## 计时边界

- CPU：只统计 `ClientKey::encrypt_radix::<u8>(value, 4)` 批量循环，排除密钥加载、warm-up 和解密验证。
- FPGA 核心：`nvme_execute_hlsacc_program()`。
- FPGA 数据通路：SSD 到 SLM、FPGA execute、output SLM 到 Host、Host layout/正确性检查。
- FPGA 单次流程：增加 SLM 创建、memory range、program load/activate 等开销。

## 已完成的本地验证

- C++ Host 程序使用 `-O2 -std=c++14` 编译通过。
- tfhe-rs CPU 基准使用 release + integer feature 构建，并用已保存 ClientKey 完成加密/解密闭环。
- 132 宿主机（Xeon Gold 6248R，单核 `taskset -c 2`）30 次串行样本的中位数：1B 为 0.459808ms，128B 为 61.481750ms。
- 完整 CPU 原始样本保存于 `logs/lwe_cpu_serial_20260810.csv`，人类可读输出保存于 `logs/lwe_cpu_serial_20260810.log`。
- FPGA 实机采样需要在 QEMU/NVMQ/ARM runtime 正常运行时执行。

## 历史数据的临时对比

此前成功日志中的单次 `nvme_execute_hlsacc_program()` 时间为 1B 约 5.781ms、128B 约 1636.552ms。与本次 CPU 中位数相比，核心比值分别为 0.080x 和 0.038x，即当前样本中 FPGA 约慢 12.6 倍和 26.6 倍。由于 FPGA 每个规模只有一个历史样本，该结果只用于判断优化方向，不作为正式性能结论。

HLS csynth 报告显示 `encrypt_mask_loop` achieved II=3，按 2048 维、4 个 radix block 和 250MHz 粗略估算约 0.098ms/u8（不考虑流背压）。它和板上 execute 时间的数量级差异提示当前瓶颈可能主要位于大密文输出与 SUDA runtime 数据通路，后续应通过分阶段实测以及 OperatorController cycle counter/ILA 继续拆分。

## 结论边界

当前 HLS 随机源和噪声采样仍为原型，本测试暂时用于量化同一密文结构与主要 LWE 算术工作量的加速效果，不应写成与生产级 tfhe-rs 加密完全安全等价。

## 2026-08-10 FPGA 连续采样异常

首次完整批量扫描在 `batch_size=16` 的第一条正式样本处失败：`nvme_execute_hlsacc_program` 返回 `-1`，随后 deactivate/unload 均返回 NVMe 状态 `880`（十六进制 `0x370`）。已有 CSV 未损坏，1、2、4、8B 各保留 10 条有效样本。

该现象不符合 16B 算法或输出容量错误，因为 16B 的 3 次 warm-up 已完成，且 128B 单次功能测试此前通过。失败前共进行了约 55 次完整资源生命周期。按每个输出 SLM 的 4KB 接收页计算，已经提交约：

```text
13 * (17 + 33 + 65 + 129) + 3 * 257 = 3943
```

个 RX 接收对象，接近 `create_axi_dma_channel()` 为 RX 通道配置的 4096 项 `spdk_axi_dma_io` 池。结合此前 ARM 日志中的 `Failed to allocate spdk_axi_dma_io`，当前首要怀疑是 AXI DMA completion/通道释放路径存在资源未稳定归还或状态未完全复位。`880` 出现在 execute 失败之后，属于控制器路径异常后的连带清理失败，不作为根因。

`run_fpga_bench.sh` 已增加 `APPEND=1` 断点续跑和 iteration 自动续号，避免恢复 runtime 后覆盖已有样本。下一步先在干净 runtime 中只执行 16B 一档；若成功，即可确认问题依赖累计执行次数而非 16B 数据规模，再针对 RX IO 池回收做计数验证和修复。

进一步检查发现 `include/spdk/env.h` 的 `spdk_simple_pool_reset()` 只重置 `io_head/io_tail`，没有恢复被 get/put 循环旋转过的 `io_ring`。例如取出并归还若干元素后直接将 head/tail 归零，会让有效区间出现重复指针，同时丢失其他池元素；每个 HLS 请求结束时 `tx_rx_channel_release()` 都会调用该函数，因此连续基准会逐轮破坏 AXI DMA IO 池。

修复方式是在 reset 时根据 `io_arr`、`elem_size` 和 `io_ring_size` 重建全部 ring 指针，再重置 head/tail。ARM 版 `nvmf_tgt` 已交叉编译成功：

```text
SHA-256 ab76cf7e2900108d4e5d42f03d4cd8447603d16d2338a2c34fe29d15f825271e
build/bin/nvmf_tgt: ELF 64-bit LSB executable, ARM aarch64
```
