# NVMQ 诊断改动回退记录

日期：2026-08-24

## 目的

验证 2026-08-21 提交 `21fc64ed4` 中加入 Host NVMQ 驱动的
`NVMQ_DIAG` 诊断代码及其重新编译产物，是否与当前管理队列 `qid=0`
执行 NVMQ Connect 超时有关。

本次采用最小 A/B 回退，不改动以下内容：

- `nr_io_queues=3`，继续避免 `qid=4` 与管理队列的 MCDMA channel 0 冲突；
- QDMA PF 驱动；
- QEMU 使用的 Fidus PCI BDF `d9:00.0`；
- ARM `nvmf_tgt`、FPGA BOOT.bin 和 HLS 算子；
- NVMQ Makefile、trace 头文件和可移植路径修复。

## 回退内容

只将 `host/drivers/nvmq/qdma.c` 恢复到 `21fc64ed4` 父提交中的内容，即删除：

- `nvmq_diag_req_cmd()`；
- `nvmq_diag_interesting_cmd()`；
- `nvmq_diag_log_cmd()`；
- QUEUE_RQ、H2C_SUBMIT、H2C_DONE、C2H_POST、C2H_CQE 和 TIMEOUT
  路径中的 `NVMQ_DIAG` 调用。

恢复后已通过逐字节比较确认：

```text
host/drivers/nvmq/qdma.c == 21fc64ed4^:host/drivers/nvmq/qdma.c
```

## 备份

修改前源码、模块和队列配置保存在：

```text
backups/nvmq_pre_diag_rollback_20260824/
```

修改前 `nvmq.ko` SHA-256：

```text
604ecf6308b97541cddb264fdc4b299663d643a2b08690376fd94b85dc599127
```

## 构建结果

在 `host/drivers/nvmq` 中执行 `make clean` 和 `make`，模块编译、MODPOST
和链接成功。构建输出含仓库原有 warning，没有 error。

回退版 `nvmq.ko`：

```text
SHA-256: 34e327b9a923e8ef24f5a19983555645f11d7ffb6b84a48a3badda5af90d3d46
vermagic: 5.4.211-dirty SMP mod_unload
Build ID: dcca7659edb4bfa5d6612959a561d92a795c91e5
contains NVMQ_DIAG: no
```

## 判定方法

必须重启 QEMU，让 guest 重新加载回退版模块。连接成功说明 8 月 21 日
NVMQ 诊断改动或对应重编译产物构成回归；如果管理队列 Connect 仍然超时，
则可以排除该改动，应检查 QDMA/FPGA/MCDMA 在无 FLR 条件下的硬件残留状态。

## 上板 A/B 结果

QEMU guest 已核对实际使用的回退版模块：

```text
34e327b9a923e8ef24f5a19983555645f11d7ffb6b84a48a3badda5af90d3d46  nvmq.ko
```

QDMA PF 探测正常，识别为 EQDMA Soft IP、Vivado 2020.2，当前 BAR 布局为
`user_bar_id=4`、AXI Master Lite BAR 2。随后 `cat nvmq0_opts >
/dev/nvmq-fabrics` 仍在管理队列 `qid=0` 的 1088B Connect 请求处超时：

```text
nvmq nvmq0: I/O 0 QID 0 timeout
nvmq nvmq0: Connect command failed, error wo/DNR bit: 881
nvmq: Failed to send qdma request: -6
nvmq: QDMA recv error: -6
```

结论：删除 `NVMQ_DIAG` 后故障特征和超时时间均未改变，因此 8 月 21 日
加入的诊断代码不是本次 Connect 故障的根因。`nr_io_queues=3` 也不是该阶段
的原因，因为 I/O 队列尚未创建，失败发生在独立的管理队列 `qid=0`。

结合此前 ARM MCDMA S2MM channel 0 已启动但 packet count 仍为 0，下一步应在
保持该回退版模块不变的条件下对 Fidus/PL/PCIe endpoint 做真正的断电冷启动，
然后重新启动 ARM runtime、QEMU 和 NVMQ。仅重启 QEMU、重新绑定 vfio-pci 或
重启 ARM Linux 不能保证清除该 QDMA Soft IP 的内部状态，因为设备报告
`flr_present=0`。
