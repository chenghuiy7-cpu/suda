DT_GEN := $(BOOT_STRAP_LOC)/dt-gen
DT_LOC := $(BOOT_STRAP_LOC)/dt

DT_GEN_TCL := $(BOOT_STRAP_LOC)/scripts/dt.tcl

DT_XSCT_FLAGS := $(DT_GEN_TCL) \
	$(HDF_FILE) $(DT_LOC) $(DT_GEN)

# Design-specific dtsi files
PL_DT := $(PRJ_DT_LOC)/pl.dtsi
PS_DT := $(PRJ_DT_LOC)/design.dtsi
SYS_DT := $(PRJ_DT_LOC)/design_top.dtsi

# DTB overlay
ifeq ($(ARCH),)
OVERLAY_DTSI := $(wildcard $(abspath $(PRJ_DT_LOC))/overlay/*.dtsi)
OVERLAY_DTBO := $(patsubst %.dtsi,%.dtbo,$(OVERLAY_DTSI))
endif

# DT building target
dts := system-top.dts
dtb := zynqmp.dtb

DTS := $(DT_LOC)/$(dts)
DTB := $(DT_LOC)/$(dtb)

KERN_DTB := $(abspath ../$(O)/$(dtb))

#==========================================
# Device Tree Source and Blob compilation 
#==========================================
dt: $(DTB) $(OVERLAY_DTBO)

$(DTB): .dt_gen FORCE
ifneq ($(wildcard $(BOOT_STRAP_LOC)/dt-board/$(FPGA_BD).dtsi),)
	@cp $(BOOT_STRAP_LOC)/dt-board/$(FPGA_BD).dtsi $(DT_LOC)/board.dtsi
endif
ifneq ($(wildcard $(BOOT_STRAP_LOC)/dt-board/$(FPGA_BD)_top.dtsi),)
	@cp $(BOOT_STRAP_LOC)/dt-board/$(FPGA_BD)_top.dtsi $(DT_LOC)/board_top.dtsi
endif
ifneq ($(wildcard $(PS_DT)),)
	@cp $(PS_DT) $(DT_LOC)/design.dtsi
endif
ifneq ($(wildcard $(SYS_DT)),)
	@cp $(SYS_DT) $(DT_LOC)/design_top.dtsi
endif
ifneq ($(wildcard $(PL_DT)),)
	@cp $(PL_DT) $(DT_LOC)/pl.dtsi
endif
	@mkdir -p $(abspath ../$(O))
	$(EXPORT_DTC_PATH) && \
		cpp -I $(DT_LOC) -E -P -x assembler-with-cpp $(DTS) | \
		dtc -I dts -O dtb -o $@ -
	cp $@ $(KERN_DTB)

%.dtbo: FORCE
	@mkdir -p $(abspath ../$(O)/dtbo)
	$(EXPORT_DTC_PATH) && \
		dtc -O dtb -o $@ -b 0 -@ $(patsubst %.dtbo,%.dtsi,$@)
	mv $@ $(abspath ../$(O)/dtbo/)

.dt_gen:
	$(XSCT) $(DT_XSCT_FLAGS)
	@sed -i '13i #include \"board.dtsi\"' $(DTS)
ifneq ($(wildcard $(PS_DT)),)
	@sed -i '14i #include \"design.dtsi\"' $(DTS)
endif
ifneq ($(wildcard $(BOOT_STRAP_LOC)/dt-board/$(FPGA_BD)_top.dtsi),)
	@sed -i '$$i #include \"board_top.dtsi\"' $(DTS)
endif
ifneq ($(wildcard $(SYS_DT)),)
	@sed -i '$$i #include \"design_top.dtsi\"' $(DTS)
endif
	@touch $@

dt_clean:
	@rm -f $(DTB) $(abspath ../$(O)/dtbo) $(KERN_DTB)

dt_distclean:
	@rm -rf .dt_gen .dt_cp $(KERN_DTB) $(DTB) $(O)/dtbo
	@rm -rf $(DT_LOC)
