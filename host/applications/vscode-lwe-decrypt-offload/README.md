# vscode-lwe-decrypt-offload

该程序用于独立验证 SUDA 算子池中的 `lwe_decrypt`（operator type ID 3）。它将 psi64/V80 HPU-native Big-LWE 密文写入 input SLM，执行 FPGA 解密，并从 output SLM 读取连续的 `u8` 明文。

Host 到 input SLM 的写入粒度默认为 128 KiB（最后一笔按剩余的 4 KiB
对齐长度），替代原来的逐 4 KiB 同步请求。可用
`--slm-write-chunk-bytes 67108864` 可让当前约 8.4 MiB 的 128B 密文批次
使用一条 NVMe 请求；Host 使用多页 PRP 链，ARM 按 1 MiB MCDMA 窗口滚动传输。

输入支持三种形式：

- `LWEHLS01`：现有加密/远端 HPU 程序保存的逻辑 Big-LWE dump。默认在 Host 内存中将其重排为 HPU-native；增加 `--fpga-input-layout cpu` 后，只去除文件头和明文参考区，将自然顺序系数直接送入 FPGA。dump 中的明文参考值用于逐字节校验。
- `hpu-native`：不带文件头的原始 HPU-native payload。每个 `u8` 固定占 `98304B`；可配合 `--expect-file` 校验。
- `logical`：不带文件头的紧密拼接 Big-LWE payload。每个 LWE 为自然顺序 `mask[2048] + body`，共 `16392B`；4个 LWE 表示一个 u8，共 `65568B`。使用 `--input-format logical`，自动选择 FPGA layout 0，不进行 HPU mask 重排。原始文件没有明文参考区，可配合 `--expect-file` 校验。

逻辑输入模式需要重新生成含新版 `lwe_decrypt` RTL 的 BOOT.bin。原有 native
layout 1 和应用默认行为保持兼容。65568B 是有效 payload 大小；SLM 写入和输入
range 仍按4KB对齐，1B明文对应 `69632B` 的实际传输范围，尾部补零由算子根据
`input_count` 忽略。不能把 encrypt 的 `65792B` 逐 LWE 64B填充流当作紧密逻辑流。

## 直接解密逻辑布局

下面的命令使用现有远端 HPU 的1B结果。它从 LWEHLS01 中提取65568B逻辑
payload，直接交给 FPGA 解密，不再转换成98304B的 HPU-native 输入。

```bash
cd /mnt/suda/host/applications/vscode-lwe-decrypt-offload
make
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_1b.bin \
  --fpga-input-layout cpu \
  --expect 60 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output lwe_decrypt_logical_result_1b.bin \
  --benchmark
```

先增加 `--inspect-only` 可仅检查输入和 Host 参考解密。原始逻辑 payload 文件
则使用 `--input-format logical`；若文件含 LBA 尾部填充，必须再指定
`--plaintext-bytes N`，程序仅接受零填充。算子输出仍为连续 u8，不改变输出协议。

## 编译

```bash
cd /mnt/suda/host/applications/vscode-lwe-decrypt-offload
make
```

ARM 端的 `/root/software_stack/nf_spdk/config.json` 还必须包含
`operator_type_id=3`、`operator_type_name=lwe_decrypt`、`slot_id=3`，并使用不带
`-b` 参数的 `mcdma/run_nvmq.sh` 启动。修改配置后需要重启 ARM `nvmf_tgt` 和
QEMU/NVMQ 连接，配置不会通过更换 `BOOT.bin` 自动更新。

## 先检查输入文件

该步骤不访问 FPGA，但会用同一 secret key 执行一次 Host 参考解密，检查 dump、重排结果和 key 是否匹配：

```bash
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_1b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --inspect-only
```

## 直接验证上一阶段 FPGA 加密结果

如果刚运行过 `vscode-lwe-encrypt-offload` 的 128B 示例，可直接解密其
`LWEHLS01` dump，不依赖远端 HPU：

```bash
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_fpga_ciphertexts_128b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output lwe_decrypt_fpga_result_128b.bin \
  --benchmark 2>&1 | tee lwe_decrypt_fpga_result_128b.log
```

输入 dump 保存了加密阶段恢复出的明文参考值，因此程序会自动逐字节校验，
不需要再传 `--expect-file`。

## 上板验证远端 HPU 的 1B 结果

已知原始明文为 `59`，远端执行 `+1` 后应解密为 `60`：

```bash
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_1b.bin \
  --expect 60 \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output lwe_decrypt_remote_hpu_result_1b.bin \
  --benchmark 2>&1 | tee lwe_decrypt_remote_hpu_result_1b.log
```

## 验证 128B 结果

`LWEHLS01` 自带 128 个预期明文字节，因此会自动进行完整逐字节比较：

```bash
./vscode-lwe-decrypt-offload \
  --input ../vscode-lwe-encrypt-offload/lwe_encrypt_remote_hpu_result_128b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output lwe_decrypt_remote_hpu_result_128b.bin \
  --benchmark 2>&1 | tee lwe_decrypt_remote_hpu_result_128b.log
```

成功标志包括：

```text
lwe_decrypt FPGA execution passed
correctness_checked=yes
```

若最终网络服务直接返回原始 HPU-native payload，可使用：

```bash
./vscode-lwe-decrypt-offload \
  --input remote_hpu_native.bin \
  --input-format hpu-native \
  --plaintext-bytes 128 \
  --expect-file expected_plaintext_128b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin
```
# Padding 输入补充

输入布局需要显式指定，不能通过系数中的零值自动判断：

| FPGA 输入布局 | 每个 u8 的输入字节数 | padding 处理 |
|---|---:|---|
| `cpu` | 65568 | 紧凑自然序；只允许在已知数量之后补零到 LBA |
| `cpu-padded` | 65792 | 每个 LWE 的 16392B 后有 56B 零 padding；另允许末尾 LBA 补零 |
| `hpu-native`（默认） | 98304 | 保留原有 PC0/PC1 固定槽位与内部 padding |

裸的逐 LWE 补齐数据使用 `--input-format logical --fpga-input-layout cpu-padded`。
如果文件还含有末尾 LBA padding，必须提供 `--plaintext-bytes N`。
`LWEHLS01` 文件也可以选择 `--fpga-input-layout cpu-padded`，由 Host 按指定布局准备输入。
这不是把 65792B 当作紧凑 65568B 读取；算子会逐块消费 7 个零 u64 padding。
非零 padding、截断数据均报错。协议的 TUSER=0xff 结束包不能当作文件 padding。
