#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=${SUDA_HPU_ROOT:-$(cd -- "${script_dir}/.." && pwd)}
suda_root=$(cd -- "${hpu_root}/.." && pwd)
tfhe_root=${TFHE_RS_ROOT:-${hpu_root}/tfhe-rs}
runtime_root=${HPU_REMOTE_RUNTIME_ROOT:-${hpu_root}/runtime}
artifact_root=${HPU_PRIVATE_ARTIFACT_ROOT:-${hpu_root}/artifacts/private}
backend_source="${tfhe_root}/backends/tfhe-hpu-backend"
backend_runtime="${runtime_root}/tfhe-hpu-backend"
hpu_archive_source=${HPU_ARCHIVE_SOURCE:-${artifact_root}/psi64.hpu}
server_key_source=${SERVER_KEY_SOURCE:-${hpu_root}/keys/psi64/psi64_integer_compressed_server_key.bincode}

source "${hpu_root}/manifests/remote-hpu.env"
expected_server_key_size=${HPU_REMOTE_SERVER_KEY_SIZE:-${HPU_SERVER_KEY_SIZE}}
expected_server_key_sha256=${HPU_REMOTE_SERVER_KEY_SHA256:-${HPU_SERVER_KEY_SHA256}}

verify_file() {
    local label=$1
    local path=$2
    local expected_size=$3
    local expected_sha256=$4
    local actual_size
    local actual_sha256

    if [[ ! -f ${path} ]]; then
        printf 'error: missing %s: %s\n' "${label}" "${path}" >&2
        exit 1
    fi

    actual_size=$(stat -c %s "${path}")
    if [[ ${actual_size} != "${expected_size}" ]]; then
        printf 'error: unexpected %s size: expected=%s actual=%s path=%s\n' \
            "${label}" "${expected_size}" "${actual_size}" "${path}" >&2
        exit 1
    fi

    actual_sha256=$(sha256sum "${path}" | awk '{print $1}')
    if [[ ${actual_sha256} != "${expected_sha256}" ]]; then
        printf 'error: unexpected %s SHA-256: expected=%s actual=%s path=%s\n' \
            "${label}" "${expected_sha256}" "${actual_sha256}" "${path}" >&2
        exit 1
    fi
}

actual_revision=$(git -C "${tfhe_root}" rev-parse HEAD)
if [[ ${actual_revision} != "${TFHE_RS_REVISION}" ]]; then
    printf 'error: TFHE-rs revision mismatch: expected=%s actual=%s\n' \
        "${TFHE_RS_REVISION}" "${actual_revision}" >&2
    exit 1
fi
if [[ -n $(git -C "${tfhe_root}" status --porcelain --untracked-files=all) ]]; then
    printf 'error: TFHE-rs submodule must remain unmodified: %s\n' \
        "${tfhe_root}" >&2
    exit 1
fi

verify_file "HPU archive" "${hpu_archive_source}" \
    "${HPU_ARCHIVE_SIZE}" "${HPU_ARCHIVE_SHA256}"
verify_file "compressed server key" "${server_key_source}" \
    "${expected_server_key_size}" "${expected_server_key_sha256}"

# Runtime configuration is copied outside the submodule so installing the real
# HPU archive never changes the pinned upstream checkout.
mkdir -p "${backend_runtime}/config_store" "${backend_runtime}/scripts"
cp -a "${backend_source}/config_store/." "${backend_runtime}/config_store/"
cp -a "${backend_source}/scripts/." "${backend_runtime}/scripts/"
install -D -m 0644 "${hpu_archive_source}" \
    "${backend_runtime}/config_store/v80_archives/${HPU_ARCHIVE_FILE}"
install -D -m 0600 "${server_key_source}" \
    "${runtime_root}/${HPU_SERVER_KEY_FILE}"

verify_file "installed HPU archive" \
    "${backend_runtime}/config_store/v80_archives/${HPU_ARCHIVE_FILE}" \
    "${HPU_ARCHIVE_SIZE}" "${HPU_ARCHIVE_SHA256}"
verify_file "installed compressed server key" \
    "${runtime_root}/${HPU_SERVER_KEY_FILE}" \
    "${expected_server_key_size}" "${expected_server_key_sha256}"

if [[ -n $(git -C "${tfhe_root}" status --porcelain --untracked-files=all) ]]; then
    printf 'error: runtime preparation unexpectedly modified the TFHE-rs submodule\n' >&2
    exit 1
fi

printf '%s\n' \
    "suda_root=${suda_root}" \
    "tfhe_revision=${actual_revision}" \
    "tfhe_submodule_modified=no" \
    "hpu_backend_runtime=${backend_runtime}" \
    "hpu_archive=${backend_runtime}/config_store/v80_archives/${HPU_ARCHIVE_FILE}" \
    "server_key=${runtime_root}/${HPU_SERVER_KEY_FILE}" \
    'remote_runtime_ready=yes'
