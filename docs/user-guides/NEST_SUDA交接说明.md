# NEST SUDA 交接说明

本文给出 `nest` 分支的源码边界和最小复现入口。完整的跨机器部署、
TFHE-rs/HPU 服务端和实验室私有 artifact 安装由 NEST 顶层仓库统一管理。

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
git clone -b nest git@10.30.19.43:yangchenghui/suda.git "$SUDA_ROOT"
git -C "$SUDA_ROOT" submodule update --init --recursive
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
export NEST_LWE_KEYSET_SOURCE=/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata
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

需要生成个人新 keyset 时，在应用过 NEST overlay 的 TFHE-rs 源码树执行：

```bash
export TFHE_RS_ROOT="$HOME/hpu/tfhe-rs"
export HPU_BACKEND_DIR="$TFHE_RS_ROOT/backends/tfhe-hpu-backend"

cd "$TFHE_RS_ROOT"
cargo run --release -p tfhe --features hpu \
  --example hpu_export_lwe_encrypt_key -- \
  --params "$TFHE_RS_ROOT/mockups/tfhe-hpu-mockup/params/tuniform_64b_pfail128_psi64.toml" \
  --output-dir "$SUDA_ROOT/device/operators/hls/lwe_encrypt/testdata" \
  --sync-write
```

生成新 keyset 不需要重新生成 CSD 比特流，因为 Big-LWE key 在运行时装入
算子 context；但必须把新 compressed ServerKey 部署到 129 并重启 HPU 服务。
旧 keyset 对应的密文不能与新 keyset 混用。

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
    -> Host memory -> SLM -> FPGA decrypt -> SSD
```

密钥和远端 HPU 服务必须来自同一套 psi64 参数和 client key。
