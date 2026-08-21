# LWE HPU-native 性能测试口径对齐记录（2026-08-17）

## 背景

FPGA `lwe_encrypt` 已由 CPU-LWE packet 输出更新为 psi64/V80 HPU-native 物理输出。旧性能数据的 FPGA 输出为约 65,792B/u8，而新输出为 98,304B/u8，因此旧加速比不能直接代表当前版本。

## 本次修改

1. CPU benchmark 在 tfhe-rs radix 加密后，按照当前 FPGA 完全相同的规则生成 HPU-native 输出：
   - 2048 维 mask 做 11-bit bit-reversal；
   - 以 16 个系数为一组交织到 PC0/PC1；
   - body 放在 PC0 word 1024；
   - PC0 和 PC1 分别补零到 12KB；
   - 每个 u8 共 4 个 Big-LWE，物理输出 98,304B。
2. CPU 分别记录 `encrypt_ms`、`native_pack_ms` 和 `encrypt_and_pack_ms`。输出缓冲区分配不计时，native 数据与 padding 的写入计时。
3. CPU 增加 `--input-file`，可与 SSD 使用同一份明文文件。
4. FPGA CSV 增加 `output_layout`、`physical_output_bytes_per_u8`、`transport_ready_ms` 和 `one_shot_transport_ready_ms`。
5. `run_fpga_bench.sh` 默认并显式传递 `OUTPUT_LAYOUT=hpu-native`，断点续测按布局筛选，避免混入旧样本。
6. 汇总脚本的主加速比修改为：

```text
CPU encrypt_and_pack_ms / FPGA fpga_execute_ms
```

旧 CSV 缺少布局或阶段字段时会直接报错，不再静默使用 65,792B/u8 的旧假设。

## 计时边界

- CPU 同层起点：明文已在 Host 内存；终点：HPU-native 数据已写入预分配 Host buffer。
- FPGA 同层起点：明文已在 input SLM；终点：HPU-native 数据已写入预分配 output SLM。
- `transport_ready_ms`：SSD->SLM + FPGA execute + output SLM->Host，不含 Host 校验。

CPU 与 FPGA 的本地内存位置不同，因此同层加速比用于比较软件和算子的等价变换；严格的系统对比仍需补充 SSD->Host->CPU 加密与打包路径。

## 本地验证

- Host C++ 程序重新编译通过；只有原有 libnvme 头文件 warning。
- CPU release benchmark 构建通过。
- 1B CPU 冒烟测试通过 ClientKey 解密和 HPU-native mask/body/padding 校验。
- 新 CSV 汇总冒烟测试通过，25 列 FPGA CSV 字段完全对齐。
- 旧 FPGA CSV 拒绝测试通过。

完整正式测试命令见：

```text
/home/yangchenghui/suda/docs/architecture/LWE加密算子性能测试方法.md
```

## 限制

当前 HLS 随机源和噪声采样仍是原型，不是 tfhe-rs CSPRNG 的安全等价实现。现阶段加速比只能描述相同 LWE 结构、主要算术和 HPU-native 布局生成的原型性能。
