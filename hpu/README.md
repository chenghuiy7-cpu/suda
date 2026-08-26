# SUDA 远端 HPU 服务端覆盖层

本目录只保存 SUDA 访问远端 V80 HPU 所需的自研源码，不保存完整的
TFHE-rs、HPU FPGA、AVED 或 QDMA 仓库。SUDA 侧的 `LWERPC01` C++ 客户端已经位于：

```text
host/applications/vscode-lwe-encrypt-remote-offload/
host/applications/vscode-lwe-full-pipeline/
```

因此这里仅纳入远端 Rust 服务端、协议和密文布局桥接、V80 no-reload
保护，以及部署脚本。所有文件都是 SUDA 仓库中的普通文件；提交时只需要在
`suda/` 仓库执行一次 `git push`。

## 目录结构

```text
hpu/
├── config/hpu-server.env.example
├── manifests/remote-hpu.env
├── overlays/tfhe-rs/          # 相对于固定 TFHE-rs 基线的源码覆盖层
├── scripts/apply_tfhe_rs_overlay.sh
├── scripts/bootstrap_tfhe_rs.sh
└── scripts/verify_remote_hpu.sh
```

`overlays/tfhe-rs/` 不是独立 Cargo workspace。它必须覆盖到
`manifests/remote-hpu.env` 固定的 TFHE-rs revision 后再编译。推荐把可再生成的完整
工作树放在 `hpu/worktree/tfhe-rs/`；该目录被 SUDA 的 `.gitignore` 排除。

## 准备本地工作树

在 SUDA 根目录执行：

```bash
bash hpu/scripts/bootstrap_tfhe_rs.sh
```

默认从上游 clone 固定 revision 到 `hpu/worktree/tfhe-rs` 并应用覆盖层。也可以复用
本地镜像，避免重新下载 Git/LFS 历史：

```bash
TFHE_RS_SOURCE=/path/to/tfhe-rs \
  bash hpu/scripts/bootstrap_tfhe_rs.sh
```

若已经有一份干净且 revision 正确的 TFHE-rs checkout，可直接执行：

```bash
bash hpu/scripts/apply_tfhe_rs_overlay.sh /path/to/tfhe-rs
```

覆盖层更新后，允许刷新一份只包含旧覆盖层改动的工作树：

```bash
bash hpu/scripts/apply_tfhe_rs_overlay.sh --refresh hpu/worktree/tfhe-rs
```

脚本会拒绝错误 revision、已暂存改动以及覆盖层之外的本地改动。

## 构建与打包

```bash
export TFHE_RS_ROOT="$PWD/hpu/worktree/tfhe-rs"
cd "$TFHE_RS_ROOT"

cargo build --release --features hpu-v80 \
  --example hpu_lwe_remote_server
```

打包前必须显式提供私有 ServerKey 路径和校验值：

```bash
export SERVER_KEY_SOURCE=/secure/path/psi64_integer_compressed_server_key.bincode
export HPU_REMOTE_SERVER_KEY_SHA256=<sha256>

./scripts/lwe_remote_hpu/package_server_129.sh
```

启动 129 服务前，复制并填写环境模板；真实板卡序列号和本机路径不得提交：

```bash
cp /path/to/suda/hpu/config/hpu-server.env.example /secure/path/hpu-server.env
source /secure/path/hpu-server.env
./scripts/lwe_remote_hpu/start_server_129.sh
```

## 不进入 Git 的内容

- `psi64.hpu`、PDI/XSA/DCP 和 Vivado 输出；
- ClientKey、CompressedServerKey、Big-LWE 私钥及密文 dump；
- TFHE-rs 完整工作树、Cargo `target/` 和部署 tar 包；
- AMI/QDMA 构建产物、板卡序列号、SSH 私钥和机器专用 `.env`。

当前验证过的必需二进制资产及 SHA-256 记录在
`manifests/remote-hpu.env`，文件本身继续由实验室制品存储管理。

提交前执行：

```bash
bash hpu/scripts/verify_remote_hpu.sh
git add -- hpu/ .gitignore
git diff --cached --stat
```
