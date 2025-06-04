SPDK_ROOT_DIR := $(abspath $(CURDIR)/../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk

SO_VER := 9
SO_MINOR := 0

C_SRCS = axi_dma.c
LIBNAME = axi_dma

SPDK_MAP_FILE = $(abspath $(CURDIR)/spdk_axi_dma.map)

include $(SPDK_ROOT_DIR)/mk/spdk.lib.mk