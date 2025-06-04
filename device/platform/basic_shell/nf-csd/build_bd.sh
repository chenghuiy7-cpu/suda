TARGET_BOARD=fidus
SHELL_TYPE="nvme"

AR_BRIDGE_EN=0
R_BRIDGE_EN=0

export AR_BRIDGE_EN
export R_BRIDGE_EN

# make -C fpga/shell/nf-pcie/vscode-op && \
#make -C work_farm PRJ=shell:virt_one_drive:accframework FPGA_BD=$TARGET_BOARD FPGA_ACT=dcp_gen vivado_prj
#make -C work_farm PRJ=shell:virt_one_drive:pcie_ep FPGA_BD=$TARGET_BOARD FPGA_ACT=dcp_gen vivado_prj && \
#make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD FPGA_ACT=prj_gen vivado_prj
#make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD FPGA_ACT=run_syn vivado_prj && \
#make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD FPGA_ACT=bit_gen vivado_prj && \
make -C work_farm PRJ=shell:virt_one_drive FPGA_BD=$TARGET_BOARD WITH_BIT=y IO_CACHE_COHERENCE=y bootbin # boot.bin
