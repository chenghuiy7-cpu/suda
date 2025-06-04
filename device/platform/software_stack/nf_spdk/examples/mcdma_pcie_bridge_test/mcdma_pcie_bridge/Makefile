SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

APP = mcdma_pcie_bridge

C_SRCS = mcdma_pcie_bridge.c

SPDK_LIB_LIST = $(ALL_MODULES_LIST) axi_dma hlsacccompute

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk
