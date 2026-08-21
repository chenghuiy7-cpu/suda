# LWE 加密算子 context 偏移修复记录（2026-07-10）

## 1. 问题现象

上板运行 `vscode-lwe-encrypt-offload` 时，ARM runtime 收到的第一包输出为：

```text
LWE_RX_LAST_DUMP ... bytes=64 tuser=0xff ...
qword=[4c57454552524f52 0000000000000001 ...]
HLSACC_REQ_CB ... result=0
```

- `0x4c57454552524f52` 是 HLS 算子写出的 `LWEERROR` 标识。
- 错误码 `1` 表示算子读取到的 `mask_dimension` 为 0 或超出上限。
- runtime 日志同时证明 host 写入的静态 context 正确：
  `staged_u32=[2048,1,2,0,17]` 与 `static_u32=[2048,1,2,0,17]` 一致。

因此问题不是 host 未传入配置或密钥，而是 HLS 算子读取了错误的 BRAM 地址。

## 2. 根因

SUDA 的 `AccContext` 在算子静态数据前保留 192 字节 runtime 私有区，包含 FIFO 描述符及运行时状态。HLS context BRAM 数据宽度为 512 bit，即每个 word 为 64 字节，因此：

- context word 0～2：SUDA runtime 私有区，共 192 字节；
- context word 3：LWE 配置（`static_data` 起点）；
- context word 4 起：2048 bit Big-LWE secret key。

原算子从 `context[0]` 读取配置、从 `context[1]` 读取密钥，与硬件 context 布局不一致。原 CSIM testbench 也将数据放在 word 0/1，所以软件仿真未暴露该问题。

## 3. 代码修改

涉及文件：

- `device/operators/hls/lwe_encrypt/lwe_encrypt.hpp`
- `device/operators/hls/lwe_encrypt/lwe_encrypt.cpp`
- `device/operators/hls/lwe_encrypt/test.cpp`
- `device/operators/hls/lwe_encrypt/test_deep.cpp`
- `host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp`（补充布局注释，host 打包逻辑不变）

新增并使用以下地址常量：

```cpp
#define LWE_ENCRYPT_STATIC_CONTEXT_BASE 3
#define LWE_ENCRYPT_KEY_CONTEXT_BASE (LWE_ENCRYPT_STATIC_CONTEXT_BASE + 1)
```

修改结果：

- LWE 配置从 `context[3]` 读取；
- secret key 从 `context[4]` 起读取；
- CSIM 和深度测试使用与 SUDA 硬件一致的 context 布局；
- 保留此前的数据 payload 与 `TUSER=0xff` 任务结束包分离逻辑。

## 4. 验证结果

### 4.1 代码检查

`git diff --check` 通过，没有空白符错误。

### 4.2 HLS CSIM

通过，输出：

```text
lwe_encrypt HLS smoke test passed. mask_dimension=2048 u8_inputs=1 radix_blocks=4 decrypt_checked=yes
CSim done with 0 errors.
```

### 4.3 深度 C++ 测试

通过以下测试：

```text
small encoded-input LWE reference passed. dimension=16 ciphertext_words=34 decrypt_checked=yes
HPU Big-LWE clear-input saved-key reference passed. dimension=2048 ciphertext_words=8196 decrypt_checked=yes
loaded saved psi64 Big-LWE secret key. dimension=2048 ones=1015
HPU u8-radix saved-key reference passed. u8_inputs=4 radix_blocks=16 ciphertext_words=32784 decrypt_checked=yes
```

生成的参考密文：

```text
device/operators/hls/lwe_encrypt/testdata/psi64_u8_radix_hls_ciphertexts.bin
```

### 4.4 HLS 综合和 RTL 导出

HLS 综合成功：

- 目标周期：4.00 ns；
- HLS 估算周期：3.302 ns；
- 估算频率：约 302.82 MHz；
- 资源估算：8 DSP、13293 FF、30649 LUT；
- `encrypt_mask_loop` 实际 II=3，属于吞吐率告警，不是本次功能错误。

Vitis HLS 2020.2 导出时遇到日期生成的 `core_revision` 溢出，已有 `run_hls.tcl` fallback 使用 revision=1 重新打包并成功生成 IP。

新 IP：

```text
device/operators/hls/lwe_encrypt/lwe_encrypt_ip.zip
SHA256=660de2feeaf8fd2593a8d345904aae711a1a8a6b6fb6d3a49626ac12ee0aee03
```

RTL 静态检查确认：

```text
lwe_encrypt.v: context_Addr_A_orig = 64'd3
lwe_encrypt.v: context_Addr_A = context_Addr_A_orig << 6
lwe_encrypt_encrypt_encoded_lwe.v: context_word = (index >> 9) + 4
```

即配置读取字节地址为 `3 * 64 = 192`，密钥读取从 BRAM word 4 开始。

### 4.5 COSIM

COSIM 没有 clean pass。C testbench 通过，Verilog 编译和 snapshot 构建成功，但 XSIM 启动仿真时报错：

```text
Built simulation snapshot lwe_encrypt
Vivado Simulator 2020.2
ERROR: unknown error occurred
ERROR: [COSIM 212-4] *** C/RTL co-simulation finished: FAIL ***
```

当前没有出现算子输出不匹配或 testbench assertion failure；失败点位于 Vivado 2020.2 XSIM runtime 启动阶段。因此不能声称 COSIM 已通过，也不能据此判定 RTL 功能失败。

独立日志：

- `device/operators/hls/lwe_encrypt/vitis_hls.rtl_context_offset_fix_20260710.log`
- `device/operators/hls/lwe_encrypt/vitis_hls.cosim_context_offset_fix_20260710.log`

## 5. 算子池更新与备份

旧文件备份：

- `device/operators/hls/lwe_encrypt/lwe_encrypt_ip.before_context_offset_fix_20260710.zip`
  - SHA256：`93df9bb6213b995e1a3a1d9618c6f9f3ac9ee0985ff49775cb721bfb6ca399cd`
- `device/operators/hls/lwe_encrypt/vitis_hls.before_context_offset_fix_20260710.log`
- `backups/lwe_encrypt_pool_before_context_offset_fix_20260710/`

新 IP 中 11 个 Verilog 文件已更新到：

```text
device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/
```

已执行目录级 `diff -qr`，IP zip 解压出的 `hdl/verilog` 与算子池目录内容完全一致。

## 6. 下一步

本轮没有执行整板 `build_bd.sh`，按约定由用户在 tmux 中运行：

```bash
cd /home/yangchenghui/suda/device/platform/basic_shell/nf-csd
source /opt/Xilinx_2020.2/Vivado/2020.2/settings64.sh
bash build_bd.sh 2>&1 | tee build_bd_20260710_lwe_context_offset_fix.log
```

整板生成完成后必须检查 implementation timing 是否通过，再替换板卡 `BOOT.bin` 并上板验证。预期修复后的 ARM 日志不再出现 `LWEERROR/code=1`，而应先收到约 65792 字节 ciphertext payload，再收到独立的 `TUSER=0xff` 任务结束包。
