#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=$(cd -- "${script_dir}/.." && pwd)
suda_root=$(cd -- "${hpu_root}/.." && pwd)
overlay_root="${hpu_root}/overlays/tfhe-rs"
status=0

required_files=(
    backends/tfhe-hpu-backend/scripts/v80-pcie-perms.sh
    backends/tfhe-hpu-backend/src/ffi/v80/mod.rs
    scripts/lwe_remote_hpu/package_server_129.sh
    scripts/lwe_remote_hpu/start_server_129.sh
    tfhe/Cargo.toml
    tfhe/examples/hpu/lwe_remote/bridge.rs
    tfhe/examples/hpu/lwe_remote/protocol.rs
    tfhe/examples/hpu/lwe_remote_server.rs
    tfhe/src/integer/hpu/ciphertext/mod.rs
)

for relative_path in "${required_files[@]}"; do
    if [[ ! -f ${overlay_root}/${relative_path} ]]; then
        printf 'error: missing remote HPU overlay file: %s\n' \
            "${relative_path}" >&2
        status=1
    fi
done

if find "${hpu_root}" \
    -path "${hpu_root}/worktree" -prune -o \
    -path "${hpu_root}/artifacts" -prune -o \
    -name .git -print -quit | grep -q .; then
    printf 'error: nested .git found under the tracked HPU source area\n' >&2
    status=1
fi

while IFS= read -r -d '' path; do
    size=$(stat -c %s "${path}")
    if (( size > 50 * 1024 * 1024 )); then
        printf 'error: file larger than 50 MiB under hpu/: %s\n' \
            "${path#"${suda_root}/"}" >&2
        status=1
    fi
done < <(find "${hpu_root}" \
    -path "${hpu_root}/worktree" -prune -o \
    -path "${hpu_root}/artifacts" -prune -o \
    -type f -print0)

if rg -n '/home/yangchenghui|BEGIN (OPENSSH|RSA|EC|DSA) PRIVATE KEY|XFL[[:alnum:]]{8,}' \
    "${hpu_root}" \
    --glob '!worktree/**' --glob '!artifacts/**' \
    --glob '!hpu/worktree/**' --glob '!hpu/artifacts/**' \
    --glob '!scripts/verify_remote_hpu.sh' \
    --glob '!hpu/scripts/verify_remote_hpu.sh'; then
    printf 'error: machine-specific path, board serial, or private key marker found under hpu/\n' >&2
    status=1
fi

if [[ $(rg -c '^name = "hpu_lwe_remote_server"$' \
    "${overlay_root}/tfhe/Cargo.toml") -ne 1 ]]; then
    printf 'error: Cargo manifest must declare exactly one remote HPU server example\n' >&2
    status=1
fi

if rg -n '^name = "hpu_(pipeline|export|lwe_remote_client|lwe_encrypt)' \
    "${overlay_root}/tfhe/Cargo.toml"; then
    printf 'error: unrelated HPU examples leaked into the minimal server overlay\n' >&2
    status=1
fi

while IFS= read -r script; do
    bash -n "${script}" || status=1
done < <(find "${hpu_root}" -path "${hpu_root}/worktree" -prune -o \
    -type f -name '*.sh' -print)

if [[ ${status} -ne 0 ]]; then
    exit "${status}"
fi

printf 'Remote HPU source boundary checks passed.\n'
