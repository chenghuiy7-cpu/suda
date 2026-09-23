# 近存储过滤与 LWE 应用执行命令

本文覆盖当前 TPC-H `lineitem`、FPGA 过滤/加解密、远端 HPU 和 SSD 回写实验用到的应用。除“129 远端服务”和“132 x86 宿主机”特别标注外，命令均在 **132 的 QEMU Host VM** 中执行；该 VM 中仓库路径为 `/mnt/suda`。先确认板卡加载了包含 `selective_filter`、`lwe_encrypt`、`lwe_decrypt` 的匹配 BOOT.bin，ARM `nvmf_tgt` 使用当前 `config.json`，QEMU 能访问 `/dev/nvmq0n1`。ARM 的 `mcdma/run_nvmq.sh` 不加 `-b` 时默认使用该配置。

以下测试默认源数据位于 SSD namespace 1、LBA 65536；回写测试的目标位于 LBA 131072。**`--write`、`--run`、数据生成器和两种完整回写程序都会覆盖指定 SSD 区域**。示例命令适用于实验预留区域，执行前确认 LBA 没有业务数据。加密、解密和过滤应用本身不写 SSD；其输出文件写在 Host 文件系统。

统一密钥路径：

```bash
export LWE_KEY=/mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin
```

## 0. 启动远端 HPU 服务（需要远端计算的应用）

在 **129 远端服务器** 的 `tmux` 中执行；部署和首次配置见 [LWE 远程 HPU 部署与运行命令](LWE远程HPU部署与运行命令.md)：

```bash
source "$HOME/.config/suda/hpu-server.env"
"$SUDA_HPU_ROOT/scripts/start_remote_server.sh" \
  2>&1 | tee "$SUDA_HPU_ROOT/suda-remote-hpu-server.log"
```

应看到 `hpu_device_ready=yes`、`listen_addr=0.0.0.0:19090`。不涉及远端 HPU 的应用跳过此步骤。

## 1. 准备真实 TPC-H 数据：`tpch-lineitem-fpga-test`

首次在仓库 checkout 中初始化上游数据生成器并生成 128 条 512B 定长 `lineitem` 记录（`--prepare` 只生成文件，不写 SSD）：

```bash
cd /mnt/suda
git submodule update --init third_party/tpch-dbgen
cd host/applications/tpch-lineitem-fpga-test
./run_tpch_filter_test.sh --prepare
sha256sum testdata/tpch_lineitem_512b.bin
```

当前固定样本预期 SHA-256 为 `1d7c173a673471debd3ecc8e20af91de90674e77961441f11e32adb55766405f`，共 65536B。将这 16 个 4KB LBA 写到 SSD 并回读校验，**覆盖源 LBA 65536–65551**：

```bash
./run_tpch_filter_test.sh --write
```

单独验证 `SSD → FPGA SQL 过滤 → FPGA LWE 加密 → Host`。此脚本**先重新写入上述源 LBA**，再执行 FPGA 程序：

```bash
set -o pipefail
./run_tpch_filter_test.sh --run 2>&1 | tee tpch_lineitem_q6_run.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

预期 `selected_count=5`、`decrypted_prefix=15170d130e`、`selective_filter -> lwe_encrypt FPGA pipeline passed` 和退出码 0。Q6 风格条件为 `shipdate` 在 1994 年、`discount_bp` 为 500–700、`quantity < 24`，投影 `quantity:u8@25`；这是 Q6 谓词和投影测试，不执行 SQL 聚合。

## 2. 合成数据与任意支持范围内 SQL：`vscode-selective-lwe-encrypt-offload`

以下生成 16 条合成 512B 记录，**写入时覆盖源 LBA 65536–65537**。运行本节后若要继续真实 TPC-H 测试，先重新执行上一节的 `--write`。

```bash
cd /mnt/suda/host/applications/vscode-selective-lwe-encrypt-offload
make -j2
./generate_tpch_like.py --records 16 --predicate gt --threshold 32
./write_dataset_to_ssd.py \
  --input testdata/tpch_like_512b.bin --device /dev/nvmq0n1 --lba 65536
```

旧版固定 `quantity:u8@4 > 32` 调用：

```bash
set -o pipefail
./vscode-selective-lwe-encrypt-offload \
  --ssd-nsid 1 --ssd-lba 65536 --records 16 \
  --predicate gt --threshold 32 \
  --reference testdata/tpch_like_512b.bin \
  --key "$LWE_KEY" --output selective_lwe_fpga_ciphertexts.bin \
  --benchmark 2>&1 | tee selective_lwe_legacy.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

SQL metadata v2 可先离线查看编译后的 FPGA 过滤计划（不访问设备）：

```bash
./vscode-selective-lwe-encrypt-offload \
  --record-bytes 512 \
  --schema 'id:u32@0,quantity:u8@4,order_key:u64@8,price:u64@16' \
  --query 'SELECT quantity FROM lineitem WHERE quantity > 32 AND id >= 100000' \
  --explain-filter
```

然后按同一 schema 对合成数据实际执行：

```bash
./vscode-selective-lwe-encrypt-offload \
  --ssd-nsid 1 --ssd-lba 65536 --records 16 --record-bytes 512 \
  --schema 'id:u32@0,quantity:u8@4,order_key:u64@8,price:u64@16' \
  --query 'SELECT quantity FROM lineitem WHERE (quantity >= 40 AND id != 100010) OR id = 100014' \
  --reference testdata/tpch_like_512b.bin \
  --key "$LWE_KEY" --output selective_lwe_sql_ciphertexts.bin \
  --benchmark
```

SQL metadata v2 的比较字段可为 `u8/u16/u32/u64/i8/i16/i32/i64`；当前连接到 LWE 的投影字段仍须为 `u8`。`--reference` 要与 SSD 中的记录完全一致，因为 Host 用它计算选中数量和 DMA 接收长度。

## 3. 独立 FPGA 加密：`vscode-lwe-encrypt-offload`

下面从真实 TPC-H 源 LBA 取**前 128 个原始字节**，不做过滤，也不按 `quantity` 字段取值。若需确认它仍是 TPC-H 数据，先运行第 1 节 `--write`。

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-offload
make -j2
set -o pipefail
taskset -c 1 ./vscode-lwe-encrypt-offload \
  --ssd-nsid 1 --ssd-lba 65536 --input-lbas 1 \
  --plaintext-bytes 128 --output-layout hpu-native \
  --key "$LWE_KEY" --benchmark --skip-dump \
  2>&1 | tee lwe_encrypt_no_filter_128b.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

预期 `lwe_encrypt FPGA execution passed`、`decrypted_count=128`、`host_key_decrypt_checked=yes`。`--skip-dump` 只测量并校验，不保存密文。若随后要用独立解密程序，去掉 `--skip-dump`，改加 `--output lwe_encrypt_fpga_ciphertexts_128b.bin`。

重复基准测试：

```bash
OUTPUT_LAYOUT=hpu-native WARMUP=3 ITERATIONS=10 INPUT_LBAS=1 \
  ./run_fpga_bench.sh
```

## 4. 独立 FPGA 解密：`vscode-lwe-decrypt-offload`

上一节须先生成 `LWEHLS01` 密文 dump（不能使用 `--skip-dump`）。本程序只读文件、写 SLM 和 Host 输出，不写 SSD：

```bash
cd /mnt/suda/host/applications/vscode-lwe-decrypt-offload
make -j2
set -o pipefail
taskset -c 1 ./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_fpga_ciphertexts_128b.bin \
  --key "$LWE_KEY" --output lwe_decrypt_fpga_result_128b.bin \
  --benchmark 2>&1 | tee lwe_decrypt_fpga_result_128b.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

预期 `lwe_decrypt FPGA execution passed`、`correctness_checked=yes`。只检查密文格式、Host 参考解密和密钥匹配，不访问 FPGA 时改用 `--inspect-only`。

## 5. 独立加密后送远端 HPU：`vscode-lwe-encrypt-remote-offload`

先启动第 0 节的远端服务。此应用不做过滤，不写 SSD；它会将前 128 个原始字节加密、送 129 做 `+1`，并把远端结果保存为可供第 4 节解密的 dump：

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-remote-offload
make -j2
make test
set -o pipefail
./vscode-lwe-encrypt-remote-offload \
  --ssd-nsid 1 --ssd-lba 65536 --plaintext-bytes 128 \
  --key "$LWE_KEY" --server 10.16.0.129 --server-port 19090 \
  --remote-operation adds --scalar 1 \
  --output lwe_encrypt_remote_hpu_result_128b.bin \
  --benchmark 2>&1 | tee lwe_encrypt_remote_hpu_128b.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

预期 `remote_hpu_ciphertext_compute=passed` 和 `lwe_encrypt remote HPU pipeline passed`。

如需单独在 FPGA 上解密刚保存的远端结果，在第 4 节的解密目录运行：

```bash
cd /mnt/suda/host/applications/vscode-lwe-decrypt-offload
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-remote-offload/lwe_encrypt_remote_hpu_result_128b.bin \
  --key "$LWE_KEY" --output lwe_decrypt_remote_hpu_result_128b.bin \
  --benchmark
```

做重复的 `echo/adds` 网络与 HPU 对比测试：

```bash
BATCH_SIZES="1 16 32 128" REMOTE_OPERATIONS="echo adds" \
WARMUP=2 ITERATIONS=10 SETTLE_SECONDS=1 SLM_READ_CHUNK_BYTES=131072 \
OUTPUT_CSV=remote_pipeline_benchmark_auto_lba.csv \
  ./run_remote_pipeline_bench.sh
./summarize_remote_pipeline.py remote_pipeline_benchmark_auto_lba.csv \
  | tee remote_pipeline_benchmark_summary.md
```

当前应用的 SLM→Host 和 Host→SLM 默认请求均为 128 KiB，允许的单请求范围为
4–256 KiB且必须按4 KiB对齐。ARM QDMA transport 的 `max_io_size` 同步为
256 KiB。一个256 KiB请求由64个4 KiB PRP/IOV组成，但仍是一条NVMe命令，
ARM MCDMA以一次multi-BD事务提交并在最后一个BD完成后返回。

## 6. 不过滤的 SSD→HPU→FPGA 解密→SSD：`vscode-lwe-full-pipeline`

该程序把 SSD 前 128 个原始字节做远端 `+1`，FPGA 解密后从 SLM 直接 Copy 到目标 SSD。**覆盖目标 LBA 131072 的一个 4KB 页**。先确认第 1 节写入的 TPC-H 数据仍在源 LBA；`--expect` 是源 SSD 第一个字节，下面从同一参考文件计算：

```bash
cd /mnt/suda/host/applications/vscode-lwe-full-pipeline
make -j2
FIRST_U8=$(od -An -tu1 -N1 \
  ../tpch-lineitem-fpga-test/testdata/tpch_lineitem_512b.bin | xargs)
set -o pipefail
./vscode-lwe-full-pipeline \
  --ssd-nsid 1 --ssd-lba 65536 --input-lbas 1 --plaintext-bytes 128 \
  --output-ssd-nsid 1 --output-ssd-lba 131072 \
  --expect "$FIRST_U8" \
  --server 10.16.0.129 --server-port 19090 --scalar 1 \
  --key "$LWE_KEY" --benchmark \
  2>&1 | tee lwe_full_pipeline_128b.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

预期 `lwe full SSD-to-remote-HPU-to-SSD pipeline passed` 和 `destination_ssd_readback_checked=yes`。此应用处理原始字节，不保留或修改 TPC-H 行结构。

## 7. 完整 TPC-H 过滤/加密/远端计算/解密/行更新：`vscode-selective-lwe-full-pipeline`

先启动第 0 节远端服务，并确保第 1 节 `--prepare` 已生成数据集。脚本**先覆盖源 LBA 65536–65551**，再将修改后的完整 64KB 记录镜像写入**目标 LBA 131072–131087**，回读逐字节校验。默认是源/目标分离的 copy-on-write 测试：

```bash
cd /mnt/suda/host/applications/vscode-selective-lwe-full-pipeline
make -j2
make test
set -o pipefail
./run_tpch_q6_update.sh 2>&1 | tee tpch_q6_full_update.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

脚本默认读写粒度为128 KiB。验证256 KiB单请求路径时使用：

```bash
SLM_READ_CHUNK_BYTES=262144 SLM_WRITE_CHUNK_BYTES=262144 \
  ./run_tpch_q6_update.sh 2>&1 | tee tpch_q6_full_update_256k.log
```

预期 `selective SSD-to-remote-HPU-to-SSD update pipeline passed`、`selected_count=5`、`updated_count=5`、`destination_readback_checked=yes` 和退出码 0。当前 128 行样本中的行索引 `55,79,81,85,99`，`quantity` 由 `21,23,13,19,14` 变为 `22,24,14,20,15`。本流程使用 Host read/modify/write 保留每条记录其余字节；它与第 6 节直接将解密页 Copy 到 SSD 的落盘方式不同。

如果远端计算已完成而 FPGA 解密失败，脚本保留精确的 5 条原生密文与预期明文；恢复设备连接后可独立复测解密，不重跑过滤和远端 HPU：

```bash
set -o pipefail
./run_decrypt_isolation.sh 2>&1 | tee tpch_q6_decrypt_isolation.log
test_rc=${PIPESTATUS[0]}
printf 'test_exit_code=%s\n' "$test_rc"
```

## 8. 可选：独立 4KB 测试数据生成器 `vscode-lwe-encrypt-data-gen`

如果不想用 TPC-H 行的原始字节测独立加密，可先选取实验专用、未占用的 LBA。**该应用覆盖所选 4KB LBA**；不要在后续 TPC-H 流程继续假定该 LBA 仍是 `lineitem`：

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-data-gen
make -j2
mkdir -p testdata
dd if=/dev/urandom of=testdata/plaintext_u8_4k.bin bs=4096 count=1 status=none
UNUSED_SSD_LBA=196608  # 示例值；先确认该 LBA 确实空闲
./vscode-lwe-encrypt-data-gen \
  --input testdata/plaintext_u8_4k.bin \
  --ssd-nsid 1 --ssd-lba "$UNUSED_SSD_LBA"
```

独立加密时将第 3 节的 `--ssd-lba 65536` 换成 `--ssd-lba "$UNUSED_SSD_LBA"`。

## 9. 可选：x86 CPU 对照应用 `suda-lwe-cpu-baseline`

在 **132 x86 宿主机** 编译；QEMU 中 `/mnt/suda` 应能看到生成的程序。运行前准备服务和客户端密钥，输出 LBA 会被覆盖：

```bash
cd /home/yangchenghui/suda
cargo build --release --manifest-path hpu/cpu-baseline/Cargo.toml
```

在 **132 QEMU Host VM** 执行与 FPGA 通路同源的 Fidus SSD 基线。当前固定 TPC-H 样本的首字节为 `1`；若换数据，应从对应参考镜像重新取得第一个字节：

```bash
cd /mnt/suda
FIRST_U8=$(od -An -tu1 -N1 \
  host/applications/tpch-lineitem-fpga-test/testdata/tpch_lineitem_512b.bin | xargs)
./hpu/cpu-baseline/target/release/suda-lwe-cpu-baseline \
  --storage-backend fidus-ssd \
  --input-ssd /dev/nvmq0n1 --input-lba 65536 --plaintext-bytes 128 \
  --output-ssd /dev/nvmq0n1 --output-lba 131072 \
  --client-key /mnt/suda/hpu/keys/psi64/psi64_shortint_ks32_client_key.bincode \
  --server 10.16.0.129:19090 --scalar 1 --cpu-threads 1 \
  --expect "$FIRST_U8" \
  --csv hpu/cpu-baseline/cpu_full_pipeline_128b.csv \
  2>&1 | tee hpu/cpu-baseline/cpu_full_pipeline_128b.log
```

CPU 基线也有 x86 本地 SSD 模式、16 线程模式和详细计时定义，见 [CPU 基线 README](../../hpu/cpu-baseline/README.md)。

## 运行结果与性能口径

所有带 `tee` 的 FPGA 命令都用紧跟其后的 `PIPESTATUS[0]` 判断应用退出码；`tee` 成功不代表应用成功。完整 TPC-H 测试已经有一次上板通过记录：128 行中选中/更新 5 行，`filter_encrypt_result_bytes=491520`，`remote_result_bytes=491520`，`decrypt_result_bytes=64`，退出码 0。独立 128B 加密也已通过；一次关闭 ARM 热路径日志后的样本显示 FPGA 执行约 30.9ms、SLM→Host 约 253.6ms。上述数字是历史单次实测，不是性能保证。

当前 FPGA LWE 数据面按每个 `u8` 生成四个 2-bit radix Big-LWE，HPU-native 物理密文为 `98304B/u8`。独立原始字节测试与 TPC-H `quantity` 字段测试应分别解读。详细原理、限制和参数见各应用目录的 README。
