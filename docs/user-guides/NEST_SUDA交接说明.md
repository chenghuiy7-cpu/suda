# NEST SUDA 交接说明

本文给出 GitHub `main` 分支的源码边界和最小复现入口。完整的跨机器部署、
TFHE-rs/HPU 服务端和实验室私有 artifact 安装均由 SUDA 仓库统一管理。

## 1. 本分支包含的内容

- psi64 HPU-native LWE 加密算子和 LWE 解密算子；
- 已生成并用于 block design 的算子池 Verilog；
- MCDMA、AXI DMA 和 HLS runtime 修复；
- NVMQ/QDMA 诊断和三 I/O 队列配置；
- 数据生成、加密、远端 HPU、解密和完整闭环 Host 应用；
- 性能测试脚本、结果和开发记录。

## 2. 不进入 Git 的机器相关文件

以下内容必须通过实验室 artifact 包安装，并用其中的 `SHA256SUMS` 校验：

- `BOOT.bin` 和 `zynqmp.dtb`；
- ARM 侧 `nvmf_tgt` 及其动态库；
- QEMU 镜像、自定义内核和 QEMU 可执行文件；
- Big-LWE secret key、TFHE client key 和 compressed server key；
- 明文、密文、运行日志和 Vivado/Cargo 构建产物。

不要把密钥、板卡镜像或个人 SSH 私钥提交到 Git。

## 3. 在个人 HOME 下准备源码

```bash
export SUDA_ROOT="$HOME/suda"
git clone --recurse-submodules -b main \
  https://github.com/chenghuiy7-cpu/suda.git "$SUDA_ROOT"
```

自定义 guest kernel 所需的 QDMA headers 和 `bio_map_user_iov` 导出修改已经保存为：

```text
host/kernel/0001-Add-QDMA-header-files.patch
host/kernel/0002-Export-bio_map_user_iov.patch
```

`host/qemu/init.sh` 会在内核子模块中应用它们。不要提交“已应用 patch 后”的
dirty 子模块状态，patch 文件才是可重复构建的来源。

如果本机空间不足，可以把构建目录放在 `/data/$USER`，再使用软链接连接到
个人 HOME 下的源码树。

## 4. 安装或重新生成匹配密钥

仅 clone 源码不能执行 LWE 加解密。当前实验室已验证 keyset 不进入 Git，
但可从本机只读 artifact 源安装到个人 clone：

```bash
export NEST_LWE_KEYSET_SOURCE=/path/to/validated/psi64-keyset
bash "$SUDA_ROOT/device/operators/hls/lwe_encrypt/testdata/install_validated_keyset.sh" \
  "$NEST_LWE_KEYSET_SOURCE"
```

安装器会校验并安装同一套 psi64 keyset 中的三份文件：

- `psi64_big_lwe_secret_key.bin`：FPGA 加密、解密算子运行时 context；
- `psi64_shortint_ks32_client_key.bincode`：Host/mockup 解密和正确性验证；
- `psi64_integer_compressed_server_key.bincode`：远端 V80 HPU 服务端。

使用当前已启动且 keyset 未变的 129 服务时，本机应用至少需要第一份密钥；
为了完整验证和独立启动服务，建议安装全部三份。三者必须来自同一次生成，
不能混用。

需要生成个人新 keyset 时，在 SUDA 根目录执行独立 keygen。它只读取未修改子模块的
公开 API，不需要 NEST overlay：

```bash
cd "$SUDA_ROOT"
bash hpu/scripts/generate_psi64_keyset.sh
bash hpu/scripts/install_psi64_keyset.sh
```

生成新 keyset 不需要重新生成 CSD 比特流，因为 Big-LWE key 在运行时装入
算子 context；但必须把新 compressed ServerKey 部署到 129 并重启 HPU 服务。
旧 keyset 对应的密文不能与新 keyset 混用。

新生成的 ServerKey 与 manifest 中记录的实验室基线摘要不同。部署前记录其大小和摘要：

```bash
export GENERATED_SERVER_KEY="$SUDA_ROOT/device/operators/hls/lwe_encrypt/testdata/psi64_integer_compressed_server_key.bincode"
export HPU_REMOTE_SERVER_KEY_SIZE=$(stat -c %s "$GENERATED_SERVER_KEY")
export HPU_REMOTE_SERVER_KEY_SHA256=$(sha256sum "$GENERATED_SERVER_KEY" | awk '{print $1}')
```

在准备 129 runtime 和启动服务时必须导出同样的两个变量。

## 5. 生成 HLS RTL

```bash
export VITIS_HLS_ROOT=/opt/Xilinx_2020.2/Vitis_HLS/2020.2
export VIVADO_ROOT=/opt/Xilinx_2020.2/Vivado/2020.2
export HLS_HOST_ARCH_INCLUDE=/usr/include/x86_64-linux-gnu

cd "$SUDA_ROOT/device/operators"
make lwe_encrypt_hwop
make lwe_decrypt_hwop
```

把生成的 Verilog 更新到算子池后，确认算子池文件与
`solution1/syn/verilog` 一致，再生成完整比特流。

## 6. 生成完整 Fidus 启动文件

```bash
cd "$SUDA_ROOT/device/platform/basic_shell/nf-csd"
source /opt/Xilinx_2020.2/Vivado/2020.2/settings64.sh
bash build_bd.sh 2>&1 | tee "build_bd_$(date +%Y%m%d_%H%M%S).log"
```

上板前必须备份引导分区现有文件并校验新旧 SHA-256。不要仅凭文件名判断
板卡镜像版本。

## 7. 启动 QEMU

共享宿主机上可能存在多张 Xilinx 卡。先确认目标卡，再显式指定 BDF：

```bash
export NEST_CSD_PCI_BDF=0000:d9:00.0  # 示例，必须替换为当次确认的 Fidus BDF
cd "$SUDA_ROOT/host/qemu"
sudo -E bash bind_vfio.sh
sudo -E bash run_qemu.sh -f
```

脚本会拒绝绑定非 Xilinx accelerator，但无法仅凭设备 ID 区分所有 XDMA/QDMA
镜像。操作前仍需确认板卡归属，避免影响其他用户。

## 8. 初始化 NVMQ

在 QEMU guest 中执行：

```bash
cd /mnt/suda/host/drivers/nvmq
bash init_nvmq.sh
cat nvmq0_opts > /dev/nvmq-fabrics
```

当前验证配置固定为 `nr_io_queues=3`，避免四 vCPU 配置中 admin `qid=0` 与
`qid=4` 映射到同一物理 QDMA 队列。

## 9. 构建应用

```bash
make -C /mnt/suda/host/api
make -C /mnt/suda/host/applications
```

完整闭环应用位于：

```text
host/applications/vscode-lwe-full-pipeline
```

其数据路径为：

```text
SSD -> SLM -> FPGA encrypt -> Host memory -> TCP -> remote HPU
    -> Host memory -> decrypt input SLM -> FPGA decrypt
    -> decrypt output SLM -> NVMe Copy -> SSD
```

密钥和远端 HPU 服务必须来自同一套 psi64 参数和 client key。

## 10. 最小加解密测试命令

以下命令在 QEMU guest 中执行，并假设 ARM `nvmf_tgt` 正常运行、NVMQ 已连接，
当前上板镜像包含 `lwe_encrypt`（type ID 2）和 `lwe_decrypt`（type ID 3）。示例
会覆盖 SSD LBA `65536` 和 `131072`，执行前必须确认它们属于可覆盖测试区且互不
重叠。

### 10.1 准备 128B 测试明文并写入 SSD

数据生成程序按 4KB LBA 写入，因此先生成一页随机数据，其中前 128B 是本次
加密输入：

```bash
export SUDA_VM_ROOT=/mnt/suda
export INPUT_LBA=65536
export OUTPUT_LBA=131072
export LWE_KEY="$SUDA_VM_ROOT/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin"

cd "$SUDA_VM_ROOT/host/applications/vscode-lwe-encrypt-data-gen"
mkdir -p testdata
dd if=/dev/urandom of=testdata/plaintext_u8_4k.bin \
  bs=4096 count=1 status=none
export FIRST_U8=$(od -An -tu1 -N1 testdata/plaintext_u8_4k.bin | xargs)
echo "FIRST_U8=$FIRST_U8"

./vscode-lwe-encrypt-data-gen \
  --input testdata/plaintext_u8_4k.bin \
  --ssd-nsid 1 \
  --ssd-lba "$INPUT_LBA"
```

成功标志是 `u8 plaintext SSD write passed` 和 `readback_verified=yes`。

### 10.2 单独测试 FPGA 加密算子

```bash
cd "$SUDA_VM_ROOT/host/applications/vscode-lwe-encrypt-offload"

./vscode-lwe-encrypt-offload \
  --ssd-nsid 1 \
  --ssd-lba "$INPUT_LBA" \
  --input-lbas 1 \
  --plaintext-bytes 128 \
  --expect "$FIRST_U8" \
  --key "$LWE_KEY" \
  --output lwe_encrypt_fpga_ciphertexts_128b.bin \
  --benchmark 2>&1 | tee lwe_encrypt_128b.log
```

成功标志是 `lwe_encrypt FPGA execution passed`、`decrypted_count=128` 和
`host_key_decrypt_checked=yes`。

### 10.3 单独测试 FPGA 解密算子

上一步生成的 `LWEHLS01` dump 可直接作为解密程序输入。程序会将其中的逻辑
Big-LWE 重排为 HPU-native 物理布局后写入 input SLM：

```bash
cd "$SUDA_VM_ROOT/host/applications/vscode-lwe-decrypt-offload"

./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_fpga_ciphertexts_128b.bin \
  --key "$LWE_KEY" \
  --output lwe_decrypt_fpga_result_128b.bin \
  --benchmark 2>&1 | tee lwe_decrypt_128b.log
```

成功标志是 `lwe_decrypt FPGA execution passed`、`decrypted_count=128` 和
`correctness_checked=yes`。

### 10.4 测试 SSD 到远端 HPU 再回 SSD 的完整闭环

先在 129 启动使用同一 ServerKey 的 `suda-remote-hpu-server`，确认端口
`19090` 正在监听，然后在 QEMU guest 执行：

```bash
cd "$SUDA_VM_ROOT/host/applications/vscode-lwe-full-pipeline"

./vscode-lwe-full-pipeline \
  --ssd-nsid 1 \
  --ssd-lba "$INPUT_LBA" \
  --input-lbas 1 \
  --plaintext-bytes 128 \
  --output-ssd-nsid 1 \
  --output-ssd-lba "$OUTPUT_LBA" \
  --expect "$FIRST_U8" \
  --server 10.16.0.129 \
  --server-port 19090 \
  --scalar 1 \
  --key "$LWE_KEY" \
  --benchmark 2>&1 | tee lwe_full_pipeline_128b.log
```

这里 `--expect` 指加密前的首字节；程序会自行计算远端 `+1` 后的预期结果。
成功标志包括：

```text
lwe full SSD-to-remote-HPU-to-SSD pipeline passed
destination_ssd_readback_checked=yes
fpga_decrypt_checked=yes
remote_hpu_ciphertext_compute=passed
```

默认模式会在 NVMe Copy 完成后从目标 SSD 回读明文到 Host，并做端到端正确性校验。
若只测量 `decrypt output SLM -> SSD` 的直接数据路径，可增加
`--skip-ssd-readback`；此时不会把解密明文回读到 Host，成功标志中的
`destination_ssd_readback_checked` 和 `fpga_decrypt_checked` 均为 `no`。
