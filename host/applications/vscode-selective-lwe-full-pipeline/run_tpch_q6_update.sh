#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
suda_root=$(cd -- "$script_dir/../../.." && pwd)
tpch_test_dir="$suda_root/host/applications/tpch-lineitem-fpga-test"

records=${TPCH_RECORDS:-128}
source_lba=${TPCH_SSD_LBA:-65536}
destination_lba=${TPCH_OUTPUT_SSD_LBA:-131072}
ssd_nsid=${TPCH_SSD_NSID:-1}
ssd_device=${TPCH_SSD_DEVICE:-/dev/nvmq0n1}
server=${HPU_SERVER:-10.16.0.129}
server_port=${HPU_SERVER_PORT:-19090}
scalar=${HPU_SCALAR:-1}
operation=${HPU_OPERATION:-adds}
slm_read_chunk_bytes=${SLM_READ_CHUNK_BYTES:-131072}
slm_write_chunk_bytes=${SLM_WRITE_CHUNK_BYTES:-131072}
slm_read_queue_depth=${SLM_READ_QUEUE_DEPTH:-1}
skip_ssd_prepare=${TPCH_SKIP_SSD_PREPARE:-0}
key_path=${TPCH_LWE_KEY:-$suda_root/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin}
binary="$tpch_test_dir/testdata/tpch_lineitem_512b.bin"
diag_dir="$script_dir/testdata"
remote_native="$diag_dir/tpch_q6_remote_hpu_native.bin"
remote_expected="$diag_dir/tpch_q6_remote_expected_u8.bin"

schema='orderkey:u64@0,partkey:u64@8,suppkey:u64@16,linenumber:u8@24,quantity:u8@25,returnflag:u8@26,linestatus:u8@27,shipmode_code:u8@28,shipinstruct_code:u8@29,extendedprice_cents:u64@32,discount_bp:u16@40,tax_bp:u16@42,shipdate:u32@44,commitdate:u32@48,receiptdate:u32@52,source_row:u64@56'
query='SELECT quantity FROM lineitem WHERE shipdate >= 19940101 AND shipdate < 19950101 AND discount_bp >= 500 AND discount_bp <= 700 AND quantity < 24'

if [[ ! -s "$binary" ]]; then
    echo "Prepared TPC-H image is missing: $binary" >&2
    echo "Run: cd $tpch_test_dir && ./run_tpch_filter_test.sh --prepare" >&2
    exit 1
fi
expected_bytes=$((records * 512))
actual_bytes=$(stat -c '%s' "$binary")
if [[ "$actual_bytes" -ne "$expected_bytes" ]]; then
    echo "TPC-H image has $actual_bytes bytes; expected $expected_bytes" >&2
    exit 1
fi

mkdir -p "$diag_dir"

case "$skip_ssd_prepare" in
    0)
        echo "[selective_full_runner] writing and verifying source TPC-H SSD image"
        python3 -u "$tpch_test_dir/write_tpch_dataset_to_ssd.py" \
            --input "$binary" \
            --device "$ssd_device" \
            --lba "$source_lba"
        ;;
    1)
        echo "[selective_full_runner] reusing existing source SSD image without rewriting it"
        ;;
    *)
        echo "TPCH_SKIP_SSD_PREPARE must be 0 or 1" >&2
        exit 2
        ;;
esac

make -C "$script_dir" -j2
exec "$script_dir/vscode-selective-lwe-full-pipeline" \
    --ssd-nsid "$ssd_nsid" \
    --ssd-lba "$source_lba" \
    --output-ssd-nsid "$ssd_nsid" \
    --output-ssd-lba "$destination_lba" \
    --records "$records" \
    --record-bytes 512 \
    --schema "$schema" \
    --query "$query" \
    --reference "$binary" \
    --server "$server" \
    --server-port "$server_port" \
    --scalar "$scalar" \
    --remote-operation "$operation" \
    --slm-read-chunk-bytes "$slm_read_chunk_bytes" \
    --slm-write-chunk-bytes "$slm_write_chunk_bytes" \
    --slm-read-queue-depth "$slm_read_queue_depth" \
    --remote-native-output "$remote_native" \
    --remote-expected-output "$remote_expected" \
    --key "$key_path" \
    --verify-decrypt-slm-write \
    --benchmark
