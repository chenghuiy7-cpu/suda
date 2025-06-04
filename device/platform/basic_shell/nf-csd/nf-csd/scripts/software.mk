.PHONY: qdma_drv

NF_CSD_SW_LOC := target/nf-csd/software

QDMA_SRC := $(NF_CSD_SW_LOC)/qdma/QDMA/linux-kernel

# source file locations of QDMA driver and apps
QDMA_DRV_SRC := $(QDMA_SRC)/driver
QDMA_APP_SRC := $(QDMA_SRC)/apps

# generated Linux symbol version file
modulesymfile := $(abspath $(NF_CSD_SW_LOC)/build/qdma/drv/Module.symvers)

qdma_drv: 
	@echo "Compiling QDMA Linux driver..."
	@mkdir -p $(NF_CSD_SW_LOC)/build/qdma/drv 
	$(MAKE) $(MODULE) -C $(QDMA_DRV_SRC) modulesymfile=$(modulesymfile)

qdma_drv_clean:
	$(MAKE) -C $(QDMA_DRV_SRC) clean 

