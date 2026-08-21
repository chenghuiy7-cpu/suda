# LWE FPGA 密文 HPU mockup 兼容性测试（2026-07-13）

## 1. 测试目标

验证 SUDA 板上 `lwe_encrypt` 算子生成的密文是否能够：

1. 被 tfhe-rs 包装为标准 CPU `RadixCiphertext`；
2. 转换为 `HpuRadixCiphertext`；
3. 作为 HPU mockup 的真实运算输入；
4. 完成同态运算并由原 ClientKey 正确解密。

## 2. 输入文件

```text
FPGA ciphertext dump:
/home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/lwe_encrypt_fpga_ciphertexts.bin
SHA256=e5b53bcd2aa0341c7d70463996a62b7f590deca0255a1440d57bfb3f0e92b325

tfhe-rs KS32 ClientKey:
/home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_shortint_ks32_client_key.bincode
SHA256=582947a24c185d9b90f52cb12bec9f608b66dcf9a0a47e8c347c6dc9e0184c76

HPU parameters:
/home/yangchenghui/hpu/tfhe-rs/mockups/tfhe-hpu-mockup/params/tuniform_64b_pfail128_psi64.toml
```

## 3. 新增程序

```text
/home/yangchenghui/hpu/tfhe-rs/tfhe/examples/hpu/lwe_encrypt_mockup_test.rs
```

Cargo example 名称：

```text
hpu_lwe_encrypt_mockup_test
```

程序执行以下验证链路：

```text
LWEHLS01 dump
  -> 4 个 shortint Big-LWE block
  -> CPU RadixCiphertext
  -> ClientKey.decrypt_radix
  -> HpuRadixCiphertext::from_radix_ciphertext
  -> HPU mockup ADDS(1)
  -> HpuRadixCiphertext::to_radix_ciphertext
  -> ClientKey.decrypt_radix
```

## 4. 构建结果

```bash
cd /home/yangchenghui/hpu/tfhe-rs
cargo check --features hpu --example hpu_lwe_encrypt_mockup_test
cargo build --release --features hpu \
  --bin hpu_mockup \
  --example hpu_lwe_encrypt_mockup_test
```

两项均通过。

## 5. 运行命令

终端 1 启动真实 psi64 参数的 mockup：

```bash
cd /home/yangchenghui/hpu/tfhe-rs
source setup_hpu.sh --config sim
RUST_LOG=info ./target/release/hpu_mockup \
  --params /home/yangchenghui/hpu/tfhe-rs/mockups/tfhe-hpu-mockup/params/tuniform_64b_pfail128_psi64.toml
```

终端 2 运行导入测试：

```bash
cd /home/yangchenghui/hpu/tfhe-rs
source setup_hpu.sh --config sim
RUST_LOG=info ./target/release/examples/hpu_lwe_encrypt_mockup_test \
  --ciphertext-dump /home/yangchenghui/suda/host/applications/vscode-lwe-encrypt-offload/lwe_encrypt_fpga_ciphertexts.bin \
  --client-key /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/testdata/psi64_shortint_ks32_client_key.bincode \
  --scalar 1
```

## 6. 客户端结果

```text
imported_u8_count=1
imported_mask_dimension=2048
cpu_radix_import_decrypt_checked=yes
input[0]=59 scalar=1 decrypted_result=60 expected=60
hpu_operation=ADDS
hpu_mockup_ciphertext_compute_checked=yes
lwe_encrypt_mockup_compatibility=passed
```

## 7. Mockup 执行报告

```text
Report for IOp: ADDS <I8 I8> <I8@0x04> <I8@0x00> <0x1>
TimeRpt { cycle: 978360, duration: 3.261ms }
InstructionKind { MemLd: 4, MemSt: 4, Arith: 21, Pbs: 11, Sync: 1 }
```

mockup 配置频率为 300 MHz。`3.261 ms` 是 mockup 给出的 HPU 行为模型估算，不是该 x86 进程的实际墙钟耗时，也不是真实 HPU 板卡实测延迟。

## 8. 结论

本次测试证明，FPGA `lwe_encrypt` 生成的四个 2048 维 Big-LWE block 可以被包装成 tfhe-rs `RadixCiphertext`，并通过正式的 `HpuRadixCiphertext::from_radix_ciphertext` 路径进入 HPU mockup。mockup 对该密文执行了包含 PBS 的 8-bit 标量加法，结果密文经原 ClientKey 解密得到正确结果 60。

因此，当前 FPGA 密文已经通过 HPU mockup 的格式和同态计算兼容性验证。

仍需注意：

- 该结论针对 HPU mockup，不等同于真实 HPU FPGA 硬件验证；
- 当前 HLS mask PRNG 和噪声采样仍是原型实现，不具备 tfhe-rs 等价的密码学安全性；
- 当前只验证了一个 u8 和一次 `ADDS(1)`，还需扩展到多数据和更多 IOp。
