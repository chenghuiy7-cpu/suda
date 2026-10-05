# vscode-lwe-decrypt-offload

该程序用于独立验证 SUDA 算子池中的 `lwe_decrypt`（operator type ID 3）。它将 psi64/V80 HPU-native Big-LWE 密文写入 input SLM，执行 FPGA 解密，并从 output SLM 读取连续的 `u8` 明文。

Host 到 input SLM 的写入粒度默认为 128 KiB（最后一笔按剩余的 4 KiB
对齐长度），替代原来的逐 4 KiB 同步请求。可用
`--slm-write-chunk-bytes 67108864` 可让当前约 8.4 MiB 的 128B 密文批次
使用一条 NVMe 请求；Host 使用多页 PRP 链，ARM 按 1 MiB MCDMA 窗口滚动传输。

输入支持三种形式：

- `LWEHLS01`：现有加密/远端 HPU 程序保存的逻辑 Big-LWE dump。默认在 Host 内存中将其重排为 HPU-native；增加 `--fpga-input-layout cpu` 后，只去除文件头和明文参考区，将自然顺序系数直接送入 FPGA。dump 中的明文参考值用于逐字节校验。
- `hpu-native`：不带文件头的原始 HPU-native payload。每个 `u8` 固定占 `98304B`；可配合 `--expect-file` 校验。
- `logical`：不带文件头的紧密拼接 Big-LWE payload。每个 LWE 为自然顺序 `mask[2048] + body`，共 `16392B`；4个 LWE 表示一个 u8，共 `65568B`。使用 `--input-format logical`，自动选择 FPGA layout 0，不进行 HPU mask 重排。原始文件没有明文参考区，可配合 `--expect-file` 校验。

逻辑输入模式需要重新生成含新版 `lwe_decrypt` RTL 的 BOOT.bin。原有 native
layout 1 和应用默认行为保持兼容。65568B 是有效 payload 大小；SLM 写入和输入
range 仍按4KB对齐，1B明文对应 `69632B` 的实际传输范围，尾部补零由算子根据
`input_count` 忽略。不能把 encrypt 的 `65792B` 逐 LWE 64B填充流当作紧密逻辑流。

## 直接解密逻辑布局

下面的命令使用现有远端 HPU 的1B结果。它从 LWEHLS01 中提取65568B逻辑
payload，直接交给 FPGA 解密，不再转换成98304B的 HPU-native 输入。

```bash
cd /mnt/suda/host/applications/vscode-lwe-decrypt-offload
make
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_1b.bin \
  --fpga-input-layout cpu \
  --expect 60 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output lwe_decrypt_logical_result_1b.bin \
  --benchmark
```

先增加 `--inspect-only` 可仅检查输入和 Host 参考解密。原始逻辑 payload 文件
则使用 `--input-format logical`；若文件含 LBA 尾部填充，必须再指定
`--plaintext-bytes N`，程序仅接受零填充。算子输出仍为连续 u8，不改变输出协议。

## 编译

```bash
cd /mnt/suda/host/applications/vscode-lwe-decrypt-offload
make
```

ARM 端的 `/root/software_stack/nf_spdk/config.json` 还必须包含
`operator_type_id=3`、`operator_type_name=lwe_decrypt`、`slot_id=3`，并使用不带
`-b` 参数的 `mcdma/run_nvmq.sh` 启动。修改配置后需要重启 ARM `nvmf_tgt` 和
QEMU/NVMQ 连接，配置不会通过更换 `BOOT.bin` 自动更新。

## 先检查输入文件

该步骤不访问 FPGA，但会用同一 secret key 执行一次 Host 参考解密，检查 dump、重排结果和 key 是否匹配：

```bash
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_1b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --inspect-only
```

## 直接验证上一阶段 FPGA 加密结果

如果刚运行过 `vscode-lwe-encrypt-offload` 的 128B 示例，可直接解密其
`LWEHLS01` dump，不依赖远端 HPU：

```bash
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_fpga_ciphertexts_128b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output lwe_decrypt_fpga_result_128b.bin \
  --benchmark 2>&1 | tee lwe_decrypt_fpga_result_128b.log
```

输入 dump 保存了加密阶段恢复出的明文参考值，因此程序会自动逐字节校验，
不需要再传 `--expect-file`。

## 上板验证远端 HPU 的 1B 结果

已知原始明文为 `59`，远端执行 `+1` 后应解密为 `60`：

```bash
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_1b.bin \
  --expect 60 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output lwe_decrypt_remote_hpu_result_1b.bin \
  --benchmark 2>&1 | tee lwe_decrypt_remote_hpu_result_1b.log
```

## 验证 128B 结果

`LWEHLS01` 自带 128 个预期明文字节，因此会自动进行完整逐字节比较：

```bash
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_128b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output lwe_decrypt_remote_hpu_result_128b.bin \
  --benchmark 2>&1 | tee lwe_decrypt_remote_hpu_result_128b.log
```

成功标志包括：

```text
lwe_decrypt FPGA execution passed
correctness_checked=yes
```

若最终网络服务直接返回原始 HPU-native payload，可使用：

```bash
./vscode-lwe-decrypt-offload \
  --input remote_hpu_native.bin \
  --input-format hpu-native \
  --plaintext-bytes 128 \
  --expect-file expected_plaintext_128b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin
```
# Padding 输入补充

输入布局需要显式指定，不能通过系数中的零值自动判断：

| FPGA 输入布局 | 每个 u8 的输入字节数 | padding 处理 |
|---|---:|---|
| `cpu` | 65568 | 紧凑自然序；只允许在已知数量之后补零到 LBA |
| `cpu-padded` | 65792 | 每个 LWE 的 16392B 后有 56B 零 padding；另允许末尾 LBA 补零 |
| `hpu-native`（默认） | 98304 | 保留原有 PC0/PC1 固定槽位与内部 padding |

裸的逐 LWE 补齐数据使用 `--input-format logical --fpga-input-layout cpu-padded`。
如果文件还含有末尾 LBA padding，必须提供 `--plaintext-bytes N`。
`LWEHLS01` 文件也可以选择 `--fpga-input-layout cpu-padded`，由 Host 按指定布局准备输入。
这不是把 65792B 当作紧凑 65568B 读取；算子会逐块消费 7 个零 u64 padding。
非零 padding、截断数据均报错。协议的 TUSER=0xff 结束包不能当作文件 padding。

## 1B–4096B CPU / FPGA 解密批量对比

`benchmark_decrypt.py` 用现有的 128B `LWEHLS01` 真密文构造各批输入；
超过 128B 时循环使用这些密文。两侧读取**完全相同**的紧凑自然序
Big-LWE payload，使用相同的 2048 位二值密钥，并逐字节核对解密结果。
这是批量吞吐实验，重复密文不代表 4096 个独立生成的样本。

在装有当前支持 `--fpga-input-layout cpu` 的 BOOT.bin、NVMQ 设备可用的
Host/QEMU 环境中运行：

```bash
cd /mnt/suda/host/applications/vscode-lwe-decrypt-offload
make -j2
set -o pipefail
taskset -c 0 ./benchmark_decrypt.py \
  --trials 5 --cpu-runs 5 \
  --slm-write-chunk-bytes 16777216 \
  2>&1 | tee benchmark_decrypt_sweep.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

默认批量为 1、16、32、64、128、256、512、1024、2048、4096B，
每个规模重复 5 次。结果写入带时间戳的
`benchmark_decrypt_*/results.csv`、`summary.csv` 和逐次日志。
FPGA 报错或单次超过 240 秒时立即停止，已完成的逐次结果仍保留。
临时密文在每个批次完成后删除，最大批次约占 256 MiB 临时磁盘空间。
若设备中途失联，修复 NVMQ 后可添加
`--resume --output-dir benchmark_decrypt_YYYYMMDD_HHMMSS`；
脚本读取该目录的 `results.csv`，跳过已成功的 trial，
并继续生成完整的汇总表。

`summary.csv` 的 `operator_speedup` 是同次测试的
`CPU 解密中位数 / FPGA execute 中位数`；
`pipeline_speedup` 是 `CPU 解密中位数 / FPGA pipeline 中位数`。
前者的 FPGA 时间包含计算命令往返，后者还包括 SLM 建立、输入上传、
程序设置和输出读取，因此两列解释不同。CPU 计时只包含参考解密，
不包含密文文件解析和重排。CPU 基线是此程序的优化单线程标量
Big-LWE 解密实现，不是 tfhe-rs `ClientKey.decrypt_radix`。

若当前环境没有 `/dev/nvmq*`，可只验证 CPU 侧：
`./benchmark_decrypt.py --cpu-only --trials 5 --cpu-runs 5`。
这种运行不会生成 FPGA 时间或加速比，且不能与另一台机器的 FPGA 时间混算。

## Device timing and benchmark boundaries (2026-10-04)

New BOOT.bin adds RTL cycle counters to both LWE slots. The new ARM runtime
preserves their diagnostic footer after the physical plaintext/ciphertext
payload, while excluding it from `exec_result`. Successful standalone runs print:

```
hw_profile available=yes version=1 clock_hz=250000000 boundary=ap_start_to_finish_valid cycles=... stream_ms=... input_wait_ms=... output_wait_ms=... input_beats=... output_beats=...
```

The timer includes key setup and AXIS stalls. It excludes the finish-marker
transfer wait. Input/output waits can overlap; summing/subtracting them does not
produce a reliable compute-only duration. Old firmware/runtime remains usable
and prints `hw_profile available=no`.

The sweep reports three distinct metrics:

- `hardware_stream_speedup`: warm CPU reference kernel / measured device stream
  interval, including device-side waiting. This is not stall-free kernel time.
- `execute_request_speedup`: CPU kernel / synchronous NVMe execute call. This
  includes ARM scheduling, context transport, SLM DMA and completion; the old
  misleading `operator_speedup` column is now named for its actual boundary.
- `pipeline_speedup`: CPU kernel / SLM create, input transfer, program setup,
  execute and output read. It excludes parsing and cleanup, and is a ratio of
  different scopes, not equal-scope end-to-end CPU/FPGA speedup.

The CPU baseline remains the same custom C++ reference decrypt, with active key
indices prepared outside timing and an initial correctness call before repeated
timed calls. It is not automatically a production tfhe-rs benchmark.

After rebuilding/replacing BOOT.bin and ARM runtime, run inside QEMU:

```bash
cd /mnt/suda/host/applications/vscode-lwe-decrypt-offload
make -j2
set -o pipefail
out_dir="benchmark_decrypt_profile_$(date +%Y%m%d_%H%M%S)"
taskset -c 0 ./benchmark_decrypt.py \
  --sizes 1 16 32 64 128 256 512 1024 2048 4096 \
  --trials 5 --cpu-runs 5 \
  --fpga-input-layout cpu \
  --slm-write-chunk-bytes 134217728 \
  --require-hw-profile --output-dir "$out_dir" \
  2>&1 | tee "$out_dir.log"
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\nresults_dir=%s\n' "$test_rc" "$out_dir"
```

`--require-hw-profile` stops if instrumentation is unavailable. Use a fresh
output directory rather than mixing earlier board measurements with new RTL.
`measurement.json` records timing boundaries and benchmark options. The existing
`--source` fixture is repeated for larger batches, so this sweep evaluates batch
latency, not diversity of independently generated ciphertexts. `--fpga-input-layout`
also accepts `cpu-padded` and `hpu-native`.

The new RTL, protocol and validation are documented in
`device/operators/hls/lwe_decrypt/README.md` and
`logs/lwe_decrypt_parallel_20261004.md`.
