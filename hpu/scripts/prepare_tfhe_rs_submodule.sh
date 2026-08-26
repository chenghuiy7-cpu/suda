#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=$(cd -- "${script_dir}/.." && pwd)
suda_root=$(cd -- "${hpu_root}/.." && pwd)
tfhe_root="${hpu_root}/tfhe-rs"
source "${hpu_root}/manifests/remote-hpu.env"

git -C "${suda_root}" submodule update --init -- hpu/tfhe-rs

actual_revision=$(git -C "${tfhe_root}" rev-parse HEAD)
if [[ ${actual_revision} != "${TFHE_RS_REVISION}" ]]; then
    printf 'TFHE-rs revision mismatch\n  expected: %s\n  actual:   %s\n' \
        "${TFHE_RS_REVISION}" "${actual_revision}" >&2
    exit 1
fi

if [[ -n $(git -C "${tfhe_root}" status --porcelain --untracked-files=all) ]]; then
    printf 'TFHE-rs submodule must remain unmodified: %s\n' "${tfhe_root}" >&2
    exit 1
fi

printf 'TFHE-rs submodule ready at %s (%s)\n' \
    "${tfhe_root}" "${actual_revision}"
