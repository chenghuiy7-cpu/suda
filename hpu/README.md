# SUDA 远端 HPU 服务

本目录把 SUDA 的远端 HPU 功能与上游 TFHE-rs 源码明确分开：

- `tfhe-rs/` 是 `https://github.com/zama-ai/tfhe-rs.git` 的 Git 子模块；
- 子模块工作树必须保持干净，不在其中添加或修改 SUDA 代码；
- TCP 服务、协议、HPU-native 布局桥接和部署脚本全部由 SUDA 在本目录维护。

SUDA 的 C++ 客户端仍位于：

```text
host/applications/vscode-lwe-encrypt-remote-offload/
host/applications/vscode-lwe-full-pipeline/
```

## 目录结构

```text
hpu/
├── tfhe-rs/                    # 未修改的上游 Git 子模块
├── keygen/                     # SUDA 独立 psi64 匹配密钥生成器
├── remote-hpu/                 # SUDA 独立 Rust 服务端 crate
│   ├── Cargo.toml
│   └── src/{main,protocol,bridge}.rs
├── config/hpu-server.env.example
├── manifests/remote-hpu.env
└── scripts/
    ├── prepare_remote_runtime.sh
    ├── prepare_tfhe_rs_submodule.sh
    ├── generate_psi64_keyset.sh
    ├── install_psi64_keyset.sh
    ├── package_remote_server.sh
    ├── start_remote_server.sh
    ├── v80-pcie-perms.sh
    └── verify_remote_hpu.sh
```

## 子模块版本

Git 子模块总是记录一个确定提交，而不是随 `main` 自动漂移。`.gitmodules`
声明跟踪上游 `main`，当前 gitlink 固定在已经完成实机验证的：

```text
e8ab4484545a9f6512f42d2b75509855093e8597
```

上游 `main` 的 HPU backend、固件和配置仍在快速演进。更新子模块 SHA 必须作为
独立升级进行，并重新验证 V80 bitstream、AMI/QDMA、ServerKey 和完整流水线。

首次 clone 使用：

```bash
git clone --recurse-submodules https://github.com/chenghuiy7-cpu/suda.git
```

已有 checkout 使用：

```bash
bash hpu/scripts/prepare_tfhe_rs_submodule.sh
```

## 独立构建

远端服务通过 path dependency 使用子模块公开 API，不会向 TFHE-rs 写入文件：

```bash
cargo +1.91.1 build --release \
  --manifest-path hpu/remote-hpu/Cargo.toml
```

HPU-native 请求先在 `remote-hpu/src/bridge.rs` 中通过上游公开转换 API 变成
普通 TFHE radix ciphertext，再交给 `HpuRadixCiphertext::from_radix_ciphertext`。
返回 native 布局时执行逆转换。因此不再需要给上游类型添加
`from_hpu_lwe_ciphertexts` 或 `to_hpu_lwe_ciphertexts` 方法。

匹配密钥也由 SUDA 独立 crate 生成，不再向 TFHE-rs 子模块添加 example：

```bash
bash hpu/scripts/generate_psi64_keyset.sh
bash hpu/scripts/install_psi64_keyset.sh
```

默认生成目录为 `hpu/keys/psi64`，该目录被 Git 忽略。安装脚本把同一套 keyset 同步到
FPGA Host 应用使用的 `device/operators/hls/lwe_encrypt/testdata`，避免手工复制后混用。

## 批量 u8 RPC 协议

Rust 服务端与两个 Host 应用共享同一套 `LWERPC01` wire contract。C++ 端定义位于
`host/applications/vscode-lwe-encrypt-remote-offload/lwe_remote_protocol.hpp`，完整回环测试
位于同目录的 `test_protocol.cpp`。

原有 operation 0–3 保持兼容：

- `0`：echo；
- `1`：CPU-LWE 输入的 `encrypted_u8 + scalar_u8`；
- `2`：HPU-native 输入、CPU-LWE 输出的同一标量加法；
- `3`：HPU-native 输入和输出的同一标量加法。

新增 operation 包括：

- 密文—密文算术：`ADD/SUB/MUL/DIV/REM`（`0x100..0x104`）；
- 位运算：`AND/OR/XOR/NOT`（`0x110..0x113`）；
- 密文移位/旋转：`SHL/SHR/ROTL/ROTR`（`0x120..0x123`）；
- 比较：`EQ/NE/LT/LE/GT/GE`（`0x130..0x135`）；
- 标量运算：`SUB/RSUB/MUL/DIV/REM` 和标量移位/旋转
  （`0x200..0x213`；标量加法继续使用 `1`）。

新 operation 可按位组合 `0x4000000000000000`（HPU-native 输入）与
`0x2000000000000000`（HPU-native 输出）。双密文请求的 payload 固定为
`lhs_batch || rhs_batch`；header metadata 描述单个操作数批次，因此
`ciphertext_word_count` 不包含 RHS。两个批次的 item 数和形状必须一致，服务端逐项执行
`lhs[i] op rhs[i]`。普通运算返回与输入同样的 4 个 radix block；比较每项返回 1 个
Boolean block。

服务端和两个 C++ Host 应用的默认载荷/响应上限均为 4 GiB，I/O 超时为 3600 秒。服务端
逐项解码、执行和编码，不让整个批次的中间 TFHE/HPU 对象常驻内存；wire 请求和最终响应
缓冲区仍需由主机内存容纳。可以分别通过
`HPU_REMOTE_MAX_REQUEST_BYTES`、`HPU_REMOTE_IO_TIMEOUT_SECS` 和客户端命令行参数覆盖。

## 129 部署与启动

部署包中的启动脚本会显式向服务进程传递上面的请求上限与超时。目标目录为
`$HOME/suda-remote-hpu` 时，129 上的启动命令为：

```bash
source $HOME/.config/suda/hpu-server.env
cd $HOME/suda-remote-hpu
./scripts/start_remote_server.sh
```

启动前可用 `HPU_REMOTE_PREFLIGHT_ONLY=1 ./scripts/start_remote_server.sh` 检查二进制、
HPU archive、ServerKey、Vivado、AMI 与 V80 环境。环境文件至少应包含：

```bash
export SUDA_HPU_ROOT=$HOME/suda-remote-hpu
export HPU_REMOTE_BIND=0.0.0.0:19090
export HPU_REMOTE_MAX_REQUEST_BYTES=4294967296
export HPU_REMOTE_IO_TIMEOUT_SECS=3600
```

## V80 启动边界

验证基线的上游 TFHE-rs 使用 `force_reload="false"`：当前硬件状态有效时直接复用；
状态无效时，上游 backend 可能执行恢复性 reload。旧 overlay 中 SUDA 自行加入的
`force_reload="never"` 已删除，因为它会修改上游源码。启动前应先确认 V80、AMI、
QDMA 和真实 `psi64.hpu` 状态，脚本也会明确打印这一风险。

真实 `psi64.hpu` 是实验室制品，不应覆盖或提交到子模块。132 上由
`package_remote_server.sh` 把校验通过的真实制品、ServerKey、上游配置和已编译
服务端收入最小运行包。129 不需要 SUDA 或 TFHE-rs 源码树。

## 运行与打包

在 132 的完整 SUDA checkout 中准备私有制品：

```bash
mkdir -p hpu/artifacts/private
# 将实验室发放的 psi64.hpu 放入上面的目录；ServerKey 从 hpu/keys/psi64 读取。
bash hpu/scripts/verify_remote_hpu.sh
```

在 132 构建自包含运行包：

```bash
bash hpu/scripts/package_remote_server.sh
```

不设置 `CARGO_TARGET_DIR` 时，Cargo 构建产物保存在当前 SUDA checkout 内的
`hpu/remote-hpu/target/`；生成的部署包保存在 `hpu/artifacts/`。新用户不需要在
`/data` 或仓库外创建任何构建目录。

将 `hpu/artifacts/suda-remote-hpu-server.tar.gz` 用 `scp` 发到 129。129 只需解压、填写
`config/hpu-server-bundle.env.example` 中的机器参数，然后运行：

```bash
source "$HOME/.config/suda/hpu-server.env"
HPU_REMOTE_PREFLIGHT_ONLY=1 \
  "$SUDA_HPU_ROOT/scripts/start_remote_server.sh"
"$SUDA_HPU_ROOT/scripts/start_remote_server.sh"
```

完整命令见 `docs/user-guides/LWE远程HPU部署与运行命令.md`。

## 不进入 Git 的内容

- `psi64.hpu`、PDI/XSA/DCP、Vivado 输出和部署 tar 包；
- ClientKey、CompressedServerKey、Big-LWE 私钥及密文 dump；
- Cargo `target/`、AMI/QDMA 构建产物、板卡序列号和机器专用 `.env`。

提交前执行：

```bash
bash hpu/scripts/verify_remote_hpu.sh
git diff --submodule=short
```
