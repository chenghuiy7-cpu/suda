#!/usr/bin/env bash

set -euo pipefail

# image directory
SCRIPT_DIR=$(readlink -f "$(dirname "$0")")
IMGDIR=${NEST_QEMU_IMAGE_DIR:-$SCRIPT_DIR}
# Virtual machine disk image
KERNELIMGF=${NEST_QEMU_KERNEL:-$SCRIPT_DIR/bzImage}
# KERNELIMGF=/boot/vmlinuz-`uname -r`
OSIMGF=${NEST_QEMU_IMAGE:-$IMGDIR/debian_qdma_dev.img}
QEMU_BIN=${NEST_QEMU_BIN:-$SCRIPT_DIR/qemu/build/qemu-system-x86_64}
SUDA_ROOT=${NEST_SUDA_ROOT:-$(readlink -f "$SCRIPT_DIR/../..")}
# SEEDIMGF=$IMGDIR/seed.img
# NVMEIMGF=/home/jinhao/tmp/nvme.img
# NVMEDISKF=/dev/nvme1n1
# VHOSTSOCKF=vhost_3c_1
SSHPORT=${NEST_QEMU_SSH_PORT:-8999}
PCI_DEVICE=${NEST_CSD_PCI_BDF:-}

SHOULD_KILL=false

if [[ ! -e "$OSIMGF" ]]; then
	echo ""
	echo "VM disk image couldn't be found ..."
	echo "Please prepare a usable VM image and place it as $OSIMGF"
	echo "Once VM disk image is ready, please rerun this script again"
	echo ""
	exit
fi

if [ ! -x "$QEMU_BIN" ]; then
    echo "QEMU binary is unavailable: $QEMU_BIN" >&2
    exit 1
fi

if [ -z "$PCI_DEVICE" ]; then
    echo "Set NEST_CSD_PCI_BDF to the Fidus PCI BDF before starting QEMU." >&2
    echo "Example: sudo env NEST_CSD_PCI_BDF=0000:d9:00.0 bash run_qemu.sh" >&2
    exit 2
fi

# FIND PROCESS
function p(){
        ps aux | grep -i $1 | grep -v grep
}

# KILL ALL
function ka(){

    cnt=$( p $1 | wc -l)  # total count of processes found
    klevel=${2:-15}       # kill level, defaults to 15 if argument 2 is empty

    echo -e "\nSearching for '$1' -- Found" $cnt "Running Processes .. "
    p $1

    echo -e '\nTerminating' $cnt 'processes .. '

    ps aux | grep -i $1 | grep -v grep | awk '{print $2}' | xargs -r sudo kill -$klevel
    echo -e "Done!\n"

    echo "Running search again:"
    p "$1"
    echo -e "\n"
}

while getopts :ft flag
do
    echo $flag
    case "$flag" in
    "f")
        SHOULD_KILL=true
        ;;

    "t")
        OSIMGF=$IMGDIR/dwin_focal_2.img
        VHOSTSOCKF=vhost_3c_2
        SSHPORT=8081
        ;;
    esac
done

if [ "$SHOULD_KILL" = true ] ; then
    ka "qdma-test-0" 9
    sleep 1
fi

device_path="/sys/bus/pci/devices/$PCI_DEVICE"
if [ ! -d "$device_path" ]; then
    echo "PCI device does not exist: $PCI_DEVICE" >&2
    exit 1
fi

vendor=$(cat "$device_path/vendor")
class=$(cat "$device_path/class")
if [ "$vendor" != "0x10ee" ] || [[ "$class" != 0x12* ]]; then
    echo "Refusing passthrough for $PCI_DEVICE: vendor=$vendor class=$class is not a Xilinx accelerator." >&2
    exit 1
fi

driver=$(basename "$(readlink -f "$device_path/driver" 2>/dev/null)" 2>/dev/null)
iommu_group=$(basename "$(readlink -f "$device_path/iommu_group" 2>/dev/null)" 2>/dev/null)

if [ "$driver" != "vfio-pci" ]; then
    echo "$PCI_DEVICE is not bound to vfio-pci."
    echo "Run: sudo bash bind_vfio.sh"
    exit 1
fi

if [ -z "$iommu_group" ] || [ ! -e "/dev/vfio/$iommu_group" ]; then
    echo "VFIO device node /dev/vfio/${iommu_group:-<group>} is unavailable."
    echo "Check the IOMMU configuration and vfio-pci binding."
    exit 1
fi

echo "" > /sys/kernel/debug/tracing/trace
echo $$ >> /sys/kernel/debug/tracing/set_event_pid

"$QEMU_BIN" \
    -name "qdma-test-0",debug-threads=on \
    -machine accel=kvm \
    -cpu host \
    -smp 4 \
    -m 16G \
    -device virtio-scsi-pci,id=scsi0 \
    -kernel $KERNELIMGF \
    -drive file=$OSIMGF,format=raw \
    -append "root=/dev/sda rw console=ttyS0 nokaslr net.ifnames=0 biosdevname=0 cgroup_no_v1=all" \
    -netdev user,id=net0,hostfwd=tcp::$SSHPORT-:22 \
    -device virtio-net-pci,netdev=net0 \
    -device pcie-root-port,id=pcie.1,addr=08.0,slot=1 \
    -device vfio-pci,host=${PCI_DEVICE#0000:},bus=pcie.1 \
    -fsdev local,id=fs1,path="$SUDA_ROOT",security_model=none \
    -device virtio-9p-pci,fsdev=fs1,mount_tag=suda \
    -monitor unix:./qmp-sock,server,nowait \
    -serial stdio 
