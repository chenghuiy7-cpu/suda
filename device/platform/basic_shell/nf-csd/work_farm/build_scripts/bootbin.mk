BOOTBIN_DEP := fsbl
# ZynqMP BOOT.bin dependency
ifeq ($(FPGA_ARCH),zynqmp)
BOOTBIN_DEP += pmufw atf
endif
BOOTBIN_DEP += uboot

obj-bootbin-clean-y := $(foreach obj,$(BOOTBIN_DEP),$(obj)_clean)
obj-bootbin-dist-y := $(foreach obj,$(BOOTBIN_DEP),$(obj)_distclean)

ifneq ($(TARGET_DESIGN),)
CUSTOM_INIT := $(wildcard $(abspath ./$(TARGET_LOC)/$(TARGET_PRJ)/$(TARGET_DESIGN)/init/$(FPGA_BD).init))
else
CUSTOM_INIT := $(wildcard $(abspath ./$(TARGET_LOC)/$(TARGET_PRJ)/init/$(FPGA_BD).init))
endif

# BOOT.bin generation flags
bootbin-flag := BOOT_GEN=$(BOOT_GEN_BIN) \
	    WITH_BIT=$(WITH_BIT) BIT_LOC=$(FPGA_PRJ_NAME) \
	    FPGA_ARCH=$(FPGA_ARCH) \
	    WITH_TOS=$(WITH_TOS) \
	    CUSTOM_INIT=$(CUSTOM_INIT) O=$(INSTALL_LOC)

# BOOT.bin generation
bootbin: $(BOOTBIN_DEP) FORCE
	@echo "Generating BOOT.bin image..."
	@mkdir -p $(INSTALL_LOC)/$(ARCH)
	$(MAKE) -C ./bootstrap $(bootbin-flag) boot_bin

bootbin_clean: $(obj-bootbin-clean-y)
	@rm -f $(INSTALL_LOC)/BOOT.bin

bootbin_distclean: $(obj-bootbin-dist-y)
	$(MAKE) -C ./bootstrap boot_bin_distclean
	@rm -rf $(INSTALL_LOC)/BOOT.bin

