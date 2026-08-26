# SUDA 2026-08-21 变更审计

审计时间：2026-08-24（Asia/Shanghai）

## 审计边界与结论

- 8 月 21 日 15:00:02 从旧基线 `28714b23a5e47b397ef1c6c7a1946a33c67f38ac` 创建 `nest` 分支。
- 当天共创建 8 个提交，最终提交为 `24ff93866699f0051d343fca77e197bdf1136217`。
- 逐提交文件并集与基线到最终提交的整树差异完全一致：117 个唯一文件，其中 89 个新增、28 个修改，没有删除文件，也没有当天修改后又恢复而被净差异隐藏的文件。
- reflog 显示当天只有建分支、8 次 commit 和对应 push，没有 reset、rebase、amend 或 checkout 到其他提交。
- 这些提交把旧 HEAD 之后工作区累计的开发内容一次性纳入版本控制；Git 可以准确证明 8 月 21 日提交了哪些内容，但不能证明每个文件的实际编辑动作都发生在当天。

## QEMU 启动脚本结论

是的，8 月 21 日的 `21fc64ed4556bb7b364f75d79dd6d5541e3aa655` 同时修改了：

- `host/qemu/bind_vfio.sh`
- `host/qemu/run_qemu.sh`

具体变化：

- `bind_vfio.sh`：从固定绑定 `0000:3b:00.0`，改成由参数或 `NEST_CSD_PCI_BDF` 指定，并增加 Xilinx vendor/class、VFIO driver 和 IOMMU group 检查。
- `run_qemu.sh`：增加可配置镜像、内核、QEMU 路径、SUDA 根目录、SSH 端口和 PCI BDF；增加 VFIO/IOMMU 校验；新增 `-device vfio-pci,host=...` 将选定卡传入 QEMU。
- 8 月 21 日之前 Git 基线中的 `run_qemu.sh` 没有 `-device vfio-pci` 行；基线 `bind_vfio.sh` 固定的是 `3b:00.0`。
- 当前工作区不是 8 月 21 日提交版本：按后续恢复要求，两个脚本已改回简化形式并固定为 `d9:00.0`；当前 `run_qemu.sh` 第 87 行传入 `d9:00.0`。

当前故障关联证据：

- 当前宿主机 `3b:00.0` 位于物理槽位 0、绑定 `xdma`，BAR 为 32 MiB/64 KiB/16 GiB；这是已明确不能影响的其他用户 XDMA 卡。
- 当前 `86:00.0` 位于物理槽位 4、未绑定，BAR0/BAR1/BAR2 分别为 256 KiB/4 MiB/4 MiB。
- 当前 `d9:00.0` 位于物理槽位 8、绑定 `vfio-pci`，BAR0/BAR2/BAR4 分别为 256 KiB/2 GiB/16 GiB。
- 历史已知成功的 Guest 探测日志为 `reg_addr 10C is 2`、`user_bar_id is 2`，并识别 AXI Master Lite BAR 1、AXI Bridge Master BAR 2；当前透传 `d9:00.0` 后则为 `4/4`，只打印 AXI Master Lite BAR 2。
- 因此 BDF 选择会直接影响当前故障，且现有证据更支持“重新验证 `86:00.0` 是否才是曾经跑通的目标卡”，不能仅凭文档中的 `d9:00.0` 示例把它永久硬编码。该差异仍需通过物理槽位或一次受控的 `86:00.0` 透传测试最终确认。

## 提交统计

| 提交 | 时间 | 文件数 | 说明 |
|---|---|---:|---|
| `e54de5489f` | 2026-08-21T15:05:03+08:00 | 35 | Add HPU-native LWE encrypt and decrypt operators |
| `3bf52fd801` | 2026-08-21T15:05:49+08:00 | 12 | Fix MCDMA streaming and LWE runtime integration |
| `21fc64ed45` | 2026-08-21T15:08:06+08:00 | 35 | Add LWE host applications and QDMA diagnostics |
| `aeefbe5485` | 2026-08-21T15:10:44+08:00 | 31 | Document NEST LWE pipeline and benchmark results |
| `d743be23c3` | 2026-08-21T15:15:06+08:00 | 3 | Improve SUDA checkout portability and artifact hygiene |
| `d64fbe7acf` | 2026-08-21T15:25:32+08:00 | 3 | Make NVMQ build independent of checkout path |
| `7728447ad9` | 2026-08-21T15:41:30+08:00 | 3 | Document and validate the laboratory LWE keyset |
| `24ff938666` | 2026-08-21T16:27:58+08:00 | 6 | Add runnable LWE operator quick-start commands |

## 117 个唯一文件完整清单

状态：`A` 为新增，`M` 为修改。

```text
M	.gitignore
M	.gitmodules
M	README.md
M	device/operators/Makefile
A	device/operators/hls/lwe_decrypt/lwe_decrypt.cpp
A	device/operators/hls/lwe_decrypt/lwe_decrypt.hpp
A	device/operators/hls/lwe_decrypt/run_hls.tcl
A	device/operators/hls/lwe_decrypt/test.cpp
A	device/operators/hls/lwe_encrypt/lwe_encrypt.cpp
A	device/operators/hls/lwe_encrypt/lwe_encrypt.hpp
A	device/operators/hls/lwe_encrypt/run_hls.tcl
A	device/operators/hls/lwe_encrypt/test.cpp
A	device/operators/hls/lwe_encrypt/test_deep.cpp
A	device/operators/hls/lwe_encrypt/testdata/README.md
A	device/operators/hls/lwe_encrypt/testdata/install_validated_keyset.sh
M	device/operators/scripts/toolset.mk
M	device/platform/basic_shell/nf-csd/build_bd.sh
M	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/scripts/accframework.tcl
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt_flush_clear_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt_forward_done_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt_write_clear_byte.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt_write_error_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_encoded_lwe.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_encoded_lwe_hpu_native.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_encoded_lwe_hpu_native_pc0_V.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_one_lwe.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_u8_radix.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_flush_output_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_forward_done_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_mul_64s_64s_64_5_1.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_regslice_both.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_write_error_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_write_hpu_native_pc_slot.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_write_output_word.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_write_synthetic_done_packet.v
M	device/platform/basic_shell/nf-csd/shell/virt_one_drive/scripts/prj_setup.tcl
M	device/platform/software_stack/nf_spdk/config.json
A	device/platform/software_stack/nf_spdk/config_legacy_blowfish.json
M	device/platform/software_stack/nf_spdk/dpdk/drivers/bus/axi_dma/rte_axi_dma.c
M	device/platform/software_stack/nf_spdk/dpdk/drivers/bus/axi_dma/rte_axi_dma.h
M	device/platform/software_stack/nf_spdk/include/spdk/env.h
M	device/platform/software_stack/nf_spdk/include/spdk/hlsacccompute.h
M	device/platform/software_stack/nf_spdk/lib/axi_dma/axi_dma.c
M	device/platform/software_stack/nf_spdk/lib/env_dpdk/env.mk
M	device/platform/software_stack/nf_spdk/lib/hlsacccompute/hlsacccompute.c
M	device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
M	device/platform/software_stack/nf_spdk/mcdma/run_nvmq.sh
M	device/platform/software_stack/nf_spdk/scripts/arm_cross_compile.sh
A	docs/architecture/CSD到远端HPU端到端模块说明.md
A	docs/architecture/LWE加密算子原型开发与测试总结.md
A	docs/architecture/LWE加密算子性能测试方法.md
A	docs/architecture/LWE远程HPU计算通路.md
A	docs/user-guides/LWE远程HPU部署与运行命令.md
A	docs/user-guides/NEST_SUDA交接说明.md
M	host/applications/Makefile
M	host/applications/vscode-blowfish-offload/vscode-blowfish-offload.cpp
A	host/applications/vscode-lwe-decrypt-offload/Makefile
A	host/applications/vscode-lwe-decrypt-offload/README.md
A	host/applications/vscode-lwe-decrypt-offload/vscode-lwe-decrypt-offload.cpp
A	host/applications/vscode-lwe-encrypt-data-gen/Makefile
A	host/applications/vscode-lwe-encrypt-data-gen/README.md
A	host/applications/vscode-lwe-encrypt-data-gen/vscode-lwe-encrypt-data-gen.cpp
A	host/applications/vscode-lwe-encrypt-offload/Makefile
A	host/applications/vscode-lwe-encrypt-offload/README.md
A	host/applications/vscode-lwe-encrypt-offload/run_fpga_bench.sh
A	host/applications/vscode-lwe-encrypt-offload/run_slm_queue_depth_sweep.sh
A	host/applications/vscode-lwe-encrypt-offload/run_slm_read_sweep.sh
A	host/applications/vscode-lwe-encrypt-offload/summarize_benchmark.py
A	host/applications/vscode-lwe-encrypt-offload/summarize_slm_request_trace.py
A	host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp
A	host/applications/vscode-lwe-encrypt-remote-offload/Makefile
A	host/applications/vscode-lwe-encrypt-remote-offload/README.md
A	host/applications/vscode-lwe-encrypt-remote-offload/lwe_remote_protocol.cpp
A	host/applications/vscode-lwe-encrypt-remote-offload/lwe_remote_protocol.hpp
A	host/applications/vscode-lwe-encrypt-remote-offload/prepare_ablation_plot_data.py
A	host/applications/vscode-lwe-encrypt-remote-offload/run_remote_pipeline_bench.sh
A	host/applications/vscode-lwe-encrypt-remote-offload/summarize_remote_pipeline.py
A	host/applications/vscode-lwe-encrypt-remote-offload/test_protocol.cpp
A	host/applications/vscode-lwe-encrypt-remote-offload/vscode-lwe-encrypt-remote-offload.cpp
A	host/applications/vscode-lwe-full-pipeline/Makefile
A	host/applications/vscode-lwe-full-pipeline/README.md
A	host/applications/vscode-lwe-full-pipeline/vscode-lwe-full-pipeline.cpp
M	host/drivers/nvmq/Makefile
A	host/drivers/nvmq/diagnose_qdma.sh
M	host/drivers/nvmq/init_nvmq.sh
M	host/drivers/nvmq/nvmq0_opts
M	host/drivers/nvmq/qdma.c
M	host/drivers/nvmq/trace.h
M	host/qemu/bind_vfio.sh
M	host/qemu/run_qemu.sh
A	logs/lwe_cpu_hpu_native_serial_20260817.csv
A	logs/lwe_cpu_serial_20260810.csv
A	logs/lwe_decrypt_operator_20260819.md
A	logs/lwe_encrypt_benchmark_20260810.md
A	logs/lwe_encrypt_context_offset_fix_20260710.md
A	logs/lwe_encrypt_hpu_mockup_test_20260713.md
A	logs/lwe_encrypt_hpu_native_layout_20260813.md
A	logs/lwe_encrypt_operator_2026-06-09.md
A	logs/lwe_encrypt_stream_fix_2026-06-26.md
A	logs/lwe_encrypt_verilog_pool_20260702.sha256
A	logs/lwe_fpga_256lba.csv
A	logs/lwe_fpga_hpu_native_20260817.csv
A	logs/lwe_full_pipeline_20260820.md
A	logs/lwe_hpu_native_benchmark_alignment_20260817.md
A	logs/lwe_hpu_native_benchmark_results_20260817.md
A	logs/lwe_hpu_native_speedup_20260817.md
A	logs/lwe_remote_hpu_20260717.md
A	logs/lwe_remote_hpu_native_timeout_fix_20260817.md
A	logs/lwe_remote_input_lba_auto_20260813.md
A	logs/lwe_remote_pipeline_benchmark_20260812.md
A	logs/lwe_runtime_finish_completion_fix_20260713.md
A	logs/lwe_runtime_rx_batch_20260811.md
A	logs/lwe_slm_read_chunk_optimization_20260811.md
A	logs/lwe_speedup_256lba.md
A	logs/nest_suda_handoff_20260821.md
```

## 逐提交完整清单

### e54de5489f Add HPU-native LWE encrypt and decrypt operators

时间：2026-08-21T15:05:03+08:00

```text
M	.gitignore
M	device/operators/Makefile
A	device/operators/hls/lwe_decrypt/lwe_decrypt.cpp
A	device/operators/hls/lwe_decrypt/lwe_decrypt.hpp
A	device/operators/hls/lwe_decrypt/run_hls.tcl
A	device/operators/hls/lwe_decrypt/test.cpp
A	device/operators/hls/lwe_encrypt/lwe_encrypt.cpp
A	device/operators/hls/lwe_encrypt/lwe_encrypt.hpp
A	device/operators/hls/lwe_encrypt/run_hls.tcl
A	device/operators/hls/lwe_encrypt/test.cpp
A	device/operators/hls/lwe_encrypt/test_deep.cpp
A	device/operators/hls/lwe_encrypt/testdata/README.md
M	device/operators/scripts/toolset.mk
M	device/platform/basic_shell/nf-csd/build_bd.sh
M	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/scripts/accframework.tcl
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt_flush_clear_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt_forward_done_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt_write_clear_byte.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt/lwe_decrypt_write_error_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_encoded_lwe.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_encoded_lwe_hpu_native.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_encoded_lwe_hpu_native_pc0_V.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_one_lwe.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_encrypt_u8_radix.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_flush_output_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_forward_done_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_mul_64s_64s_64_5_1.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_regslice_both.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_write_error_packet.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_write_hpu_native_pc_slot.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_write_output_word.v
A	device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_encrypt/lwe_encrypt_write_synthetic_done_packet.v
M	device/platform/basic_shell/nf-csd/shell/virt_one_drive/scripts/prj_setup.tcl
```

### 3bf52fd801 Fix MCDMA streaming and LWE runtime integration

时间：2026-08-21T15:05:49+08:00

```text
M	device/platform/software_stack/nf_spdk/config.json
A	device/platform/software_stack/nf_spdk/config_legacy_blowfish.json
M	device/platform/software_stack/nf_spdk/dpdk/drivers/bus/axi_dma/rte_axi_dma.c
M	device/platform/software_stack/nf_spdk/dpdk/drivers/bus/axi_dma/rte_axi_dma.h
M	device/platform/software_stack/nf_spdk/include/spdk/env.h
M	device/platform/software_stack/nf_spdk/include/spdk/hlsacccompute.h
M	device/platform/software_stack/nf_spdk/lib/axi_dma/axi_dma.c
M	device/platform/software_stack/nf_spdk/lib/env_dpdk/env.mk
M	device/platform/software_stack/nf_spdk/lib/hlsacccompute/hlsacccompute.c
M	device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
M	device/platform/software_stack/nf_spdk/mcdma/run_nvmq.sh
M	device/platform/software_stack/nf_spdk/scripts/arm_cross_compile.sh
```

### 21fc64ed45 Add LWE host applications and QDMA diagnostics

时间：2026-08-21T15:08:06+08:00

```text
M	host/applications/Makefile
M	host/applications/vscode-blowfish-offload/vscode-blowfish-offload.cpp
A	host/applications/vscode-lwe-decrypt-offload/Makefile
A	host/applications/vscode-lwe-decrypt-offload/README.md
A	host/applications/vscode-lwe-decrypt-offload/vscode-lwe-decrypt-offload.cpp
A	host/applications/vscode-lwe-encrypt-data-gen/Makefile
A	host/applications/vscode-lwe-encrypt-data-gen/README.md
A	host/applications/vscode-lwe-encrypt-data-gen/vscode-lwe-encrypt-data-gen.cpp
A	host/applications/vscode-lwe-encrypt-offload/Makefile
A	host/applications/vscode-lwe-encrypt-offload/README.md
A	host/applications/vscode-lwe-encrypt-offload/run_fpga_bench.sh
A	host/applications/vscode-lwe-encrypt-offload/run_slm_queue_depth_sweep.sh
A	host/applications/vscode-lwe-encrypt-offload/run_slm_read_sweep.sh
A	host/applications/vscode-lwe-encrypt-offload/summarize_benchmark.py
A	host/applications/vscode-lwe-encrypt-offload/summarize_slm_request_trace.py
A	host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.cpp
A	host/applications/vscode-lwe-encrypt-remote-offload/Makefile
A	host/applications/vscode-lwe-encrypt-remote-offload/README.md
A	host/applications/vscode-lwe-encrypt-remote-offload/lwe_remote_protocol.cpp
A	host/applications/vscode-lwe-encrypt-remote-offload/lwe_remote_protocol.hpp
A	host/applications/vscode-lwe-encrypt-remote-offload/prepare_ablation_plot_data.py
A	host/applications/vscode-lwe-encrypt-remote-offload/run_remote_pipeline_bench.sh
A	host/applications/vscode-lwe-encrypt-remote-offload/summarize_remote_pipeline.py
A	host/applications/vscode-lwe-encrypt-remote-offload/test_protocol.cpp
A	host/applications/vscode-lwe-encrypt-remote-offload/vscode-lwe-encrypt-remote-offload.cpp
A	host/applications/vscode-lwe-full-pipeline/Makefile
A	host/applications/vscode-lwe-full-pipeline/README.md
A	host/applications/vscode-lwe-full-pipeline/vscode-lwe-full-pipeline.cpp
A	host/drivers/nvmq/diagnose_qdma.sh
M	host/drivers/nvmq/init_nvmq.sh
M	host/drivers/nvmq/nvmq0_opts
M	host/drivers/nvmq/qdma.c
M	host/drivers/nvmq/trace.h
M	host/qemu/bind_vfio.sh
M	host/qemu/run_qemu.sh
```

### aeefbe5485 Document NEST LWE pipeline and benchmark results

时间：2026-08-21T15:10:44+08:00

```text
A	docs/architecture/CSD到远端HPU端到端模块说明.md
A	docs/architecture/LWE加密算子原型开发与测试总结.md
A	docs/architecture/LWE加密算子性能测试方法.md
A	docs/architecture/LWE远程HPU计算通路.md
A	docs/user-guides/LWE远程HPU部署与运行命令.md
A	docs/user-guides/NEST_SUDA交接说明.md
A	logs/lwe_cpu_hpu_native_serial_20260817.csv
A	logs/lwe_cpu_serial_20260810.csv
A	logs/lwe_decrypt_operator_20260819.md
A	logs/lwe_encrypt_benchmark_20260810.md
A	logs/lwe_encrypt_context_offset_fix_20260710.md
A	logs/lwe_encrypt_hpu_mockup_test_20260713.md
A	logs/lwe_encrypt_hpu_native_layout_20260813.md
A	logs/lwe_encrypt_operator_2026-06-09.md
A	logs/lwe_encrypt_stream_fix_2026-06-26.md
A	logs/lwe_encrypt_verilog_pool_20260702.sha256
A	logs/lwe_fpga_256lba.csv
A	logs/lwe_fpga_hpu_native_20260817.csv
A	logs/lwe_full_pipeline_20260820.md
A	logs/lwe_hpu_native_benchmark_alignment_20260817.md
A	logs/lwe_hpu_native_benchmark_results_20260817.md
A	logs/lwe_hpu_native_speedup_20260817.md
A	logs/lwe_remote_hpu_20260717.md
A	logs/lwe_remote_hpu_native_timeout_fix_20260817.md
A	logs/lwe_remote_input_lba_auto_20260813.md
A	logs/lwe_remote_pipeline_benchmark_20260812.md
A	logs/lwe_runtime_finish_completion_fix_20260713.md
A	logs/lwe_runtime_rx_batch_20260811.md
A	logs/lwe_slm_read_chunk_optimization_20260811.md
A	logs/lwe_speedup_256lba.md
A	logs/nest_suda_handoff_20260821.md
```

### d743be23c3 Improve SUDA checkout portability and artifact hygiene

时间：2026-08-21T15:15:06+08:00

```text
M	.gitignore
M	.gitmodules
M	docs/user-guides/NEST_SUDA交接说明.md
```

### d64fbe7acf Make NVMQ build independent of checkout path

时间：2026-08-21T15:25:32+08:00

```text
M	host/drivers/nvmq/Makefile
M	host/drivers/nvmq/init_nvmq.sh
M	host/drivers/nvmq/trace.h
```

### 7728447ad9 Document and validate the laboratory LWE keyset

时间：2026-08-21T15:41:30+08:00

```text
M	.gitignore
A	device/operators/hls/lwe_encrypt/testdata/install_validated_keyset.sh
M	docs/user-guides/NEST_SUDA交接说明.md
```

### 24ff938666 Add runnable LWE operator quick-start commands

时间：2026-08-21T16:27:58+08:00

```text
M	README.md
M	docs/user-guides/NEST_SUDA交接说明.md
M	host/applications/vscode-lwe-decrypt-offload/README.md
M	host/applications/vscode-lwe-encrypt-data-gen/README.md
M	host/applications/vscode-lwe-encrypt-offload/README.md
M	host/applications/vscode-lwe-full-pipeline/README.md
```

## 被 Git 忽略但 mtime 落在 8 月 21 日的文件

以下 54 项不属于 Git 提交差异，主要是 HLS/应用编译产物。这里只作为补充候选保留；mtime 可能因复制时保留时间戳而产生误导，不能据此断言当天实际编辑过。

```text
backups/nvmq_pre_diag_rollback_20260824/nvmq.ko
device/operators/hls/lwe_decrypt/lwe_decrypt/hls.app
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/.autopilot/.autopilot_exit
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/.autopilot/db/.message_csim.xml
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/.autopilot/db/autopilot.flow.log
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/.autopilot/db/dsp_style
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/.lst_opt.tcl
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/build/csim.exe
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/build/csim.mk
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/build/obj/.dir
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/build/obj/lwe_decrypt.d
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/build/obj/lwe_decrypt.o
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/build/obj/test.d
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/build/obj/test.o
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/build/run_sim.tcl
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/build/sim.sh
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/csim/report/lwe_decrypt_csim.log
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/solution1.aps
device/operators/hls/lwe_decrypt/lwe_decrypt/solution1/solution1.log
device/operators/hls/lwe_decrypt/vitis_hls.log
device/operators/hls/lwe_encrypt/lwe_encrypt/hls.app
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/.autopilot/.autopilot_exit
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/.autopilot/db/.message_csim.xml
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/.autopilot/db/autopilot.flow.log
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/.autopilot/db/dsp_style
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/.lst_opt.tcl
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/build/csim.exe
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/build/csim.mk
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/build/obj/.dir
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/build/obj/lwe_encrypt.d
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/build/obj/lwe_encrypt.o
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/build/obj/test.d
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/build/obj/test.o
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/build/run_sim.tcl
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/build/sim.sh
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/csim/report/lwe_encrypt_csim.log
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/solution1.aps
device/operators/hls/lwe_encrypt/lwe_encrypt/solution1/solution1.log
device/operators/hls/lwe_encrypt/vitis_hls.log
host/applications/vscode-lwe-decrypt-offload/vscode-lwe-decrypt-offload
host/applications/vscode-lwe-decrypt-offload/vscode-lwe-decrypt-offload.o
host/applications/vscode-lwe-encrypt-data-gen/vscode-lwe-encrypt-data-gen
host/applications/vscode-lwe-encrypt-data-gen/vscode-lwe-encrypt-data-gen.o
host/applications/vscode-lwe-encrypt-offload/__pycache__/summarize_benchmark.cpython-313.pyc
host/applications/vscode-lwe-encrypt-offload/__pycache__/summarize_slm_request_trace.cpython-313.pyc
host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload
host/applications/vscode-lwe-encrypt-offload/vscode-lwe-encrypt-offload.o
host/applications/vscode-lwe-encrypt-remote-offload/__pycache__/prepare_ablation_plot_data.cpython-313.pyc
host/applications/vscode-lwe-encrypt-remote-offload/__pycache__/summarize_remote_pipeline.cpython-313.pyc
host/applications/vscode-lwe-encrypt-remote-offload/lwe_remote_protocol.o
host/applications/vscode-lwe-encrypt-remote-offload/test_protocol
host/applications/vscode-lwe-encrypt-remote-offload/test_protocol.o
host/applications/vscode-lwe-encrypt-remote-offload/vscode-lwe-encrypt-remote-offload
host/applications/vscode-lwe-encrypt-remote-offload/vscode-lwe-encrypt-remote-offload.o
```

## 审计时当前未提交状态

以下内容发生在 `24ff938` 之后，不属于上述 8 月 21 日提交清单；保留在此处是为了避免后续混淆：

```text
 M README.md
 M device/operators/hls/grep/run_hls.tcl
 M device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c
 M docs/user-guides/NEST_SUDA交接说明.md
 M host/applications/vscode-lwe-full-pipeline/README.md
 M host/applications/vscode-lwe-full-pipeline/vscode-lwe-full-pipeline.cpp
 M host/drivers/nvmq/init_nvmq.sh
 M host/drivers/nvmq/qdma.c
 M host/drivers/nvmq/tests/single_write.fio
 m host/kernel/source
 M host/qemu/bind_vfio.sh
 M host/qemu/bzImage
 m host/qemu/qemu
 M host/qemu/run_qemu.sh
?? device/operators/hls/hwgrep/
?? host/applications/vscode-tfhe-mul/
?? logs/lwe_slm_to_ssd_direct_20260824.md
?? logs/nvmq_diag_rollback_20260824.md
```

## 文件时间戳补充

排除 `.git` 后，当前工作树中有 66 个文件的 mtime 落在 8 月 21 日：12 个受 Git 管理、54 个被忽略的 HLS/编译产物、0 个非忽略未跟踪文件。mtime 不能用于确认真实编辑日期，例如 8 月 24 日复制到备份目录的文件可能保留 8 月 21 日原始时间，因此最终完整清单以 Git 对象和 reflog 为准。
