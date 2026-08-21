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

如果本机空间不足，可以把构建目录放在 `/data/$USER`，再使用软链接连接到
个人 HOME 下的源码树。

## 4. 生成 HLS RTL

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

## 5. 生成完整 Fidus 启动文件

```bash
cd "$SUDA_ROOT/device/platform/basic_shell/nf-csd"
source /opt/Xilinx_2020.2/Vivado/2020.2/settings64.sh
bash build_bd.sh 2>&1 | tee "build_bd_$(date +%Y%m%d_%H%M%S).log"
```

上板前必须备份引导分区现有文件并校验新旧 SHA-256。不要仅凭文件名判断
板卡镜像版本。

## 6. 启动 QEMU

共享宿主机上可能存在多张 Xilinx 卡。先确认目标卡，再显式指定 BDF：

```bash
export NEST_CSD_PCI_BDF=0000:d9:00.0  # 示例，必须替换为当次确认的 Fidus BDF
cd "$SUDA_ROOT/host/qemu"
sudo -E bash bind_vfio.sh
sudo -E bash run_qemu.sh -f
```

脚本会拒绝绑定非 Xilinx accelerator，但无法仅凭设备 ID 区分所有 XDMA/QDMA
镜像。操作前仍需确认板卡归属，避免影响其他用户。

## 7. 初始化 NVMQ

在 QEMU guest 中执行：

```bash
cd /mnt/suda/host/drivers/nvmq
bash init_nvmq.sh
cat nvmq0_opts > /dev/nvmq-fabrics
```

当前验证配置固定为 `nr_io_queues=3`，避免四 vCPU 配置中 admin `qid=0` 与
`qid=4` 映射到同一物理 QDMA 队列。

## 8. 构建应用

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
