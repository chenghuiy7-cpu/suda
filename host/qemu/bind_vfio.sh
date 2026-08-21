#!/usr/bin/env bash

set -euo pipefail

script_dir=$(readlink -f "$(dirname "$0")")
pci_device=${1:-${NEST_CSD_PCI_BDF:-}}

if [ -z "$pci_device" ]; then
    echo "Specify the Fidus PCI BDF as argument 1 or NEST_CSD_PCI_BDF." >&2
    echo "Example: sudo env NEST_CSD_PCI_BDF=0000:d9:00.0 bash bind_vfio.sh" >&2
    exit 2
fi

device_path="/sys/bus/pci/devices/$pci_device"
if [ ! -d "$device_path" ]; then
    echo "PCI device does not exist: $pci_device" >&2
    exit 1
fi

vendor=$(cat "$device_path/vendor")
class=$(cat "$device_path/class")
if [ "$vendor" != "0x10ee" ] || [[ "$class" != 0x12* ]]; then
    echo "Refusing to bind $pci_device: vendor=$vendor class=$class is not a Xilinx accelerator." >&2
    exit 1
fi

"$script_dir/../../device/platform/software_stack/nf_spdk/dpdk/usertools/dpdk-devbind.py" \
    -b vfio-pci "$pci_device"

driver=$(basename "$(readlink -f "$device_path/driver")")
iommu_group=$(basename "$(readlink -f "$device_path/iommu_group")")

echo "$pci_device driver: $driver"
echo "$pci_device IOMMU group: $iommu_group"
ls -l "/dev/vfio/$iommu_group"
