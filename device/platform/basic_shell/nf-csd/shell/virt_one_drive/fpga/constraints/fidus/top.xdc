#========================================================
# Vivado constraint file
# Based on Vivado 2019.1
# Author: Yisong Chang (changyisong@ict.ac.cn)
# Date: 02/01/2021
#========================================================

# PCIe RP #0 GT reference clock
create_clock -period 10.000 -name pcie_rc_ref_clk_0 -waveform {0.000 5.000} [get_ports pcie_rc_gt_ref_clk_0_clk_p]

set_property PACKAGE_PIN AB12 [get_ports {pcie_rc_gt_ref_clk_0_clk_p[0]}]

# PCIe RP #1 GT reference clock
create_clock -period 10.000 -name pcie_rc_ref_clk_1 -waveform {0.000 5.000} [get_ports pcie_rc_gt_ref_clk_1_clk_p]

set_property PACKAGE_PIN Y12 [get_ports {pcie_rc_gt_ref_clk_1_clk_p[0]}]

# PCIe RP #2 GT reference clock
create_clock -period 10.000 -name pcie_rc_ref_clk_2 -waveform {0.000 5.000} [get_ports pcie_rc_gt_ref_clk_2_clk_p]

set_property PACKAGE_PIN AB34 [get_ports {pcie_rc_gt_ref_clk_2_clk_p[0]}]

# PCIe RP #3 GT reference clock
create_clock -period 10.000 -name pcie_rc_ref_clk_3 -waveform {0.000 5.000} [get_ports pcie_rc_gt_ref_clk_3_clk_p]

set_property PACKAGE_PIN W32 [get_ports {pcie_rc_gt_ref_clk_3_clk_p[0]}]

# PCIe EP GT reference clock
create_clock -period 10.000 -name pcie_ep_ref_clk -waveform {0.000 5.000} [get_ports pcie_ep_gt_ref_clk_clk_p]

set_property PACKAGE_PIN AF12 [get_ports {pcie_ep_gt_ref_clk_clk_p[0]}]

# PL DDR reference clock
create_clock -period 10.000 -name ddr4_mig_sys_clk -waveform {0.000 5.000} [get_ports ddr4_mig_sys_clk_clk_p]

set_property IOSTANDARD DIFF_SSTL12 [get_ports ddr4_mig_sys_clk_clk_n]
set_property IOSTANDARD DIFF_SSTL12 [get_ports ddr4_mig_sys_clk_clk_p]

set_property PACKAGE_PIN AT27 [get_ports ddr4_mig_sys_clk_clk_n]
set_property PACKAGE_PIN AR27 [get_ports ddr4_mig_sys_clk_clk_p]

set_property CLOCK_DEDICATED_ROUTE BACKBONE [get_pins -hier -filter {NAME =~ */u_ddr4_infrastructure/gen_mmcme4.u_mmcme_adv_inst/CLKIN1}]

# PCIe RP #0 perstn physical location
set_property PACKAGE_PIN C8 [get_ports pcie_rc_perstn_0]
set_property IOSTANDARD LVCMOS18 [get_ports pcie_rc_perstn_0]

# PCIe RP #1 perstn physical location
set_property PACKAGE_PIN F8 [get_ports pcie_rc_perstn_1]
set_property IOSTANDARD LVCMOS18 [get_ports pcie_rc_perstn_1]

# PCIe RP #2 perstn physical location
set_property PACKAGE_PIN H9 [get_ports pcie_rc_perstn_2]
set_property IOSTANDARD LVCMOS18 [get_ports pcie_rc_perstn_2]

# PCIe RP #3 perstn physical location
set_property PACKAGE_PIN J8 [get_ports pcie_rc_perstn_3]
set_property IOSTANDARD LVCMOS18 [get_ports pcie_rc_perstn_3]

# PCIe EP perstn physical location
set_property PACKAGE_PIN AL23 [get_ports pcie_ep_perstn]
set_property IOSTANDARD LVCMOS18 [get_ports pcie_ep_perstn]

# Timing exceptions
# ZynqMP PL CLK1 vs. PCIe RP0 user clk
set_clock_groups -name async_pl_clk1_pcie_rp0_user -asynchronous \
    -group [get_clocks clk_pl_1] \
    -group [get_clocks -filter {NAME =~ *gen_channel_container[4].*TXOUTCLK}]

# ZynqMP PL CLK1 vs. PCIe RP1 user clk
set_clock_groups -name async_pl_clk1_pcie_rp1_user -asynchronous \
    -group [get_clocks clk_pl_1] \
    -group [get_clocks -filter {NAME =~ *gen_channel_container[5].*TXOUTCLK}]

# ZynqMP PL CLK1 vs. PCIe RP2 user clk
# set_clock_groups -name async_pl_clk1_pcie_rp2_user -asynchronous \
#    -group [get_clocks clk_pl_1] \
#    -group [get_clocks -filter {NAME =~ *gen_channel_container[5].*TXOUTCLK}]

# ZynqMP PL CLK1 vs. PCIe RP3 user clk
# set_clock_groups -name async_pl_clk1_pcie_rp3_user -asynchronous \
#    -group [get_clocks clk_pl_1] \
#    -group [get_clocks -filter {NAME =~ *gen_channel_container[5].*TXOUTCLK}]

## DDR4 MIG ui_clk vs. ZynqMP PL CLK1
set_clock_groups -name async_ui_clk_pl_clk1 -asynchronous \
		-group [get_clocks mmcm_clkout0] \
		-group [get_clocks clk_pl_1]

