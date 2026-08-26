#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=$(cd -- "${script_dir}/.." && pwd)
suda_root=$(cd -- "${hpu_root}/.." && pwd)
remote_root="${hpu_root}/remote-hpu"
tfhe_root="${hpu_root}/tfhe-rs"
output=${1:-${hpu_root}/artifacts/suda-remote-hpu-server.tar.gz}
server_key_source=${SERVER_KEY_SOURCE:-}
expected_server_key_sha256=${HPU_REMOTE_SERVER_KEY_SHA256:-}

source "${hpu_root}/manifests/remote-hpu.env"

: "${server_key_source:?set SERVER_KEY_SOURCE to the compressed server key}"
: "${expected_server_key_sha256:?set HPU_REMOTE_SERVER_KEY_SHA256 from the private manifest}"

actual_revision=$(git -C "${tfhe_root}" rev-parse HEAD)
if [[ ${actual_revision} != "${TFHE_RS_REVISION}" ]]; then
    printf 'unexpected TFHE-rs submodule revision: %s\n' "${actual_revision}" >&2
    exit 1
fi
if [[ -n $(git -C "${tfhe_root}" status --porcelain --untracked-files=all) ]]; then
    printf 'refusing to package with a modified TFHE-rs submodule\n' >&2
    exit 1
fi

if [[ ! -f ${server_key_source} ]]; then
    printf 'missing compressed server key: %s\n' "${server_key_source}" >&2
    exit 1
fi
actual_key_sha256=$(sha256sum "${server_key_source}" | awk '{print $1}')
if [[ ${actual_key_sha256} != "${expected_server_key_sha256}" ]]; then
    printf 'unexpected compressed server key SHA-256: %s\n' "${actual_key_sha256}" >&2
    exit 1
fi

cargo "+${TFHE_RS_RUST_TOOLCHAIN}" build --release \
    --manifest-path "${remote_root}/Cargo.toml"
binary="${remote_root}/target/release/suda-remote-hpu-server"

staging=$(mktemp -d)
trap 'rm -rf "${staging}"' EXIT
install -D -m 0755 "${binary}" "${staging}/bin/suda-remote-hpu-server"
install -D -m 0755 "${script_dir}/start_remote_server.sh" \
    "${staging}/scripts/start_remote_server.sh"
install -D -m 0755 "${script_dir}/v80-pcie-perms.sh" \
    "${staging}/scripts/v80-pcie-perms.sh"
install -D -m 0644 "${hpu_root}/config/hpu-server.env.example" \
    "${staging}/config/hpu-server.env.example"
install -D -m 0644 "${hpu_root}/manifests/remote-hpu.env" \
    "${staging}/manifests/remote-hpu.env"
install -D -m 0644 "${server_key_source}" \
    "${staging}/runtime/psi64_integer_compressed_server_key.bincode"

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
printf 'server_key_sha256=%s\n' "${actual_key_sha256}"
printf 'source_of_truth=%s\n' "${suda_root}"
