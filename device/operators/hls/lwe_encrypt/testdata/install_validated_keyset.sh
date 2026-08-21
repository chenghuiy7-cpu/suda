#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir=${1:-${NEST_LWE_KEYSET_SOURCE:-}}
destination=${2:-${NEST_LWE_KEYSET_DESTINATION:-$script_dir}}

if [[ -z "$source_dir" ]]; then
    echo "Usage: $0 KEYSET_SOURCE_DIR [DESTINATION_DIR]" >&2
    echo "Or set NEST_LWE_KEYSET_SOURCE and NEST_LWE_KEYSET_DESTINATION." >&2
    exit 2
fi

declare -A expected_sha256=(
    [psi64_big_lwe_secret_key.bin]=b5f4d159d1b14870a5b3e45da870c1dd281175485a4f649d5405c76c30fd47e9
    [psi64_shortint_ks32_client_key.bincode]=582947a24c185d9b90f52cb12bec9f608b66dcf9a0a47e8c347c6dc9e0184c76
    [psi64_integer_compressed_server_key.bincode]=09f605c234ccc85425dd548796e3eeb6cb1cfd6da26fe5b9c327835c9c423e18
)

files=(
    psi64_big_lwe_secret_key.bin
    psi64_shortint_ks32_client_key.bincode
    psi64_integer_compressed_server_key.bincode
)

for file in "${files[@]}"; do
    source_file="$source_dir/$file"
    if [[ ! -f "$source_file" ]]; then
        echo "Missing key artifact: $source_file" >&2
        exit 1
    fi

    actual=$(sha256sum "$source_file" | awk '{print $1}')
    if [[ "$actual" != "${expected_sha256[$file]}" ]]; then
        echo "SHA-256 mismatch for $source_file" >&2
        echo "expected=${expected_sha256[$file]}" >&2
        echo "actual=$actual" >&2
        exit 1
    fi
done

install -d -m 0700 "$destination"
for file in "${files[@]}"; do
    source_file=$(realpath -m "$source_dir/$file")
    destination_file=$(realpath -m "$destination/$file")
    if [[ "$source_file" != "$destination_file" ]]; then
        install -m 0600 "$source_file" "$destination_file"
    else
        chmod 0600 "$destination_file"
    fi
done

(
    cd "$destination"
    sha256sum "${files[@]}" > psi64_keyset.sha256
)
chmod 0600 "$destination/psi64_keyset.sha256"

echo "validated_keyset_installed=yes"
echo "keyset_destination=$(realpath -m "$destination")"
echo "big_lwe_secret_key=$destination/psi64_big_lwe_secret_key.bin"
echo "tfhe_client_key=$destination/psi64_shortint_ks32_client_key.bincode"
echo "tfhe_server_key=$destination/psi64_integer_compressed_server_key.bincode"
