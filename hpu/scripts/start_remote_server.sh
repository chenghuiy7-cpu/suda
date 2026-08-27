#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=${SUDA_HPU_ROOT:-$(cd -- "${script_dir}/.." && pwd)}
tfhe_root=${TFHE_RS_ROOT:-${hpu_root}/tfhe-rs}
remote_root=${SUDA_REMOTE_HPU_ROOT:-${hpu_root}/remote-hpu}
runtime_root=${HPU_REMOTE_RUNTIME_ROOT:-${hpu_root}/runtime}
manifest=${hpu_root}/manifests/remote-hpu.env

if [[ ! -f ${manifest} ]]; then
    printf 'missing remote HPU manifest: %s\n' "${manifest}" >&2
    exit 1
fi
source "${manifest}"

export HPU_BACKEND_DIR=${HPU_BACKEND_DIR:-${runtime_root}/tfhe-hpu-backend}
config=${HPU_REMOTE_CONFIG:-${HPU_BACKEND_DIR}/config_store/v80/hpu_config.toml}
server_key=${HPU_REMOTE_SERVER_KEY:-${runtime_root}/${HPU_SERVER_KEY_FILE}}
hpu_archive=${HPU_REMOTE_HPU_ARCHIVE:-${HPU_BACKEND_DIR}/config_store/v80_archives/${HPU_ARCHIVE_FILE}}
bind_addr=${HPU_REMOTE_BIND:-0.0.0.0:19090}
expected_server_key_sha256=${HPU_REMOTE_SERVER_KEY_SHA256:-${HPU_SERVER_KEY_SHA256}}
expected_server_key_size=${HPU_REMOTE_SERVER_KEY_SIZE:-${HPU_SERVER_KEY_SIZE}}

export HPU_CONFIG=${HPU_CONFIG:-v80}
export RUST_LOG=${RUST_LOG:-info}

: "${XILINX_VIVADO:?set XILINX_VIVADO to the authorized Vivado installation}"
: "${V80_PCIE_DEV:?set V80_PCIE_DEV to the local V80 PCIe bus ID}"
: "${V80_SERIAL_NUMBER:?set V80_SERIAL_NUMBER for the local V80 board}"
: "${AMI_PATH:?set AMI_PATH to the installed AMI driver directory}"

export XILINX_VIVADO V80_PCIE_DEV V80_SERIAL_NUMBER AMI_PATH

if [[ ${XILINX_VIVADO} == /path/to/Vivado || ! -x ${XILINX_VIVADO}/bin/vivado ]]; then
    printf 'invalid XILINX_VIVADO or missing bin/vivado: %s\n' \
        "${XILINX_VIVADO}" >&2
    exit 1
fi
if [[ ! ${V80_PCIE_DEV} =~ ^[[:xdigit:]]{2}$ ]]; then
    printf 'invalid V80_PCIE_DEV; expected the two-digit PCIe bus ID: %s\n' \
        "${V80_PCIE_DEV}" >&2
    exit 1
fi
if [[ ${V80_SERIAL_NUMBER} == REPLACE_ME ]]; then
    printf 'replace the V80_SERIAL_NUMBER placeholder before starting\n' >&2
    exit 1
fi
if [[ ! -f ${AMI_PATH}/ami.ko ]]; then
    printf 'missing AMI kernel module: %s/ami.ko\n' "${AMI_PATH}" >&2
    exit 1
fi

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

grep -Eq 'force_reload[[:space:]]*=[[:space:]]*"false"' "${config}" || {
    printf 'unmodified upstream TFHE-rs requires force_reload="false": %s\n' "${config}" >&2
    exit 1
}

verify_file "HPU archive" "${hpu_archive}" \
    "${HPU_ARCHIVE_SIZE}" "${HPU_ARCHIVE_SHA256}"
verify_file "compressed server key" "${server_key}" \
    "${expected_server_key_size}" "${expected_server_key_sha256}"

printf '%s\n' \
    "TFHE_RS_ROOT=${tfhe_root}" \
    "HPU_BACKEND_DIR=${HPU_BACKEND_DIR}" \
    "HPU_CONFIG=${HPU_CONFIG}" \
    "V80_PCIE_DEV=${V80_PCIE_DEV}" \
    "V80_SERIAL_NUMBER=${V80_SERIAL_NUMBER}" \
    "HPU_REMOTE_CONFIG=${config}" \
    "HPU_REMOTE_HPU_ARCHIVE=${hpu_archive}" \
    "HPU_REMOTE_SERVER_KEY=${server_key}" \
    "HPU_REMOTE_BIND=${bind_addr}"

printf '%s\n' \
    'warning: upstream force_reload=false may recover invalid hardware by reloading it;' \
    'verify the V80/AMI/QDMA state before starting this service.' >&2

binary=${HPU_REMOTE_SERVER_BINARY:-${hpu_root}/bin/suda-remote-hpu-server}
if [[ ${HPU_REMOTE_PREFLIGHT_ONLY:-0} == 1 ]]; then
    if [[ ! -x ${binary} ]]; then
        printf 'missing executable remote server binary: %s\n' "${binary}" >&2
        exit 1
    fi
    printf 'remote_server_preflight=passed\n'
    exit 0
fi

if [[ ! -x ${binary} ]]; then
    cargo "+${TFHE_RS_RUST_TOOLCHAIN}" build --release \
        --manifest-path "${remote_root}/Cargo.toml"
    binary="${remote_root}/target/release/suda-remote-hpu-server"
fi

exec sudo -E env RUST_BACKTRACE=1 RUST_LOG="${RUST_LOG}" \
    "${binary}" \
    --bind "${bind_addr}" \
    --config "${config}" \
    --server-key "${server_key}"
