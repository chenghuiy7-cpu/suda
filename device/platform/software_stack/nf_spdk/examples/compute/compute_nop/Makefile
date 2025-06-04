SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

APP = compute_nop

C_SRCS := compute_nop.c

SPDK_LIB_LIST = $(ALL_MODULES_LIST) compute

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk