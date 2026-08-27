#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=$(cd -- "${script_dir}/.." && pwd)
suda_root=$(cd -- "${hpu_root}/.." && pwd)
remote_root="${hpu_root}/remote-hpu"
tfhe_root="${hpu_root}/tfhe-rs"
output=${1:-${hpu_root}/artifacts/suda-remote-hpu-server.tar.gz}
hpu_archive_source=${HPU_ARCHIVE_SOURCE:-${hpu_root}/artifacts/private/psi64.hpu}
server_key_source=${SERVER_KEY_SOURCE:-${hpu_root}/keys/psi64/psi64_integer_compressed_server_key.bincode}
ami_module_source=${AMI_MODULE_SOURCE:-}

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
        printf 'missing %s: %s\n' "${label}" "${path}" >&2
        exit 1
    fi
    actual_size=$(stat -c %s "${path}")
    if [[ ${actual_size} != "${expected_size}" ]]; then
        printf 'unexpected %s size: expected=%s actual=%s path=%s\n' \
            "${label}" "${expected_size}" "${actual_size}" "${path}" >&2
        exit 1
    fi
    actual_sha256=$(sha256sum "${path}" | awk '{print $1}')
    if [[ ${actual_sha256} != "${expected_sha256}" ]]; then
        printf 'unexpected %s SHA-256: expected=%s actual=%s path=%s\n' \
            "${label}" "${expected_sha256}" "${actual_sha256}" "${path}" >&2
        exit 1
    fi
}

actual_revision=$(git -C "${tfhe_root}" rev-parse HEAD)
if [[ ${actual_revision} != "${TFHE_RS_REVISION}" ]]; then
    printf 'unexpected TFHE-rs submodule revision: %s\n' "${actual_revision}" >&2
    exit 1
fi
if [[ -n $(git -C "${tfhe_root}" status --porcelain --untracked-files=all) ]]; then
    printf 'refusing to package with a modified TFHE-rs submodule\n' >&2
    exit 1
fi

verify_file "HPU archive" "${hpu_archive_source}" \
    "${HPU_ARCHIVE_SIZE}" "${HPU_ARCHIVE_SHA256}"
verify_file "compressed server key" "${server_key_source}" \
    "${expected_server_key_size}" "${expected_server_key_sha256}"

cargo_target_dir=${CARGO_TARGET_DIR:-${remote_root}/target}
if [[ ${cargo_target_dir} != /* ]]; then
    cargo_target_dir=$(realpath -m "${PWD}/${cargo_target_dir}")
fi

CARGO_TARGET_DIR=${cargo_target_dir} \
cargo "+${TFHE_RS_RUST_TOOLCHAIN}" build --release \
    --manifest-path "${remote_root}/Cargo.toml"
binary="${cargo_target_dir}/release/suda-remote-hpu-server"

staging=$(mktemp -d)
trap 'rm -rf "${staging}"' EXIT
install -D -m 0755 "${binary}" "${staging}/bin/suda-remote-hpu-server"
install -D -m 0755 "${script_dir}/start_remote_server.sh" \
    "${staging}/scripts/start_remote_server.sh"
install -D -m 0755 "${script_dir}/v80-pcie-perms.sh" \
    "${staging}/scripts/v80-pcie-perms.sh"
install -D -m 0644 "${hpu_root}/config/hpu-server.env.example" \
    "${staging}/config/hpu-server.env.example"
install -D -m 0644 "${hpu_root}/config/hpu-server-bundle.env.example" \
    "${staging}/config/hpu-server-bundle.env.example"
install -D -m 0644 "${hpu_root}/manifests/remote-hpu.env" \
    "${staging}/manifests/remote-hpu.env"
install -D -m 0644 "${server_key_source}" \
    "${staging}/runtime/psi64_integer_compressed_server_key.bincode"

mkdir -p "${staging}/runtime/tfhe-hpu-backend/config_store"
mkdir -p "${staging}/runtime/tfhe-hpu-backend/scripts"
cp -a "${tfhe_root}/backends/tfhe-hpu-backend/config_store/." \
    "${staging}/runtime/tfhe-hpu-backend/config_store/"
cp -a "${tfhe_root}/backends/tfhe-hpu-backend/scripts/." \
    "${staging}/runtime/tfhe-hpu-backend/scripts/"
install -D -m 0644 "${hpu_archive_source}" \
    "${staging}/runtime/tfhe-hpu-backend/config_store/v80_archives/${HPU_ARCHIVE_FILE}"

if [[ -n ${ami_module_source} ]]; then
    if [[ ! -f ${ami_module_source} ]]; then
        printf 'missing AMI kernel module: %s\n' "${ami_module_source}" >&2
        exit 1
    fi
    install -D -m 0644 "${ami_module_source}" \
        "${staging}/runtime/ami-driver/ami.ko"
fi

(
    cd "${staging}"
    find . -type f ! -name SHA256SUMS -print0 \
        | sort -z \
        | xargs -0 sha256sum > SHA256SUMS
)

mkdir -p "$(dirname -- "${output}")"
tar -C "${staging}" -czf "${output}" .
printf 'server_bundle=%s\n' "${output}"
printf 'server_bundle_sha256=%s\n' "$(sha256sum "${output}" | awk '{print $1}')"
printf 'server_key_sha256=%s\n' "${expected_server_key_sha256}"
printf 'hpu_archive_sha256=%s\n' "${HPU_ARCHIVE_SHA256}"
printf 'ami_module_included=%s\n' "$([[ -n ${ami_module_source} ]] && echo yes || echo no)"
printf 'source_of_truth=%s\n' "${suda_root}"
