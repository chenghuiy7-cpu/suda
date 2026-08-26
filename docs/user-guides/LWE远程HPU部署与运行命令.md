# LWE 远程 HPU 部署与运行命令

## 1. 适用范围与源码边界

本手册覆盖以下链路：132 保存唯一源码并准备部署文件，129 启动真实 V80 HPU 服务，
SUDA 将 HPU-native 密文发送到 129 执行 `ADDS +1`，接收结果后进入 FPGA 解密链路。

远端 HPU 自研源码已经统一放入 SUDA：

```text
hpu/overlays/tfhe-rs/                                  # Rust 服务端覆盖层
host/applications/vscode-lwe-encrypt-remote-offload/  # SUDA C++ 客户端
host/applications/vscode-lwe-full-pipeline/           # 完整闭环客户端
```

`hpu/overlays/tfhe-rs` 不是完整 Cargo workspace。完整 TFHE-rs 基仓固定到
`hpu/manifests/remote-hpu.env` 中的 revision，并在需要构建时生成到被 Git 忽略的
`hpu/worktree/tfhe-rs`。以后只需提交和 push SUDA 仓库，不再提交顶层 `hpu/` 中的
独立仓库。

前置条件：

- 132 已有 SUDA 源码、匹配的 HPU 归档和密钥制品；
- 129 已安装 Vivado、AMI/QDMA 驱动及 V80 HPU 运行环境；
- ClientKey 和 Big-LWE 私钥只保留在 132；129 只接收 `CompressedServerKey`；
- SSH 私钥、真实板卡序列号和密钥文件均放在 SUDA 仓库之外。

先在 132 设置本机变量：

```bash
export SUDA_ROOT=/path/to/suda
export TFHE_RS_ROOT="$SUDA_ROOT/hpu/worktree/tfhe-rs"
export HPU_ARCHIVE_SOURCE=/secure/path/psi64.hpu
export SERVER_KEY_SOURCE=/secure/path/psi64_integer_compressed_server_key.bincode
export SSH_KEY=/secure/path/to/id_ed25519
export HPU_REMOTE_USER=your-user
export HPU_REMOTE_HOST=10.16.0.129
export HPU_REMOTE_SSH_PORT=2222

source "$SUDA_ROOT/hpu/manifests/remote-hpu.env"
export HPU_REMOTE_SERVER_KEY_SHA256="$HPU_SERVER_KEY_SHA256"
```

## 2. 在 132 准备可构建工作树

```bash
cd "$SUDA_ROOT"
bash hpu/scripts/bootstrap_tfhe_rs.sh "$TFHE_RS_ROOT"
bash hpu/scripts/verify_remote_hpu.sh
```

如果本机已有 TFHE-rs 镜像，可避免重新下载 Git/LFS 历史：

```bash
TFHE_RS_SOURCE=/path/to/local/tfhe-rs \
  bash "$SUDA_ROOT/hpu/scripts/bootstrap_tfhe_rs.sh" "$TFHE_RS_ROOT"
```

工作树中的 overlay 改动是可再生成副本；应修改并提交
`$SUDA_ROOT/hpu/overlays/tfhe-rs`，再用 `apply_tfhe_rs_overlay.sh --refresh` 刷新工作树。

## 3. 新用户：129 还没有 TFHE-rs 源码

### 3.1 在 132 生成首次部署文件

```bash
cd "$TFHE_RS_ROOT"

SHORT_COMMIT=$(git rev-parse --short=12 HEAD)
BASE_ARCHIVE="target/tfhe-rs-base-${SHORT_COMMIT}.tar.gz"

# 基线源码和 SUDA 管理的 overlay 分开打包；git archive 不包含工作树改动。
git archive --format=tar.gz --output="${BASE_ARCHIVE}" HEAD
install -D -m 0644 "$HPU_ARCHIVE_SOURCE" target/psi64.hpu

./scripts/lwe_remote_hpu/package_server_129.sh

(
  cd target
  sha256sum \
    "$(basename "${BASE_ARCHIVE}")" \
    lwe_remote_hpu_server_129.tar.gz \
    psi64.hpu \
    > lwe_remote_hpu_bootstrap_SHA256SUMS
)

ls -lh \
  "${BASE_ARCHIVE}" \
  target/lwe_remote_hpu_server_129.tar.gz \
  target/psi64.hpu \
  target/lwe_remote_hpu_bootstrap_SHA256SUMS
```

`package_server_129.sh` 会再次校验 ServerKey SHA-256。`psi64.hpu` 也必须与
`hpu/manifests/remote-hpu.env` 中记录的大小和 SHA-256 一致。

### 3.2 从 132 复制到 129

```bash
cd "$TFHE_RS_ROOT"
BASE_ARCHIVE=$(awk '$2 ~ /^tfhe-rs-base-/ {print "target/" $2}' \
  target/lwe_remote_hpu_bootstrap_SHA256SUMS)

scp -P "$HPU_REMOTE_SSH_PORT" -i "$SSH_KEY" \
  "${BASE_ARCHIVE}" \
  target/lwe_remote_hpu_server_129.tar.gz \
  target/psi64.hpu \
  target/lwe_remote_hpu_bootstrap_SHA256SUMS \
  "${HPU_REMOTE_USER}@${HPU_REMOTE_HOST}:/tmp/"
```

### 3.3 在 129 建立源码树并校验

```bash
export TFHE_RS_ROOT=/path/to/remote/tfhe-rs

cd /tmp
sha256sum -c lwe_remote_hpu_bootstrap_SHA256SUMS
BASE_ARCHIVE=$(awk '$2 ~ /^tfhe-rs-base-/ {print $2}' \
  lwe_remote_hpu_bootstrap_SHA256SUMS)

mkdir -p "$TFHE_RS_ROOT"
tar -xzf "/tmp/${BASE_ARCHIVE}" -C "$TFHE_RS_ROOT"
tar -xzf /tmp/lwe_remote_hpu_server_129.tar.gz -C "$TFHE_RS_ROOT"

install -D -m 0644 /tmp/psi64.hpu \
  "$TFHE_RS_ROOT/backends/tfhe-hpu-backend/config_store/v80_archives/psi64.hpu"

cd "$TFHE_RS_ROOT"
sha256sum -c SHA256SUMS
```

## 4. 旧用户：129 已经跑通过

如果旧服务仍在运行，先在 129 的服务终端按 `Ctrl+C`，再更新部署文件。

在 132 生成并发送新的覆盖包：

```bash
cd "$TFHE_RS_ROOT"
./scripts/lwe_remote_hpu/package_server_129.sh

scp -P "$HPU_REMOTE_SSH_PORT" -i "$SSH_KEY" \
  target/lwe_remote_hpu_server_129.tar.gz \
  "${HPU_REMOTE_USER}@${HPU_REMOTE_HOST}:/tmp/"
```

在 129 更新：

```bash
export TFHE_RS_ROOT=/path/to/remote/tfhe-rs
cd "$TFHE_RS_ROOT"
tar -xzf /tmp/lwe_remote_hpu_server_129.tar.gz
sha256sum -c SHA256SUMS
```

旧用户不需要重新复制基础源码和 `psi64.hpu`。除非明确更新了 HPU 归档，否则不要覆盖
129 上已经验证可用的 `psi64.hpu`。

## 5. 在 129 启动服务

先把 SUDA 中的模板复制到仓库外，填写真实环境后安全传给 129：

```bash
cp "$SUDA_ROOT/hpu/config/hpu-server.env.example" /secure/path/hpu-server.env
```

在 129 的 `tmux` 中执行：

```bash
export TFHE_RS_ROOT=/path/to/remote/tfhe-rs
source /secure/path/hpu-server.env

tmux new -s hpu-lwe-server
cd "$TFHE_RS_ROOT"
./scripts/lwe_remote_hpu/start_server_129.sh \
  2>&1 | tee hpu_lwe_remote_server.log
```

成功后服务会停在 `listen_addr` 等待请求，这不是卡死：

```text
hardware_reload_policy=never
hpu_device_ready=yes
client_key_loaded=no
listen_addr=0.0.0.0:19090
```

`start_server_129.sh` 会拒绝缺少 `force_reload="never"` 的配置、错误的 ServerKey
校验值和不包含 no-reload 保护的二进制。

## 6. 在 132 运行 SUDA 客户端

推荐直接使用 SUDA 已纳入 Git 的 C++ 客户端，不再依赖 TFHE-rs 中的 Rust 诊断客户端。
先在 132 构建：

```bash
cd "$SUDA_ROOT/host/applications/vscode-lwe-encrypt-remote-offload"
make -j4
make test
```

启动 129 服务后，在 132 QEMU 中执行：

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

完整的 SSD→FPGA 加密→远端 HPU→FPGA 解密→SSD 闭环使用：

```text
host/applications/vscode-lwe-full-pipeline/
```

## 7. 停止服务

回到 129 的服务 tmux：

```bash
tmux attach -t hpu-lwe-server
```

按 `Ctrl+C` 停止服务，然后确认端口释放：

```bash
ss -ltnp | grep 19090
```

没有输出即表示服务已关闭。

## 8. 安全与制品规则

- 实验网 TCP 原型没有认证、加密和防重放；正式部署应增加 mTLS 和请求签名。
- 不要提交 ClientKey、ServerKey、Big-LWE 私钥、SSH 私钥、真实板卡序列号或密文 dump。
- `psi64.hpu` 是必需的部署制品，但不是普通源码；只在外部制品库保存，并按 manifest
  校验。
- no-reload 服务只允许复用已装载且匹配的固件。硬件不匹配时应退出，由管理员在维护
  窗口单独装载，不允许网络服务自动卸载驱动或重编程板卡。
