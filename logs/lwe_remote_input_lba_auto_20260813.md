# 远程端到端测试按明文批量自动选择输入 LBA

日期：2026-08-13

## 问题

此前 `run_remote_pipeline_bench.sh` 默认设置 `INPUT_LBAS=256`，导致所有明文批量都从
SSD 向 input SLM 搬运 1MiB。FPGA 实际只消费 `plaintext_bytes` 指定的连续明文字节，
所以固定 1MiB 会让端到端测试中的 SSD 到 SLM 阶段与被加密的数据量不一致。

## 修改

1. benchmark 脚本将 `INPUT_LBAS` 默认值改为 `auto`。
2. 自动模式按 `ceil(batch_size / 4096)` 计算实际输入 LBA 数，并将该值写入 CSV。
3. 仍支持显式设置 `INPUT_LBAS=N`，用于固定搬运量的对照实验。
4. 更新运行文档，常规端到端测试不再传入固定的 `--input-lbas 256`。
5. 默认输出文件改为 `remote_pipeline_benchmark_auto_lba.csv`，避免覆盖固定 1MiB
   搬运的历史基线。
6. benchmark 与汇总工具同时兼容历史 `adds` 和新 HPU 原生布局的
   `adds-hpu-native` 操作名。

## 测量口径

NVMe/SLM 接口以 4KB LBA 为最小搬运单位，不能只搬运任意字节数。因此：

- 1B、16B、32B、128B 明文：均实际搬运 4096B；
- 4096B 明文：实际搬运 4096B；
- 4097B 明文：实际搬运 8192B。

这里“按实际数据量”是指按明文字节数向上对齐到最小 4KB LBA，而不是按字节精确搬运。

## 历史数据

`remote_pipeline_benchmark.csv` 是 2026-08-12 固定搬运 256 LBA（1MiB）的历史基线，
不改写。采用自动 LBA 后应输出到新的 CSV，不能与历史样本直接混合汇总。
