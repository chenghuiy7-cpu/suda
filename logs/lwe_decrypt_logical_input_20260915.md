# lwe_decrypt 逻辑 Big-LWE 输入支持（2026-09-15）

## 目标与输入协议

为 `lwe_decrypt` 新增 layout 0，使2048维自然顺序 Big-LWE 可以直接进入
FPGA 解密。保留原有 layout 1 的 psi64/V80 HPU-native 输入。加密算子、ARM
runtime、远端 TCP 服务和完整 pipeline 不作修改；本次验证入口为独立解密应用。

| layout | 每个 LWE | 每个 u8（4个 radix block） |
|---|---:|---:|
| 0：紧密逻辑布局 | `(2048 + 1) * 8 = 16392B` | `65568B` |
| 1：HPU-native | `2 * 12288 = 24576B` | `98304B` |

layout 0 数据依次为每个 LWE 的 `mask[0..2047], body`，u8的4个2-bit block
按低位到高位排序。不在各 LWE 之间补齐64B。它不同于 encrypt 的 cpu-layout
AXIS物理输出65792B；该物理流需要去除逐 LWE 填充才能作为 layout 0 输入，
或者使用本记录末尾补充的 layout 2 直接输入。

FPGA每拍仍接收512-bit AXIS，顺序消费8个u64系数。由于一个 LWE 有2049个
u64，body及下一个LWE的mask可能位于同一拍。算子用系数索引区分mask和body，
读到body立即执行模2^64的 `phase = body - dot`，按delta=2^59解码为2-bit
block，再拼接u8。新旧布局共用相同解码逻辑和明文输出协议。

context中的维度、delta、message_width、radix_blocks保持原值，只有
`input_layout`（Host byte offset 24，对应HLS cfg[223:192]）新增值0。

SLM写入与输入range仍按4KB对齐：

- 1B明文：有效密文65568B，SLM传输范围69632B，尾部补零4064B；
- 128B明文：有效密文8392704B，已经是4KB整数倍；
- `input_count != 0` 时按明确数量停止解码，仅允许其后有效u64为零，等待独立
  SUDA结束包；
- `input_count == 0` 时，TKEEP必须限定精确payload，不能添加有效零填充；
- 结束包到达时，若u8、LWE或系数尚未完整，返回error code 5；非零尾部返回
  code 6；不完整的u64 TKEEP返回code 7。

## 代码修改与备份

修改前源码备份：

```text
/home/yangchenghui/suda_backups/lwe_decrypt_before_logical_20260915/
```

修改文件：

- `device/operators/hls/lwe_decrypt/lwe_decrypt.hpp`：新增逻辑长度常量和layout 0；
- `device/operators/hls/lwe_decrypt/lwe_decrypt.cpp`：逐系数逻辑输入解析；
- `device/operators/hls/lwe_decrypt/test.cpp`：双布局、跨拍、LBA填充和异常输入测试；
- `host/applications/vscode-lwe-decrypt-offload/vscode-lwe-decrypt-offload.cpp`：
  新增 `--fpga-input-layout cpu` 和 `--input-format logical`，区分有效payload
  与LBA对齐范围，Host参考解密支持自然顺序索引；
- 解密应用README：记录输入协议和上板命令；
- `device/operators/hls/lwe_decrypt/update_shell_rtl.sh`：导出后更新算子池，拒绝
  早于当前cpp/hpp的旧RTL，旧算子池移到 `/home/yangchenghui/suda_backups/rtl/`。

默认LWEHLS01输入仍转为HPU-native。传入 `--fpga-input-layout cpu` 时，Host
仅读取文件结构并提取自然顺序密文系数，不再做bit-reversal或PC切分。文件头和
明文参考区不会送入FPGA。裸logical文件使用 `--input-format logical` 自动选
layout 0；文件尾部有LBA填充时须指定 `--plaintext-bytes N`。

## 已完成的本地验证

使用Xilinx `ap_uint`/`hls::stream`头文件直接编译、运行HLS C模型：

```bash
cd /home/yangchenghui/suda
g++ -O0 -std=c++14 -DUSING_XILINX_STREAM \
  -Idevice/shared_components/hls \
  -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include \
  device/operators/hls/lwe_decrypt/lwe_decrypt.cpp \
  device/operators/hls/lwe_decrypt/test.cpp \
  -o /tmp/lwe_decrypt_logical_test -lgmp
/tmp/lwe_decrypt_logical_test
```

通过的用例：

- native layout 1：3B、65B回归；
- logical layout 0：1B、3B、65B、128B，包含LBA零填充；
- logical layout 0：3B直到结束包模式，最后半拍TKEEP限定有效数据；
- 最后body缺失：拒绝输入，code 5；
- 尾部填充非零：拒绝输入，code 6。
- u64只有4字节TKEEP有效：拒绝输入，code 7。

Host应用重新编译通过。使用现有1B远端HPU结果：默认native路径得到98304B
payload，逻辑路径得到65568B payload，参考解密均为0x3c（60）。裸65568B
logical输入和补齐为69632B的输入均通过 `--inspect-only` 验证。

以上是C模型和Host参考解密结果，不是新版FPGA上板结果；未执行Vivado完整
实现、HLS RTL综合或RTL COSIM。现有算子池中的生成RTL尚未更新，需执行下一节。

## 用户在tmux执行的综合步骤

先导出新版解密RTL并更新算子池，再构建完整BOOT.bin。每步顺序执行，出错时
停止，不能在rtl_gen失败后继续使用旧RTL。

```bash
source /opt/Xilinx_2020.2/Vivado/2020.2/settings64.sh
set -o pipefail

cd /home/yangchenghui/suda/device/operators/hls/lwe_decrypt
/opt/Xilinx_2020.2/Vitis_HLS/2020.2/bin/vitis_hls \
  -f run_hls.tcl -tclargs lwe_decrypt rtl_gen \
  2>&1 | tee hls_rtl_gen_logical_20260915.log

# 只有上一条成功才执行更新；脚本也会检查RTL是否过期。
bash update_shell_rtl.sh 2>&1 | tee update_shell_rtl_logical_20260915.log

cd /home/yangchenghui/suda/device/platform/basic_shell/nf-csd
bash build_bd.sh 2>&1 | tee build_bd_20260915_decrypt_logical.log
```

完成后检查构建日志中的ERROR、最终timing和新BOOT.bin的哈希，备份ARM当前
可启动镜像后再替换。不要根据HLS C模型通过就宣称新版比特流已验证。

## 新版BOOT.bin上板后的最小验证

在QEMU终端执行：

```bash
cd /mnt/suda/host/applications/vscode-lwe-decrypt-offload
make
set -o pipefail
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_1b.bin \
  --fpga-input-layout cpu \
  --expect 60 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output lwe_decrypt_logical_result_1b.bin \
  --benchmark 2>&1 | tee lwe_decrypt_logical_result_1b.log
```

预期显示 `layout_id=0`、`ciphertext_payload_bytes=65568`，SLM输入范围69632B，
最终明文0x3c。再移除 `--fpga-input-layout cpu` 回归同一文件的native模式。
128B测试使用相应的128B LWEHLS01文件，预期payload 8392704B，逐字节校验通过。
# 补充：逐 LWE padding 输入检查（2026-09-15）

用户提醒输入可能含 padding。复查确认此前实现只覆盖紧凑数据的末尾 LBA 补零，
不能直接读取加密算子 CPU 输出的逐 LWE 64B 补齐布局，因此新增显式 layout=2。

- layout=0：4 × 2049 × 8 = 65568B/u8，紧凑自然序。
- layout=2：每块 16392B + 56B = 16448B，4 块共 65792B/u8。
- layout=1：原有 HPU-native，98304B/u8；内部固定槽位 padding 保持兼容。
- layout=2 在每块 body 后继续消费 7 个零 u64，再进入下一块。最后一个 u8
  解密完成后仍必须消费最后一块的 padding，否则结束包触发不完整输入错误。
- layout=0/2 的末尾 LBA 补零由非零 input_count 界定，非零额外数据触发错误6。
- Host 裸文件逐 LWE padding 同样检查为零；不自动猜测布局。

命令选择：`--input-format logical --fpga-input-layout cpu-padded`；有末尾
LBA padding 的裸文件还必须指定 `--plaintext-bytes N`。已有 LWEHLS01 文件
也支持选择 cpu-padded，由 Host 插入逐块零 padding 后供 FPGA 测试。
新增 C 模型覆盖 1/3/65/128 个 u8，末尾 LBA padding、截断块内 padding、
损坏块内 padding 和损坏末尾 padding。综合与上板仍由用户执行，不宣称时序或 RTL 已通过。

验证结果：新增后共17项直接 HLS C模型测试通过；Host应用编译通过。
已有128B远端结果转为 layout=2 时 payload=8421376B，Host解密参考前缀
`aca548f0b93f53e57b92aa9a9a0be541...` 一致，inspect_only=passed。
加密源代码的 flush_output_packet 在发送后清零 packet_data，故其 CPU
输出 body 后的56B确实为零，与新增布局约定一致。未修改加密算子或子模块。
