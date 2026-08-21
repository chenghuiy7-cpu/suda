# HPU 原生密文远端请求超时分析与修复（2026-08-17）

## 现象

融合程序完成 FPGA 加密、output SLM 回读和 CSD 资源释放后，在等待 129
服务器响应时失败：

```text
remote HPU RPC failed: TCP receive failed: Resource temporarily unavailable
```

## 定位结果

129 服务端仍在监听，且失败连接的 TCP 统计显示接收了 `12583041B`：

```text
128B 明文对应 HPU 原生密文 = 128 * 4 * 2 * 12288 = 12582912B
协议头                       = 129B（TCP统计包含连接控制信息）
```

因此请求载荷已经到达 129，客户端错误是等待响应超时，不是 FPGA、SLM
回读或网络丢包。服务端停留在请求处理期间并持续占用一个 CPU 核。

## 修复

1. Rust 协议层不再逐个 `u64` 调用 `read_exact`，改为一次读取完整载荷后按
   little-endian 批量解码。
2. 服务端不再预先持有整个批次的 `HpuRadixCiphertext`，改为逐个 u8 执行
   “原生 slot 导入、HPU ADDS、等待、结果回读、释放”。
3. 增加 `request_received`、`request_decoded` 和每 16 项一次的
   `hpu_progress`，用于区分读包、原生导入和 HPU 执行阶段。
4. 修复 `package_server_129.sh`，加入此前遗漏的
   `tfhe/src/integer/hpu/ciphertext/mod.rs`。

## 后续验证顺序

重新编译并启动 129 服务端后，依次测试：

1. 1B `echo`：验证 HPU 原生载荷的 TCP 收发。
2. 1B `adds-hpu-native`：验证最小真实 HPU 计算。
3. 128B `adds-hpu-native`：验证批量逐项处理和最终结果。

## 本地验证

执行：

```text
cargo test --release --features hpu-v80 --example hpu_lwe_remote_server
```

结果：

```text
7 passed; 0 failed
```

覆盖原生帧 round-trip、载荷上限、计时帧和
`native_slot_import_matches_tfhe_cpu_conversion`。
