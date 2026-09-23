# LWE 解密算子时序与吞吐整改记录（2026-09-16）

## 原问题及证据

用户完成 2026-09-15 padding 版本的完整构建，实际于 09-16 凌晨完成。
BOOT.bin 已打包，但 build_bd.sh 最后返回失败，布线后报告为：
WNS=-0.178ns、TNS=-11.280ns、183 个失败端点，目标 clk_pl_1=250MHz。
最差路径由 static_var_bram3 的密钥 context 输出，经选择逻辑进入
lwe_decrypt 的 64bit dot 累加寄存器，包含 12 级逻辑。

同一版本 HLS logical_word_loop 请求 II=1，实际 II=13。
原因是系数解析/累加循环同时调用 radix 解码、u8 拼接、512bit 输出包写入
和 flush，输出 packet_byte_count 等回环依赖进入了系数热循环。
旧版 HLS 估计资源为 33130 LUT、25072 FF、0 DSP、0 BRAM_18K。
HLS 估计频率不是布线后时序保证，旧版 HLS 估计 334.56MHz 也没有避免最终失败。

## 备份及修改范围

修改前源码、头文件、测试、Tcl 脚本及旧综合报告已保存到：
`/home/yangchenghui/suda_backups/lwe_decrypt_before_dataflow_20260916/`。
算子池更新脚本仍将旧 RTL 备份移到 suda_backups/rtl，不放在综合源目录内。
本次不修改 lwe_encrypt、ARM runtime、Host 应用、子模块或 250MHz 时钟。

## 分阶段整改

1. 启动时从 context 读取四个密钥字，并装入本地 2048×1bit LUTRAM。
   每次调用均重新加载，避免前一请求的密钥残留被下一请求使用。
   HLS 报告加载耗时 2050 cycles，即 250MHz 下 8.200us。
2. 使用 DATAFLOW 分离五个过程：512bit输入序列化、布局解析和系数选择、
   内积累加、radix 解码、u8 输出打包。中间 FIFO 深度分别为16、32、4、4；
   结束标记独立 FIFO 深度4，指定为小型 SRL，避免自动映射造成过大的 BRAM 估计。
   64bit 累加器只读取 FIFO 中已选择的系数，不再在同一组合路径读取 context。
3. 第一轮 HLS 报告仍有 4.559ns 关键路径，定位为保留 AXIS beat 后动态选择
   word/keep 的大范围切片。改为读取低64bit/低8bit，再固定移位，避免动态大位宽选择。
4. 去除 Tcl 中显式的 Vitis 仿真 include 路径，让 HLS 自动选择综合/仿真头文件。
   否则新增内部 hls::stream 的构造函数会误用 STL 仿真容器，导致综合失败。
   内部结束 FIFO 用普通 FinishToken，不用仅允许作为顶层 AXIS 端口的 ap_axis 类型。
5. 数据错误先锁存，再持续消费输入直到 TUSER 高半字节非零的 SUDA 结束包。
   随后发送单个错误结束事件，使 DATAFLOW 各阶段依次退出，不提前留下上游 DMA 未消费。
6. RTL仿真发现中间版本虽显示内层 II=1，但含多个 continue/return 的外层
   解析循环被 HLS 拆成多状态控制，65B compact 输入仍耗时约12.799ms。
   因此改为单出口 while(!finished)，去除热循环内部 continue/return，
   并统一每轮一次 terms FIFO 写入。只看日志中的某个 II=1 不足以确认吞吐。
7. 完整解析循环流水化后，出现 3.474ns 的输入mux到零值检查路径，
   超过含时钟不确定性的2.920ns预算。将512bit输入序列化和64bit布局解析
   再用FIFO隔开，完整热循环保持II=1，同时恢复2.895ns估计周期。
8. COSIM编译补充旧版MPFR所需的 __gmp_const=const 宏。
   沙箱内XSIM启动报 unknown error，沙箱外相同snapshot已进入并完成六次RTL仿真；
   说明本次启动失败与沙箱运行限制有关，不能据此判定算子数据逻辑失败。

## 保持的布局和错误检查

| layout | 每个 u8 的输入大小 | 说明 |
|---|---:|---|
| 0 / cpu | 65568B | 2048个 mask u64 + body，4块紧密连续；块边界可跨 AXIS beat |
| 1 / hpu-native | 98304B | PC0/PC1 固定槽位、bit-reversal/interleave；槽位内部 padding 不参与内积 |
| 2 / cpu-padded | 65792B | 每块 body 后有56B零 padding；完整消费 padding 后才更新记录完成计数 |

layout 0/2 在已知 input_count 后允许末尾 LBA 补零，不把零值当作自动布局标识。
逐块 padded 区和尾部出现非零额外数据报错6，部分 u64 TKEEP 报错7，
截断块或数量不符报错5。原有参数校验错误1至4保留。
native 流现在明确要求每个输入字具有完整 u64 TKEEP，避免静默读取无效字。
参数在流启动前校验失败时仍沿用原行为直接返回错误；运行中数据错误才执行 drain。

## 当前 HLS 验证结果

`hls_dataflow_csynth_final_20260916.log` 和对应报告显示：

| 阶段 | 目标 II | 实际 II |
|---|---:|---:|
| 密钥加载 | 1 | 1 |
| serialize_word_loop | 1 | 1 |
| parse_word_loop | 1 | 1 |
| accumulate_word_loop | 1 | 1 |
| decode_block_loop | 1 | 1 |
| pack_clear_loop | 1 | 1 |

HLS 目标周期 4ns，估计周期 2.895ns，估计 Fmax=345.47MHz；
估计资源 13666 LUT、4652 FF、16 BRAM_18K、0 DSP、0 URAM。
上述仅为 HLS 估计，不是 Vivado 最终利用率和时序结论。

吞吐口径：稳态无停顿输入时，每拍处理一个64bit字，即250MHz下2GB/s；
512bit AXIS 接口仍保持不变，但每个 beat 分8拍消费，不宣称16GB/s算子吞吐。
128B明文的 compact 输入共有1049088字，纯解析下界约4.196ms；
HPU-native 共1572864字，下界约6.291ms，另加约8.2us密钥加载。
这些是解析下界，不是实测 execute 延迟；DDR、DMA、背压和控制成本需上板测量。

完整 C模型回归覆盖1/3/63/64/65/128B、三种布局、跨beat边界、跨输出包边界、
末尾LBA padding、逐块padding截断/损坏、部分u64 TKEEP，以及全零/全一密钥
与0/255/127/128等明文边界值，共26项全部通过。
加强检查：正常结束包的data/keep/strb/user/last/id/dest完整保留，
异常用例也必须消费完输入，不再由测试程序清空残留以掩盖提前退出。
日志：`logs/lwe_decrypt_dataflow_cmodel_20260916.log`。
RTL COSIM 使用六项缩减测试集验证有限FIFO，包含native 1B、compact 65B、
padded 3B及padding截断/损坏、部分u64 TKEEP。
最终SRL版 `hls_dataflow_cosim_srl_final_20260916.log` 已明确返回
`C/RTL co-simulation finished: PASS`，六项RTL输出均通过C post checking。
报告另存到 `logs/lwe_decrypt_dataflow_cosim_20260916.rpt`，避免后续rtl_gen重置工程丢失证据。

## RTL仿真周期与吞吐核对

逐项周期保存在 `logs/lwe_decrypt_dataflow_cosim_transactions_20260916.rpt`。
仿真时钟为4ns，报告为算子ap_start至ap_done，不是Host execute或端到端时间。

| 正常用例 | RTL周期数 | 250MHz下延迟 | 说明 |
|---|---:|---:|---|
| native 1B | 14357 | 57.428us | 12288个输入u64 + 密钥加载及控制开销 |
| compact 65B，LBA补零 | 535061 | 2.140244ms | 532992个输入u64 + 约2050周期密钥加载及少量控制 |
| padded 3B，按结束标记计数 | 26741 | 106.964us | 24672个输入u64 + 密钥加载及控制 |

compact 65B的输入实际补齐到1041个4096B LBA，即4263936B，
532992个u64；不是将C模型执行耗时或XSIM运行32秒当作硬件延迟。
去除约2050周期加载后，仿真周期接近输入字数，支持完整热循环每周期一字的结论。
中间多出口控制版同一65B用例3199760周期（12.799040ms），最终版约快5.98倍；
这仅比较本次整改的中间RTL版本与最终版本，不是对CPU或此前已上板版本的加速比。

## 生成及算子池同步

六项COSIM通过后，执行默认工程rtl_gen并同步算子池。
导出日志：`device/operators/hls/lwe_decrypt/hls_rtl_gen_dataflow_20260916.log`。
同步日志：`device/operators/hls/lwe_decrypt/update_shell_rtl_dataflow_20260916.log`。
默认工程rtl_gen已成功退出；初次IP导出触发2020.2版本core_revision日期溢出，
现有脚本自动改为安全revision=1后重打包成功，没有降低时钟或绕过设计错误。
已将全部19个Verilog文件同步到独立lwe_decrypt算子池目录，
清除仅在旧实现中使用的4个helper文件，避免旧模块继续参加综合。
同步前完整旧目录保存到：
`/home/yangchenghui/suda_backups/rtl/lwe_decrypt_before_logical_20260916_112058.dwlvty/old_shell_rtl/`。

新版算子池顶层lwe_decrypt.v SHA-256：
`714c0b0880cfacf88baeec2614226065a1317c27a78c0e5af81e38f9b48dfd81`。
算子池与默认工程syn/verilog中的全部Verilog逐文件一致；顶层AXIS/BRAM端口保留。
本次没有执行完整build_bd.sh，也没有替换ARM BOOT.bin。

## 用户最终完整构建

HLS RTL 验证、生成及算子池同步后，在 tmux 执行：

```bash
cd /home/yangchenghui/suda/device/platform/basic_shell/nf-csd
source /opt/Xilinx_2020.2/Vivado/2020.2/settings64.sh
set -o pipefail
bash build_bd.sh 2>&1 | tee build_bd_20260916_decrypt_dataflow.log
```

只有新 post_route_timing.rpt 明确包含 `Timing constraints are met.`，
且 build_bd.sh 成功退出，才判定完整构建通过。生成文件不等于时序通过。
完整 Vivado 综合/布线/BOOT.bin 生成由用户在 tmux 执行。

## 完整构建结果

用户随后执行了上述 `build_bd.sh`。本次综合、布局布线和 BOOT 镜像打包均产生了
新文件，但脚本最终返回失败，因此不能判定为可发布的完整构建。

新产物如下：

| 产物 | 生成时间 | 大小 | SHA-256 |
|---|---|---:|---|
| `shell/virt_one_drive/ready_for_download/fidus/BOOT.bin` | 2026-09-16 14:38:36 | 37518168B | `dcf691398af897352f31549b87c87fcccb5472b67ab6e73bc054b43d3565fc33` |
| `work_farm/fpga/vivado_out/shell_virt_one_drive_fidus/dcp/synth.dcp` | 2026-09-16 13:10:35 | 178431857B | `f7a27f59d46892c8e85862f8157b38b4a3aaac12e4bd2a45f6c9b0bfb329c46c` |
| `work_farm/fpga/vivado_out/shell_virt_one_drive_accframework_fidus/dcp/accframework.dcp` | 2026-09-16 12:10:31 | 27756252B | `e9ab33a80cd36d3f1524a8ddabbb7c7e78b2eda196e4e855a33b46b6cccfb20a` |

`post_route_status.rpt` 显示 551116 条需要布线的网络全部完成，routing error 为0；
失败原因不是未布线网络，而是250MHz `clk_pl_1` 时钟域仍有setup违例：

| WNS | TNS | 违例端点 | 总端点 |
|---:|---:|---:|---:|
| -0.123ns | -0.127ns | 2 | 1016498 |

最差路径从 `OperatorController_3/inst/fsm_state_reg[13]` 到该控制器内部FIFO的
`mem_reg_8/DINADIN[0]`，数据路径3.682ns，其中逻辑1.154ns、布线2.528ns，
共11级逻辑。该路径经过控制响应、AXIS ready/valid和FIFO写入逻辑，已不在
`lwe_decrypt` 的点积累加数据通路内。第二条违例仅为-0.004ns，从PCIe MMIO
AXI互连寄存器到PS8接口，同样属于shell系统级路径。

此前阻塞实现的 `context BRAM -> lwe_decrypt dot accumulator` 路径已经不再是
布线后关键路径，说明本次密钥本地化和dataflow拆分确实解决了目标算子路径；
当前剩余问题是控制器/FIFO及shell布线收敛，而不是解密算术热循环。

最终全设计资源占用为243301个CLB LUT（46.55%）、325346个CLB寄存器
（31.12%）、560个Block RAM Tile（56.91%）、49个URAM（38.28%）和10个DSP
（0.51%）。CLB使用52810/65340（80.82%），布局较拥挤，这与最差路径中
68.66%的延迟来自布线相吻合。

结论：`BOOT.bin` 虽然由Bootgen成功生成，但它没有通过布线后时序约束，
不应标记为timing-clean版本，也不建议直接替换ARM当前可用镜像。下一步应保留
现有 `lwe_decrypt` 数据流实现，优先处理 `OperatorController_3 -> FIFO` 控制路径
或调整实现/物理优化策略，然后重新执行完整构建，直至WNS和TNS均不小于0。
