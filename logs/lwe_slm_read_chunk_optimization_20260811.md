# LWE 输出 SLM 回读分块优化记录

日期：2026-08-11

## 1. 问题背景

128B 连续明文加密的分阶段结果为：

| 阶段 | 延迟 |
|---|---:|
| FPGA execute | 22.180 ms |
| output SLM -> Host | 35121.063 ms |
| data path | 35174.382 ms |
| process | 35344.492 ms |

本次输出 SLM 大小为 8425472B。旧程序固定使用 4096B 的同步
`nvme_slm_read`，共发出 2057 个请求，平均每个请求约 17ms。因此当前端到端
瓶颈并非 LWE 计算，而是大量小粒度 SLM 回读请求及其控制面往返。

## 2. 优化范围

本轮只修改 Host 应用和 benchmark 工具，不修改 HLS、FPGA 比特流、ARM
`nvmf_tgt` 或 NVMQ 驱动：

1. `vscode-lwe-encrypt-offload` 新增 `--slm-read-chunk-bytes`。
2. 参数范围固定为 4096B 到 131072B，且必须按 4096B 对齐。
3. 默认值由旧版 4096B 调整为 131072B；可显式传入 4096 恢复旧行为。
4. 128B 明文对应的回读请求数由 2057 次减少为 65 次。
5. benchmark CSV 增加 SLM 分块大小及全部阶段延迟，避免不同策略混合统计。
6. 新增 `run_slm_read_sweep.sh`，默认扫描 128KB、32KB；4KB、8KB
   仅在显式指定时测试。
7. 汇总脚本增加 SLM 回读中位数、P95 和实际 MiB/s，并兼容旧版 CSV。

选择 128KB 作为上限的依据是 MCDMA transport 中
`SPDK_NVMF_MCDMA_DEFAULT_MAX_IO_SIZE=131072`。SLM_READ 的 PRP-list 路径能够
处理多页目标缓冲区；本次仍采用单请求同步回读，暂未引入并发或 io_uring，
以便先隔离“请求粒度”这一变量。

## 3. 本地验证

- Host C++ 应用编译通过。
- `--help` 正确显示新参数。
- 非 4KB 对齐值能够被参数检查拒绝。
- 两个 shell 脚本通过 `bash -n`。
- Python 汇总脚本通过语法检查。
- 旧版 benchmark CSV 可继续读取；新版 CSV 的分块维度和 SLM 指标可正确输出。

本地环境无法替代真实 QEMU、NVMQ、ARM runtime 和 FPGA 数据通路测试，因此
性能收益及 128KB 请求稳定性必须在板卡环境确认。

## 4. 板卡测试顺序

先运行单次 128KB 回读：

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-offload

./vscode-lwe-encrypt-offload \
  --ssd-nsid 1 \
  --ssd-lba 65536 \
  --input-lbas 256 \
  --plaintext-bytes 128 \
  --slm-read-chunk-bytes 131072 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --benchmark \
  --skip-dump
```

成功后扫描不同请求大小：

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-offload

BATCH_SIZES=128 \
WARMUP=1 \
ITERATIONS=3 \
INPUT_LBAS=256 \
OUTPUT_CSV=slm_read_sweep_128b.csv \
./run_slm_read_sweep.sh
```

若大块请求失败或阻塞，先恢复 NVMQ/ARM runtime 的干净状态，再显式使用
`--slm-read-chunk-bytes 4096` 验证旧路径。扫描脚本按分块大小逐组保存数据，
后续组失败不会丢失已经写入 CSV 的样本。

第一次全粒度扫描中，4KB 组在完成预热和两次测量后，第三次测量的 SLM
回读阻塞超过 20 分钟。Host 内核仍记录到 `C2H completion len=4160`，即一次
4096B 数据及 64B 协议开销已经到达，但用户态同步 ioctl 未结束。这说明
问题位于重复小请求的 NVMQ/QDMA 完成链路，而不是仍在执行 LWE HLS。
因此脚本默认不再对 4KB 路径进行多轮压力测试，已有旧基线继续用于对比。

## 5. 判定标准

重点比较同一批量下的：

- `slm_to_host_ms`：本轮优化的直接指标；
- `SLM回读(MiB/s)`：排除密文大小不同带来的误判；
- `data_path_ms`：SSD -> SLM、FPGA execute、SLM -> Host 和 Host 校验之和；
- `one_shot_pipeline_ms`：包含 SLM 创建和程序装载等一次性成本。

若 128KB 请求稳定且 `slm_to_host_ms` 显著下降，则将其作为默认同步路径。
若延迟仍主要受回读影响，下一步再评估多个 SLM read 的异步并发和计算/回读
流水化，而不立即修改已经验证通过的 LWE HLS 核心。

## 6. 板卡实测结果

测试条件为 128B 连续明文、`input_lbas=256`。32KB 和 128KB 各执行 1 次
预热及 5 次正式测量；4KB 只采用阻塞前已经完成的 2 个样本。

| SLM 读块 | 请求数 | FPGA execute 中位数 | SLM 回读中位数 | SLM 回读吞吐 | data path 中位数 | one-shot 中位数 |
|---:|---:|---:|---:|---:|---:|---:|
| 4KB | 2057 | 19.798 ms | 40629.901 ms | 0.198 MiB/s | 40676.506 ms | 40757.234 ms |
| 32KB | 258 | 19.128 ms | 6611.906 ms | 1.215 MiB/s | 6657.638 ms | 6740.704 ms |
| 128KB | 65 | 19.944 ms | 2497.868 ms | 3.217 MiB/s | 2548.838 ms | 2631.493 ms |

相对于 4KB 基线，128KB 将 SLM 回读中位延迟降低约 16.27 倍，将 data path
中位延迟降低约 15.96 倍。相对于 32KB，128KB 的 SLM 回读仍快约 2.65 倍。
因此当前同步实现确定使用 128KB 默认值。

128KB 的 5 次 SLM 回读分别为 2497.868、2491.608、7191.578、3530.917、
2453.638ms，存在明显长尾；P95（5 个样本下等于最大值）为 7191.578ms。
虽然其 P95 仍低于 32KB 的 7591.341ms，但后续正式性能报告应增加到至少
20 次测量。当前 128KB 回读仍占 data path 中位延迟约 98%，仍是端到端瓶颈。

同批量 tfhe-rs CPU 串行加密中位数为 61.482ms，FPGA execute 中位数为
19.944ms，核心计算加速约 3.08 倍；但包含 SLM 回读的 data path 尚未超过
CPU。下一轮优化应针对同步 SLM 回读的控制面往返及数据搬运，而不是继续
调整已经获得核心加速的 LWE HLS 计算逻辑。

## 7. 128KB二十次正式测量

在3次预热后继续完成20次128KB回读测量，结果如下：

| 指标 | 结果 |
|---|---:|
| FPGA execute中位数 | 19.072ms |
| FPGA execute P95 | 20.737ms |
| SLM回读中位数 | 2945.050ms |
| SLM回读均值 | 3421.080ms |
| SLM回读标准差 | 1579.700ms |
| SLM回读最小值 | 2401.763ms |
| SLM回读P90 | 4682.605ms |
| SLM回读P95 | 7218.751ms |
| SLM回读最大值 | 8373.801ms |
| SLM回读中位吞吐 | 2.728MiB/s |
| data path中位数 | 3001.066ms |
| one-shot中位数 | 3089.258ms |

20次样本中有3次超过4s、2次超过7s，均值高于中位数，呈明显右偏长尾。
128KB相对于4KB基线的SLM回读中位加速约13.80倍，相对于32KB约2.25倍。
FPGA execute相对于CPU串行加密的核心加速更新为3.22倍；但data path仍约为
CPU加密延迟的48.8倍，其中SLM回读占data path中位延迟约98.1%。

因此128KB仍是当前最佳且应保留为默认同步回读粒度，但“增大单请求”已经
接近现有transport配置的上限，且无法消除秒级基础传输时间和长尾。下一步
需要记录65个128KB子请求各自的延迟，区分固定控制面开销与少数异常慢请求，
再决定采用异步并发、请求流水化或绕过Host落地。

## 8. 子请求延迟诊断工具

Host应用新增可选参数 `--slm-read-trace PATH`。启用后使用
`std::chrono::steady_clock` 记录每个同步 `nvme_slm_read` 的请求序号、offset、
长度、总耗时、返回值、errno和EINTR重试次数。样本先保存在内存中，整轮回读
结束后一次性追加到CSV，避免逐请求文件输出进入被测区间。

`run_fpga_bench.sh` 新增环境变量 `SLM_READ_TRACE`，只在正式测量轮次传入trace，
预热轮次不记录。新增 `summarize_slm_request_trace.py`，用于输出：

- 每轮65个请求的总和、P50、P95、最大值及最慢序号；
- 全部请求的P50、P95、P99和慢请求数量；
- 按P95排序的固定慢请求位置；
- 全局最慢的单次请求及其run nonce、offset和长度。

推荐测试命令：

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-offload

BATCH_SIZES=128 \
WARMUP=3 \
ITERATIONS=20 \
INPUT_LBAS=256 \
SLM_READ_CHUNK_BYTES=131072 \
SETTLE_SECONDS=1 \
SLM_READ_TRACE=slm_read_requests_128k_20runs.csv \
OUTPUT_CSV=slm_read_trace_runs_128k_20runs.csv \
./run_fpga_bench.sh

./summarize_slm_request_trace.py slm_read_requests_128k_20runs.csv \
  | tee slm_read_requests_128k_20runs_summary.md
```

如果目标trace文件已经存在，程序会继续追加；开始新实验时应使用新的文件名，
避免将不同软件版本或不同测试条件的请求样本混合。

## 9. 128KB子请求trace结论

完成20轮、每轮65个请求，共1300个请求级样本，所有请求均成功且没有EINTR
重试。全局统计如下：

| 指标 | 结果 |
|---|---:|
| 请求延迟均值 | 50.808ms |
| 请求延迟P50 | 38.177ms |
| 请求延迟P95 | 87.386ms |
| 请求延迟P99 | 116.296ms |
| 请求最大延迟 | 4118.603ms |
| 大于等于100ms的请求 | 33/1300 |

正常轮次的单请求P50约35～46ms，65个同步请求串行累计后约为2.3～3.0s，
与整轮SLM回读中位延迟一致。因此秒级基础延迟主要来自每个同步请求的固定
往返开销被串行放大，而不是密文校验或FPGA计算。

另外出现5个超过1s的极端请求，分别位于序号27、9、1、34、1，最大值为
4118.603ms。慢请求位置没有固定周期，也不集中于最后一个尾块。序号64的
36KB尾请求在20轮中耗时19.271～36.947ms，全部正常，排除了尾包处理导致
长尾的假设。全部请求ret、errno和EINTR重试均为0，说明长尾发生在一次成功
同步ioctl内部的请求完成等待阶段。

由此确定下一步采用小队列深度的并发回读实验。先实现QD=2，在不同Host缓冲
区上并发读取不重叠的SLM offset；确认数据正确、无队列阻塞后再测试QD=4，
暂不直接使用更高队列深度。理论上不考虑随机长尾时，QD=2和QD=4可将约
2.48s的串行基础时间分别压缩到约1.24s和0.62s。随机1～4s完成停顿仍需通过
请求并发结果判断是否能被其他请求隐藏；若并发反而触发QDMA/NVMQ阻塞，则
转向修复完成队列和IRQ/credit链路。

## 10. QD=2/4并发回读实现

Host应用新增 `--slm-read-queue-depth 1|2|4`，默认值为1。QD大于1时使用
`std::thread` worker池和原子请求序号分配器；每个worker读取互不重叠的SLM
offset，并写入Host输出缓冲区中对应的非重叠区域。全部worker join之后才进行
密文布局转换和ClientKey解密校验，因此输出顺序和已有密文格式不变。

实现保留以下安全边界：

- 只允许QD=1、2、4，不开放任意高并发；
- 任一请求失败后停止分配新请求，等待已经提交的请求返回；
- QD=1使用同一个worker逻辑但不创建线程，作为已验证串行回退；
- benchmark CSV增加queue depth维度，不混合不同QD样本；
- 请求trace增加queue depth和worker index，保留并发下的长尾定位能力；
- 自动扫描脚本默认只测试QD=1和QD=2，QD=4必须显式指定。

本次只修改x86 Host应用，不修改ARM `nvmf_tgt`、NVMQ驱动、HLS RTL或比特流。
板卡测试必须先单次执行QD=2，并确认128个明文全部解密正确；QD=2多轮稳定后
才能测试QD=4。若出现不可中断的D状态，说明当前NVMQ/QDMA完成链路不支持或
无法稳定承受该并发，应恢复QD=1并转向runtime/driver修复。

为避免旧版串行trace与新增QD字段发生CSV错列，应用会校验已有trace文件表头。
QD实验必须使用新的trace文件名；表头不匹配时只放弃写trace，不影响已经完成的
密文正确性校验和benchmark主结果。
