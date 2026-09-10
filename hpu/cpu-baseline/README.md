# x86 CPU加解密与远端HPU基线

该程序提供与SUDA FPGA完整通路可对比的软件基线：

```text
Fidus SSD或x86宿主机本地SSD
  -> Host内存
  -> tfhe-rs CPU加密
  -> CPU打包为HPU-native psi64/V80格式
  -> TCP
  -> 远端真实HPU执行u8标量加法
  -> TCP返回HPU-native密文
  -> CPU解包、软件解密
  -> Host内存
  -> x86可见SSD
```

CPU和FPGA路径均使用四个2-bit radix块表示一个`u8`，每个`u8`的网络密文均为
`4 x 2 x 12KB = 98,304B`。远端调用同一个
`adds-hpu-native-roundtrip`服务操作，因此网络载荷和HPU计算口径一致。

## 编译

在132机器的SUDA仓库根目录执行：

```bash
cd /home/user/suda

cargo build --release \
  --manifest-path hpu/cpu-baseline/Cargo.toml
```

生成程序：

```text
hpu/cpu-baseline/target/release/suda-lwe-cpu-baseline
```

## 两条存储基线

程序显式区分两种数据源，但二者进入Host内存后的CPU加密、HPU-native打包、TCP、
远端HPU计算和CPU解密完全相同：

- `fidus-ssd`：在QEMU中从`/dev/nvmq0n1`读取Fidus板载SSD，用于与SUDA FPGA通路做同源对比。
- `x86-local-ssd`：在x86宿主机上直接读取本地SSD中的块设备或普通文件，用于比较CSD存储与普通SSD。

为了让结果可追溯，`storage_backend`、输入/输出路径及LBA都会写入CSV。

## 基线一：Fidus SSD

先在129机器启动SUDA远端HPU服务，然后在132机器的QEMU终端执行：

```bash
cd /mnt/suda

./hpu/cpu-baseline/target/release/suda-lwe-cpu-baseline \
  --storage-backend fidus-ssd \
  --input-ssd /dev/nvmq0n1 \
  --input-lba 65536 \
  --plaintext-bytes 128 \
  --output-ssd /dev/nvmq0n1 \
  --output-lba 131072 \
  --client-key /mnt/suda/hpu/keys/psi64/psi64_shortint_ks32_client_key.bincode \
  --server 10.16.0.129:19090 \
  --scalar 1 \
  --cpu-threads 1 \
  --expect 59 \
  --csv hpu/cpu-baseline/cpu_full_pipeline_128b.csv \
  2>&1 | tee hpu/cpu-baseline/cpu_full_pipeline_128b.log
```

`--expect`是加法前SSD中第一个明文字节；应按当前测试数据调整。目标LBA会被覆盖，
必须与源LBA分开。

## 基线二：x86宿主机本地SSD

这条命令直接在x86宿主机执行，不进入QEMU。先在本地SSD目录准备一个4KB测试文件；
下例目录应确实位于待测x86 SSD的挂载点：

```bash
mkdir -p /home/user/suda/hpu/cpu-baseline/testdata

dd if=/dev/urandom \
  of=/home/user/suda/hpu/cpu-baseline/testdata/x86_ssd_plaintext_4k.bin \
  bs=4096 count=1 status=none

sha256sum /home/user/suda/hpu/cpu-baseline/testdata/x86_ssd_plaintext_4k.bin
```

运行相同的CPU加密、远端HPU计算和CPU解密通路：

```bash
cd /home/user/suda

hpu/cpu-baseline/target/release/suda-lwe-cpu-baseline \
  --storage-backend x86-local-ssd \
  --input-ssd hpu/cpu-baseline/testdata/x86_ssd_plaintext_4k.bin \
  --input-lba 0 \
  --plaintext-bytes 128 \
  --output-ssd hpu/cpu-baseline/testdata/x86_ssd_result_4k.bin \
  --output-lba 0 \
  --create-output-file \
  --client-key hpu/keys/psi64/psi64_shortint_ks32_client_key.bincode \
  --server 10.16.0.129:19090 \
  --scalar 1 \
  --cpu-threads 1 \
  --csv hpu/cpu-baseline/cpu_x86_ssd_128b.csv \
  2>&1 | tee hpu/cpu-baseline/cpu_x86_ssd_128b.log
```

普通文件访问可能命中Linux页缓存，因此`ssd_read`/`ssd_write`只代表当前文件I/O口径。
若论文中需要比较裸盘时延，应改用x86本地SSD上预留且不会破坏数据的块设备/LBA，
并去掉`--create-output-file`；不要直接选择含有文件系统的原始设备区域。

## 并行CPU吞吐基线

例如使用16个CPU线程：

```bash
./hpu/cpu-baseline/target/release/suda-lwe-cpu-baseline \
  --storage-backend fidus-ssd \
  --input-ssd /dev/nvmq0n1 \
  --input-lba 65536 \
  --plaintext-bytes 128 \
  --output-ssd /dev/nvmq0n1 \
  --output-lba 131072 \
  --client-key hpu/keys/psi64/psi64_shortint_ks32_client_key.bincode \
  --server 10.16.0.129:19090 \
  --scalar 1 \
  --cpu-threads 16 \
  --csv hpu/cpu-baseline/cpu_full_pipeline_128b_parallel.csv
```

单线程结果用于比较单核软件与FPGA算子；多线程结果表示x86 Host可达到的软件吞吐上限。

## 计时口径

程序分别输出：

- `ssd_read`：SSD到x86 Host内存。
- `cpu_encrypt`：tfhe-rs `ClientKey::encrypt_radix`。
- `cpu_native_pack`：普通Big-LWE到HPU-native物理布局。
- `request_send`、`wait_response_header`、`response_receive`：TCP阶段。
- `remote_stage_ms`：129服务器接收、HPU准备、入队、等待、输出转换和发送。
- `cpu_native_unpack`、`cpu_decrypt`：返回密文的软件解包与解密。
- `ssd_write`、`ssd_readback`：明文结果写回及可选回读校验。
- `online_e2e`：从SSD读取开始，到目标SSD写入并完成回读校验。

密钥加载时间单独报告为`key_load`，不计入`online_e2e`，但计入`process`。
