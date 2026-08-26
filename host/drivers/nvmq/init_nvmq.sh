#!/usr/bin/env bash

set -e

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
pci_device=${NEST_QDMA_GUEST_BDF:-0000:01:00.0}
qdma_module=${NEST_QDMA_MODULE:-"$script_dir/../qdma/driver/src/qdma-pf.ko"}
qmax_file="/sys/bus/pci/devices/$pci_device/qdma/qmax"
fabrics_dev=/dev/nvmq-fabrics

# A failed/manual redirect can leave a regular file here and hide the misc node.
if [ -e "$fabrics_dev" ] && [ ! -c "$fabrics_dev" ]; then
	echo "Removing stale non-device $fabrics_dev" >&2
	rm -f -- "$fabrics_dev"
fi

insmod "$qdma_module"

if [ ! -e "$qmax_file" ]; then
    echo "QDMA probe failed: $qmax_file was not created." >&2
    echo "Run ./diagnose_qdma.sh and inspect dmesg before loading nvmq." >&2
    exit 1
fi

echo 128 > "$qmax_file"
insmod nvmq.ko # dyndbg==pf

for _ in {1..20}; do
	[ -c "$fabrics_dev" ] && break
	sleep 0.1
done

if [ ! -c "$fabrics_dev" ]; then
	echo "NVMQ probe failed: $fabrics_dev is not a character device." >&2
	echo "Inspect dmesg and /sys/class/misc/nvmq-fabrics/dev." >&2
	exit 1
fi
