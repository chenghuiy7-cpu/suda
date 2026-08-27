# LWE 远程 HPU 部署与运行命令

## 1. 源码边界

本手册覆盖 132 上的 SUDA 客户端到 129 上真实 V80 HPU 服务的链路。远端计算为
密文 `u8 ADDS`，结果密文返回后再进入本机验证或 FPGA 解密链路。

相关源码统一由 SUDA 仓库管理：

```text
hpu/tfhe-rs/                                      # 未修改的上游 Git 子模块
hpu/remote-hpu/                                   # SUDA 独立 Rust 服务端
hpu/scripts/                                      # 准备、校验、打包和启动脚本
host/applications/vscode-lwe-encrypt-remote-offload/
host/applications/vscode-lwe-full-pipeline/
```

`hpu/tfhe-rs` 的 URL 跟踪上游 `main`，gitlink 固定在已经通过实机验证的
`e8ab4484545a9f6512f42d2b75509855093e8597`。不要在子模块中添加 SUDA 代码；升级
gitlink 时必须重新验证 V80 固件、AMI/QDMA、ServerKey 和完整流水线。

ClientKey、Big-LWE 私钥只保留在 132；129 只接收 `CompressedServerKey`。SSH 私钥、
真实板卡序列号、密钥和 `psi64.hpu` 均不纳入 Git；后两项可安装到被忽略的
`suda/hpu/runtime/`。

## 2. 在 132 准备和验证源码

首次 clone：

```bash
git clone --recurse-submodules https://github.com/chenghuiy7-cpu/suda.git
cd suda
```

已有 SUDA checkout：

```bash
cd /path/to/suda
bash hpu/scripts/prepare_tfhe_rs_submodule.sh
bash hpu/scripts/verify_remote_hpu.sh
```

构建和测试独立服务：

```bash
cargo +1.91.1 test --manifest-path hpu/remote-hpu/Cargo.toml
cargo +1.91.1 build --release --manifest-path hpu/remote-hpu/Cargo.toml
```

这些命令只通过 path dependency 读取子模块，不会修改其中源码。

## 3. 在 132 构建最小远端运行包

129 是 HPU 服务的部署机，**不 clone SUDA，也不编译 Rust 源码**。唯一源码工作树在
132。由 132 完成子模块校验、release 编译和制品打包，再通过 `scp` 向 129 发布。

打包前，132 应已有下列两个被 Git 忽略的私有制品：

```text
hpu/artifacts/private/psi64.hpu
hpu/keys/psi64/psi64_integer_compressed_server_key.bincode
```

执行：

```bash
export SUDA_ROOT="$HOME/suda"
export CARGO_TARGET_DIR="/data/$USER/cargo-targets/suda-remote-hpu"

cd "$SUDA_ROOT"
bash hpu/scripts/prepare_tfhe_rs_submodule.sh
bash hpu/scripts/verify_remote_hpu.sh
bash hpu/scripts/package_remote_server.sh

ls -lh hpu/artifacts/suda-remote-hpu-server.tar.gz
sha256sum hpu/artifacts/suda-remote-hpu-server.tar.gz
```

打包脚本会校验 `psi64.hpu` 和 ServerKey 的大小及 SHA-256，并拒绝从有本地改动的
TFHE-rs 子模块构建。包含以下内容：

```text
bin/suda-remote-hpu-server                 # 已编译的服务端
runtime/psi64_integer_compressed_server_key.bincode
runtime/tfhe-hpu-backend/config_store/     # V80 配置和真实 psi64.hpu
scripts/start_remote_server.sh
config/hpu-server-bundle.env.example
manifests/remote-hpu.env
SHA256SUMS
```

ClientKey 和 Big-LWE 私钥不会进入运行包，只保留在 132。

## 4. 从 132 发布到 129

以已验证的 SSH 端口和密钥为例：

```bash
export REMOTE_USER=your-user
export REMOTE_HOST=10.16.0.129
export REMOTE_PORT=2222
export REMOTE_KEY=/secure/path/to/id_ed25519

ssh -p "$REMOTE_PORT" -i "$REMOTE_KEY" \
  "$REMOTE_USER@$REMOTE_HOST" \
  'mkdir -p "$HOME/suda-remote-hpu-incoming"'

scp -P "$REMOTE_PORT" -i "$REMOTE_KEY" \
  "$SUDA_ROOT/hpu/artifacts/suda-remote-hpu-server.tar.gz" \
  "$REMOTE_USER@$REMOTE_HOST:suda-remote-hpu-incoming/"
```

如果 132 上已有与 129 当前驱动匹配的 `ami.ko`，可在打包时一并收录：

```bash
AMI_MODULE_SOURCE=/secure/path/ami.ko \
  bash hpu/scripts/package_remote_server.sh
```

没有时不影响打包；129 可复用本机已验证的 AMI 模块。

## 5. 在 129 解压并启动服务

在 129 上解压到独立运行目录：

```bash
archive="$HOME/suda-remote-hpu-incoming/suda-remote-hpu-server.tar.gz"
deploy="$HOME/suda-remote-hpu"

if [[ -d "$deploy" ]]; then
  mv "$deploy" "${deploy}.backup.$(date +%Y%m%d-%H%M%S)"
fi
mkdir -p "$deploy"
tar -xzf "$archive" -C "$deploy"

cd "$deploy"
sha256sum -c SHA256SUMS
```

如果包内没有 `runtime/ami-driver/ami.ko`，把 129 上已验证的模块复制进最小运行目录：

```bash
install -D -m 0644 \
  /path/to/validated/ami.ko \
  "$deploy/runtime/ami-driver/ami.ko"
```

这是一次性迁移；启动后不再依赖旧的 `~/hpu/tfhe-rs` 源码树。

创建机器专用环境文件：

```bash
mkdir -p "$HOME/.config/suda"
cp "$deploy/config/hpu-server-bundle.env.example" \
  "$HOME/.config/suda/hpu-server.env"
```

编辑 `~/.config/suda/hpu-server.env`，填入 129 实际的 Vivado 路径、PCIe bus ID 和板卡
序列号。下列值中的占位符必须在预检前替换：

```bash
export SUDA_HPU_ROOT="$HOME/suda-remote-hpu"
export HPU_REMOTE_RUNTIME_ROOT="$SUDA_HPU_ROOT/runtime"
export XILINX_VIVADO=/opt/Xilinx_2025.1/Vivado/2025.1/Vivado
export HPU_CONFIG=v80
export V80_PCIE_DEV=xx
export V80_SERIAL_NUMBER=REPLACE_ME
export HPU_BACKEND_DIR="$HPU_REMOTE_RUNTIME_ROOT/tfhe-hpu-backend"
export HPU_REMOTE_CONFIG="$HPU_BACKEND_DIR/config_store/v80/hpu_config.toml"
export HPU_REMOTE_SERVER_KEY="$HPU_REMOTE_RUNTIME_ROOT/psi64_integer_compressed_server_key.bincode"
export HPU_REMOTE_SERVER_BINARY="$SUDA_HPU_ROOT/bin/suda-remote-hpu-server"
export AMI_PATH="$HPU_REMOTE_RUNTIME_ROOT/ami-driver"
export HPU_REMOTE_BIND=0.0.0.0:19090
export RUST_LOG=info
```

不访问板卡，先校验包内路径、ServerKey 和真实 `psi64.hpu`：

```bash
source "$HOME/.config/suda/hpu-server.env"
HPU_REMOTE_PREFLIGHT_ONLY=1 \
  "$SUDA_HPU_ROOT/scripts/start_remote_server.sh"
```

成功标志为 `remote_server_preflight=passed`。

在 129 的 `tmux` 中启动：

```bash
tmux new -s hpu-lwe-server

source "$HOME/.config/suda/hpu-server.env"
"$SUDA_HPU_ROOT/scripts/start_remote_server.sh" \
  2>&1 | tee "$SUDA_HPU_ROOT/suda-remote-hpu-server.log"
```

成功标志包括：

```text
hardware_reload_policy=upstream-validate-and-recover
hpu_device_ready=yes
client_key_loaded=no
listen_addr=0.0.0.0:19090
```

上游基线只支持 `force_reload="false"`：硬件状态有效时会复用，检查失败时 backend
可能执行恢复性 reload。该策略不是“禁止重载”。启动网络服务前，管理员必须确认
V80、AMI/QDMA、PDI/HIS/UUID 状态；需要严格禁止自动 reload 时，应先在维护流程中
解决，而不能重新修改 TFHE-rs 子模块。

确认监听：

```bash
ss -ltnp | grep 19090
```

该流程的唯一源码入口是 132 上的 SUDA checkout。129 只保留可删除、可替换的运行包，
不需要 Git、Rust、Cargo 或 TFHE-rs 子模块。

## 6. 在 132 运行 SUDA 客户端

```bash
cd "$SUDA_ROOT/host/applications/vscode-lwe-encrypt-remote-offload"
make -j4
make test
```

在 132 QEMU guest 中执行：

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-remote-offload

./vscode-lwe-encrypt-remote-offload \
  --ssd-nsid 1 \
  --ssd-lba 65536 \
  --input-lbas 256 \
  --plaintext-bytes 128 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --server 10.16.0.129 \
  --server-port 19090 \
  --scalar 1 \
  --output lwe_encrypt_remote_hpu_result_128b.bin
```

成功标志：

```text
intermediate_ciphertext_dump=none
host_big_lwe_key_decrypt_checked=yes
remote_hpu_ciphertext_compute=passed
```

完整 SSD→FPGA 加密→远端 HPU→FPGA 解密→SSD 闭环位于：

```text
host/applications/vscode-lwe-full-pipeline/
```

先在 132 Host checkout 中构建：

```bash
cd "$SUDA_ROOT/host/applications/vscode-lwe-full-pipeline"
make clean && make -j
```

确认 ARM `nvmf_tgt`、QEMU NVMQ 和 129 服务均已启动后，在 132 QEMU guest 中执行：

```bash
cd /mnt/suda/host/applications/vscode-lwe-full-pipeline

./vscode-lwe-full-pipeline \
  --ssd-nsid 1 \
  --ssd-lba 65536 \
  --input-lbas 1 \
  --plaintext-bytes 128 \
  --output-ssd-nsid 1 \
  --output-ssd-lba 131072 \
  --expect 171 \
  --server 10.16.0.129 \
  --server-port 19090 \
  --scalar 1 \
  --slm-read-chunk-bytes 131072 \
  --slm-write-chunk-bytes 131072 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --benchmark \
  2>&1 | tee lwe_full_pipeline_128b.log
```

`--expect 171` 必须替换为源 SSD 数据第一个字节的十进制值。成功标志包括：

```text
lwe full SSD-to-remote-HPU-to-SSD pipeline passed
destination_ssd_readback_checked=yes
remote_hpu_ciphertext_compute=passed
```

## 7. 停止与安全规则

在服务 `tmux` 中按 `Ctrl+C`，然后使用 `ss -ltnp | grep 19090` 确认端口已释放。

- 当前实验网 TCP 原型没有认证、加密和防重放；正式部署应增加 mTLS 和请求签名。
- 不要提交 ClientKey、ServerKey、Big-LWE 私钥、SSH 私钥、真实板卡序列号或密文 dump。
- `psi64.hpu` 只在外部制品库保存，并按 manifest 校验。
- 多卡机器必须确认 `V80_PCIE_DEV` 与目标 PF0/序列号一致。
