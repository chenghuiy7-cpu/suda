# LWE 密文远程 HPU 计算通路

## 1. 目标与边界

当前原型链路为：

```text
CSD SSD
  -> SUDA input SLM
  -> FPGA lwe_encrypt
  -> Host 中的 LWEHLS01 密文
  -> LWERPC01/TCP
  -> 10.16.0.129 上的真实 V80 HPU
  -> 密文 ADDS
  -> TCP 返回结果密文
  -> 本机 ClientKey 验证或 FPGA 解密
```

本机保留 `ClientKey` 和 FPGA 加密使用的 Big-LWE 私钥；远端只部署由同一
`ClientKey` 派生的整数 `CompressedServerKey`。网络不传 ClientKey、Big-LWE 私钥
或明文参考。

## 2. 实现与仓库边界

远端 HPU 的 SUDA 自研代码位于：

- `hpu/remote-hpu/src/main.rs`：加载 ServerKey、初始化 V80、监听 TCP 并执行 HPU
  `ADDS`；
- `hpu/remote-hpu/src/protocol.rs`：定义帧、操作码、metadata、payload 上限和远端
  计时尾帧；
- `hpu/remote-hpu/src/bridge.rs`：在协议的 LWE/HPU-native words 与 TFHE radix
  ciphertext 之间转换；
- `hpu/scripts/`：子模块准备、边界校验、打包和启动；
- `host/applications/vscode-lwe-encrypt-remote-offload`：FPGA 加密后直接调用远端
  HPU；
- `host/applications/vscode-lwe-full-pipeline`：完整闭环。

`hpu/tfhe-rs` 是未修改的 `https://github.com/zama-ai/tfhe-rs.git` Git 子模块。
`.gitmodules` 声明上游 `main`，gitlink 固定在实机验证过的
`e8ab4484545a9f6512f42d2b75509855093e8597`。SUDA 独立 crate 通过 path dependency
使用其公开 API，不向子模块写入代码。

桥接流程使用上游公开转换：协议 native words 先构造成 `HpuLweCiphertextOwned`，
转换为普通 `LweCiphertextOwned`/`RadixCiphertext`，再调用
`HpuRadixCiphertext::from_radix_ciphertext`。响应方向执行逆转换。因此不再需要给
TFHE-rs 添加本地导入/导出方法，也不再存在 overlay。

## 3. 协议与数据量

TCP 帧使用小端定长头和变长 payload：

- magic `LWERPC01`、协议版本和 frame kind；
- request ID、操作码与 scalar；
- Big-LWE dimension、u8 数量和 radix block 数；
- message/carry/padding width 与 `delta_log2`；
- ciphertext word count 和 payload byte count；
- 连续 `u64` LWE 系数。

服务端默认最大请求为 512 MiB，可通过 `--max-request-bytes` 调整。客户端也限制最大
响应，避免错误头触发过大分配。

128 个 u8、每个 u8 4 个 radix block、每个 block 为 2048 维 Big-LWE 时：

```text
128 * 4 * (2048 + 1) * 8 = 8,392,704B
```

一次操作约发送 8.39 MB 密文并接收同等大小的结果。

## 4. 密钥与制品

已验证的压缩 ServerKey：

```text
文件：psi64_integer_compressed_server_key.bincode
大小：28,868,804B
SHA-256：09f605c234ccc85425dd548796e3eeb6cb1cfd6da26fe5b9c327835c9c423e18
```

该文件属于外部私有制品，不进入 Git；元数据记录在
`hpu/manifests/remote-hpu.env`。重新生成 keyset 时，必须同时更新 FPGA 私钥、
ClientKey、ServerKey 和 manifest。

真实 `psi64.hpu` 同样由实验室制品库管理。TFHE-rs 子模块保持干净，运行时可以让
`HPU_BACKEND_DIR` 指向含已验证固件的安装树。

## 5. 构建、部署与启动策略

```bash
git submodule update --init hpu/tfhe-rs
bash hpu/scripts/verify_remote_hpu.sh
cargo +1.91.1 test --manifest-path hpu/remote-hpu/Cargo.toml
cargo +1.91.1 build --release --manifest-path hpu/remote-hpu/Cargo.toml
```

部署包由 SUDA 根目录生成：

```bash
export SERVER_KEY_SOURCE=/secure/path/psi64_integer_compressed_server_key.bincode
source hpu/manifests/remote-hpu.env
export HPU_REMOTE_SERVER_KEY_SHA256="$HPU_SERVER_KEY_SHA256"
bash hpu/scripts/package_remote_server.sh
```

默认产物是 `hpu/artifacts/suda-remote-hpu-server.tar.gz`，包含 release 二进制、启动
脚本、配置模板、manifest、ServerKey 和 `SHA256SUMS`，不包含 ClientKey、Big-LWE
私钥或真实 HPU 固件。详细命令见
`docs/user-guides/LWE远程HPU部署与运行命令.md`。

验证基线的上游 V80 backend 使用 `force_reload="false"`。当前硬件状态有效时直接
复用；状态无效时可能执行恢复性 reload。旧版 SUDA overlay 自行加入的
`force_reload="never"` 已移除，以保证子模块不被修改。启动网络服务前必须由管理员
确认 V80、AMI/QDMA、PDI/HIS/UUID 状态，并接受这一上游策略。

服务启动成功标志：

```text
hardware_reload_policy=upstream-validate-and-recover
hpu_device_ready=yes
client_key_loaded=no
listen_addr=0.0.0.0:19090
```

## 6. 已完成的实机验证

1B 测试：

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

128B 批量测试：

```text
发送密文：8,392,704B
返回密文：8,392,704B
RPC 往返：1,350.662ms
decrypted_count=128
local_client_key_decrypt_checked=yes
remote_hpu_ciphertext_compute=passed
```

这些结果验证了 FPGA 密文参数、radix/LWE 结构、TCP 序列化、HPU 格式转换、真实
HPU 同态计算及返回解密的一致性。代码迁移到独立 crate 后，桥接和协议单元测试也在
干净子模块上通过；再次部署到真实 V80 后仍需执行回归验证。

## 7. 当前限制

- 当前是实验网 TCP 原型，没有认证、加密和防重放；正式部署应增加 mTLS、服务端
  身份认证和请求签名。
- 服务端当前串行处理连接和每个 u8 密文，性能阶段可加入批处理、连接复用和流水并发。
- 当前只实现 `ADDS`。密文加密文需要协议携带第二个密文批次并增加操作分支。
- 上游 `force_reload=false` 可能恢复性重载硬件；严格 no-reload 不能在不修改上游的
  前提下由当前版本保证。
