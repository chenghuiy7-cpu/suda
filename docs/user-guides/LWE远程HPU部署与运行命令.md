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
真实板卡序列号、密钥和 `psi64.hpu` 均放在 Git 仓库外。

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

## 3. 在 132 生成部署包

准备仓库外的 ServerKey 并设置其预期摘要：

```bash
export SUDA_ROOT=/path/to/suda
export SERVER_KEY_SOURCE=/secure/path/psi64_integer_compressed_server_key.bincode

source "$SUDA_ROOT/hpu/manifests/remote-hpu.env"
export HPU_REMOTE_SERVER_KEY_SHA256="$HPU_SERVER_KEY_SHA256"

cd "$SUDA_ROOT"
bash hpu/scripts/package_remote_server.sh
```

默认产物是 `hpu/artifacts/suda-remote-hpu-server.tar.gz`。脚本会拒绝版本不符或有本地
修改的 TFHE-rs 子模块，校验 ServerKey，构建 release 二进制，并生成包内
`SHA256SUMS`。部署包不包含 ClientKey、Big-LWE 私钥和 `psi64.hpu`。

复制到 129：

```bash
export SSH_KEY=/secure/path/to/id_ed25519
export HPU_REMOTE_USER=your-user
export HPU_REMOTE_HOST=10.16.0.129
export HPU_REMOTE_SSH_PORT=2222

scp -P "$HPU_REMOTE_SSH_PORT" -i "$SSH_KEY" \
  "$SUDA_ROOT/hpu/artifacts/suda-remote-hpu-server.tar.gz" \
  "${HPU_REMOTE_USER}@${HPU_REMOTE_HOST}:/tmp/"
```

## 4. 在 129 安装

129 需要一个未修改、位于相同 gitlink 的 TFHE-rs checkout，以及包含已经验证的真实
`psi64.hpu` 的 HPU runtime。可以 clone SUDA 子模块，也可以使用管理员预装的同版本
TFHE-rs/runtime tree；不要在 checkout 中应用 SUDA 补丁。

解压并校验服务包：

```bash
export HPU_INSTALL=/opt/suda-remote-hpu
mkdir -p "$HPU_INSTALL"
tar -xzf /tmp/suda-remote-hpu-server.tar.gz -C "$HPU_INSTALL"
cd "$HPU_INSTALL"
sha256sum -c SHA256SUMS
```

如果 129 能访问 GitHub，可准备同版本子模块：

```bash
git clone https://github.com/zama-ai/tfhe-rs.git /opt/tfhe-rs
git -C /opt/tfhe-rs checkout --detach \
  e8ab4484545a9f6512f42d2b75509855093e8597
test -z "$(git -C /opt/tfhe-rs status --porcelain)"
```

真实 `psi64.hpu` 应由实验室制品库安装到 runtime tree，并按
`hpu/manifests/remote-hpu.env` 的大小和 SHA-256 校验。不要用仓库内的占位文件覆盖
已经验证可用的制品。

## 5. 在 129 启动服务

从模板创建仓库外环境文件，填写真实设备参数：

```bash
cp "$HPU_INSTALL/config/hpu-server.env.example" /secure/path/hpu-server.env
```

至少应设置：

```bash
export SUDA_HPU_ROOT="$HPU_INSTALL"
export TFHE_RS_ROOT=/opt/tfhe-rs
export HPU_BACKEND_DIR=/path/to/validated/tfhe-hpu-backend-runtime
export HPU_REMOTE_CONFIG="$HPU_BACKEND_DIR/config_store/v80/hpu_config.toml"
export HPU_REMOTE_SERVER_KEY="$HPU_INSTALL/runtime/psi64_integer_compressed_server_key.bincode"
export HPU_REMOTE_SERVER_KEY_SHA256=<verified-sha256>
export XILINX_VIVADO=/path/to/Vivado
export HPU_CONFIG=v80
export V80_PCIE_DEV=xx
export V80_SERIAL_NUMBER=REPLACE_ME
export AMI_PATH=/path/to/installed/ami-driver
```

在 129 的 `tmux` 中启动：

```bash
tmux new -s hpu-lwe-server
source /secure/path/hpu-server.env
bash "$HPU_INSTALL/scripts/start_remote_server.sh" \
  2>&1 | tee "$HPU_INSTALL/suda-remote-hpu-server.log"
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

## 7. 停止与安全规则

在服务 `tmux` 中按 `Ctrl+C`，然后使用 `ss -ltnp | grep 19090` 确认端口已释放。

- 当前实验网 TCP 原型没有认证、加密和防重放；正式部署应增加 mTLS 和请求签名。
- 不要提交 ClientKey、ServerKey、Big-LWE 私钥、SSH 私钥、真实板卡序列号或密文 dump。
- `psi64.hpu` 只在外部制品库保存，并按 manifest 校验。
- 多卡机器必须确认 `V80_PCIE_DEV` 与目标 PF0/序列号一致。
