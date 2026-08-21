# LWE 加密算子性能测试方法

## 1. 测试目标

当前性能测试比较以下两条使用同一套 psi64/KS32 参数的路径：

- CPU：tfhe-rs `ClientKey::encrypt_radix::<u8>(value, 4)`，随后转换为 psi64/V80 HPU-native 物理布局。
- FPGA：SUDA `lwe_encrypt` 直接从连续 u8 生成相同的 HPU-native 物理布局。

每个 u8 拆成 4 个 2-bit radix block。每个 block 是一个 2048 维 Big-LWE：

```text
逻辑 CPU-LWE：4 * (2048 + 1) * 8 = 65,568 B/u8

HPU-native：
4 个 Big-LWE/u8 * 2 个 PC/Big-LWE * 12,288 B/PC
= 98,304 B/u8
```

HPU-native 转换包含 11-bit bit-reversal、每 16 个系数在 PC0/PC1 间交织、body 放到 PC0 word 1024，以及每个 PC 补零到 12KB。

旧版 `65,792B/u8` FPGA 流是 CPU-LWE 的 64B packet 补齐格式，不得与当前 HPU-native 样本混合计算加速比。

## 2. 公平的计时边界

### 2.1 等价算子性能

这是主加速比，输入和输出语义一致：

- CPU 起点：N 个 u8 已在 Host 内存；终点：连续 HPU-native 密文已在 Host 内存。
- FPGA 起点：N 个 u8 已在 input SLM；终点：连续 HPU-native 密文已在 output SLM。

CPU benchmark 分别记录：

- `encrypt_ms`：只执行 tfhe-rs radix 加密。
- `native_pack_ms`：把 CPU-LWE 转换并补齐为 HPU-native 物理布局。
- `encrypt_and_pack_ms`：从开始加密到 native 输出完成，包含两阶段之间的真实连续开销。

FPGA 使用：

- `fpga_execute_ms`：`nvme_execute_hlsacc_program()` 调用前后，包含 SUDA execute 控制和算子边界流传输，但不包含 SLM 创建、SSD 搬入和 output SLM 回读。

正式加速比为：

```text
等价输出加速比 = CPU encrypt_and_pack_ms 中位数 / FPGA fpga_execute_ms 中位数
```

比值大于 1 表示 FPGA 更快。CPU 的密钥加载、warm-up、native 输出缓冲区分配、布局校验和 ClientKey 解密检查均不计时。native 重排、body 放置和全部 padding 写入仍计入 `native_pack_ms`。这与 FPGA 在 `fpga_execute_ms` 之前已经创建 output SLM 的边界一致。

### 2.2 系统数据通路

FPGA Host 程序还记录：

- `transport_ready_ms`：`SSD->SLM + FPGA execute + output SLM->Host`，不包含 Host 正确性验证。
- `data_path_ms`：`transport_ready_ms + host_verify_ms`，保留用于调试和历史分析。
- `one_shot_transport_ready_ms`：从开始创建 SLM 到 HPU-native 数据全部回到 Host，包含 SLM 创建、memory range 和 program setup。
- `one_shot_pipeline_ms`：再包含 Host 布局与解密正确性验证。
- `process_ms`：再包含密钥读取、Host buffer 准备和资源清理；使用 `--skip-dump` 时不包含密文落盘。

`transport_ready_ms` 不能直接与只从 Host 内存开始的 CPU 基线计算公平加速比。严格的系统对比还需增加一条 `SSD->Host->CPU加密->native打包` 软件路径。

## 3. CPU 软件基线（132 宿主机）

记录环境：

```bash
lscpu | tee /home/yangchenghui/suda/logs/lwe_hpu_native_cpu_info.txt
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
cd /home/yangchenghui/hpu/tfhe-rs
git rev-parse HEAD
```

编译 release 软件基线：

```bash
cd /home/yangchenghui/hpu/tfhe-rs
cargo build --release --features integer \
  --example hpu_lwe_encrypt_cpu_bench
```

单核串行是主算法基线：

```bash
taskset -c 2 ./target/release/examples/hpu_lwe_encrypt_cpu_bench \
  --input-file /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-data-gen/testdata/plaintext_u8_4k_128b.bin \
  --batch-sizes 1,16,32,128 \
  --warmup 5 \
  --iterations 30 \
  --mode serial \
  --csv /home/yangchenghui/suda/logs/lwe_cpu_hpu_native_serial_20260817.csv \
  2>&1 | tee /home/yangchenghui/suda/logs/lwe_cpu_hpu_native_serial_20260817.log
```

可额外测试多核吞吐，但必须单独标注为 parallel：

```bash
RAYON_NUM_THREADS=24 \
./target/release/examples/hpu_lwe_encrypt_cpu_bench \
  --input-file /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-data-gen/testdata/plaintext_u8_4k_128b.bin \
  --batch-sizes 1,16,32,128 \
  --warmup 5 \
  --iterations 30 \
  --mode parallel \
  --csv /home/yangchenghui/suda/logs/lwe_cpu_hpu_native_parallel_20260817.csv
```

## 4. FPGA 基线（QEMU 虚拟机）

确认 ARM `nvmf_tgt`、QEMU NVMQ 和当前 HPU-native BOOT.bin 均已启动。然后在 QEMU 中执行：

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-offload
make

SSD_NSID=1 \
SSD_LBA=65536 \
INPUT_LBAS=1 \
OUTPUT_LAYOUT=hpu-native \
SLM_READ_CHUNK_BYTES=131072 \
SLM_READ_QUEUE_DEPTH=1 \
SETTLE_SECONDS=1 \
WARMUP=5 \
ITERATIONS=30 \
BATCH_SIZES="1 16 32 128" \
OUTPUT_CSV=/mnt/suda/logs/lwe_fpga_hpu_native_20260817.csv \
./run_fpga_bench.sh \
  2>&1 | tee /mnt/suda/logs/lwe_fpga_hpu_native_20260817.log
```

128B 以内明文只需搬运一个 4KB LBA。`SETTLE_SECONDS=1` 用于降低连续创建/销毁 SUDA 资源造成的 runtime 抖动。若中途失败，脚本会保留已完成样本并打印带 `APPEND=1` 的恢复命令。

新 FPGA CSV 显式记录：

- `output_layout=hpu-native-psi64-v80`
- `physical_output_bytes_per_u8=98304`
- SLM 回读块大小与 queue depth
- 各阶段时间和不含验证的 `transport_ready_ms`

汇总脚本会拒绝缺少这些字段的旧 CSV。

## 5. 汇总等价输出加速比

回到 132 宿主机：

```bash
python3 \
  /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/summarize_benchmark.py \
  --cpu /home/yangchenghui/suda/logs/lwe_cpu_hpu_native_serial_20260817.csv \
  --fpga /home/yangchenghui/suda/logs/lwe_fpga_hpu_native_20260817.csv \
  --cpu-mode serial \
  | tee /home/yangchenghui/suda/logs/lwe_hpu_native_speedup_20260817.md
```

主结果采用中位数，并同时报告 P95。建议绘制：

1. 批量大小与 CPU 加密、CPU native 打包、FPGA execute 延迟。
2. 批量大小与等价输出加速比。
3. FPGA execute、SLM 回读和 `transport_ready_ms` 的阶段分解。

## 6. 复现实验需要记录的信息

- CPU 型号、编译 feature、CPU governor、线程数和绑核方式。
- BOOT.bin SHA-256、FPGA 时钟、implementation timing 和 HLS/RTL 版本。
- Host、ARM runtime 和 tfhe-rs 代码版本。
- 同一密钥、message/carry modulus、4 个 radix block、2048 维 Big-LWE。
- 批量大小、warm-up、正式迭代次数、SLM read chunk 和 queue depth。

远程 TCP 和 HPU 同态计算不计入 LWE 加密算子加速比，它们属于完整业务通路指标。

当前 HLS mask PRNG 和 noise sampler 仍是内部原型，不是 tfhe-rs CSPRNG/噪声采样的安全等价实现。因此论文中应表述为“相同密文结构、LWE 算术和 HPU-native 输出布局下的原型性能对比”，不能直接宣称生产级 TFHE 加密加速。
