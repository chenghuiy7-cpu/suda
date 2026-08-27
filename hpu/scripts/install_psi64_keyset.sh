#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
hpu_root=$(cd -- "${script_dir}/.." && pwd)
suda_root=$(cd -- "${hpu_root}/.." && pwd)
source_dir=${1:-${SUDA_PSI64_KEY_DIR:-${hpu_root}/keys/psi64}}
destination=${2:-${suda_root}/device/operators/hls/lwe_encrypt/testdata}

files=(
    psi64_big_lwe_secret_key.bin
    psi64_shortint_ks32_client_key.bincode
    psi64_integer_compressed_server_key.bincode
)

for file in "${files[@]}"; do
    if [[ ! -f ${source_dir}/${file} ]]; then
        printf 'missing psi64 key file: %s\n' "${source_dir}/${file}" >&2
        exit 1
    fi
done

big_lwe_key="${source_dir}/psi64_big_lwe_secret_key.bin"
if [[ $(stat -c %s "${big_lwe_key}") != 2048 ]]; then
    printf 'unexpected Big-LWE key size: %s\n' "$(stat -c %s "${big_lwe_key}")" >&2
    exit 1
fi
if ! LC_ALL=C tr -d '\000\001' < "${big_lwe_key}" | cmp -s - /dev/null; then
    printf 'Big-LWE key contains coefficients other than 0 and 1\n' >&2
    exit 1
fi

mkdir -p "${destination}"
for file in "${files[@]}"; do
    source_file=$(realpath -m "${source_dir}/${file}")
    destination_file=$(realpath -m "${destination}/${file}")
    if [[ ${source_file} != "${destination_file}" ]]; then
        install -m 0600 "${source_file}" "${destination_file}"
    fi
done

if [[ -f ${source_dir}/psi64_key_manifest.txt ]]; then
    install -m 0600 "${source_dir}/psi64_key_manifest.txt" \
        "${destination}/psi64_key_manifest.txt"
fi

(
    cd "${destination}"
    sha256sum "${files[@]}" > psi64_keyset.sha256
)
chmod 0600 "${destination}/psi64_keyset.sha256"

printf '%s\n' \
    "keyset_source=$(realpath -m "${source_dir}")" \
    "keyset_destination=$(realpath -m "${destination}")" \
    'psi64_keyset_installed=yes'
