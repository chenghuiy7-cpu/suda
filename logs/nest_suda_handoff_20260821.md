# NEST SUDA 源码交接整理记录（2026-08-21）

## 整理目标

将当前已跑通的 LWE 加密、远端 HPU 计算和 FPGA 解密闭环从实验工作区中
提取为可审查、可复现的 Git 提交，同时保留现有构建产物和备份，不对它们
做删除或回退。

## 提交分层

1. HPU-native LWE 加密/解密 HLS 源码、测试和算子池 RTL；
2. MCDMA、AXI DMA、HLS runtime 和 ARM 交叉编译修复；
3. Host 应用、NVMQ/QDMA 诊断和 QEMU 安全配置；
4. 架构说明、测试方法、历史调试记录和基准数据。

## 排除内容

- Vivado/Vitis HLS、SPDK、DPDK 和 Cargo 构建目录；
- QEMU 磁盘镜像、内核和本机构建的二进制；
- `BOOT.bin`、DCP、bitstream、ARM `nvmf_tgt`；
- TFHE 密钥、LWE secret key、明文和密文；
- 临时备份、运行日志、对象文件和个人目录软链接。

上述机器相关文件由 NEST 的私有 artifact 清单管理，并通过 SHA-256 校验。

## 额外可移植性修复

- HLS 脚本支持 `VIVADO_ROOT`、`VITIS_HLS_ROOT` 和
  `HLS_HOST_ARCH_INCLUDE`；
- QEMU/VFIO 脚本取消固定 PCI BDF，要求显式设置
  `NEST_CSD_PCI_BDF` 并验证 Xilinx accelerator 类型；
- Host 顶层 Makefile 仅引用本次实际纳入的应用目录；
- `.gitignore` 排除密钥、二进制、备份和交叉编译产物。
