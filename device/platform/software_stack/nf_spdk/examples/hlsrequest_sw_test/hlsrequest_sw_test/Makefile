SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../..)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

APP = hlsrequest_sw_test

C_SRCS = hlsrequest_sw_test.c

SPDK_LIB_LIST = $(ALL_MODULES_LIST) hlsacccompute

include $(SPDK_ROOT_DIR)/mk/spdk.app.mk
