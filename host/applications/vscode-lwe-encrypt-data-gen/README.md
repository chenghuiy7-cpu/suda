# lwe_encrypt SSD Data Generator

This standalone program reads a fixed 4096-byte `u8` plaintext file, writes
the 4KB page to a selected SSD namespace/LBA, and verifies the complete page
with an NVMe readback.

The default test vector is local test data and is intentionally not tracked by
Git. Create an exactly 4096-byte file before the first run:

```bash
mkdir -p testdata
dd if=/dev/urandom of=testdata/plaintext_u8_4k.bin \
  bs=4096 count=1 status=none
FIRST_U8=$(od -An -tu1 -N1 testdata/plaintext_u8_4k.bin | xargs)
echo "FIRST_U8=$FIRST_U8"
```

Keep the file and its SHA-256 with the experiment log when repeatability is
required.

```bash
cd /mnt/suda/host/applications/vscode-lwe-encrypt-data-gen
make
./vscode-lwe-encrypt-data-gen \
  --input testdata/plaintext_u8_4k.bin \
  --ssd-nsid 1 \
  --ssd-lba "$UNUSED_SSD_LBA"
```

Use `--input FILE` to select another exactly 4096-byte plaintext file.

The selected 4KB block is overwritten. The program prints `first_u8`; use it
with `--expect` when testing the encryption operator.

```bash
cd ../vscode-lwe-encrypt-offload
./vscode-lwe-encrypt-offload \
  --ssd-nsid 1 \
  --ssd-lba "$UNUSED_SSD_LBA" \
  --input-lbas 1 \
  --plaintext-bytes 128 \
  --expect "$FIRST_U8" \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin
```
