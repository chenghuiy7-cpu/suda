#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
suda_root=$(cd -- "$script_dir/../../.." && pwd)
decrypt_dir="$suda_root/host/applications/vscode-lwe-decrypt-offload"
native_file="$script_dir/testdata/tpch_q6_remote_hpu_native.bin"
expected_file="$script_dir/testdata/tpch_q6_remote_expected_u8.bin"
key_path=${TPCH_LWE_KEY:-$suda_root/device/operators/hls/lwe_encrypt/testdata/psi64_big_lwe_secret_key.bin}

if [[ ! -s "$native_file" || ! -s "$expected_file" ]]; then
    echo "Missing diagnostic fixture. Run ./run_tpch_q6_update.sh first." >&2
    exit 1
fi

plaintext_bytes=$(stat -c '%s' "$expected_file")
expected_native_bytes=$((plaintext_bytes * 98304))
actual_native_bytes=$(stat -c '%s' "$native_file")
if [[ "$actual_native_bytes" -ne "$expected_native_bytes" ]]; then
    echo "Native fixture has $actual_native_bytes bytes; expected $expected_native_bytes." >&2
    exit 1
fi

make -C "$decrypt_dir" -j2
exec taskset -c 1 "$decrypt_dir/vscode-lwe-decrypt-offload" \
    --input "$native_file" \
    --input-format hpu-native \
    --plaintext-bytes "$plaintext_bytes" \
    --expect-file "$expected_file" \
    --key "$key_path" \
    --skip-output \
    --benchmark
