# Prerequisite

1. Download all required submodules in repository

`git submodule update --init --recursive`

2. Launch the following commands 
`mkdir -p work_farm/target/` 
`cd work_farm/target` 
`ln -s ../../nf-csd nf-csd`   
`cd ../ && ln -s ../shell shell`

3. All compilation operations are launched in the directory of `work_farm`

# FPGA SHELL Generation

## FPGA design flow

FPGA SHELLs for different NVMe virtualization 
scenarios are located in 
the directory `work_farm/shell`. 
In each SHELL design, different acceleration cards
(i.e., in-house NF v3 card, 
and commodity Fidus card) 
are separated in individual directories.  

Please conduct the following commands to generate the bitstream file  
that would be deployed onto the target acceleration card 
(taking the SHELL of virtualizing one NVMe drive 
on the Fidus card as an illustrative example):    

`make PRJ=shell:virt_one_drive:pcie_ep FPGA_BD=fidus FPGA_ACT=dcp_gen vivado_prj`    
`make PRJ=shell:virt_one_drive FPGA_BD=fidus FPGA_ACT=prj_gen vivado_prj`    
`make PRJ=shell:virt_one_drive FPGA_BD=fidus FPGA_ACT=run_syn vivado_prj`   
`make PRJ=shell:virt_one_drive FPGA_BD=fidus FPGA_ACT=bit_gen vivado_prj`
    
The SHELL bitstream file (named `system.bit`) is located in   
`work_farm/hw_plat/shell_virt_one_drive_fidus/` 

Log files, timing/utilization reports and 
design checkpoint files (.dcp) generated during 
Xilinx Vivado design flow are located in   
`work_farm/fpga/vivado_out/shell_virt_one_drive_fidus/` 

The generated Vivado project is located in   
`work_farm/fpga/vivado_prj/shell_virt_one_drive_fidus/` 

## BOOT.bin of Zynq MPSoC generation

`make PRJ=shell:virt_one_drive FPGA_BD=fidus fsbl`    
`make PRJ=shell:virt_one_drive FPGA_BD=fidus pmufw`     
`make PRJ=shell:virt_one_drive FPGA_BD=fidus dt`   
`make PRJ=shell:virt_one_drive FPGA_BD=fidus atf`   
`make PRJ=shell:virt_one_drive FPGA_BD=fidus uboot`    
`make PRJ=shell:virt_one_drive FPGA_BD=fidus WITH_BIT=y bootbin`    

The BOOT.bin file is located in   
`work_farm/shell/virt_one_drive/ready_for_download/fidus/` 
