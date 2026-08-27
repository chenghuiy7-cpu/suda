#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=$(cd -- "${script_dir}/.." && pwd)
suda_root=$(cd -- "${hpu_root}/.." && pwd)
output_dir=${SUDA_PSI64_KEY_DIR:-${hpu_root}/keys/psi64}
source "${hpu_root}/manifests/remote-hpu.env"

cd "${suda_root}"
cargo "+${TFHE_RS_RUST_TOOLCHAIN}" run --release \
    --manifest-path hpu/keygen/Cargo.toml -- \
    --output-dir "${output_dir}" \
    --sync-write "$@"

(
    cd "${output_dir}"
    sha256sum \
        psi64_big_lwe_secret_key.bin \
        psi64_shortint_ks32_client_key.bincode \
        psi64_integer_compressed_server_key.bincode \
        > psi64_keyset.sha256
)
chmod 0600 "${output_dir}"/*

server_key="${output_dir}/psi64_integer_compressed_server_key.bincode"
printf '%s\n' \
    "psi64_keyset_dir=${output_dir}" \
    "server_key_size=$(stat -c %s "${server_key}")" \
    "server_key_sha256=$(sha256sum "${server_key}" | awk '{print $1}')" \
    'psi64_keyset_generated=yes'
