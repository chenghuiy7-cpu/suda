#!/usr/bin/env bash

set -e

script_dir=$(readlink -f "$(dirname "$0")")
pci_device=0000:86:00.0

"$script_dir/../../device/platform/software_stack/nf_spdk/dpdk/usertools/dpdk-devbind.py" \
    -b vfio-pci "$pci_device"

driver=$(basename "$(readlink -f "/sys/bus/pci/devices/$pci_device/driver")")
iommu_group=$(basename "$(readlink -f "/sys/bus/pci/devices/$pci_device/iommu_group")")

echo "$pci_device driver: $driver"
echo "$pci_device IOMMU group: $iommu_group"
ls -l "/dev/vfio/$iommu_group"
