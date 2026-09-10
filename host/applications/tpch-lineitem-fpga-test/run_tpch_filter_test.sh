#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
suda_root=$(cd -- "$script_dir/../../.." && pwd)
dbgen_dir="$suda_root/third_party/tpch-dbgen"
filter_app_dir="$suda_root/host/applications/vscode-selective-lwe-encrypt-offload"

mode=${1:---prepare}
tpch_scale=${TPCH_SCALE:-1}
tpch_records=${TPCH_RECORDS:-128}
tpch_skip_rows=${TPCH_SKIP_ROWS:-0}
tpch_data_dir=${TPCH_DATA_DIR:-/tmp/tpch-sf${tpch_scale}}
ssd_device=${TPCH_SSD_DEVICE:-/dev/nvmq0n1}
ssd_nsid=${TPCH_SSD_NSID:-1}
ssd_lba=${TPCH_SSD_LBA:-65536}
key_path=${TPCH_LWE_KEY:-$suda_root/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin}

lineitem_tbl="$tpch_data_dir/lineitem.tbl"
output_dir="$script_dir/testdata"
binary="$output_dir/tpch_lineitem_512b.bin"
manifest="$output_dir/tpch_lineitem_512b.csv"

schema='orderkey:u64@0,partkey:u64@8,suppkey:u64@16,linenumber:u8@24,quantity:u8@25,returnflag:u8@26,linestatus:u8@27,shipmode_code:u8@28,shipinstruct_code:u8@29,extendedprice_cents:u64@32,discount_bp:u16@40,tax_bp:u16@42,shipdate:u32@44,commitdate:u32@48,receiptdate:u32@52,source_row:u64@56'
query='SELECT quantity FROM lineitem WHERE shipdate >= 19940101 AND shipdate < 19950101 AND discount_bp >= 500 AND discount_bp <= 700 AND quantity < 24'

case "$mode" in
    --prepare|--write|--run) ;;
    *)
        echo "usage: $0 [--prepare|--write|--run]" >&2
        exit 2
        ;;
esac

if [[ "$mode" == "--prepare" ]]; then
    if [[ ! -f "$dbgen_dir/.git" && ! -d "$dbgen_dir/.git" ]]; then
        echo "TPC-H submodule is missing; run: git submodule update --init third_party/tpch-dbgen" >&2
        exit 1
    fi
    make -C "$dbgen_dir" -j2 dbgen
    mkdir -p "$tpch_data_dir" "$output_dir"
    if [[ ! -s "$lineitem_tbl" ]]; then
        echo "Generating TPC-H lineitem.tbl at scale factor $tpch_scale"
        DSS_PATH="$tpch_data_dir" "$dbgen_dir/dbgen" -s "$tpch_scale" -f -T L
    fi
    python3 "$script_dir/convert_lineitem.py" \
        --input "$lineitem_tbl" \
        --output "$binary" \
        --manifest "$manifest" \
        --records "$tpch_records" \
        --skip-rows "$tpch_skip_rows"
    python3 "$script_dir/verify_lineitem_binary.py" \
        --input "$lineitem_tbl" \
        --binary "$binary" \
        --records "$tpch_records" \
        --skip-rows "$tpch_skip_rows"
    (
        cd "$script_dir"
        python3 -m unittest -v test_convert_lineitem.py
    )
    echo "Prepared dataset only: $binary"
    exit 0
fi

if [[ ! -s "$binary" ]]; then
    echo "Prepared TPC-H binary is missing: $binary" >&2
    echo "Run $0 --prepare first" >&2
    exit 1
fi
expected_bytes=$((tpch_records * 512))
actual_bytes=$(stat -c '%s' "$binary")
if [[ "$actual_bytes" -ne "$expected_bytes" ]]; then
    echo "Prepared binary size is $actual_bytes; expected $expected_bytes for $tpch_records records" >&2
    echo "Run $0 --prepare with the same TPCH_RECORDS value" >&2
    exit 1
fi
sha256sum "$binary"

python3 "$script_dir/write_tpch_dataset_to_ssd.py" \
    --input "$binary" \
    --device "$ssd_device" \
    --lba "$ssd_lba"

if [[ "$mode" == "--write" ]]; then
    echo "Dataset written and verified; FPGA execution was not requested"
    exit 0
fi

make -C "$filter_app_dir" -j2
set +e
"$filter_app_dir/vscode-selective-lwe-encrypt-offload" \
    --ssd-nsid "$ssd_nsid" \
    --ssd-lba "$ssd_lba" \
    --records "$tpch_records" \
    --record-bytes 512 \
    --schema "$schema" \
    --query "$query" \
    --reference "$binary" \
    --key "$key_path" \
    --output "$output_dir/tpch_lineitem_q6_ciphertexts.bin" \
    --benchmark \
    2>&1 | tee "$output_dir/tpch_lineitem_q6_fpga.log"
test_rc=${PIPESTATUS[0]}
set -e
printf 'test_exit_code=%s\n' "$test_rc"
exit "$test_rc"
