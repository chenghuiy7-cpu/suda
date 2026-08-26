#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=$(cd -- "${script_dir}/.." && pwd)
source "${hpu_root}/manifests/remote-hpu.env"

if [[ $# -gt 1 ]]; then
    printf 'usage: %s [DESTINATION]\n' "$0" >&2
    exit 2
fi

destination=${1:-${hpu_root}/worktree/tfhe-rs}
source_repo=${TFHE_RS_SOURCE:-${TFHE_RS_URL}}

if [[ ! -e ${destination} ]]; then
    mkdir -p "$(dirname -- "${destination}")"
    GIT_LFS_SKIP_SMUDGE=1 git clone --no-checkout "${source_repo}" "${destination}"
    GIT_LFS_SKIP_SMUDGE=1 git -C "${destination}" checkout --detach \
        "${TFHE_RS_REVISION}"
elif [[ ! -e ${destination}/.git ]]; then
    printf 'error: destination exists but is not a Git checkout: %s\n' \
        "${destination}" >&2
    exit 1
fi

"${script_dir}/apply_tfhe_rs_overlay.sh" "${destination}"
printf 'TFHE-rs worktree ready at %s\n' "$(cd -- "${destination}" && pwd)"
