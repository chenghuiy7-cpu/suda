# modprobe -r qdma-pf
echo 0x10ee 0x903f > /sys/bus/pci/drivers/vfio-pci/new_id
echo 0x10ee 0x913f > /sys/bus/pci/drivers/vfio-pci/new_id
echo 0x10ee 0x923f > /sys/bus/pci/drivers/vfio-pci/new_id
echo 0x10ee 0x933f > /sys/bus/pci/drivers/vfio-pci/new_id

# For NVMe passthrough
# echo 0000:af:00.0 > /sys/bus/pci/devices/0000:af:00.0/driver/unbind
# echo 0x8086 0x0a54 > /sys/bus/pci/drivers/vfio-pci/new_id
