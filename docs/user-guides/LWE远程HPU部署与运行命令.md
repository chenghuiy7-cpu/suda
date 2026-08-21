# LWE 远程 HPU 部署与运行命令

## 1. 适用范围

本手册覆盖以下链路：132 保存唯一源码并准备部署文件，129 启动真实 V80 HPU 服务，
132 将已有 `LWEHLS01` 密文发送到 129 执行 `ADDS +1`，接收结果后用本地 ClientKey
解密验证。

前置条件：

- 132 已有 `/home/yangchenghui/hpu/tfhe-rs`、SUDA 密文和对应密钥；
- 129 已安装 Vivado、AMI/QDMA 驱动及 V80 HPU 运行环境；
- 129 SSH 地址为 `10.16.0.129:2222`；
- ClientKey 和 Big-LWE 私钥只保留在 132；129 只接收 `CompressedServerKey`。

## 2. 新用户：129 还没有 tfhe-rs 源码

### 2.1 在 132 生成首次部署文件

```bash
cd /home/yangchenghui/hpu/tfhe-rs

SHORT_COMMIT=$(git rev-parse --short=12 HEAD)
BASE_ARCHIVE="target/tfhe-rs-base-${SHORT_COMMIT}.tar.gz"

git archive --format=tar.gz --output="${BASE_ARCHIVE}" HEAD
cp --reflink=auto \
  backends/tfhe-hpu-backend/config_store/v80_archives/psi64.hpu \
  target/psi64.hpu

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

### 2.2 在 132 复制到 129

```bash
cd /home/yangchenghui/hpu/tfhe-rs

BASE_ARCHIVE=$(awk '$2 ~ /^tfhe-rs-base-/ {print "target/" $2}' \
  target/lwe_remote_hpu_bootstrap_SHA256SUMS)

scp -P 2222 \
  -i /home/yangchenghui/id_rsa_ych_128 \
  "${BASE_ARCHIVE}" \
  target/lwe_remote_hpu_server_129.tar.gz \
  target/psi64.hpu \
  target/lwe_remote_hpu_bootstrap_SHA256SUMS \
  yangchenghui@10.16.0.129:/tmp/
```

### 2.3 在 129 建立源码树并校验

```bash
cd /tmp
sha256sum -c lwe_remote_hpu_bootstrap_SHA256SUMS

BASE_ARCHIVE=$(awk '$2 ~ /^tfhe-rs-base-/ {print $2}' \
  lwe_remote_hpu_bootstrap_SHA256SUMS)

mkdir -p /home/yangchenghui/hpu/tfhe-rs
tar -xzf "/tmp/${BASE_ARCHIVE}" \
  -C /home/yangchenghui/hpu/tfhe-rs
tar -xzf /tmp/lwe_remote_hpu_server_129.tar.gz \
  -C /home/yangchenghui/hpu/tfhe-rs

install -m 0644 /tmp/psi64.hpu \
  /home/yangchenghui/hpu/tfhe-rs/backends/tfhe-hpu-backend/config_store/v80_archives/psi64.hpu

cd /home/yangchenghui/hpu/tfhe-rs
sha256sum -c SHA256SUMS
```

## 3. 旧用户：129 已经跑通过

如果旧服务仍在运行，先在 129 的服务终端按 `Ctrl+C`，再更新部署文件。

### 3.1 在 132 重新生成并复制覆盖包

```bash
cd /home/yangchenghui/hpu/tfhe-rs

./scripts/lwe_remote_hpu/package_server_129.sh

scp -P 2222 \
  -i /home/yangchenghui/id_rsa_ych_128 \
  target/lwe_remote_hpu_server_129.tar.gz \
  yangchenghui@10.16.0.129:/tmp/
```

### 3.2 在 129 更新覆盖文件

```bash
cd /home/yangchenghui/hpu/tfhe-rs
tar -xzf /tmp/lwe_remote_hpu_server_129.tar.gz
sha256sum -c SHA256SUMS
```

旧用户不需要重新复制基础源码和 `psi64.hpu`。除非 132 明确更新了 HPU 归档，否则不要
覆盖 129 上已经验证可用的 `psi64.hpu`。

## 4. 两类用户共同执行：在 129 启动服务

建议在 129 的 `tmux` 中运行：

```bash
tmux new -s hpu-lwe-server

cd /home/yangchenghui/hpu/tfhe-rs
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

按 `Ctrl+B`、再按 `D` 可退出 tmux 视图而不停止服务。

## 5. 两类用户共同执行：在 132 测试

### 5.1 构建客户端

```bash
cd /home/yangchenghui/hpu/tfhe-rs

cargo build --release \
  --no-default-features \
  --features integer \
  --example hpu_lwe_remote_client
```

### 5.2 测试 1B

```bash
./target/release/examples/hpu_lwe_remote_client \
  --server 10.16.0.129:19090 \
  --ciphertext-dump /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/lwe_encrypt_fpga_ciphertexts.bin \
  --client-key /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_shortint_ks32_client_key.bincode \
  --scalar 1 \
  --output /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_1b.bin
```

### 5.3 测试 128B

```bash
./target/release/examples/hpu_lwe_remote_client \
  --server 10.16.0.129:19090 \
  --ciphertext-dump /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/lwe_encrypt_fpga_ciphertexts_128b.bin \
  --client-key /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_shortint_ks32_client_key.bincode \
  --scalar 1 \
  --output /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_128b.bin
```

成功标志：

```text
local_client_key_decrypt_checked=yes
remote_hpu_ciphertext_compute=passed
```

## 6. 停止服务

回到 129 的服务 tmux：

```bash
tmux attach -t hpu-lwe-server
```

按 `Ctrl+C` 停止服务，然后确认端口释放：

```bash
ss -ltnp | grep 19090
```

没有输出即表示服务已关闭。

## 7. 单程序内存直传模式

前面的 Rust 客户端流程保留为回归基线。新的融合应用位于：

```text
/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-remote-offload
```

它在一个 C++ 进程中完成 SSD 读取、SLM 管理、FPGA 加密、物理布局解包、TCP 发送、
129 HPU 计算、结果接收和最终落盘。FPGA 中间密文只存在于 output SLM 与 132 Host
内存，不生成 `lwe_encrypt_fpga_ciphertexts*.bin` 中间文件。

在 132 构建：

```bash
cd /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-remote-offload
make -j4
make test
```

先按第 4 节启动 129 服务，再进入 132 QEMU：

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

旧的 `vscode-lwe-encrypt-offload` 和 Rust `hpu_lwe_remote_client` 均保留，可继续用两段式
落盘流程做故障定位和结果对比。
