#!/usr/bin/env bash
set -euo pipefail

OPERATOR_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
SUDA_ROOT=$(cd -- "${OPERATOR_DIR}/../../../.." && pwd)
RTL_DIR="${OPERATOR_DIR}/lwe_decrypt/solution1/syn/verilog"
POOL_DIR="${SUDA_ROOT}/device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/lwe_decrypt"
BACKUP_ROOT="${SUDA_ROOT}/../suda_backups/rtl"

if [[ ! -s "${RTL_DIR}/lwe_decrypt.v" ]]; then
    echo "错误：未找到已综合的 lwe_decrypt.v，请先执行 HLS rtl_gen" >&2
    exit 1
fi
for source_file in lwe_decrypt.cpp lwe_decrypt.hpp; do
    if [[ "${RTL_DIR}/lwe_decrypt.v" -ot "${OPERATOR_DIR}/${source_file}" ]]; then
        echo "错误：RTL 早于 ${source_file}，请重新执行 HLS rtl_gen" >&2
        exit 1
    fi
done

mkdir -p "${BACKUP_ROOT}"
BACKUP_DIR=$(mktemp -d "${BACKUP_ROOT}/lwe_decrypt_before_logical_$(date +%Y%m%d_%H%M%S).XXXXXX")
STAGING_DIR="${BACKUP_DIR}/new_rtl"
mkdir -p "${STAGING_DIR}"
cp "${RTL_DIR}"/*.v "${STAGING_DIR}/"

if [[ -d "${POOL_DIR}" ]]; then
    mv "${POOL_DIR}" "${BACKUP_DIR}/old_shell_rtl"
fi
mv "${STAGING_DIR}" "${POOL_DIR}"

echo "旧算子池 RTL 备份：${BACKUP_DIR}/old_shell_rtl"
echo "新版解密 RTL 已更新到：${POOL_DIR}"
sha256sum "${POOL_DIR}/lwe_decrypt.v"
