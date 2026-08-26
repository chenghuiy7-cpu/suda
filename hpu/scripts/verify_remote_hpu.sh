#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=$(cd -- "${script_dir}/.." && pwd)
suda_root=$(cd -- "${hpu_root}/.." && pwd)
tfhe_root="${hpu_root}/tfhe-rs"
source "${hpu_root}/manifests/remote-hpu.env"
status=0

required_files=(
    README.md
    remote-hpu/Cargo.toml
    remote-hpu/src/main.rs
    remote-hpu/src/bridge.rs
    remote-hpu/src/protocol.rs
    scripts/package_remote_server.sh
    scripts/prepare_tfhe_rs_submodule.sh
    scripts/start_remote_server.sh
    scripts/v80-pcie-perms.sh
)

for relative_path in "${required_files[@]}"; do
    if [[ ! -f ${hpu_root}/${relative_path} ]]; then
        printf 'error: missing remote HPU source: %s\n' "${relative_path}" >&2
        status=1
    fi
done

gitlink=$(git -C "${suda_root}" ls-files -s -- hpu/tfhe-rs || true)
if [[ ${gitlink} != "160000 ${TFHE_RS_REVISION} 0"$'\t'"hpu/tfhe-rs" ]]; then
    printf 'error: hpu/tfhe-rs is not the expected pinned gitlink: %s\n' "${gitlink}" >&2
    status=1
fi

if [[ ! -e ${tfhe_root}/.git ]]; then
    printf 'error: TFHE-rs submodule is not initialized\n' >&2
    status=1
else
    actual_revision=$(git -C "${tfhe_root}" rev-parse HEAD)
    if [[ ${actual_revision} != "${TFHE_RS_REVISION}" ]]; then
        printf 'error: TFHE-rs revision mismatch: %s\n' "${actual_revision}" >&2
        status=1
    fi
    if [[ -n $(git -C "${tfhe_root}" status --porcelain --untracked-files=all) ]]; then
        printf 'error: TFHE-rs submodule contains local modifications\n' >&2
        status=1
    fi
fi

if [[ -e ${hpu_root}/overlays ]]; then
    printf 'error: obsolete TFHE-rs overlay directory still exists\n' >&2
    status=1
fi

while IFS= read -r -d '' path; do
    size=$(stat -c %s "${path}")
    if (( size > 50 * 1024 * 1024 )); then
        printf 'error: file larger than 50 MiB under SUDA-owned hpu source: %s\n' \
            "${path#"${suda_root}/"}" >&2
        status=1
    fi
done < <(find "${hpu_root}" \
    -path "${tfhe_root}" -prune -o \
    -path "${hpu_root}/artifacts" -prune -o \
    -path "${hpu_root}/runtime" -prune -o \
    -path "${hpu_root}/remote-hpu/target" -prune -o \
    -type f -print0)

if rg -n '/home/yangchenghui|BEGIN (OPENSSH|RSA|EC|DSA) PRIVATE KEY|XFL[[:alnum:]]{8,}' \
    "${hpu_root}/README.md" "${hpu_root}/config" "${hpu_root}/manifests" \
    "${hpu_root}/remote-hpu" "${hpu_root}/scripts" \
    --glob '!target/**' --glob '!**/verify_remote_hpu.sh'; then
    printf 'error: machine-specific path, board serial, or private key marker found under hpu/\n' >&2
    status=1
fi

if ! rg -q 'path = "\.\./tfhe-rs/tfhe"' "${hpu_root}/remote-hpu/Cargo.toml"; then
    printf 'error: remote crate does not use the TFHE-rs submodule path dependency\n' >&2
    status=1
fi
if rg -n 'from_hpu_lwe_ciphertexts|to_hpu_lwe_ciphertexts' \
    "${hpu_root}/remote-hpu/src"; then
    printf 'error: remote service still depends on locally patched TFHE-rs APIs\n' >&2
    status=1
fi

while IFS= read -r script; do
    bash -n "${script}" || status=1
done < <(find "${hpu_root}/scripts" -type f -name '*.sh' -print)

if [[ ${status} -ne 0 ]]; then
    exit "${status}"
fi

printf 'Remote HPU submodule and SUDA source boundary checks passed.\n'
