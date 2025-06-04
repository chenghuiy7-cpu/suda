#!/usr/bin/env bash

# In my test I do not use ARM's IOMMU
echo Y > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode

for i in $(seq 0 0)
do
	dev_addr=b00${i}0000.dma 
	echo "$dev_addr" > /sys/bus/platform/drivers/xilinx-vdma/unbind
	# You add to enable vfio-platform (CONFIG_VFIO_PLATFORM) for your ARM kernel build
	echo vfio-platform > /sys/bus/platform/devices/"$dev_addr"/driver_override
	echo "$dev_addr" > /sys/bus/platform/drivers_probe
	
	echo "$dev_addr -> vfio-platform"
done
