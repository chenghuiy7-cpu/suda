# Specification of Board, Chipset and Project
FPGA_BD ?= nf
PRJ ?= shell:mpsoc

# parsing target design
TARGET_LOC := $(shell echo $(PRJ) | awk -F ":" '{print $$1}')
TARGET_PRJ := $(shell echo $(PRJ) | awk -F ":" '{print $$2}')
TARGET_DESIGN := $(shell echo $(PRJ) | awk -F ":" '{print $$3}')

ifneq ($(TARGET_DESIGN),)
FPGA_PRJ_NAME := $(TARGET_LOC)_$(TARGET_PRJ)_$(TARGET_DESIGN)_$(FPGA_BD)
else
FPGA_PRJ_NAME := $(TARGET_LOC)_$(TARGET_PRJ)_$(FPGA_BD)
endif

# TODO: Potential list of boards using Zynq
ARMv7_BOARDS := pynq serve_d

ifneq ($(findstring $(FPGA_BD),$(ARMv7_BOARDS)),)
FPGA_ARCH := zynq
FPGA_PROC := ps7_cortexa9_0
else
FPGA_ARCH := zynqmp
FPGA_PROC := psu_cortexa53_0
endif

ifneq ($(wildcard $(abspath $(TARGET_LOC)/$(TARGET_PRJ)/$(TARGET_PRJ).mk)),)
include $(abspath $(TARGET_LOC)/$(TARGET_PRJ)/$(TARGET_PRJ).mk)
endif
