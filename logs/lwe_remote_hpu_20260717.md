# 2026-07-17：LWE 密文远程真实 HPU 计算通路

## 本次目标

将 CSD FPGA `lwe_encrypt` 生成的 radix/Big-LWE 密文发送到
`10.16.0.129`，在远端真实 HPU 上执行同态计算，再把结果密文返回本机并使用
ClientKey 解密验证。

## 已完成代码

- 新增本机客户端：`tfhe/examples/hpu/lwe_remote_client.rs`。
- 新增远端 HPU 服务端：`tfhe/examples/hpu/lwe_remote_server.rs`。
- 新增 `LWERPC01` TCP 协议、`LWEHLS01` 读写和 radix/LWE 转换模块。
- 首个远端操作固定为 `u8 ADDS`，协议保留操作码便于扩展。
- 请求只发送密文参数和系数，不发送 `LWEHLS01` 中的明文参考。
- ClientKey 和 Big-LWE 私钥保留在本机，远端只使用 `CompressedServerKey`。
- 新增 `hpu_export_lwe_server_key`，从现有 ClientKey 派生 ServerKey，不重新生成或
  覆盖现有密钥。

## 密钥结果

从当前已通过 FPGA 解密测试的 ClientKey 派生：

```text
ClientKey SHA-256:
582947a24c185d9b90f52cb12bec9f608b66dcf9a0a47e8c347c6dc9e0184c76

Big-LWE 私钥 SHA-256:
b5f4d159d1b14870a5b3e45da870c1dd281175485a4f649d5405c76c30fd47e9

CompressedServerKey:
psi64_integer_compressed_server_key.bincode
大小：28,868,804B
SHA-256:
09f605c234ccc85425dd548796e3eeb6cb1cfd6da26fe5b9c327835c9c423e18
```

派生过程明确输出 `source_client_key_modified=no`。

## 已完成验证

1. `hpu_lwe_remote_client` 使用 `integer` 特性、关闭默认特性后编译通过，证明本机
   客户端不依赖 V80 驱动。
2. `hpu_lwe_remote_server` 和密钥导出工具使用 `hpu` 特性编译通过。
3. `hpu_lwe_remote_server` 使用 `hpu-v80` 特性编译检查通过。
4. TCP 协议单元测试 3/3 通过：
   - 密文请求帧序列化/反序列化；
   - 远端错误帧；
   - payload 上限在内存分配前生效。
5. 对现有 128B FPGA 密文执行客户端 `--dry-run`：

```text
u8_count=128
radix_blocks=4
big_lwe_dimension=2048
ciphertext_bytes=8,392,704
decrypted_count=128
decrypted_prefix=aba447efb83e52e47a91a999990ae440
local_lwehls01_compatibility=passed
```

这证明新增客户端能无损导入当前 FPGA 输出，并仍可由原 ClientKey 解密。

6. 本机 release 客户端和密钥派生工具构建完成：

```text
hpu_lwe_remote_client SHA-256:
0b7c7747b71a52ee244847d8c0e0dd96a4286b1e5e256e410e53049d49247651

hpu_export_lwe_server_key SHA-256:
068ff4335bc1d185e638df997cda8c015cd9eb9c0750f996585ceafc57eb087f
```

## 实机验证状态

2026-07-21 已在 `10.16.0.129` 安全启动真实 V80 HPU 服务，并完成 1B 密文的
端到端 `ADDS +1` 往返：

```text
输入明文参考：0x3b（59）
远端操作：ADDS +1
返回密文解密：0x3c（60）
发送密文：65,568B
返回密文：65,568B
TCP/HPU RPC 往返：23.023ms
HPU 与 ServerKey 初始化：63,550.289ms
clear_reference_transmitted=no
client_key_loaded_on_129=no
local_client_key_decrypt_checked=yes
remote_hpu_ciphertext_compute=passed
```

该结果证明以下完整链路已经打通：

```text
132 的 CSD/FPGA LWE 加密
  -> 132 TCP 客户端
  -> 129 真实 V80 HPU 执行同态 ADDS
  -> 结果密文返回 132
  -> 132 使用原 ClientKey 解密验证
```

随后已完成 128B（128 个 u8）密文的真实 HPU `ADDS +1` 往返：

```text
u8_count=128
发送密文：8,392,704B
返回密文：8,392,704B
RPC 往返：1,350.662ms
解密前缀：aca548f0b93f53e57b92aa9a9a0be541
local_client_key_decrypt_checked=yes
remote_hpu_ciphertext_compute=passed
```

解密前缀与原始前缀逐字节加 1 完全一致，例如 `ab -> ac`、`a4 -> a5`、
`ef -> f0`、`0a -> 0b`。这证明同一连接服务能够继续处理约 8.39MB 的批量密文请求和
响应，128 个 u8 的全部解密结果均通过客户端自动校验。

尚未完成的实机验证：

- 网络发送、HPU 排队/执行、返回和本机解密各阶段的细分计时；
- 连续多轮批量请求、异常恢复和服务长期运行测试；
- 端到端吞吐率、延迟分布和不同批量大小的性能曲线。

部署和联调命令记录在：
`docs/architecture/LWE远程HPU计算通路.md`。

## 首次远端启动问题

远端服务端首次启动在解析 V80 配置时报告：

```text
ShellString used env_var <V80_SERIAL_NUMBER> not found
```

原因是 `setup_hpu.sh --config v80 -p` 只导出 `V80_PCIE_DEV`、Vivado 和 AMI 路径，
不会自动导出 `V80_SERIAL_NUMBER`；而 `config_store/v80/hpu_config.toml` 的
`board_sn` 明确引用该变量。解决方法是从所选 PF0 的 AMI sysfs
`board_serial` 读取序列号，并在启动服务端的同一 shell/tmux 中导出。

## 自动硬件重载导致 129 失联

补齐环境变量后，服务端读取到：

```text
raw_his_version="2.3.0 +0 *0 - zama ucore 0.0"
归档要求 HIS=2.0
后端解析到 zama ucore=0.0
```

当前 V80 配置为 `force_reload="false"`。该值并不禁止重载：后端在复用当前硬件失败后
仍会进入 fresh reload，调用 `sudo rmmod ami/qdma_pf`、JTAG 下载、PCIe PF remove 和
rescan。输入 sudo 密码后 129 失联，与该破坏性重载路径吻合。

修复策略：

- 远程计算服务启动前强制检查配置，只有 `force_reload="never"` 才继续；
- 使用独立的 `hpu_config_remote_no_reload.toml`，不修改其他 HPU 工具的配置；
- 版本或 UUID 不兼容时只报错退出；
- 真正的 PDI/驱动更新必须放在管理员维护窗口执行，不能由网络服务自动触发。

补充确认：129 上此前成功的示例命令使用 `sudo -E`，其二进制依赖 crates.io 的
`tfhe 1.5.1 / tfhe-hpu-backend 0.4.0`；新远程服务端使用当前本地仓库
`tfhe 1.5.0` 和加入 no-reload 支持的定制后端。服务端应同样使用 `sudo -E` 获取
AMI/QDMA 权限和保留环境变量，但只能读取 `force_reload="never"` 的专用配置。

## 2026-07-21：固定 132 为唯一源码基准

为避免 132 和 129 上的服务代码继续分叉，部署方式调整为：

- 132 保存并修改全部 HLS、SUDA Host、TCP 客户端和 HPU 服务端源码；
- 129 只作为真实 HPU 服务的部署与运行节点，不在该机器上继续手改源码；
- 132 使用 `scripts/lwe_remote_hpu/package_server_129.sh` 生成覆盖包；
- 覆盖包带有 `SHA256SUMS`，包含服务端所需源码、no-reload 后端保护、专用配置、
  启动脚本和 `CompressedServerKey`；
- 覆盖包不包含 ClientKey、Big-LWE 私钥或明文测试数据；
- 129 解压并校验后，通过 `scripts/lwe_remote_hpu/start_server_129.sh` 编译和启动；
- 启动脚本同时验证配置、后端、服务端和二进制中的 no-reload 保护，防止网络服务
  再次进入卸载驱动、重编程和 PCIe rescan 路径。

132 侧当前仍采用两段式运行：SUDA/CSD 程序先产生 `LWEHLS01`，随后
`hpu_lwe_remote_client` 将密文发送到 129。PC 可同时登录两台服务器完成部署控制，
但不参与 132 到 129 的运行时密文传输。

### 首次运行部署启动脚本的假失败

129 首次运行 `start_server_129.sh` 时，本地后端和服务端已成功完成 release 编译，
但脚本随后报告：

```text
built server does not contain the no-reload backend guard
```

该报告不是后端保护丢失。原脚本同时启用了 `set -o pipefail` 并执行：

```bash
strings <binary> | grep -q <guard-string>
```

`grep -q` 找到字符串后提前退出，仍在输出的 `strings` 收到 `SIGPIPE` 并返回 141；
`pipefail` 因而把已经匹配成功的管道误判为失败。修复为使用
`grep -aFq <guard-string> <binary>` 直接检查二进制，不再建立会触发 SIGPIPE 的管道。
该次运行在打开 HPU 设备之前退出，没有触发硬件初始化或重载。

## 2026-07-21：1B 密文真实 HPU 往返成功

129 使用 `force_reload="never"` 的专用配置复用当前 HPU，版本检查结果为
`zama ucore 2.0`，随后成功加载 `CompressedServerKey` 并监听 `0.0.0.0:19090`。
132 将现有 1B FPGA 密文发送到 129 执行 `ADDS +1`，RPC 往返耗时 `23.023ms`；
返回密文由 132 ClientKey 解密为 `0x3c`，与输入参考 `0x3b + 1` 一致。

本次验证中，网络未发送明文参考，129 未加载 ClientKey 或 Big-LWE 私钥。

## 2026-07-21：128B 批量密文真实 HPU 往返成功

在同一 129 HPU 服务进程上继续发送 128 个 u8 的 FPGA 密文。请求和响应分别为
`8,392,704B`，RPC 往返耗时 `1,350.662ms`。返回密文在 132 解密得到前缀
`aca548f0b93f53e57b92aa9a9a0be541`，与原始数据逐字节执行 `+1` 的结果一致；客户端
报告 `decrypted_count=128`，全部 128 个结果均通过自动校验。

## 2026-07-23：区分首次部署与复用部署

新增 `docs/user-guides/LWE远程HPU部署与运行命令.md`，将操作分为两套：

- 新用户：129 尚无源码，从 132 复制基础 Git 源码归档、服务覆盖包和实际
  `psi64.hpu`，校验后建立完整构建树；
- 旧用户：129 已经跑通过，只从 132 更新服务覆盖包，不重新覆盖已验证的 HPU 归档；
- 两者随后共用同一套 129 安全启动、132 侧 1B/128B 测试和服务停止命令。

132 已生成首次部署产物及统一 SHA-256 清单：

```text
tfhe-rs-base-e8ab4484545a.tar.gz: 5.4MB
lwe_remote_hpu_server_129.tar.gz: 28MB
psi64.hpu: 78MB
lwe_remote_hpu_bootstrap_SHA256SUMS
```

## 2026-07-23：新增单程序内存直传原型

为消除 `FPGA 密文文件 -> Rust 客户端再读取` 的中间落盘步骤，新增：

```text
host/applications/vscode-lwe-encrypt-remote-offload/
```

实现的数据通路为：

```text
SSD -> SLM -> FPGA lwe_encrypt
    -> Host 内存物理布局解包
    -> LWERPC01/TCP -> 129 HPU ADDS
    -> Host 内存接收结果
    -> 最终 LWEHLS01 文件
```

实现要点：

- 旧 `vscode-lwe-encrypt-offload` 不修改，继续作为两段式回归基线；
- 新程序复用已验证的 SLM、context、program 和 64B padding 解包逻辑；
- FPGA 中间密文不调用 `write_dump`，只保存在 Host `logical_words` 内存；
- TCP 阶段前主动释放 FPGA program、memory range 和 input/output SLM；
- C++ 实现与 Rust 服务端一致的 `LWERPC01` 128B 头和小端 payload；
- 网络不发送明文参考、Big-LWE 私钥或 ClientKey；
- 远端结果使用本机 Big-LWE 私钥做数学解密校验，最终只保存结果密文文件。

验证情况：

```text
新 C++ 应用构建：通过
旧 C++ 应用重建：通过
C++ localhost 协议测试：通过
Rust protocol::tests：3/3 通过
```

当前尚未在 QEMU/CSD 与 129 HPU 上运行新的融合二进制，因此本条只确认构建、资源
边界和协议兼容性；真实单程序端到端结果需下一步实机测试。
