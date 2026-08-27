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
export CARGO_TARGET_DIR=/data/$USER/cargo-targets/suda-remote-hpu
bash hpu/scripts/package_remote_server.sh
```

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
