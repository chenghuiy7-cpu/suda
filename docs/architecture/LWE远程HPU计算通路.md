# LWE 密文远程 HPU 计算通路

## 1. 目标与边界

当前原型把已经验证的本地链路扩展为：

```text
CSD SSD
  -> SUDA input SLM
  -> FPGA lwe_encrypt
  -> Host 上的 LWEHLS01 密文文件
  -> TCP
  -> 10.16.0.129 上的真实 HPU
  -> 同态 ADDS
  -> TCP 返回结果密文
  -> 本机 ClientKey 解密验证
```

首个远端操作为 `u8` 密文加明文标量，即 HPU 的 `ADDS`。协议中保留了操作码，
后续可继续加入密文加、减、乘等操作。

密钥边界如下：

- 本机保留 `ClientKey` 和 FPGA `lwe_encrypt` 使用的 Big-LWE 私钥。
- 远端只部署由同一 `ClientKey` 派生的整数 `CompressedServerKey`。
- 网络请求不携带 ClientKey、Big-LWE 私钥，也不携带 `LWEHLS01` 中用于测试校验的
  明文字节。
- 远端返回标准 radix/LWE 密文系数，本机再使用 ClientKey 解密。

## 2. 实现文件与源码边界

远端 HPU 自研代码现在由 SUDA 仓库统一管理。TFHE-rs 服务端以最小覆盖层保存于
`hpu/overlays/tfhe-rs`，并应用到 manifest 固定的完整 TFHE-rs 基仓后编译：

- `hpu/overlays/tfhe-rs/tfhe/examples/hpu/lwe_remote_server.rs`
  - 加载真实 HPU 和 `CompressedServerKey`；
  - 监听 TCP 请求；
  - 重建 `RadixCiphertext`；
  - 转为 `HpuRadixCiphertext` 并执行 `ADDS`；
  - 将结果转回标准 radix/LWE 系数并返回。
- `hpu/overlays/tfhe-rs/tfhe/examples/hpu/lwe_remote/protocol.rs`
  - 定义 `LWERPC01`、版本、request ID、操作码、密文形状和 payload 长度；
  - 对请求/响应大小进行上限检查；
  - 支持远端错误帧。
- `hpu/overlays/tfhe-rs/tfhe/examples/hpu/lwe_remote/bridge.rs`
  - 在 LWE 系数数组和 tfhe-rs `RadixCiphertext` 之间转换。
- `hpu/overlays/tfhe-rs/tfhe/src/integer/hpu/ciphertext/mod.rs`
  - 增加 HPU-native 密文的无损导入和导出。
- `hpu/overlays/tfhe-rs/backends/tfhe-hpu-backend/src/ffi/v80/mod.rs`
  - 实现远端常驻服务要求的 `force_reload="never"`。
- `host/applications/vscode-lwe-encrypt-remote-offload`
  - 在单个 C++ Host 进程中运行 SSD→SLM→FPGA 加密；
  - 在内存中将 64B 对齐物理输出转换为连续逻辑 LWE 系数；
  - 释放 CSD 资源后直接通过 `LWERPC01` 发送到 129；
  - 接收远端 HPU 结果并只将最终 `LWEHLS01` 文件落盘。

完整 TFHE-rs 工作树、HPU 固件归档和密钥均是可再生成或外部制品，不进入 SUDA Git。

## 3. 当前协议

TCP 帧使用小端定长头和变长 payload：

- magic：`LWERPC01`
- version：`1`
- frame kind：request、response 或 error
- request ID
- operation：当前 `1` 表示 `u8 ADDS`
- scalar
- Big-LWE mask dimension
- u8 密文数量
- 每个 u8 的 radix block 数
- message/carry/padding width 和 `delta_log2`
- ciphertext word count
- payload byte count
- 连续的 `u64` LWE 系数

服务端默认最大请求为 512MiB，可通过 `--max-request-bytes` 调整。客户端也独立限制
最大响应，避免错误头部触发过大的内存分配。

当前 128B 明文对应 128 个 u8，每个 u8 有 4 个 radix block，每个 block 是一个
2048 维 Big-LWE：

```text
128 * 4 * (2048 + 1) * 8 = 8,392,704B
```

同一次操作需要发送约 8.39MB 密文，并接收约 8.39MB 结果密文。

## 4. 密钥准备

已从当前通过 FPGA 测试的 ClientKey 派生：

```text
文件：psi64_integer_compressed_server_key.bincode
大小：28,868,804B
SHA-256：09f605c234ccc85425dd548796e3eeb6cb1cfd6da26fe5b9c327835c9c423e18
```

该文件属于外部私有制品，不进入 Git。大小和 SHA-256 记录在
`hpu/manifests/remote-hpu.env`。若重新生成整套 keyset，必须同时更新 FPGA 私钥、
ClientKey、ServerKey 和 manifest；不能只替换其中一份，否则密钥会失配。

## 5. 部署到 10.16.0.129

### 5.1 代码归属

SUDA 仓库是本项目唯一的源码基准：

- HLS 算子、Host 客户端和远程服务端覆盖层均在同一 SUDA Git 仓库；
- ClientKey、Big-LWE 私钥以及明文验证数据只保留在 132；
- 129 是 HPU 服务部署机器，不在 129 上继续手工修改服务源码；
- 每次更新服务端后，由 132 重新生成部署包并覆盖部署到 129；
- 129 只保存 `CompressedServerKey`，它只能用于同态计算，不能用于解密。

当前推荐数据通路由 SUDA C++ 应用直接完成 FPGA 加密和远端 HPU RPC，不再依赖
TFHE-rs 中的 Rust 诊断客户端。FPGA 加密与 TCP/HPU 计算在实现上仍是两个独立阶段。

### 5.2 在 132 生成部署包

部署包只包含服务端需要的源码覆盖文件、安全的 V80 no-reload 后端改动、专用配置、
启动脚本和 `CompressedServerKey`。它不会包含 ClientKey 或 FPGA Big-LWE 私钥：

```bash
export SUDA_ROOT=/path/to/suda
export TFHE_RS_ROOT="$SUDA_ROOT/hpu/worktree/tfhe-rs"
export SERVER_KEY_SOURCE=/secure/path/psi64_integer_compressed_server_key.bincode
source "$SUDA_ROOT/hpu/manifests/remote-hpu.env"
export HPU_REMOTE_SERVER_KEY_SHA256="$HPU_SERVER_KEY_SHA256"

cd "$TFHE_RS_ROOT"
./scripts/lwe_remote_hpu/package_server_129.sh
```

默认产物为：

```text
$TFHE_RS_ROOT/target/lwe_remote_hpu_server_129.tar.gz
```

脚本会验证 ServerKey 的 SHA-256，生成包内 `SHA256SUMS`，并打印部署包本身的
SHA-256。这样 129 上运行的一定是 132 当前整理出的版本，而不是某次远端临时修改。

### 5.3 从 132 复制到 129

129 的 SSH 端口为 `2222`。132 可以直接访问 129 时执行：

```bash
scp -P "$HPU_REMOTE_SSH_PORT" -i "$SSH_KEY" \
  "$TFHE_RS_ROOT/target/lwe_remote_hpu_server_129.tar.gz" \
  "${HPU_REMOTE_USER}@${HPU_REMOTE_HOST}:/tmp/"
```

也可以由 PC 分别登录两台机器，把同一个 tar 包从 132 下载后再上传到 129；PC 只承担
部署控制，不进入运行时数据通路。

### 5.4 在 129 校验、编译并启动

下面假设 129 已有同版本基础仓库。在 129 显式设置其路径后执行：

```bash
export TFHE_RS_ROOT=/path/to/remote/tfhe-rs
cd "$TFHE_RS_ROOT"
tar -xzf /tmp/lwe_remote_hpu_server_129.tar.gz
sha256sum -c SHA256SUMS

grep -n 'require_reload_disabled' \
  tfhe/examples/hpu/lwe_remote_server.rs
grep -n 'force == "never"' \
  backends/tfhe-hpu-backend/src/ffi/v80/mod.rs
```

启动脚本从仓库外的环境文件读取 129 的机器专用配置：

```bash
source /secure/path/hpu-server.env
cd "$TFHE_RS_ROOT"
./scripts/lwe_remote_hpu/start_server_129.sh \
  2>&1 | tee hpu_lwe_remote_server.log
```

脚本内部固定执行以下安全检查后才会启动服务：

- `hpu_config_remote_no_reload.toml` 必须包含 `force_reload="never"`；
- 后端必须实现 `force == "never"` 分支；
- 服务端必须调用 `require_reload_disabled`；
- ServerKey SHA-256 必须与 132 上派生的文件一致；
- 编译出的二进制必须包含禁止 fresh reload 的错误字符串；
- 最终通过 `sudo -E` 保留 HPU 环境并访问设备。

环境文件需要提供以下变量，真实值不得提交：

```bash
HPU_BACKEND_DIR="$TFHE_RS_ROOT/backends/tfhe-hpu-backend"
XILINX_VIVADO=/path/to/Vivado
HPU_CONFIG=v80
V80_PCIE_DEV=xx
V80_SERIAL_NUMBER=REPLACE_ME
AMI_PATH=/path/to/installed/ami-driver
RUST_LOG=info
```

多卡机器不能随意选择序列号，必须使用与 `V80_PCIE_DEV` 对应的 PF0
`0000:${V80_PCIE_DEV}:00.0`。如以后更换 HPU 卡或 PCIe BDF，应在 129 启动前显式覆盖
对应环境变量，而不是修改 129 上的脚本副本。

注意：V80 配置中的 `force_reload="false"` 不是“禁止重载”，而是“优先复用，检查失败
后自动重载”。自动重载会卸载 AMI/QDMA、移除 PCIe PF、通过 JTAG 重编程并触发 PCIe
rescan。远程常驻服务必须使用 `force_reload="never"`；如果当前固件版本或 UUID 不匹配，
服务只允许报错退出，硬件重载应由管理员在维护窗口单独执行。

129 上已验证过的 HPU 示例使用 `sudo -E`，说明 AMI/QDMA 设备访问依赖 root 权限，同时
必须保留 `HPU_BACKEND_DIR`、`HPU_CONFIG`、`V80_PCIE_DEV`、`V80_SERIAL_NUMBER`、
`XILINX_VIVADO` 和 `AMI_PATH`。远程服务沿用 `sudo -E`，但与旧示例不同的是强制
no-reload。若 no-reload 报告当前 PDI/HIS/UUID 不匹配，应先停止网络服务，再由管理员
通过 BMC/物理控制台在维护窗口运行原有硬件装载流程；装载成功后网络服务只负责复用。

另开远端终端确认监听：

```bash
ss -ltnp | grep 19090
```

建议在 `tmux` 中常驻服务，并通过防火墙只允许实验网中的本机访问 19090 端口。

## 6. 本机发送与验证

当前受支持的客户端位于 SUDA，不再从 TFHE-rs 覆盖层携带 Rust 诊断客户端：

```bash
export SUDA_ROOT=/path/to/suda
cd "$SUDA_ROOT/host/applications/vscode-lwe-encrypt-remote-offload"
make -j4
make test
```

运行命令和完整闭环入口见
`docs/user-guides/LWE远程HPU部署与运行命令.md`。成功标志包括：

```text
host_big_lwe_key_decrypt_checked=yes
remote_hpu_ciphertext_compute=passed
```

### 6.1 已完成的 1B 实机结果

2026-07-21 已完成 132 到 129 真实 V80 HPU 的首次端到端测试：

```text
输入参考：0x3b（59）
远端操作：ADDS +1
返回密文解密：0x3c（60）
请求/响应密文：各 65,568B
RPC 往返：23.023ms
clear_reference_transmitted=no
local_client_key_decrypt_checked=yes
remote_hpu_ciphertext_compute=passed
```

这说明 FPGA 输出密文的参数、radix/LWE 结构、TCP 序列化、HPU 格式转换、真实 HPU
同态计算以及返回后的 ClientKey 解密均已对齐。

### 6.2 已完成的 128B 实机结果

同一服务进程随后完成 128 个 u8 的批量 `ADDS +1`：

```text
发送密文：8,392,704B
返回密文：8,392,704B
RPC 往返：1,350.662ms
decrypted_count=128
decrypted_prefix=aca548f0b93f53e57b92aa9a9a0be541
local_client_key_decrypt_checked=yes
remote_hpu_ciphertext_compute=passed
```

返回前缀与输入前缀逐字节加 1 一致，128 个 u8 均通过 ClientKey 自动解密校验。
因此 1B 正确性测试和约 8.39MB 的 128B 批量数据测试均已打通。

## 7. 当前限制

- 当前是实验网 TCP 原型，没有认证、加密和防重放；密文本身不暴露明文，但控制命令
  和流量特征仍可被观察或篡改。正式部署应增加 mTLS、服务端身份认证和请求签名。
- 服务端当前串行处理连接和每个 u8 密文，先保证正确性；性能阶段再加入批处理、
  连接复用和流水并发。
- 返回的测试 `LWEHLS01` 文件仍保存本机计算出的预期明文字节，便于自动解密校验。
  这些参考值不会经过网络；生产格式应把明文参考从密文文件中完全移除。
- 当前只实现 `ADDS`。密文加密文需要协议携带第二个密文批次，并在 HPU 服务端增加
  对应操作分支。
