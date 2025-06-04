#========================================================
# Vivado constraint file
# Based on Vivado 2019.1
# Author: Yisong Chang (changyisong@ict.ac.cn)
# Date: 02/01/2021
#========================================================

# PCIe RP #0 GT reference clock
create_clock -period 10.000 -name pcie_rc_ref_clk_0 -waveform {0.000 5.000} [get_ports pcie_rc_gt_ref_clk_0_clk_p]

set_property PACKAGE_PIN AA10 [get_ports {pcie_rc_gt_ref_clk_0_clk_p[0]}]

# PCIe RP #1 GT reference clock
create_clock -period 10.000 -name pcie_rc_ref_clk_1 -waveform {0.000 5.000} [get_ports pcie_rc_gt_ref_clk_1_clk_p]

set_property PACKAGE_PIN W10 [get_ports {pcie_rc_gt_ref_clk_1_clk_p[0]}]

# PCIe EP GT reference clock
create_clock -period 10.000 -name pcie_ep_ref_clk -waveform {0.000 5.000} [get_ports pcie_ep_gt_ref_clk_clk_p]

set_property PACKAGE_PIN AF12 [get_ports {pcie_ep_gt_ref_clk_clk_p[0]}]

# PL DDR reference clock
create_clock -period 10.000 -name ddr4_mig_sys_clk -waveform {0.000 5.000} [get_ports ddr4_mig_sys_clk_clk_p]

set_property IOSTANDARD DIFF_SSTL12 [get_ports ddr4_mig_sys_clk_clk_n]
set_property IOSTANDARD DIFF_SSTL12 [get_ports ddr4_mig_sys_clk_clk_p]

set_property PACKAGE_PIN AV21 [get_ports ddr4_mig_sys_clk_clk_n]
set_property PACKAGE_PIN AU21 [get_ports ddr4_mig_sys_clk_clk_p]

set_property CLOCK_DEDICATED_ROUTE BACKBONE [get_pins -hier -filter {NAME =~ */u_ddr4_infrastructure/gen_mmcme4.u_mmcme_adv_inst/CLKIN1}]

# PCIe RC GT physical location
set_property LOC GTHE4_CHANNEL_X0Y16 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[4].*gen_gthe4_channel_inst[3].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y17 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[4].*gen_gthe4_channel_inst[2].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y18 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[4].*gen_gthe4_channel_inst[1].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y19 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[4].*gen_gthe4_channel_inst[0].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y20 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[5].*gen_gthe4_channel_inst[3].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y21 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[5].*gen_gthe4_channel_inst[2].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y22 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[5].*gen_gthe4_channel_inst[1].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y23 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[5].*gen_gthe4_channel_inst[0].GTHE4_CHANNEL_PRIM_INST}]

# PCIe EP GT physical location
set_property LOC GTHE4_CHANNEL_X0Y15 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[0].*gen_gthe4_channel_inst[0].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y14 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[0].*gen_gthe4_channel_inst[1].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y13 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[0].*gen_gthe4_channel_inst[2].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y12 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[0].*gen_gthe4_channel_inst[3].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y11 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[1].*gen_gthe4_channel_inst[0].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y10 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[1].*gen_gthe4_channel_inst[1].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y9 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[1].*gen_gthe4_channel_inst[2].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y8 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[1].*gen_gthe4_channel_inst[3].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y7 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[2].*gen_gthe4_channel_inst[0].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y6 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[2].*gen_gthe4_channel_inst[1].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y5 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[2].*gen_gthe4_channel_inst[2].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y4 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[2].*gen_gthe4_channel_inst[3].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y3 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[3].*gen_gthe4_channel_inst[0].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y2 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[3].*gen_gthe4_channel_inst[1].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y1 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[3].*gen_gthe4_channel_inst[2].GTHE4_CHANNEL_PRIM_INST}]
set_property LOC GTHE4_CHANNEL_X0Y0 [get_cells -hierarchical -filter {NAME =~ *gen_channel_container[3].*gen_gthe4_channel_inst[3].GTHE4_CHANNEL_PRIM_INST}]

# PCIe RP #0 perstn physical location
set_property PACKAGE_PIN D17 [get_ports pcie_rc_perstn_0]
set_property IOSTANDARD LVCMOS18 [get_ports pcie_rc_perstn_0]

# PCIe RP #1 perstn physical location
set_property PACKAGE_PIN E17 [get_ports pcie_rc_perstn_1]
set_property IOSTANDARD LVCMOS18 [get_ports pcie_rc_perstn_1]

# PCIe EP perstn physical location
set_property PACKAGE_PIN N10 [get_ports pcie_ep_perstn]
set_property IOSTANDARD LVCMOS33 [get_ports pcie_ep_perstn]

# PL DDR4 calibration done LED
#set_property PACKAGE_PIN B11 [get_ports ddr4_mig_calib_done]
#set_property IOSTANDARD LVCMOS33 [get_ports ddr4_mig_calib_done]

# Timing exceptions
# ZynqMP PL CLK1 vs. PCIe RP0 user clk
set_clock_groups -name async_pl_clk1_pcie_rp0_user -asynchronous \
    -group [get_clocks clk_pl_1] \
    -group [get_clocks -filter {NAME =~ *gen_channel_container[4].*TXOUTCLK}]

# ZynqMP PL CLK1 vs. PCIe RP1 user clk
set_clock_groups -name async_pl_clk1_pcie_rp1_user -asynchronous \
    -group [get_clocks clk_pl_1] \
    -group [get_clocks -filter {NAME =~ *gen_channel_container[5].*TXOUTCLK}]

## DDR4 MIG ui_clk vs. ZynqMP PL CLK1
set_clock_groups -name async_ui_clk_pl_clk1 -asynchronous \
		-group [get_clocks mmcm_clkout0] \
		-group [get_clocks clk_pl_1]

