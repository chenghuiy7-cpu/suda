# LWE 完整远端 HPU 通路

该程序把以下路径放在同一个进程中执行，中间密文只保存在 SLM、Host 内存和 TCP 缓冲区中：

```text
SSD -> SLM -> FPGA lwe_encrypt -> Host memory -> TCP
    -> remote HPU ADDS -> TCP -> Host memory -> SLM
    -> FPGA lwe_decrypt -> SSD
```

远端请求和响应都使用 `hpu-native-psi64-v80` 固定槽位布局。Host 不再把 HPU 结果转换为逻辑 LWE 后再交给解密算子。

## 构建

```bash
cd /mnt/suda/host/applications/vscode-lwe-full-pipeline
make clean && make -j
```

## 运行示例

下面读取 SSD `nsid=1,lba=65536` 的前 128B，远端执行 `+1`，解密后写入同一 SSD 的 `lba=131072`：

```bash
export FIRST_U8=$(od -An -tu1 -N1 \
  ../vscode-lwe-encrypt-data-gen/testdata/plaintext_u8_4k.bin | xargs)

./vscode-lwe-full-pipeline \
  --ssd-nsid 1 \
  --ssd-lba 65536 \
  --input-lbas 1 \
  --plaintext-bytes 128 \
  --output-ssd-nsid 1 \
  --output-ssd-lba 131072 \
  --expect "$FIRST_U8" \
  --server 10.16.0.129 \
  --server-port 19090 \
  --scalar 1 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --benchmark
```

`FIRST_U8` 是源 SSD 数据的第一个字节，通常在写入 SSD 前通过
`od -An -tu1 -N1 plaintext_u8_4k.bin | xargs` 获得。这里 `--expect` 检查
加密前输入；远端 `+1` 后的预期明文由程序自动计算。

源和目标 LBA 范围不能重叠。目标写入按 4KB 补零，默认在返回成功前回读目标 LBA 并校验；`--skip-ssd-readback` 可关闭该检查。

远端服务必须包含协议操作 `op=3`（HPU 原生格式请求、HPU 原生格式响应）。旧 `op=2` 保持兼容，仍返回 CPU 逻辑 LWE，不能直接作为该程序的解密输入。
