#!/bin/bash

# image directory
IMGDIR=/home/lgf/images
# Virtual machine disk image
KERNELIMGF=/home/lgf/nf_csd/linux-x86/arch/x86/boot/bzImage
# KERNELIMGF=/boot/vmlinuz-`uname -r`
OSIMGF=$IMGDIR/qemu_fs_2.img
# SEEDIMGF=$IMGDIR/seed.img
# NVMEIMGF=/home/jinhao/tmp/nvme.img
# NVMEDISKF=/dev/nvme1n1
# VHOSTSOCKF=vhost_3c_1
SSHPORT=8084

SHOULD_KILL=false

if [[ ! -e "$OSIMGF" ]]; then
	echo ""
	echo "VM disk image couldn't be found ..."
	echo "Please prepare a usable VM image and place it as $OSIMGF"
	echo "Once VM disk image is ready, please rerun this script again"
	echo ""
	exit
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

    ps aux  |  grep -i $1 |  grep -v grep   | awk '{print $2}' | xargs sudo kill -$klevel
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
    ka "qdma-test-2" 9
    sleep 1
fi

echo "" > /sys/kernel/debug/tracing/trace
echo $$ >> /sys/kernel/debug/tracing/set_event_pid

/home/lgf/nf_csd/qemu/build/x86_64-softmmu/qemu-system-x86_64 \
    -name "qdma-test-2",debug-threads=on \
    -machine accel=kvm \
    -cpu host \
    -smp 4 \
    -m 16G \
    -device virtio-scsi-pci,id=scsi0 \
    -kernel $KERNELIMGF \
    -hda $OSIMGF \
    -daemonize \
    -append "root=/dev/sda rw console=ttyS0 nokaslr cgroup_no_v1=all" \
    -device pcie-root-port,id=pcie.1,addr=08.0,slot=1 \
    -device vfio-pci,host=3b:00.2,bus=pcie.1 \
    -net user,hostfwd=tcp::$SSHPORT-:22 \
    -net nic,model=virtio \
    -fsdev local,id=fs1,path=/home/lgf/nf_csd/linux-x86,security_model=none \
    -device virtio-9p-pci,fsdev=fs1,mount_tag=linux \
    -fsdev local,id=fs2,path=/home/lgf/nf_csd/nvmq,security_model=none \
    -device virtio-9p-pci,fsdev=fs2,mount_tag=nvme-qdma \
    -fsdev local,id=fs3,path=/home/lgf/nf_csd/qdma,security_model=none \
    -device virtio-9p-pci,fsdev=fs3,mount_tag=qdma \
    -monitor unix:./qmp-sock,server,nowait # 2>&1 | tee log
    # -device vfio-pci,host=3b:00.0,bus=pcie.1 \
    # -numa node,memdev=mem \
    # -object memory-backend-file,id=mem,size=16G,mem-path=/dev/hugepages,share=on \
    # -chardev socket,id=char1,path=/home/jinhao/spdk/vhost/$VHOSTSOCKF \
    # -device vhost-user-blk-pci,id=blk0,chardev=char1 \
    # -fsdev local,id=fs1,path=/home/jinhao/dwin-5.4,security_model=none \
    # -hdb $SEEDIMGF \
    # -nographic \
    # -object iothread,id=iothread0,poll-max-ns=0 \
    # -drive file=$NVMEDISKF,if=none,id=nvm0 \
    # -device nvme,serial=deadbeef,drive=nvm0,ioeventfd=on,irq-eventfd=on \
    # -gdb tcp::6789 \
    # -S \
    # -drive "id=nvm1,if=none,file=null-co://,file.read-zeroes=on,format=raw" \
    # -drive "id=nvm2,if=none,file=null-co://,file.read-zeroes=on,format=raw" \
    # -drive "id=nvm3,if=none,file=null-co://,file.read-zeroes=on,format=raw" \
    # -object iothread,id=iothread1,poll-max-ns=0 \
    # -object iothread,id=iothread2,poll-max-ns=0 \
    # -object iothread,id=iothread3,poll-max-ns=0 \
    # -device nvme,serial=deadbeee,drive=nvm1,ioeventfd=on,irq-eventfd=on \
    # -device nvme,serial=deadbeed,drive=nvm2,ioeventfd=on,irq-eventfd=on \
    # -device nvme,serial=deadbeec,drive=nvm3,ioeventfd=on,irq-eventfd=on \
    # -trace events=/home/jinhao/qemu/events_iothread \
    # -D log_iothread.txt \
    # -drive "id=nvm0,if=none,file=null-co://,file.read-zeroes=on,format=raw" \
    # -device nvme,serial=deadbeef,drive=nvm,ioeventfd=on,iothread=iothread0 \
    #,iothread=iothread0 \
    # -append "root=/dev/sda single console=ttyS0 systemd.unit=graphical.target" \
    # -device femu,devsz_mb=65536,femu_mode=1 \
    # -serial none \
