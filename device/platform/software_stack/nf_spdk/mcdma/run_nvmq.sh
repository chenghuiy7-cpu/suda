#! /usr/bin/env bash

script_dir=$(readlink -f "$(dirname "$0")")
spdk_dir=$(readlink -f "$script_dir/..")
conf_file=conf_debug.json
runtime_lib_dir="$spdk_dir/build/arm-runtime-lib"
# Always start from the current four-slot layout.  A value inherited from an
# older shell must not silently keep the legacy Blowfish layout active.
operator_config="$spdk_dir/config.json"
prefix=
log_args=()
runtime_trace=0

if [ -d "$runtime_lib_dir" ]; then
    export LD_LIBRARY_PATH="$runtime_lib_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

cd "$script_dir"

while getopts :bdmtc: flag
do
    echo "$flag"
    case "$flag" in
    "b")
        operator_config="$spdk_dir/config_legacy_blowfish.json"
        ;;
    "c")
        operator_config=$(readlink -f "$OPTARG")
        ;;
    "d")
        prefix="gdb --args"
        ;;
    "m")
	prefix="taskset 0x01"
	;;
    "t")
        runtime_trace=1
        log_args=(-L nvme -L nvmq -L nvmf)
        ;;
    *)
        echo "invalid flag '$flag'"
    esac
done

if [ ! -r "$operator_config" ]; then
    echo "operator configuration is not readable: $operator_config" >&2
    exit 1
fi
export HLSACC_OPERATOR_CONFIG="$operator_config"
export HLSACC_RUNTIME_TRACE="$runtime_trace"
echo "HLSACC_OPERATOR_CONFIG=$HLSACC_OPERATOR_CONFIG"
sha256sum "$HLSACC_OPERATOR_CONFIG"
grep -E 'operator_type_id|operator_type_name|slot_id' "$HLSACC_OPERATOR_CONFIG"

if ((runtime_trace == 0)); then
    echo "HLSACC_RUNTIME_TRACE=disabled (use -t to enable nvme/nvmq/nvmf debug logs)"
else
    echo "HLSACC_RUNTIME_TRACE=enabled"
fi

$prefix "$spdk_dir/build/bin/nvmf_tgt" -c "$conf_file" -m 0x0f -e nvmf_mcdma "${log_args[@]}"
# $prefix ../build/bin/nvmf_tgt -c $conf_file -m 0x01 -e axi_dma # nvmf_mcdma # ,axi_dma
# $prefix ../build/bin/nvmf_tgt -c $conf_file -m 0x01
