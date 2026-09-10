# Near-storage selective TFHE 原型开发记录

## 1. 目标

在不修改现有 `lwe_encrypt` 核心的前提下，新增以下数据通路：

```text
SSD 中的 512B 定长 record
  -> input SLM
  -> FPGA selective_filter
  -> FPGA lwe_encrypt
  -> output SLM 中的 HPU-native TFHE 密文
```

筛选条件第一版支持 `quantity > threshold` 和
`quantity == threshold`。`quantity` 既是筛选字段，也是筛选通过后送入
TFHE 加密的连续 `u8` 明文。

## 2. 2026-09-01 接口确认

现有 `lwe_encrypt` 的输入是 512-bit AXI-Stream。U8 radix 模式会遍历
每个输入 beat 的 64 个 byte lane，并将 `TKEEP=1` 的每个字节解释成一条
独立的 `u8` 明文。因此不能把完整 512B record 直接送入 LWE，否则 record
中的 ID 和 padding 也会被加密。

本次采用两种筛选输出模式：

1. `FULL_RECORD`：独立验证 filter 时输出所有命中的完整 512B record。
2. `QUANTITY_ONLY`：与 LWE 级联时，每条命中 record 只输出一个
   `TKEEP=1` 的 `quantity` 字节。

LWE 上下文中的 `input_count` 设置为 0，表示处理所有有效字节直到收到
SUDA 的 `TUSER=0xff` 结束包。这样筛选结果数量可以动态变化，LWE 无需提前
知道 `selected_count`。

## 3. record 格式

第一版二进制 record 固定为 512B，即 8 个 512-bit AXI beat：

| 字节偏移 | 大小 | 字段 |
|---:|---:|---|
| 0 | 4B | little-endian `uint32 id` |
| 4 | 1B | `uint8 quantity` |
| 5 | 1B | flags |
| 6 | 2B | reserved |
| 8 | 8B | `uint64 order_key` |
| 16 | 8B | `uint64 extended_price_cents` |
| 24 | 488B | deterministic payload/padding |

谓词字段放在第一个 64B beat 内。筛选算子在收到 record 的第一个 beat 后
立即完成比较，后续七个 beat 可以直接转发或丢弃，不缓存整条 record，更
不会缓存整个数据集。

## 4. 统计与结束协议

筛选算子复用 SUDA 的 `TUSER=0xff` 独立结束包，并在其 `TDATA` 中写入
`total_count`、`selected_count`、配置和错误码。级联时现有 LWE 会原样转发
该结束包。ARM runtime 的 `mcdma_clear_rx_finish_beat()` 会在完成记账后清零
输出 SLM 中的 64B 结束 beat，防止协议标志污染密文，因此 Host 不直接读取
该状态 payload。第一版上板程序根据 `result_bytes / 每条记录的固定密文字节数`
精确反算 FPGA 的 `selected_count`，再与主机参考筛选结果核对；原始状态包的
字段和值由 filter 单元测试和 filter->LWE 联合 C 仿真验证。

## 5. 算子池集成策略

当前 block design 只有四个 operator slot。为避免扩展交换机和控制器造成
较大时序风险，本原型已用 `selective_filter` 替换 slot 0 中仅用于示例的
`accExamplePlusOperator`：

| slot/type | 原配置 | 新配置 |
|---:|---|---|
| 0 | add example | selective_filter |
| 1 | legacy encrypt | 不变 |
| 2 | lwe_encrypt | 不变 |
| 3 | lwe_decrypt | 不变 |

这意味着新比特流中旧的 add 示例暂不可用，但 TFHE 加密、解密和 LWE 核心
均保持不变。

## 6. 当前进度

- [x] 确认 LWE AXI-Stream、上下文和动态输入数量接口。
- [x] 新增 512B TPC-H-like 数据生成器。
- [x] 新增 streaming `selective_filter` HLS 源码。
- [x] 通过 filter 单元测试和 filter->LWE 联合 C 仿真。
- [x] 生成 RTL 并更新 FPGA operator pool 源码及 block-design Tcl。
- [x] 新增 SSD->filter->LWE->ciphertext 上板 demo 应用并通过主机编译。
- [ ] 生成新比特流并完成板上正确性测试。

## 7. 首次 C 仿真问题与修复

首次完整-record 用例发现最后一条命中 record 只输出了第一个 64B beat。
原因是 `total_count` 在最后一条 record 的 beat 0 即达到 `record_count`，原判断
随即把该 record 后续七个 beat 当成超额输入丢弃。修复后，仅在
`beat_index==0`、即准备开始一条新 record 时检查数量上限；已经开始处理的
最后一条 record 可以完整流过。这一区分也明确了 record 计数与 AXI beat
计数的不同层次。

第一次综合虽然达到 `II=1`，但可配置字段偏移会生成 512-bit 动态移位选择
网络，HLS 估算关键路径约 3.68ns。由于第一版格式已经明确 quantity 固定在
byte 4，硬件改为静态读取 `[39:32]`，上下文中的 offset 继续保留并要求为 4，
用于版本和格式一致性校验。这样避免为当前不需要的字段位置可编程性付出
时序与 LUT 代价。

最终 HLS 综合结果：主循环 `II=1`、估算周期 2.522ns（约 396.5MHz），
资源为 1,591 LUT、1,375 FF、0 BRAM、0 DSP。已将生成的
`selective_filter.v` 和 `selective_filter_regslice_both.v` 同步到
`hlsaccframework/selective_filter`，并将 block design 的 slot 0 从旧 add
示例替换为该算子。LWE encrypt/decrypt 的 RTL 和 slot 保持不变。

主机侧新增独立应用 `vscode-selective-lwe-encrypt-offload`。它构造一个包含
两个逻辑算子的 SUDA program：`0x00 -> 0x10` 连接 filter 到 LWE，随后
`0x10 -> 0xf0` 连接 LWE 到 output SLM；执行命令同时提交两页 4KB context。
output SLM 按所有 record 均命中的最坏情况分配，实际只回读动态结果对应的
密文范围，避免参考文件与 SSD 内容不一致时发生越界写。

另新增 `write_dataset_to_ssd.py`，支持将任意非零 benchmark 镜像写入
`/dev/nvmq0n1` 指定 LBA，并逐字节回读和 SHA-256 校验。非 4KB 整数倍的
输入只在传输尾部自动补零到 LBA 边界，逻辑 record 数仍由 filter context
控制。该工具解决了旧 data-gen 只支持单个 4KB 文件的问题。

收尾检查时发现该脚本最初只定义了 `main()`，却漏写 Python 命令行入口，
直接执行会返回 0 但不真正写入数据。已补充 `if __name__ == "__main__"`，
并以 `/tmp` 中的 12KB 普通文件模拟块设备，在 LBA 1 写入 8192B 数据后
完成逐字节回读和 SHA-256 校验。增加自动 LBA 补齐后，又验证了单条 512B
record 会被写成一个 4096B 传输，其中尾部补零 3584B，回读校验通过。上述
检查均不访问真实 SSD。

## 8. 软件验证结果

### 8.1 benchmark 数据

执行：

```bash
cd /home/yangchenghui/suda/host/applications/vscode-selective-lwe-encrypt-offload

./generate_tpch_like.py \
  --records 16 \
  --predicate gt \
  --threshold 32
```

得到 16 条、共 8192B 的定长 record。确定性样例中命中 6 条，命中的
`quantity` 依次为 `58, 44, 45, 41, 54, 39`。生成文件的 SHA-256 为：

```text
8b7197274c1c41b21569550083ac3c152aeeb8c83d287e1809214ecd14b3ed1f
```

### 8.2 HLS C 仿真

执行：

```bash
cd /home/yangchenghui/suda/device/operators/hls/selective_filter
/opt/Xilinx_2020.2/Vitis_HLS/2020.2/bin/vitis_hls \
  -f run_hls.tcl selective_filter csim
```

测试同时覆盖：

1. `quantity > threshold` 时输出完整的命中 record；
2. `quantity == threshold` 时只投影 quantity，并直接调用未经修改的
   `lwe_encrypt`，验证 filter->LWE 结束协议和密文包数量；
3. 在零噪声、零测试密钥配置下，从 LWE 输出恢复两条命中 quantity，确认
   二者均为期望值 7，验证筛选值和顺序确实进入了 LWE 数据路径。

结果：

```text
selective_filter unit and filter->LWE pipeline tests passed
CSim done with 0 errors.
```

### 8.3 HLS 综合

执行：

```bash
cd /home/yangchenghui/suda/device/operators/hls/selective_filter
/opt/Xilinx_2020.2/Vitis_HLS/2020.2/bin/vitis_hls \
  -f run_hls.tcl selective_filter csynth
```

目标时钟周期为 4ns。综合结果为：

| 项目 | 结果 |
|---|---:|
| 主 stream loop II | 1 |
| 估算周期 | 2.522ns |
| 估算频率 | 约 396.5MHz |
| LUT | 1,591 |
| FF | 1,375 |
| BRAM | 0 |
| DSP | 0 |

综合报告位于：

```text
device/operators/hls/selective_filter/selective_filter/solution1/syn/report/selective_filter_csynth.rpt
```

### 8.4 主机 demo

`vscode-selective-lwe-encrypt-offload` 已通过 `make -j4` 编译，并通过
`--help` 参数解析检查。硬件运行仍需包含 `selective_filter` 的新比特流，
因此不能使用当前旧 bitstream 进行板上验证。

## 9. 生成比特流和上板验证命令

完整 block design 构建时间较长，建议在 tmux 中执行：

```bash
cd /home/yangchenghui/suda/device/platform/basic_shell/nf-csd
source /opt/Xilinx_2020.2/Vivado/2020.2/settings64.sh
bash build_bd.sh 2>&1 | tee build_bd_20260901_selective_lwe.log
```

部署新 `BOOT.bin`、重启 ARM 与 QEMU，并重新连接 NVMQ 后，在 QEMU 内执行：

```bash
cd /mnt/suda/host/applications/vscode-selective-lwe-encrypt-offload

./generate_tpch_like.py \
  --records 16 \
  --predicate gt \
  --threshold 32

./write_dataset_to_ssd.py \
  --input testdata/tpch_like_512b.bin \
  --device /dev/nvmq0n1 \
  --lba 65536

make -j4

./vscode-selective-lwe-encrypt-offload \
  --ssd-nsid 1 \
  --ssd-lba 65536 \
  --records 16 \
  --predicate gt \
  --threshold 32 \
  --reference testdata/tpch_like_512b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output selective_lwe_fpga_ciphertexts.bin \
  --benchmark \
  2>&1 | tee selective_lwe_fpga_demo.log
```

成功判据包括：

```text
selective_filter -> lwe_encrypt FPGA pipeline passed
total_count=16 selected_count=6
decrypted_count=6
fpga_filter_selected_count_derived_from_result=yes
host_key_decrypt_checked=yes
```

该主机参考解密只用于最终正确性验证。被筛选出的明文 quantity 在 FPGA
filter 和 FPGA LWE 之间通过片上 AXI-Stream 直接传递，不经过 Host 或 ARM。

## 10. 完整比特流构建检查（2026-09-02）

检查对象：

```text
device/platform/basic_shell/nf-csd/build_bd_20260901_selective_lwe.log
```

构建流程已完整执行，日志中没有 `ERROR:` 或 `FATAL:`。关键完成标志如下：

```text
route_design completed successfully
write_bitstream completed successfully
[INFO]   : Bootimage generated successfully
```

最终布线状态为 545,924 条可布线 net 全部完成，routing error 为 0。新增
`selective_filter_0` 已进入综合和最终实现。其报告中出现的代表性 250MHz
setup 路径 slack 为 `+0.049ns`，该路径满足时序。

但本次实现并非 timing clean。最终 post-route timing 为：

| 指标 | 结果 |
|---|---:|
| WNS | -0.055ns |
| TNS | -1.193ns |
| setup failing endpoints | 39 |
| WPWS / TPWS | 0.000ns / 0.000ns |

报告明确给出 `Timing constraints are not met`。最差路径位于 250MHz
`clk_pl_1` 域，从 `static_var_bram1` 到 `OperatorController_1`；其余主要
违例位于 `static_var_bram3`、`OperatorController_3` 和既有
`lwe_decrypt_0` 路径，没有发现以 `selective_filter_0` 为端点的负 slack
路径。当前全设计 CLB 占用为 81.03%，较高的布局占用会增加布线压力。

本次产物如下：

```text
BOOT.bin  SHA-256: b3aee5b772d4b05757d2fa97b1b7354493f0557e8117068bd042d1ad6aa17a67
system.bit SHA-256: de9c6eb5773d393b00f21a948c4ede44015fa8f8cc889feba8432678a22e3d50
zynqmp.dtb SHA-256: 1168b1abae245b040f1c6c0d9afa39d9c9e9d1506f23b7fd483ac5016569bdb5
```

其中 `zynqmp.dtb` 与已有 recovery 版本一致；`BOOT.bin` 已包含新的
`system.bit`。结论是：镜像生成完整，可以保留并进行受控上板验证，但不能
将其记录为时序完全通过版本。部署前必须备份当前可启动镜像；上板后先验证
ARM、PCIe/QDMA 和 NVMQ，再运行 selective filter -> LWE demo。

## 11. 双算子 context 初始化卡死修复（2026-09-02）

新比特流上板后，SSD 的 8KB、两个 4KB IOV 搬运均正常完成，但执行
`selective_filter -> lwe_encrypt` 时 Host 和 ARM 同时等待。ARM 最后一条关键
日志为：

```text
LWE_CTX_VERIFY ... context_phy=0x0
staged_u32=[0,0,0,0,0] static_u32=[0,0,0,0,0]
```

问题不在 QDMA 数据搬运或新增比特流，而在 ARM runtime 从未被单算子测试覆盖
的“双 context 初始化”分支。原实现存在三个相关错误：

1. 查找第二个待初始化算子时没有越过第一个索引，导致两个 context 页都选择
   第一个 `acccontext`；
2. 4KB DMA 直接写入只有 2048B 的 `context.static_data`，越界覆盖
   `context_phy` 和链表元数据；
3. DMA 完成后只从 staging 区提交 `acccontext[0]`，没有提交第二个算子的
   static context。

单独运行 `lwe_encrypt` 或 `lwe_decrypt` 时只有一个 context，走的是另一条已
验证分支，因此此前不会出现该问题。

修复内容：

- 根据 `need_init_ctx[]` 明确生成逻辑算子索引列表；
- Host context page 0 和 page 1 分别搬入对应算子独立的第二个 4KB staging
  页，不再直接覆盖 runtime 元数据；
- 多 context 通用分支使用逻辑算子索引 `k` 选择 `acccontext[k]`，不再错误
  使用压缩后的 page 索引；
- DMA 完成后遍历所有待初始化算子，各自复制 2048B static context；
- 新增 `HLS_CTX_FETCH` 和 `HLS_CTX_VERIFY` 中文日志，记录 page、op index、
  type id、物理地址及 context 参数；后续诊断已扩展到前六个 u32。

ARM 交叉编译成功，产物为：

```text
device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt.selective_ctxfix_20260902
ELF 64-bit ARM aarch64
size: 10394208B
SHA-256: b979af6f6ae7f1c15720e0f57ae38e9dad1a7852c853a3d423356f3f74fea118
```

预期重新测试时先看到两个不同的 context 目标地址，然后看到：

```text
HLS_CTX_VERIFY ... op_index=0 type_id=0 ... staged_u32=[16,512,4,0,32,1]
HLS_CTX_VERIFY ... op_index=1 type_id=2 ... staged_u32=[2048,0,2,...,17,1]
```

其中 LWE context 的第四个 u32 由 noise mode 决定。两个 `context_phy` 必须
均为非零值，随后才应进入 filter 和 LWE 的 AXI-Stream 执行阶段。

## 12. 3008B 异常输出定位与 AXI TKEEP 修复（2026-09-02）

双 context runtime 修复上板后，请求不再卡死，但首次返回：

```text
result_bytes=3008 expected_payload_bytes=589824
```

参考数据中有 6 条记录满足谓词。`3008 = 6 * 512 - 64`，而 6 个 HPU-native
LWE 密文应为 `6 * 98304 = 589824B`。同时 ARM 从输入提交到 RX 结束只经过约
0.3ms，不足以完成 6 次 LWE 加密。因此该结果不能视为部分密文；它表明当前
硬件返回内容更接近筛选后的原始 record，且 runtime 将最后 64B 当作 SUDA
结束包从统计中剥离。当前定位重点由 context 搬运转到实际算子调度边和
filter 输出协议。

为避免继续盲目生成比特流，ARM runtime 增加以下中文诊断：

- `HLS_CTX_VERIFY` 扩展到前 6 个 `u32`，覆盖 filter 的 `output_mode` 和 LWE
  的 `output_layout`；
- `HLS_GRAPH_MAP` 记录逻辑算子到物理 slot 的映射；
- `HLS_GRAPH_EDGE` 记录重映射后的物理 `from -> to` 调度边；
- `MCDMA_RX_FINISH_DUMP` 在清零前打印 64B RX 结束包，可判断它来自
  `selective_filter` 状态包、LWE 错误包还是普通 SUDA 结束包。
- Host demo 在结果长度异常时仍回读实际 SLM 内容，并打印
  `raw_selected_record_prefix` 及首尾 16B，用于直接确认输出是否为筛选后的
  原始 record 前缀。

代码审查还发现一个与旁路问题独立、但 filter->LWE 链路必然会触发的接口
缺陷：`OperatorController.v` 原先在算子输出和输入 FIFO 两侧把 AXI-Stream
`TKEEP` 强制成全 1。filter 的 quantity 模式虽然只设置 byte lane 0 有效，
控制器却会让 LWE 把一个 64B beat 的所有 64 个 byte 都当成明文。修复后：

- HLS 输出的 `TKEEP` 原样送入 AXI switch；
- AXI switch 输入的 `TKEEP` 原样进入 controller FIFO；
- FIFO 输出的 `TKEEP` 原样送给下游 HLS 算子。

该 RTL 修复需要重新生成完整比特流。先部署带诊断的 ARM `nvmf_tgt` 并确认
物理调度边应为 `slot0 -> slot2 -> RX channel`，再生成包含 TKEEP 修复的镜像。

诊断版本已完成 ARM 交叉编译：

```text
device/platform/software_stack/nf_spdk/build/bin/nvmf_tgt.selective_graphdiag_20260902
SHA-256: 512569108dc9ca9aa9ecc85cefa83bfbe401c389be73cec9b54d356c92fc9f3c
```

Host demo 的异常回读版本也已重新编译：

```text
host/applications/vscode-selective-lwe-encrypt-offload/vscode-selective-lwe-encrypt-offload
SHA-256: 7d7bbe6629531ea11c6061e238a79d69a122757880bdd9c7634aeb8382e1dfa7
```

## 13. 实际构建 RTL 不同步与多算子路由加固（2026-09-03）

继续核对完整 bitstream 的输入文件后，发现上一节的 `TKEEP` 修复只写入了公共
RTL：

```text
device/platform/ips/rtl/OperatorController.v
```

但 `build_bd.sh` 调用的 Vivado 工程通过 `prj_setup.tcl` 实际加载的是 shell
构建目录中的另一份副本：

```text
device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/
hlsaccframework/OperatorController/OperatorController.v
```

两份文件当时并不一致。因此 SHA-256 为
`b3aee5b772d4b05757d2fa97b1b7354493f0557e8117068bd042d1ad6aa17a67`
的旧 `BOOT.bin` **没有包含 TKEEP 修复**。这解释了为什么修改公共源码后，板上
行为没有发生预期变化。该旧镜像不能用于验证最终的 filter -> LWE 数据语义。

本次完成以下整改：

1. 将公共版本的 `TKEEP` 透传修改同步到 Vivado 实际读取的 shell 副本；
2. `fifo_dest` 只在 `RECV_APPLY_REQ_PAYLOAD` 状态且控制 AXI-Stream 的
   `TVALID && TREADY` 握手成立时更新，并增加复位值，避免无效控制拍覆盖算子
   输出路由；
3. `build_bd.sh` 在每次构建前自动把公共 `OperatorController.v` 同步到 shell
   构建目录，并打印同步后 SHA-256，防止以后再次出现“源码已改、bitstream
   未包含”的情况。

当前两份 `OperatorController.v` 已逐字节一致：

```text
SHA-256: abe2b828f3b581c0014130bec1b124b6ec798cd5e4066a1ca185a3a306c5035f
```

本地验证结果：

- `selective_filter` CSim 通过，覆盖 `filter -> LWE` 软件联合测试；
- HLS C synthesis 通过，`stream_loop` 达到 `II=1`；
- 估算时钟周期 `2.522ns`，LUT 1591、FF 1375、BRAM 0、DSP 0；
- 重新生成的 filter Verilog 与算子池副本 SHA-256 一致；
- Verilator 完成 `OperatorController` 展开和语法检查，仅报告原工程已有的
  width、unused 和 missing optional pin 警告，没有新增语法错误。

下一次板上测试必须同时满足以下诊断条件：

```text
HLS_CTX_VERIFY ... op_index=0 type_id=0 ... staged_u32=[16,512,4,0,32,1]
HLS_CTX_VERIFY ... op_index=1 type_id=2 ... staged_u32=[2048,0,2,0,17,1]
HLS_GRAPH_MAP ... logical_op=0 physical_slot=0
HLS_GRAPH_MAP ... logical_op=1 physical_slot=2
HLS_GRAPH_EDGE ... logical_op=0 ... from=0x00 to=0x20
HLS_GRAPH_EDGE ... logical_op=1 ... from=0x20 to=<实际RX通道ID>
```

若上述 context 和物理边均正确，新 bitstream 的预期结果为：filter 对 16 条
512B record 逐条扫描，只把 6 个被选中的 quantity 作为 `TKEEP=0x1` 的 6 个
单字节输入送入 LWE；LWE 最终返回 `6 * 98304 = 589824B`。若仍返回约 3KB，
则应依据 `MCDMA_RX_FINISH_DUMP` 和 Host 异常输出前缀直接判定数据来自哪个
算子，不再把它误认为部分 LWE 密文。

## 14. Graphdiag 板上诊断结果（2026-09-03）

使用旧 selective BOOT 和 SHA-256 为
`512569108dc9ca9aa9ecc85cefa83bfbe401c389be73cec9b54d356c92fc9f3c`
的诊断 runtime 再次执行 16 条 record 测试，得到：

```text
HLS_CTX_VERIFY ... op_index=0 type_id=0 ...
    staged_u32=[16,512,4,0,32,1]
HLS_CTX_VERIFY ... op_index=1 type_id=2 ...
    staged_u32=[2048,0,2,0,17,1]
HLS_GRAPH_MAP ... logical_op=0 physical_slot=0
HLS_GRAPH_EDGE ... logical_op=0 ... from=0x00 to=0x20
HLS_GRAPH_MAP ... logical_op=1 physical_slot=2
HLS_GRAPH_EDGE ... logical_op=1 ... from=0x20 to=0x05
HLSACC_REQ_CB ... result=1576960
```

由此确认：

1. filter 和 LWE 的两个独立 context 均正确搬入，`output_mode=1`、
   `output_layout=1`；
2. runtime 将逻辑图正确重映射为输入 -> slot 0 filter -> slot 2 LWE ->
   RX channel 5；
3. `1576960B` 恰好等于本次 Host 分配的整个 output SLM 容量，不是合法 LWE
   结果长度；
4. 旧 BOOT 中 Controller 把 filter 的 `TKEEP=0x1` 强制改成全 1，导致每个
   被选 quantity 所在的 64B beat 被 LWE 解释成 64 个明文字节。6 条命中记录
   会被错误扩张成最多 384 个加密输入，所需输出远大于 1576960B，因此 RX 在
   收到真正结束包前先填满 output SLM。

这次结果排除了 context 和 runtime 图重映射错误。下一步无需继续修改 ARM
runtime；必须生成包含 2026-09-03 `OperatorController` TKEEP 透传及
`fifo_dest` 握手加固的新 bitstream。新镜像的唯一正确结果长度应为：

```text
6 selected quantities * 98304B = 589824B
```

## 15. TKEEP/路由修复版完整比特流构建结果（2026-09-03）

执行以下命令生成完整比特流：

```bash
cd /home/yangchenghui/suda/device/platform/basic_shell/nf-csd
source /opt/Xilinx_2020.2/Vivado/2020.2/settings64.sh
bash build_bd.sh 2>&1 | tee build_bd_20260903_selective_tkeep_route_fix.log
```

主机随后发生重启，但日志和制品时间戳确认构建已在重启前完整结束：

- `route_design completed successfully`；
- `write_bitstream completed successfully`；
- `Bootimage generated successfully`；
- 构建日志中没有 `ERROR:`，有 317 条 `CRITICAL WARNING:`；
- 构建入口打印的 `OperatorController` SHA-256 为
  `abe2b828f3b581c0014130bec1b124b6ec798cd5e4066a1ca185a3a306c5035f`，
  证明本次镜像包含 TKEEP 透传和 `fifo_dest` 握手修复。

生成制品及 SHA-256：

```text
BOOT.bin   47d983828fcb68e262e4cfbf237f53bf3a01d8164dbce9ba276c77554a4c2636
system.bit f428bebd5c87db40c2eaa751234d1625b3be6974453727acdbdcc8f5320e2dec
zynqmp.dtb 1168b1abae245b040f1c6c0d9afa39d9c9e9d1506f23b7fd483ac5016569bdb5
```

新 `BOOT.bin` 位于：

```text
/home/yangchenghui/suda/device/platform/basic_shell/nf-csd/shell/virt_one_drive/ready_for_download/fidus/BOOT.bin
```

最终 post-route timing 并非 clean pass：

```text
WNS = -0.329ns
TNS = -67.768ns
TNS failing endpoints = 718
Timing constraints are not met.
```

其中 `clk_pl_1` 域的 WNS 为 `-0.329ns`、TNS 为 `-50.479ns`，另有 PCIe GT
相关时钟域违例。因此结论是：完整镜像已经成功生成，且包含本次功能修复；但
实现时序没有完全收敛。若用于板上功能验证，应保留上一版可启动镜像并先做受控
上板测试，不能把本次构建标记为 timing-clean 版本。

## 16. TKEEP/路由修复版上板复测与标量流协议加固（2026-09-03）

使用 SHA-256 为
`47d983828fcb68e262e4cfbf237f53bf3a01d8164dbce9ba276c77554a4c2636`
的新 `BOOT.bin` 复测后，ARM runtime 得到：

```text
HLS_CTX_VERIFY ... op_index=0 type_id=0 ...
    staged_u32=[16,512,4,0,32,1]
HLS_CTX_VERIFY ... op_index=1 type_id=2 ...
    staged_u32=[2048,0,2,0,17,1]
HLS_GRAPH_EDGE ... logical_op=0 ... from=0x00 to=0x20
HLS_GRAPH_EDGE ... logical_op=1 ... from=0x20 to=0x05
LWE_TX_SEND ... size=8192
HLSACC_REQ_CB ... result=1576960
```

同时读取 `0xB0070080`，`axis_switch4_decode_err=0`、`filter_S00_err=0`、
`lwe_S03_err=0`。这说明两个 context、filter -> LWE -> RX 的图映射及 AXIS
Switch 路由均已正确，不是算子旁路或目的端口无效。

但 `1576960B` 恰好等于 Host 为本次任务分配的整个 output SLM：

```text
16 * 98304B + 4096B = 1576960B
```

日志中也没有出现正常的 `MCDMA_RX_FINISH_DUMP`。因此该 result 不是合法密文
长度，而是 LWE 输出在收到结束包之前填满了接收缓冲区。结合 6 条命中记录本应
只产生 `6 * 98304B = 589824B`，可判定 filter 每个单字节 quantity beat 在
LWE 的 packed-u8 模式中仍被展开成了多个明文字节。当前证据能够确认板上链路的
逐字节有效性语义未可靠保持，但在没有 ILA 波形的情况下，不把具体丢失点武断
归因于某一个 IP。

为消除多算子协议对稀疏 `TKEEP` 的依赖，同时保持已有批量加密行为不变，本次
新增独立输入模式：

```text
LWE_ENCRYPT_INPUT_U8_RADIX_SCALAR_STREAM = 3
```

其协议定义为：每个 `TUSER=0` 的 AXIS beat 恰好代表一个被选中的 u8，明文位于
`TDATA[7:0]`；LWE 只加密该字节，不再根据 `TKEEP` 统计该 beat 的明文数量；
`TUSER=0xff` 仍作为独立结束包透传。selective Host 程序的 LWE context 改为
mode 3。原有 standalone LWE 使用的 mode 2 保持不变，仍支持每拍最多 64 个
连续 u8，因此本修复不会改变既有 1B/128B 加密接口。

本地验证结果：

- 原有 LWE mode 2 的 65B HPU-native CSim 通过；
- selective_filter -> LWE mode 3 联合 CSim 通过；测试中还故意把 filter
  payload 的 `TKEEP` 改成全 1，确认每拍仍只加密一个 quantity；
- selective Host demo 重新编译通过；
- LWE C synthesis 和 RTL 导出通过，估算 Fmax 为 `298.78MHz`；
- 新 IP ZIP SHA-256：
  `cc45c2d9bf9723aaef2984a927a12ac027588610ecc2a47741e59fe92bdd8d39`；
- 算子池顶层 `lwe_encrypt.v` SHA-256：
  `512f06ffc93ab36a85a57f8d03fdb1d547607d831a4c19fcbc878e5f2bfd63af`；
- 旧算子池 RTL 已移出 Vivado 综合源码树，备份于
  `/home/yangchenghui/suda_backups/rtl/lwe_encrypt.backup.scalar_stream_20260903/`。

补充修复了 selective_filter HLS 脚本：增加 `unset env(DEBUG)`，避免外部
`DEBUG=release` 被 Vitis HLS 2020.2 当作裸 `g++` 参数。COSIM 已完成 C test、
RTL testbench 生成和 XSIM elaboration，但最终仍被本机既有的 XSIM
`ERROR: unknown error occurred` 阻断，不能标记为 RTL COSIM clean pass。

下一版完整 bitstream 上板后的验收条件为：

```text
LWE context staged_u32=[2048,0,3,0,17,1]
HLSACC_REQ_CB result=589824
selected_count=6
total_count=16
```

Host 还必须完成 6 个密文的解密校验，恢复出的 quantity 顺序应为：

```text
58,44,45,41,54,39
```

## 17. Scalar-stream 完整构建伪成功诊断与构建防护（2026-09-04）

检查 `build_bd_20260903_selective_scalar_stream.log` 时发现，日志末尾虽然包含：

```text
write_bitstream completed successfully
[INFO] : Bootimage generated successfully
```

但前面的 accframework DCP 阶段已经发生两个实质错误：

```text
ERROR: [BD::TCL 103-2021] The following module(s) are not found in the project: lwe_encrypt
ERROR: [Netlist 29-77] ... accframework_wrapper.edf ... port interface mismatch
```

对应的 `accframework.dcp` 只有 `11049B`，并带有 unresolved black box；正常
accframework DCP 应为 MB 级。与此同时，最终实现使用的 shell `synth.dcp`
时间戳为 `2026-09-03 12:33`，早于新版 LWE RTL 的 `16:44`，证明后半段复用
了旧综合结果。该次生成的 `BOOT.bin` SHA-256 为：

```text
7c31d55e8241107b6bd93694bda7dace84d517a0931f69cfff5973e505cf77e0
```

此文件只能说明 boot image 封装命令执行成功，不能证明 scalar-stream RTL 已被
集成，因此不得作为本次功能版本上板。

为避免误用，该文件及对应中间制品已保留并改名隔离：

```text
BOOT.bin.rejected-20260903-scalar-stream
system.bit.rejected-20260903-scalar-stream
accframework.dcp.rejected-20260903-scalar-stream
synth.dcp.stale-before-20260903-scalar-stream
```

根因包括两部分：

1. `lwe_encrypt.backup.scalar_stream_20260903/` 被放在
   `hlsaccframework/` 下，而原 `dcp_gen.tcl` 会递归加入整个目录，导致新旧
   `lwe_encrypt` 模块同时参与综合并相互覆盖。
2. Makefile 将 Vivado 输出通过 `tee` 写日志，但没有启用 `pipefail`。Vivado
   已失败时，流水线仍采用 `tee` 的返回值 0，`build_bd.sh` 因而继续使用旧 DCP
   执行实现和 boot image 封装。

完成的修复如下：

- 将 RTL 备份移至 `/home/yangchenghui/suda_backups/rtl/`，脱离综合源码树；
- `dcp_gen.tcl` 只加入算子目录当前层的 `.v` 文件，并显式跳过 backup/bak
  目录；
- Vivado Makefile 使用 Bash `pipefail`，确保 Vivado 失败会立即终止完整构建；
- `build_bd.sh` 在启动前检查源码树中的备份目录；
- 构建结束后检查 accframework DCP、shell synth DCP 和 `BOOT.bin` 必须均由
  本次任务刷新，并拒绝小于 1MiB 的可疑 accframework DCP。

修复后的静态检查确认：当前综合集合包含 55 个 accelerator Verilog 文件，
`AssScheduler`、`CtrlRspReceiver`、`OperatorController`、`selective_filter`、
`encrypt`、`lwe_encrypt` 和 `lwe_decrypt` 均只有一个顶层模块定义；未发现活动
源码目录间的重复 module declaration。

下一次完整构建必须同时满足以下条件才可标记成功：

```text
构建日志无 ERROR:
accframework.dcp 为 MB 级且时间戳属于本次构建
shell synth.dcp 时间戳属于本次构建
write_bitstream completed successfully
Bootimage generated successfully
[build_bd] 完整构建成功，关键制品均由本次任务刷新
```

Implementation timing 是否 clean 仍需独立检查，不能用 `BOOT.bin` 已生成代替。
