#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=$(cd -- "${script_dir}/.." && pwd)
overlay_root="${hpu_root}/overlays/tfhe-rs"
source "${hpu_root}/manifests/remote-hpu.env"

refresh=0
if [[ ${1:-} == "--refresh" ]]; then
    refresh=1
    shift
fi

if [[ $# -ne 1 ]]; then
    printf 'usage: %s [--refresh] TFHE_RS_CHECKOUT\n' "$0" >&2
    exit 2
fi

checkout=$1
if [[ ! -e ${checkout}/.git ]]; then
    printf 'error: not a TFHE-rs Git checkout: %s\n' "${checkout}" >&2
    exit 1
fi
checkout=$(cd -- "${checkout}" && pwd)

actual_revision=$(git -C "${checkout}" rev-parse HEAD)
if [[ ${actual_revision} != "${TFHE_RS_REVISION}" ]]; then
    printf 'error: TFHE-rs revision mismatch\n  expected: %s\n  actual:   %s\n' \
        "${TFHE_RS_REVISION}" "${actual_revision}" >&2
    exit 1
fi

overlay_matches() {
    local source_path relative_path
    while IFS= read -r -d '' source_path; do
        relative_path=${source_path#"${overlay_root}/"}
        [[ -f ${checkout}/${relative_path} ]] || return 1
        cmp -s "${source_path}" "${checkout}/${relative_path}" || return 1
    done < <(find "${overlay_root}" -type f -print0)
}

if overlay_matches; then
    printf 'TFHE-rs remote HPU overlay is already current: %s\n' "${checkout}"
    exit 0
fi

if ! git -C "${checkout}" diff --cached --quiet; then
    printf 'error: refusing to overwrite staged TFHE-rs changes: %s\n' \
        "${checkout}" >&2
    exit 1
fi

if [[ -n $(git -C "${checkout}" status --porcelain --untracked-files=all) ]]; then
    if [[ ${refresh} -ne 1 ]]; then
        printf 'error: TFHE-rs checkout is not clean; use --refresh only when all local changes are old overlay files\n' >&2
        exit 1
    fi

    declare -A allowed_paths=()
    while IFS= read -r -d '' source_path; do
        relative_path=${source_path#"${overlay_root}/"}
        allowed_paths["${relative_path}"]=1
    done < <(find "${overlay_root}" -type f -print0)

    while IFS= read -r -d '' relative_path; do
        if [[ -z ${allowed_paths["${relative_path}"]+present} ]]; then
            printf 'error: refusing --refresh because a non-overlay file changed: %s\n' \
                "${relative_path}" >&2
            exit 1
        fi
    done < <(
        git -C "${checkout}" diff --name-only -z
        git -C "${checkout}" ls-files --others --exclude-standard -z
    )
fi

cp -a "${overlay_root}/." "${checkout}/"

if ! overlay_matches; then
    printf 'error: overlay verification failed after copy: %s\n' "${checkout}" >&2
    exit 1
fi

printf 'Applied remote HPU overlay to %s\n' "${checkout}"
