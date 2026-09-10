# Near-storage selective LWE demo

This application runs one SUDA program containing two FPGA operators:

```text
SSD -> input SLM -> selective_filter -> lwe_encrypt -> output SLM -> Host verify
```

The filter has two versioned protocols. Legacy v1 consumes fixed 512-byte
records and applies `quantity@4 gt|eq threshold`. SQL metadata v2 accepts a
runtime row size, integer schema, projected field, comparisons, and boolean
expression. The host parses SQL and sends compact metadata to the FPGA; the
FPGA does not parse SQL text. Selected `u8` values enter the existing LWE core,
so records and plaintext do not return to ARM or the host between operators.

SQL metadata v2 supports:

- fixed rows from 64 to 4096 bytes, in 64-byte increments;
- little-endian `u8/u16/u32/u64/i8/i16/i32/i64` fields at runtime offsets;
- `=`, `!=`, `<`, `<=`, `>`, `>=`, `AND`, `OR`, `NOT`, and parentheses;
- up to eight comparisons and fifteen compiled expression tokens;
- one projected `u8` field for the current scalar LWE connection.

An integer field cannot cross a 64-byte AXI beat. Strings, NULL, arithmetic,
joins, aggregation, variable-length rows, and direct CSV/JSON/Parquet parsing
are outside this version.

The SSD writer pads only the final transfer to a 4KB LBA boundary, so the same
flow also supports a single 512-byte record. The configured record count keeps
that transport padding out of the filter input.

For each execution, the host turns `--schema` and `--query` into the filter's
4KB context page. The first 64 bytes contain the protocol magic/version, record
count and size, projected field offset/type, predicate count, token count, and
the postfix boolean tokens. Starting at byte 64, each 16-byte predicate contains
its field offset, integer type, comparison opcode, and 64-bit literal. The FPGA
loads this metadata before consuming the SSD row stream. Changing the query or
schema therefore changes only the request context; it does not require a new
bitstream while the query remains inside the supported subset.

```text
SQL text + schema
        |
        v
Host parser/compiler --> v2 context page --> selective_filter RTL
                         |                  |
                         |                  +--> selected projected u8 values
                         +--> row layout         --> lwe_encrypt RTL
                              predicate plan
```

Generate 16 deterministic records and write them to the CSD SSD:

```bash
cd /mnt/suda/host/applications/vscode-selective-lwe-encrypt-offload

./generate_tpch_like.py \
  --records 16 \
  --predicate gt \
  --threshold 32

sudo ./write_dataset_to_ssd.py \
  --input testdata/tpch_like_512b.bin \
  --device /dev/nvmq0n1 \
  --lba 65536
```

Build and run after deploying the matching FPGA bitstream and ARM runtime
configuration:

```bash
make -j4

sudo ./vscode-selective-lwe-encrypt-offload \
  --ssd-nsid 1 \
  --ssd-lba 65536 \
  --records 16 \
  --predicate gt \
  --threshold 32 \
  --reference testdata/tpch_like_512b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output selective_lwe_fpga_ciphertexts.bin \
  --benchmark
```

The reference image is read by the host only after the experiment starts for
correctness comparison. The FPGA data path itself reads records from the SSD
through SLM and does not send selected plaintext through the CPU or ARM.

## Runtime SQL metadata

Inspect the compiled FPGA plan without device I/O:

```bash
./vscode-selective-lwe-encrypt-offload \
  --record-bytes 512 \
  --schema 'id:u32@0,quantity:u8@4,order_key:u64@8,price:u64@16' \
  --query 'SELECT quantity FROM lineitem WHERE quantity > 32 AND id >= 100000' \
  --explain-filter
```

After deploying a bitstream containing SQL metadata v2, run:

```bash
./vscode-selective-lwe-encrypt-offload \
  --ssd-nsid 1 \
  --ssd-lba 65536 \
  --records 16 \
  --record-bytes 512 \
  --schema 'id:u32@0,quantity:u8@4,order_key:u64@8,price:u64@16' \
  --query 'SELECT quantity FROM lineitem WHERE (quantity >= 40 AND id != 100010) OR id = 100014' \
  --reference testdata/tpch_like_512b.bin \
  --key /mnt/suda/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin \
  --output selective_lwe_sql_ciphertexts.bin \
  --benchmark
```

The reference file must match the SSD range because the current runtime needs
the expected selected count to post an exact DMA receive range. Legacy CLI
arguments remain available for the currently deployed legacy bitstream.
