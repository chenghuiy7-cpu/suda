#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
OPERATOR_CONTROLLER_SRC="${SCRIPT_DIR}/../../ips/rtl/OperatorController.v"
OPERATOR_CONTROLLER_DST="${SCRIPT_DIR}/shell/virt_one_drive/fpga/sources/hlsaccframework/OperatorController/OperatorController.v"
HLSACC_SOURCE_ROOT="${SCRIPT_DIR}/work_farm/shell/virt_one_drive/fpga/sources/hlsaccframework"
ACCFRAMEWORK_DCP="${SCRIPT_DIR}/work_farm/fpga/vivado_out/shell_virt_one_drive_accframework_fidus/dcp/accframework.dcp"
SHELL_SYNTH_DCP="${SCRIPT_DIR}/work_farm/fpga/vivado_out/shell_virt_one_drive_fidus/dcp/synth.dcp"
POST_ROUTE_TIMING="${SCRIPT_DIR}/work_farm/fpga/vivado_out/shell_virt_one_drive_fidus/impl_rpt/post_route_timing.rpt"
BOOT_OUTPUT="${SCRIPT_DIR}/shell/virt_one_drive/ready_for_download/fidus/BOOT.bin"
BUILD_STAMP=$(mktemp)
trap 'rm -f "${BUILD_STAMP}"' EXIT

mapfile -t NESTED_RTL_BACKUPS < <(
    find "${HLSACC_SOURCE_ROOT}" -mindepth 1 -maxdepth 1 -type d \
        \( -iname '*.backup.*' -o -iname '*.bak.*' -o -iname 'backup*' -o -iname 'bak' \) \
        -print
)

if ((${#NESTED_RTL_BACKUPS[@]} != 0)); then
    echo "[build_bd] 错误：综合源码目录中存在 RTL 备份，可能造成模块重复定义：" >&2
    printf '  %s\n' "${NESTED_RTL_BACKUPS[@]}" >&2
    echo "[build_bd] 请先将这些目录移出 ${HLSACC_SOURCE_ROOT}" >&2
    exit 1
fi

if [[ ! -f "${OPERATOR_CONTROLLER_SRC}" ]]; then
    echo "[build_bd] 错误：找不到公共 OperatorController RTL：${OPERATOR_CONTROLLER_SRC}" >&2
    exit 1
fi

if ! cmp -s "${OPERATOR_CONTROLLER_SRC}" "${OPERATOR_CONTROLLER_DST}"; then
    echo "[build_bd] 同步公共 OperatorController RTL 到 shell 构建目录"
    install -D -m 0644 "${OPERATOR_CONTROLLER_SRC}" "${OPERATOR_CONTROLLER_DST}"
fi

echo "[build_bd] OperatorController RTL 已同步：$(sha256sum "${OPERATOR_CONTROLLER_DST}" | awk '{print $1}')"

TARGET_BOARD=fidus
SHELL_TYPE="nvme"

AR_BRIDGE_EN=0
R_BRIDGE_EN=0

export AR_BRIDGE_EN
export R_BRIDGE_EN


make -C work_farm PRJ=shell:virt_one_drive:accframework FPGA_BD=$TARGET_BOARD FPGA_ACT=dcp_gen vivado_prj
make -C work_farm PRJ=shell:virt_one_drive:pcie_ep FPGA_BD=$TARGET_BOARD FPGA_ACT=dcp_gen vivado_prj && \
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD FPGA_ACT=prj_gen vivado_prj
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD FPGA_ACT=run_syn vivado_prj && \
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD FPGA_ACT=bit_gen vivado_prj && \
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD fsbl
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD pmufw     
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD dt 
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD atf   
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD uboot    
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD WITH_BIT=y IO_CACHE_COHERENCE=y bootbin # boot.bin

for artifact in "${ACCFRAMEWORK_DCP}" "${SHELL_SYNTH_DCP}" "${POST_ROUTE_TIMING}" "${BOOT_OUTPUT}"; do
    if [[ ! -s "${artifact}" || ! "${artifact}" -nt "${BUILD_STAMP}" ]]; then
        echo "[build_bd] 错误：本次构建没有刷新有效制品：${artifact}" >&2
        exit 1
    fi
done

if ! grep -qF 'Timing constraints are met.' "${POST_ROUTE_TIMING}"; then
    echo "[build_bd] 错误：本次实现没有通过布线后时序约束：${POST_ROUTE_TIMING}" >&2
    grep -m 1 -A 7 'Design Timing Summary' "${POST_ROUTE_TIMING}" >&2 || true
    exit 1
fi

ACCFRAMEWORK_DCP_BYTES=$(stat -c '%s' "${ACCFRAMEWORK_DCP}")
if ((ACCFRAMEWORK_DCP_BYTES < 1048576)); then
    echo "[build_bd] 错误：accframework.dcp 仅 ${ACCFRAMEWORK_DCP_BYTES} B，疑似 black box DCP" >&2
    exit 1
fi

echo "[build_bd] 完整构建成功，关键制品均由本次任务刷新"
echo "[build_bd] accframework.dcp：${ACCFRAMEWORK_DCP_BYTES} B"
echo "[build_bd] BOOT.bin SHA-256：$(sha256sum "${BOOT_OUTPUT}" | awk '{print $1}')"
