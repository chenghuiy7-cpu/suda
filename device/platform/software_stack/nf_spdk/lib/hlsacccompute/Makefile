SPDK_ROOT_DIR := $(abspath $(CURDIR)/../..)


SO_VER := 9
SO_MINOR := 0

C_SRCS = hlsacccompute.c
LIBNAME = hlsacccompute
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
SPDK_MAP_FILE = $(abspath $(CURDIR)/spdk_hlsacccompute.map)

include $(SPDK_ROOT_DIR)/mk/spdk.lib.mk

