VIRT_ONE_DRIVE_LOC := shell/virt_one_drive

EXPORT_CC_PATH := export PATH=$(LINUX_GCC_PATH):$$PATH

include $(VIRT_ONE_DRIVE_LOC)/scripts/sw_target/atf.mk

include $(VIRT_ONE_DRIVE_LOC)/scripts/sw_target/tee.mk

include $(VIRT_ONE_DRIVE_LOC)/scripts/sw_target/uboot.mk

include $(VIRT_ONE_DRIVE_LOC)/scripts/sw_target/kernel.mk
