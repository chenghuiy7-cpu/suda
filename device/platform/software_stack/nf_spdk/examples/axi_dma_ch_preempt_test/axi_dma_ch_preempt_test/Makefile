SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

APP = axi_dma_ch_preempt_test

C_SRCS = axi_dma_ch_preempt_test.c

SPDK_LIB_LIST = $(ALL_MODULES_LIST) hlsacccompute

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk
