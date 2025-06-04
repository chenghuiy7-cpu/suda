SPDK_ROOT_DIR := $(abspath $(CURDIR)/../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

SO_VER := 9
SO_MINOR := 0

C_SRCS = compute.c
LIBNAME = compute

SPDK_MAP_FILE = $(abspath $(CURDIR)/spdk_compute.map)

include $(SPDK_ROOT_DIR)/mk/spdk.lib.mk