insmod /home/qemu/qdma/driver/src/qdma-pf.ko
echo 128 > /sys/bus/pci/devices/0000\:01\:00.0/qdma/qmax
insmod nvmq.ko #dyndbg==pf
