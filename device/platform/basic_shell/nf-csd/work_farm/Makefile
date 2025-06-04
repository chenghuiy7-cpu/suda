# Xilinx Vivado toolset and open-source device tree compiler
include build_scripts/toolset.mk

# Specified FPGA board, chipset and project
include build_scripts/fpga_config.mk

# Target ISA for compilation of bootstrap, firmware and system software
# Default value is empty, which means ISA is specified by FPGA chip
ARCH ?= 

# target compiler setup
include build_scripts/compiler.mk

# Temporal directory to hold hardware design output files 
# (i.e., bitstream, hardware definition file (HDF))
HW_PLATFORM := $(shell pwd)/hw_plat/$(FPGA_PRJ_NAME)
BITSTREAM := $(HW_PLATFORM)/system.bit
SYS_HDF := $(HW_PLATFORM)/system.hdf

# Temporal directory to save all image files for porting
ifneq ($(TARGET_DESIGN),)
INSTALL_LOC := $(TARGET_LOC)/$(TARGET_PRJ)/ready_for_download/$(TARGET_DESIGN)_$(FPGA_BD)
else
INSTALL_LOC := $(TARGET_LOC)/$(TARGET_PRJ)/ready_for_download/$(FPGA_BD)
endif

# Optional Trusted OS
TOS ?= 

# BOOT.bin generation flags
ifneq ($(TOS),)
WITH_TOS := y
endif
WITH_TOS ?= n

WITH_BIT ?= n

# Enable I/O cache coherence in ARMv8
IO_CACHE_COHERENCE ?= n

.PHONY: FORCE

# bootstrap compilation
ifneq ($(TARGET_BS_MK),)
include $(TARGET_LOC)/$(TARGET_PRJ)/$(TARGET_BS_MK)
endif
include build_scripts/bootstrap.mk

# software compilation
ifneq ($(TARGET_SW_MK),)
include $(TARGET_LOC)/$(TARGET_PRJ)/$(TARGET_SW_MK)
endif

# fpga design flow
ifneq ($(TARGET_FPGA_MK),)
include $(TARGET_LOC)/$(TARGET_PRJ)/$(TARGET_FPGA_MK)
endif
include build_scripts/fpga.mk

# bootbin generation for various architectures
include build_scripts/bootbin.mk

# FPGA project-specific hardware design compilation/generation
ifneq ($(TARGET_HW_MK),)
include $(TARGET_LOC)/$(TARGET_PRJ)/$(TARGET_HW_MK)
endif

