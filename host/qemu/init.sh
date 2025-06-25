#!/bin/bash
set -e

echo "编译内核"
bash -c "cd ../kernel/source && git apply ../0001-Add-QDMA-header-files.patch && git apply ../0002-Export-bio_map_user_iov.patch &&  cp ../config.txt .config && make -j"
echo "拷贝内核到当前目录"
bash -c "cp ../kernel/source/arch/x86_64/boot/bzImage ./bzImage"

# 创建一个5GB的原始虚拟磁盘
DISK_IMG="debian_qdma_dev.img"
DISK_SIZE="5G"
MOUNT_DIR="debian_rootfs"

# 1. 使用dd创建虚拟磁盘
echo "使用dd创建虚拟磁盘..."
dd if=/dev/zero of=$DISK_IMG bs=1M count=5120 

# 直接在整个磁盘上创建文件系统
echo "在磁盘上创建文件系统..."
mkfs.ext4 $DISK_IMG

# 2. 挂载文件系统并使用debootstrap安装基本系统
echo "挂载文件系统..."
mkdir -p $MOUNT_DIR
sudo mount -o loop $DISK_IMG $MOUNT_DIR

echo "使用debootstrap安装基本Debian系统..."
sudo bash -c "debootstrap --arch=amd64 bookworm $MOUNT_DIR http://deb.debian.org/debian/"

# 3. 配置系统
echo "配置系统..."

# 设置主机名
echo "debian-qdma-dev" | sudo tee $MOUNT_DIR/etc/hostname

# 设置fstab
cat << EOF | sudo tee $MOUNT_DIR/etc/fstab
/dev/sda / ext4 defaults 0 1
proc /proc proc defaults 0 0
sysfs /sys sysfs defaults 0 0
devpts /dev/pts devpts defaults 0 0
suda /mnt/suda 9p trans=virtio,version=9p2000.L,_netdev,rw 0 0
EOF

# 设置root密码和创建用户
sudo chroot $MOUNT_DIR /bin/bash -c "echo root:password | chpasswd"
sudo chroot $MOUNT_DIR /bin/bash -c "useradd -m -s /bin/bash developer && echo developer:password | chpasswd && apt-get update && apt-get install -y sudo && adduser developer sudo"
sudo chroot $MOUNT_DIR /bin/bash -c "echo "nameserver 114.114.114.114" | sudo tee /etc/resolv.conf"

# 安装编译Xilinx QDMA驱动、Linux内核、libnvme和liburing所需的依赖
sudo chroot $MOUNT_DIR /bin/bash -c "apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential \
    git \
    cmake \
    pkg-config \
    libpciaccess-dev \
    libudev-dev \
    libcap-dev \
    libkmod-dev \
    libnl-3-dev \
    libnl-genl-3-dev \
    flex \
    bison \
    bc \
    kmod \
    libelf-dev \
    libssl-dev \
    libncurses-dev \
    dwarves \
    zlib1g-dev \
    gcc-multilib \
    autoconf \
    automake \
    libtool \
    uuid-dev \
    libblkid-dev \
    libpci-dev \
    lsb-release \
    vim \
    wget \
    python3 \
    python3-pip \
    python3-dev \
    rsync \
    cpio \
    unzip \
    udev \
    initramfs-tools \
    libaio1 \
    libaio-dev \
    openssh-server \
    pciutils \
    man-db \
    curl \
    meson \
    ntpdate"


# 创建网络自动配置脚本
echo "创建网络自动配置脚本..."
cat << 'EOF' | sudo tee $MOUNT_DIR/etc/network/interfaces
# 网络接口配置文件
auto lo
iface lo inet loopback

# 主网卡自动配置
allow-hotplug eth0
iface eth0 inet dhcp
EOF

# 启用SSH服务
echo "配置SSH服务自动启动..."
sudo chroot $MOUNT_DIR /bin/bash -c "systemctl enable ssh"

# 4. 清理和卸载
echo "清理和卸载..."
sudo chroot $MOUNT_DIR /bin/bash -c "apt-get clean && apt-get autoremove -y"
sudo umount $MOUNT_DIR
rmdir $MOUNT_DIR

echo "编译QEMU"
cd qemu && mkdir build && cd build && ../configure --enable-kvm --enable-virtfs --enable-slirp --target-list=x86_64-softmmu && make && cd ../../

echo "编译完成"


echo "完成! 虚拟磁盘已创建: $DISK_IMG"
echo "使用以下命令启动虚拟机:"
echo "sudo bash run_qemu.sh -f"
echo "登录信息:"
echo "用户名: developer"
echo "密码: password"
