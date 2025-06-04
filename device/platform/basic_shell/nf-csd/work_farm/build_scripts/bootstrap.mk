PRJ_DT_LOC_TMP := $(abspath ./$(TARGET_LOC)/$(TARGET_PRJ)/dt)

ifeq ($(wildcard $(PRJ_DT_LOC_TMP)/*.dtsi),)
PRJ_DT_LOC := $(PRJ_DT_LOC_TMP)/$(FPGA_BD) 
else
PRJ_DT_LOC := $(PRJ_DT_LOC_TMP)
endif

# TODO: Change to your own compilation flags
fsbl-flag := COMPILER_PATH=$(ELF_GCC_PATH) \
	    XSCT=$(XSCT_BIN) HDF_FILE=$(SYS_HDF) \
	    FPGA_ARCH=$(FPGA_ARCH) FPGA_PROC=$(FPGA_PROC) FPGA_BD=$(FPGA_BD)

pmufw-flag := COMPILER_PATH=$(MB_GCC_PATH) \
	    XSCT=$(XSCT_BIN) HDF_FILE=$(SYS_HDF)

dt-flag := DTC_LOC=$(DTC_LOC) \
	XSCT=$(XSCT_BIN) HDF_FILE=$(SYS_HDF) \
	FPGA_BD=$(FPGA_BD) O=$(INSTALL_LOC) \
	PRJ_DT_LOC=$(PRJ_DT_LOC)

# common bootstrap target
obj-bootstrap-y := fsbl pmufw dt
obj-bootstrap-clean-y := $(foreach obj,$(obj-bootstrap-y),$(obj)_clean)
obj-bootstrap-dist-y := $(foreach obj,$(obj-bootstrap-y),$(obj)_distclean)

# common bootstrap compilation
$(obj-bootstrap-y): FORCE
	@echo "Compiling $@..."
	$(MAKE) -C ./bootstrap \
		$($(patsubst %,%-flag,$@)) \
		$($(patsubst %,%-prj-flag,$@)) $@

$(obj-bootstrap-clean-y):
	$(MAKE) -C ./bootstrap \
		$($(patsubst %_clean,%-flag,$@)) $@

$(obj-bootstrap-dist-y):
	$(MAKE) -C ./bootstrap \
		$($(patsubst %_distclean,%-flag,$@)) $@
