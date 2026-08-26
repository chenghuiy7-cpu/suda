# LWE 解密结果从 output SLM 直接写 SSD 的修改记录

日期：2026-08-24

## 目标

将完整通路末端从：

```text
FPGA decrypt -> output SLM -> Host buffer -> NVMe Write -> SSD
```

改为：

```text
FPGA decrypt -> output SLM -> NVMe Copy(format=4) -> SSD
```

Host 回读不再参与实际落盘，仅作为默认开启、可通过
`--skip-ssd-readback` 关闭的正确性验证。

## ARM runtime 修改

文件：`device/platform/software_stack/nf_spdk/lib/nvmf/mcdma.c`

原有 Copy format 4 分支属于未完成原型，存在以下问题：

1. 内部 SSD WRITE 的目标 namespace 固定为 1，没有使用外层 Copy 命令的目标 NSID。
2. `ctx->cur_bytes` 同时混用目标 LBA 和字节数，多 range 时目标地址会错误。
3. PRP2 对两页和多页数据的构造不符合 NVMe PRP 规则。
4. 源 SLM offset 被重复累加。
5. `spdk_nvme_kernel` 和内部命令被塞入单个 4KB pool 对象，存在越界风险。
6. 缺少 range 数量、4KB 对齐、SLM 查找、物理地址转换和内存分配检查。

本次实现为每个 source range 顺序生成一个内部 NVMe WRITE：

- 目标 NSID 和起始 LBA来自外层 Copy 命令；
- 目标 LBA按 `nbyte / 4096` 递增；
- 一页使用 PRP1，两页使用 PRP1+直接 PRP2，三页及以上使用 PRP list；
- source range 必须按 4KB 对齐；
- 最多支持 64 个 range；
- 内部 kernel、命令数组和 DMA PRP page 分开分配并在完成/错误路径释放；
- 增加中文 `SLM_TO_SSD_COPY` 开始、完成和错误日志。

## Host 完整流水线修改

文件：
`host/applications/vscode-lwe-full-pipeline/vscode-lwe-full-pipeline.cpp`

修改内容：

1. 创建 decrypt output SLM 后先调用 `nvme_slm_fill()` 清零，保证目标 SSD 页的无效尾部为零。
2. FPGA 解密完成后，不再调用 `nvme_slm_read()` 把 output SLM 搬到 Host。
3. 应用构造 `nvme_mc_source_range`，通过 `nvme_copy()` 的 format 4 把 output SLM 直接写入目标 SSD。
4. 默认从目标 SSD 回读并与远端 HPU 操作的预期明文比较。
5. `--skip-ssd-readback` 关闭回读；关闭后不能使用 `--plaintext-output`。
6. benchmark 将末端拆分为 `slm_zero`、`slm_to_ssd`、`ssd_readback`，CSV 标识更新为 `BENCH_FULL_PIPELINE_CSV_V2`。

## 构建检查

Host 应用：

```bash
cd /home/yangchenghui/suda/host/applications/vscode-lwe-full-pipeline
make -j8
```

结果：编译和链接成功。仅出现 libnvme 头文件中已有的 warning。

ARM runtime：

```bash
cd /home/yangchenghui/suda/device/platform/software_stack/nf_spdk
bash scripts/arm_cross_compile.sh
```

结果：AArch64 `nvmf_tgt` 交叉编译和链接成功。

本次构建产物：

```text
nvmf_tgt SHA-256:
55f679fef9c53f7e00301c6adc04ebb3861710740f3abdb0d2de0230724d5595

vscode-lwe-full-pipeline SHA-256:
402a4f5293ff355e3a7f753f5a023c27b6f69af0735b896f3531c33d24c21c87
```

## 上板验证要求

1. 先备份 ARM 当前 `nvmf_tgt`，替换为上述新产物并重启 runtime。
2. 重启 QEMU/NVMQ，避免旧请求和 QDMA queue 状态残留。
3. 先使用默认回读模式运行 1B，再运行 128B，检查目标 SSD 内容与预期一致。
4. 正确性通过后，增加 `--skip-ssd-readback` 测量不含 Host 回读的真实落盘延迟。
5. ARM 日志应出现 `SLM_TO_SSD_COPY 开始` 和 `SLM_TO_SSD_COPY 完成`，且 completion status 为 0。

本地构建只能验证编译和链接，真实 SLM、NVMe Copy、SSD 数据正确性仍需在 Fidus、ARM runtime 和 QEMU 环境中完成。
