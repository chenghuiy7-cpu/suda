#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=${SUDA_HPU_ROOT:-$(cd -- "${script_dir}/.." && pwd)}
tfhe_root=${TFHE_RS_ROOT:-${hpu_root}/tfhe-rs}
remote_root=${SUDA_REMOTE_HPU_ROOT:-${hpu_root}/remote-hpu}
config=${HPU_REMOTE_CONFIG:-${tfhe_root}/backends/tfhe-hpu-backend/config_store/v80/hpu_config.toml}
server_key=${HPU_REMOTE_SERVER_KEY:-${hpu_root}/runtime/psi64_integer_compressed_server_key.bincode}
bind_addr=${HPU_REMOTE_BIND:-0.0.0.0:19090}
expected_server_key_sha256=${HPU_REMOTE_SERVER_KEY_SHA256:-}

export HPU_BACKEND_DIR=${HPU_BACKEND_DIR:-${tfhe_root}/backends/tfhe-hpu-backend}
export HPU_CONFIG=${HPU_CONFIG:-v80}
export RUST_LOG=${RUST_LOG:-info}

: "${XILINX_VIVADO:?set XILINX_VIVADO to the authorized Vivado installation}"
: "${V80_PCIE_DEV:?set V80_PCIE_DEV to the local V80 PCIe bus ID}"
: "${V80_SERIAL_NUMBER:?set V80_SERIAL_NUMBER for the local V80 board}"
: "${AMI_PATH:?set AMI_PATH to the installed AMI driver directory}"
: "${expected_server_key_sha256:?set HPU_REMOTE_SERVER_KEY_SHA256 after verifying the key manifest}"

export XILINX_VIVADO V80_PCIE_DEV V80_SERIAL_NUMBER AMI_PATH

grep -Eq 'force_reload[[:space:]]*=[[:space:]]*"false"' "${config}" || {
    printf 'unmodified upstream TFHE-rs requires force_reload="false": %s\n' "${config}" >&2
    exit 1
}

actual_key_sha256=$(sha256sum "${server_key}" | awk '{print $1}')
if [[ ${actual_key_sha256} != "${expected_server_key_sha256}" ]]; then
    printf 'unexpected compressed server key SHA-256: %s\n' "${actual_key_sha256}" >&2
    exit 1
fi

binary=${HPU_REMOTE_SERVER_BINARY:-${hpu_root}/bin/suda-remote-hpu-server}
if [[ ! -x ${binary} ]]; then
    source "${hpu_root}/manifests/remote-hpu.env"
    cargo "+${TFHE_RS_RUST_TOOLCHAIN}" build --release \
        --manifest-path "${remote_root}/Cargo.toml"
    binary="${remote_root}/target/release/suda-remote-hpu-server"
fi

printf '%s\n' \
    "TFHE_RS_ROOT=${tfhe_root}" \
    "HPU_BACKEND_DIR=${HPU_BACKEND_DIR}" \
    "HPU_CONFIG=${HPU_CONFIG}" \
    "V80_PCIE_DEV=${V80_PCIE_DEV}" \
    "V80_SERIAL_NUMBER=${V80_SERIAL_NUMBER}" \
    "HPU_REMOTE_CONFIG=${config}" \
    "HPU_REMOTE_SERVER_KEY=${server_key}" \
    "HPU_REMOTE_BIND=${bind_addr}"

printf '%s\n' \
    'warning: upstream force_reload=false may recover invalid hardware by reloading it;' \
    'verify the V80/AMI/QDMA state before starting this service.' >&2

exec sudo -E env RUST_BACKTRACE=1 RUST_LOG="${RUST_LOG}" \
    "${binary}" \
    --bind "${bind_addr}" \
    --config "${config}" \
    --server-key "${server_key}"
