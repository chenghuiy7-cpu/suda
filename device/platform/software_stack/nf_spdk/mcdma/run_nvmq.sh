#! /usr/bin/env bash

conf_file=conf_debug.json

while getopts :dnt flag
do
    echo "$flag"
    case "$flag" in
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

$prefix ../build/bin/nvmf_tgt -c $conf_file -m 0x0f -e nvmf_mcdma -L nvme -L nvmq -L nvmf
# $prefix ../build/bin/nvmf_tgt -c $conf_file -m 0x01 -e axi_dma # nvmf_mcdma # ,axi_dma
# $prefix ../build/bin/nvmf_tgt -c $conf_file -m 0x01
