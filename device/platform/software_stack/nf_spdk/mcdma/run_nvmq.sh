#! /usr/bin/env bash

script_dir=$(readlink -f "$(dirname "$0")")
spdk_dir=$(readlink -f "$script_dir/..")
conf_file=conf_debug.json
runtime_lib_dir="$spdk_dir/build/arm-runtime-lib"
# Always start from the current four-slot layout.  A value inherited from an
# older shell must not silently keep the legacy Blowfish layout active.
operator_config="$spdk_dir/config.json"

if [ -d "$runtime_lib_dir" ]; then
    export LD_LIBRARY_PATH="$runtime_lib_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

cd "$script_dir"

while getopts :bdmntc: flag
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
    *)
        echo "invalid flag '$flag'"
    esac
done

if [ ! -r "$operator_config" ]; then
    echo "operator configuration is not readable: $operator_config" >&2
    exit 1
fi
export HLSACC_OPERATOR_CONFIG="$operator_config"
echo "HLSACC_OPERATOR_CONFIG=$HLSACC_OPERATOR_CONFIG"
sha256sum "$HLSACC_OPERATOR_CONFIG"
grep -E 'operator_type_id|operator_type_name|slot_id' "$HLSACC_OPERATOR_CONFIG"

$prefix "$spdk_dir/build/bin/nvmf_tgt" -c "$conf_file" -m 0x0f -e nvmf_mcdma -L nvme -L nvmq -L nvmf
# $prefix ../build/bin/nvmf_tgt -c $conf_file -m 0x01 -e axi_dma # nvmf_mcdma # ,axi_dma
# $prefix ../build/bin/nvmf_tgt -c $conf_file -m 0x01
