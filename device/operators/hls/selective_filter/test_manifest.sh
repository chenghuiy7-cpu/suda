#!/usr/bin/env bash
set -euo pipefail
OPERATOR_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
SUDA_ROOT=$(cd -- "${OPERATOR_DIR}/../../../.." && pwd)
HLS_INCLUDE=${VITIS_HLS_ROOT:-/opt/Xilinx_2020.2/Vitis_HLS/2020.2}/include
TEST_DIR=$(mktemp -d)
trap 'rm -rf -- "${TEST_DIR}"' EXIT

"${CXX:-g++}" -O2 -std=c++14 -DUSING_XILINX_STREAM -Wno-unknown-pragmas \
    -I"${HLS_INCLUDE}" -I"${SUDA_ROOT}/device/shared_components/hls" \
    -I"${OPERATOR_DIR}" "${OPERATOR_DIR}/selective_filter.cpp" \
    "${OPERATOR_DIR}/test.cpp" "${OPERATOR_DIR}/../lwe_encrypt/lwe_encrypt.cpp" \
    -lgmp -o "${TEST_DIR}/test_manifest"
"${TEST_DIR}/test_manifest"

if [[ "${1:-}" == "--rtl" ]]; then
    RTL_DIR="${OPERATOR_DIR}/selective_filter/solution1/syn/verilog"
    for source_file in selective_filter.cpp selective_filter.hpp selective_filter_protocol.h; do
        if [[ ! -s "${RTL_DIR}/selective_filter.v" || "${RTL_DIR}/selective_filter.v" -ot "${OPERATOR_DIR}/${source_file}" ]]; then
            echo "Missing/stale RTL; run HLS csynth before RTL validation." >&2
            exit 1
        fi
    done
    verilator --cc --exe --build -j 4 -Wno-fatal --top-module selective_filter \
        --Mdir "${TEST_DIR}/rtl" -CFLAGS "-std=c++14 -I${OPERATOR_DIR}" \
        "${RTL_DIR}"/*.v "${OPERATOR_DIR}/test_rtl_sql.cpp" \
        > "${TEST_DIR}/verilator.log" 2>&1 || { cat "${TEST_DIR}/verilator.log"; exit 1; }
    "${TEST_DIR}/rtl/Vselective_filter"
    for scenario in --manifest --empty --all --error --truncated; do
        "${TEST_DIR}/rtl/Vselective_filter" "${scenario}"
    done
elif [[ $# -ne 0 ]]; then
    echo 'Usage: test_manifest.sh [--rtl]' >&2
    exit 1
fi
