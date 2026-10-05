#!/usr/bin/env bash
set -euo pipefail

OPERATOR_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
SUDA_ROOT=$(cd -- "${OPERATOR_DIR}/../../../.." && pwd)
RTL_DIR="${OPERATOR_DIR}/selective_filter/solution1/syn/verilog"
POOL_DIR="${SUDA_ROOT}/device/platform/basic_shell/nf-csd/shell/virt_one_drive/fpga/sources/hlsaccframework/selective_filter"
BACKUP_ROOT="${SUDA_ROOT}/../suda_backups/rtl"

if [[ ! -s "${RTL_DIR}/selective_filter.v" ]]; then
    echo 'Missing synthesized selective_filter.v; run HLS csynth first.' >&2
    exit 1
fi
for source_file in selective_filter.cpp selective_filter.hpp selective_filter_protocol.h; do
    if [[ "${RTL_DIR}/selective_filter.v" -ot "${OPERATOR_DIR}/${source_file}" ]]; then
        echo "Stale RTL relative to ${source_file}; rerun HLS csynth." >&2
        exit 1
    fi
done
mkdir -p "${BACKUP_ROOT}"
BACKUP_DIR=$(mktemp -d "${BACKUP_ROOT}/selective_filter_before_manifest_$(date +%Y%m%d_%H%M%S).XXXXXX")
mkdir -p "${BACKUP_DIR}/new_rtl"
cp "${RTL_DIR}"/*.v "${BACKUP_DIR}/new_rtl/"
if [[ -d "${POOL_DIR}" ]]; then
    mv "${POOL_DIR}" "${BACKUP_DIR}/old_shell_rtl"
fi
mv "${BACKUP_DIR}/new_rtl" "${POOL_DIR}"
echo "Old RTL backup: ${BACKUP_DIR}/old_shell_rtl"
echo "Updated filter RTL: ${POOL_DIR}"
sha256sum "${POOL_DIR}/selective_filter.v"
