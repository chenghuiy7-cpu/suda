#========================================================
# Vivado BD design auto run script for mpsoc
# Based on Vivado 2020.2
# Author: Yisong Chang (changyisong@ict.ac.cn)
# Date: 07/09/2022
#========================================================

namespace eval mpsoc_bd_val {
	set design_name mpsoc
	set bd_prefix ${mpsoc_bd_val::design_name}_
	set nvme_qe_dma_num 1
	set axis_ic_channels 8
	set op_num 4
      set ar_bridge_en ${::env(AR_BRIDGE_EN)}
      set r_bridge_en ${::env(R_BRIDGE_EN)}
}

puts "ar_bridge_en ${mpsoc_bd_val::ar_bridge_en} r_bridge_en ${mpsoc_bd_val::r_bridge_en}"

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

if { ${mpsoc_bd_val::design_name} eq "" } {
   # USE CASES:
   #    1) Design_name not set

   set errMsg "Please set the variable <design_name> to a non-empty value."
   set nRet 1

} elseif { ${cur_design} ne "" && ${list_cells} eq "" } {
   # USE CASES:
   #    2): Current design opened AND is empty AND names same.
   #    3): Current design opened AND is empty AND names diff; design_name NOT in project.
   #    4): Current design opened AND is empty AND names diff; design_name exists in project.

   if { $cur_design ne ${mpsoc_bd_val::design_name} } {
      common::send_msg_id "BD_TCL-001" "INFO" "Changing value of <design_name> from <${mpsoc_bd_val::design_name}> to <$cur_design> since current design is empty."
      set design_name [get_property NAME $cur_design]
   }
   common::send_msg_id "BD_TCL-002" "INFO" "Constructing design in IPI design <$cur_design>..."

} elseif { ${cur_design} ne "" && $list_cells ne "" && $cur_design eq ${mpsoc_bd_val::design_name} } {
   # USE CASES:
   #    5) Current design opened AND has components AND same names.

   set errMsg "Design <${mpsoc_bd_val::design_name}> already exists in your project, please set the variable <design_name> to another value."
   set nRet 1
} elseif { [get_files -quiet ${mpsoc_bd_val::design_name}.bd] ne "" } {
   # USE CASES: 
   #    6) Current opened design, has components, but diff names, design_name exists in project.
   #    7) No opened design, design_name exists in project.

   set errMsg "Design <${mpsoc_bd_val::design_name}> already exists in your project, please set the variable <design_name> to another value."
   set nRet 2

} else {
   # USE CASES:
   #    8) No opened design, design_name not in project.
   #    9) Current opened design, has components, but diff names, design_name not in project.

   common::send_msg_id "BD_TCL-003" "INFO" "Currently there is no design <${mpsoc_bd_val::design_name}> in project, so creating one..."

   create_bd_design ${mpsoc_bd_val::design_name}

   common::send_msg_id "BD_TCL-004" "INFO" "Making design <${mpsoc_bd_val::design_name}> as current_bd_design."
   current_bd_design ${mpsoc_bd_val::design_name}

}

common::send_msg_id "BD_TCL-005" "INFO" "Currently the variable <design_name> is equal to \"${mpsoc_bd_val::design_name}\"."

if { $nRet != 0 } {
   catch {common::send_msg_id "BD_TCL-114" "ERROR" $errMsg}
   return $nRet
}

##################################################################
# DESIGN PROCs
##################################################################

# Procedure to create entire design; Provide argument to make
# procedure reusable. If parentCell is "", will use root.
proc create_root_design { parentCell } {

  variable script_folder

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_msg_id "BD_TCL-100" "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_msg_id "BD_TCL-101" "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj

#=============================================
# Create IP blocks
#=============================================

  # Create instance: qdma_ep properties
  set block_name qdma_ep 
  set block_cell_name u_qdma_ep
  if { [catch {set u_qdma_ep [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
  } elseif { $u_qdma_ep eq "" } {
     catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
  }

  # Create instance: qdma_ep properties
  set block_name qdma_ep_axis_wrapper 
  set block_cell_name u_qdma_ep_axis_wrapper
  if { [catch {set u_qdma_ep_axis_wrapper [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
  } elseif { $u_qdma_ep_axis_wrapper eq "" } {
     catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
  }

  # Create instance: Zynq MPSoC
  set zynq_mpsoc [ create_bd_cell -type ip -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.3 zynq_mpsoc ]
  apply_bd_automation -rule xilinx.com:bd_rule:zynq_ultra_ps_e -config {apply_board_preset "1"} $zynq_mpsoc
  set_property -dict [ list CONFIG.PSU__USE__M_AXI_GP0 {1} \
	CONFIG.PSU__USE__M_AXI_GP1 {1} \
	CONFIG.PSU__USE__M_AXI_GP2 {1} \
	CONFIG.PSU__USE__S_AXI_GP0 {1} \
	CONFIG.PSU__USE__S_AXI_GP1 {1} \
	CONFIG.PSU__USE__S_AXI_GP2 {0} \
	CONFIG.PSU__USE__S_AXI_GP3 {0} \
	CONFIG.PSU__USE__S_AXI_GP4 {0} \
	CONFIG.PSU__USE__S_AXI_GP5 {0} \
	CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ {125} \
	CONFIG.PSU__FPGA_PL1_ENABLE {1} \
	CONFIG.PSU__CRL_APB__PL1_REF_CTRL__FREQMHZ {250} \
	CONFIG.PSU__FPGA_PL2_ENABLE {1} \
	CONFIG.PSU__CRL_APB__PL2_REF_CTRL__FREQMHZ {100} \
	CONFIG.PSU__USE__IRQ0 {1} \
	CONFIG.PSU__USE__IRQ1 {1} \
	CONFIG.PSU__HIGH_ADDRESS__ENABLE {1} \
	CONFIG.PSU__EXPAND__LOWER_LPS_SLAVES {1} \
	CONFIG.PSU__DDR_SW_REFRESH_ENABLED {0} ] $zynq_mpsoc

  # Create instance: AXI PCIe Root Complex
  # axi_ctl_resetn deasserted after 2 cycles of Phy Ready in root port mode
  # as indicated in pg194

  # Create instance: AXI PCIe Root Complex
  set xdma_rp_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xdma:4.1 xdma_rp_1 ]
  set_property -dict [ list CONFIG.mode_selection {Advanced} \
	CONFIG.device_port_type {Root_Port_of_PCI_Express_Root_Complex} \
        CONFIG.functional_mode {AXI Bridge} \
        CONFIG.dma_reset_source_sel {Phy_Ready} \
	CONFIG.en_gt_selection {true} \
	CONFIG.pl_link_cap_max_link_width {X4} \
	CONFIG.pl_link_cap_max_link_speed {8.0_GT/s} \
	CONFIG.axi_addr_width {64} \
	CONFIG.pf0_class_code_sub {04} \
	CONFIG.pf0_bar0_enabled {false} \
	CONFIG.axibar2pciebar_0 {0x00000000A0100000} \
	CONFIG.c_s_axi_supports_narrow_burst {false} \
	CONFIG.plltype {QPLL1} \
        CONFIG.msi_rx_pin_en {true} \
        CONFIG.pcie_blk_locn {X1Y2} \
	CONFIG.select_quad {GTH_Quad_229} \
	CONFIG.BASEADDR {0x00000000} \
	CONFIG.HIGHADDR {0x007FFFFF} ] $xdma_rp_1

  # Create instance: AXI PCIe Root Complex
  # axi_ctl_resetn deasserted after 2 cycles of Phy Ready in root port mode
  # as indicated in pg194
  set xdma_rp_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xdma:4.1 xdma_rp_2 ]
  set_property -dict [ list CONFIG.mode_selection {Advanced} \
	CONFIG.device_port_type {Root_Port_of_PCI_Express_Root_Complex} \
        CONFIG.functional_mode {AXI Bridge} \
        CONFIG.dma_reset_source_sel {Phy_Ready} \
	CONFIG.en_gt_selection {true} \
	CONFIG.pl_link_cap_max_link_width {X4} \
	CONFIG.pl_link_cap_max_link_speed {8.0_GT/s} \
	CONFIG.axi_addr_width {64} \
	CONFIG.pf0_class_code_sub {04} \
	CONFIG.pf0_bar0_enabled {false} \
	CONFIG.axibar2pciebar_0 {0x00000000A0200000} \
	CONFIG.c_s_axi_supports_narrow_burst {false} \
	CONFIG.plltype {QPLL1} \
        CONFIG.msi_rx_pin_en {true} \
        CONFIG.pcie_blk_locn {X0Y2} \
	CONFIG.select_quad {GTY_Quad_128} \
	CONFIG.BASEADDR {0x00000000} \
	CONFIG.HIGHADDR {0x007FFFFF} ] $xdma_rp_2

  # Create instance: AXI PCIe Root Complex
  # axi_ctl_resetn deasserted after 2 cycles of Phy Ready in root port mode
  # as indicated in pg194
  set xdma_rp_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:xdma:4.1 xdma_rp_3 ]
  set_property -dict [ list CONFIG.mode_selection {Advanced} \
	CONFIG.device_port_type {Root_Port_of_PCI_Express_Root_Complex} \
        CONFIG.functional_mode {AXI Bridge} \
        CONFIG.dma_reset_source_sel {Phy_Ready} \
	CONFIG.en_gt_selection {true} \
	CONFIG.pl_link_cap_max_link_width {X4} \
	CONFIG.pl_link_cap_max_link_speed {8.0_GT/s} \
	CONFIG.axi_addr_width {64} \
	CONFIG.pf0_class_code_sub {04} \
	CONFIG.pf0_bar0_enabled {false} \
	CONFIG.axibar2pciebar_0 {0x00000000A0300000} \
	CONFIG.c_s_axi_supports_narrow_burst {false} \
	CONFIG.plltype {QPLL1} \
        CONFIG.msi_rx_pin_en {true} \
        CONFIG.pcie_blk_locn {X0Y3} \
	CONFIG.select_quad {GTY_Quad_129} \
	CONFIG.BASEADDR {0x00000000} \
	CONFIG.HIGHADDR {0x007FFFFF} ] $xdma_rp_3

  # Create instance: AXI Multi-channel DMA engines for NVMe QEs
  set i 0
  while {$i < ${mpsoc_bd_val::nvme_qe_dma_num}} {
	  set nvme_qe_dma_name axi_nvme_qe_dma_$i
	  #set nvme_qe_dma_ctrl \
	      [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_mcdma:1.1 $nvme_qe_dma_name ]
	  set nvme_qe_dma_ctrl \
	      [ create_bd_cell -type module -reference axi_mcdma $nvme_qe_dma_name ]
	  #    set_property -dict [ list CONFIG.c_prmry_is_aclk_async {1} \
	      CONFIG.c_num_mm2s_channels {8} \
	      CONFIG.c_num_s2mm_channels {8} \
	      CONFIG.c_include_mm2s_dre {1} \
	      CONFIG.c_m_axi_mm2s_data_width {128} \
	      CONFIG.c_m_axis_mm2s_tdata_width {128} \
	      CONFIG.c_mm2s_burst_size {256} \
	      CONFIG.c_m_axi_s2mm_data_width.VALUE_SRC {USER} \
	      CONFIG.c_m_axi_s2mm_data_width {128} \
	      CONFIG.c_include_s2mm_dre {1} \
	      CONFIG.c_s2mm_burst_size {256} \
	      CONFIG.c_sg_include_stscntrl_strm {0} \
	      CONFIG.c_sg_use_stsapp_length {0} \
	      CONFIG.c_addr_width {40} \
	      CONFIG.c_sg_length_width {16} ] $nvme_qe_dma_ctrl
	  set_property CONFIG.FREQ_HZ 249997498 [get_bd_intf_pins $nvme_qe_dma_name/S_AXIS_S2MM]
	  set_property CONFIG.FREQ_HZ 249997498 [get_bd_intf_pins $nvme_qe_dma_name/M_AXIS_MM2S]
         
          ## Create instance: AXI interrupt controller for each NVMe QE DMA engine
          set nvme_qe_dma_intc_name axi_nvme_qe_dma_intc_$i
	  set nvme_qe_dma_intc_ctrl \
	      [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_intc:4.1 $nvme_qe_dma_intc_name ]
          set_property -dict [ list CONFIG.C_IRQ_CONNECTION {1} \
              CONFIG.C_IRQ_IS_LEVEL {0} ] $nvme_qe_dma_intc_ctrl

	  incr i 1
  }
  
  ## Specially Set MCMDA 00
  #set_property -dict [list CONFIG.c_addr_width {64}] [get_bd_cells axi_nvme_qe_dma_0]

  
  ## Create instance: AXI interrupt controller for each NVMe admin data DMA engine
  set axi_mcdma_intr \
	[ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_intc:4.1 axi_mcdma_intr ]
  set_property -dict [ list CONFIG.C_IRQ_CONNECTION {1} \
        CONFIG.C_IRQ_IS_LEVEL {0} ] $axi_mcdma_intr

  # Create instance: DDR4 MIG leveraged by ROLE
#   set ddr4_mig [ create_bd_cell -type ip -vlnv xilinx.com:ip:ddr4:2.2 ddr4_mig ]
#   set_property -dict [ list CONFIG.C0.DDR4_isCustom {false} \
#         CONFIG.C0.DDR4_TimePeriod {938} \
# 	CONFIG.C0.DDR4_InputClockPeriod {10005} \
# 	CONFIG.C0.DDR4_MemoryType {SODIMMs} \
# 	CONFIG.C0.DDR4_MemoryPart {MTA16ATF2G64HZ-2G3} \
# 	CONFIG.C0.DDR4_CasLatency {15} \
# 	CONFIG.C0.DDR4_AxiDataWidth {512} \
# 	CONFIG.System_Clock {No_Buffer} ] $ddr4_mig

  # Create instance: Address Concat for MMIO port of PCIe RP #1
  set xlconcat_rp1_ar [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_rp1_ar ]
  set_property -dict [list CONFIG.NUM_PORTS {1} CONFIG.IN0_WIDTH {23}] $xlconcat_rp1_ar

  set xlconcat_rp1_aw [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_rp1_aw ]
  set_property -dict [list CONFIG.NUM_PORTS {1} CONFIG.IN0_WIDTH {23}] $xlconcat_rp1_aw

  # Create instance: Address Concat for MMIO port of PCIe RP #2
  set xlconcat_rp2_ar [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_rp2_ar ]
  set_property -dict [list CONFIG.NUM_PORTS {1} CONFIG.IN0_WIDTH {23}] $xlconcat_rp2_ar

  set xlconcat_rp2_aw [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_rp2_aw ]
  set_property -dict [list CONFIG.NUM_PORTS {1} CONFIG.IN0_WIDTH {23}] $xlconcat_rp2_aw

  # Create instance: Address Concat for MMIO port of PCIe RP #3
  set xlconcat_rp3_ar [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_rp3_ar ]
  set_property -dict [list CONFIG.NUM_PORTS {1} CONFIG.IN0_WIDTH {23}] $xlconcat_rp3_ar

  set xlconcat_rp3_aw [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_rp3_aw ]
  set_property -dict [list CONFIG.NUM_PORTS {1} CONFIG.IN0_WIDTH {23}] $xlconcat_rp3_aw

  # Create instance: AXI IC for AXI-MM interfaces of PCIe RC BAR and DMA

  set axi_ic_pcie_rp_1_dma [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_pcie_rp_1_dma ]
  set_property -dict [ list CONFIG.NUM_MI {2} \
	CONFIG.NUM_SI {1} \
	CONFIG.M00_HAS_REGSLICE {1} \
	CONFIG.M01_HAS_REGSLICE {1} \
        CONFIG.STRATEGY {2} ] $axi_ic_pcie_rp_1_dma

  set axi_ic_pcie_rp_2_dma [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_pcie_rp_2_dma ]
  set_property -dict [ list CONFIG.NUM_MI {2} \
	CONFIG.NUM_SI {1} \
	CONFIG.M00_HAS_REGSLICE {1} \
	CONFIG.M01_HAS_REGSLICE {1} \
        CONFIG.STRATEGY {2} ] $axi_ic_pcie_rp_2_dma

  set axi_ic_pcie_rp_3_dma [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_pcie_rp_3_dma ]
  set_property -dict [ list CONFIG.NUM_MI {2} \
	CONFIG.NUM_SI {1} \
	CONFIG.M00_HAS_REGSLICE {1} \
	CONFIG.M01_HAS_REGSLICE {1} \
        CONFIG.STRATEGY {2} ] $axi_ic_pcie_rp_3_dma

  set axi_ic_pcie_rc_dma [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_pcie_rc_dma ]
  set_property -dict [ list CONFIG.NUM_MI {1} \
	CONFIG.NUM_SI {4} \
	CONFIG.M00_HAS_REGSLICE {1} \
        CONFIG.STRATEGY {2} ] $axi_ic_pcie_rc_dma

  set axi_ic_pcie_rc_bar [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_pcie_rc_bar ]
  set_property -dict [ list CONFIG.NUM_MI {4} \
	CONFIG.NUM_SI {2} \
        CONFIG.S00_HAS_REGSLICE {1} ] $axi_ic_pcie_rc_bar

  set axi_ic_pcie_rc_mmio [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_pcie_rc_mmio ]
  set_property -dict [ list CONFIG.NUM_MI {4} \
	CONFIG.NUM_SI {1} ] $axi_ic_pcie_rc_mmio

  # 4 QE MCDMA engines w/ 4 AXI interrupt controllers
  set axi_ic_nvme_qe_dma_mmio [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_nvme_qe_dma_mmio ]
  set_property -dict [ list CONFIG.NUM_MI {2} \
	CONFIG.NUM_SI {1} \
	CONFIG.M00_HAS_REGSLICE {4} \
	CONFIG.M01_HAS_REGSLICE {4} \
	CONFIG.S00_HAS_REGSLICE {3} ] $axi_ic_nvme_qe_dma_mmio

  set axi_ic_mcdma_mmio [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_mcdma_mmio ]
  set_property -dict [ list CONFIG.NUM_MI {7} \
	CONFIG.NUM_SI {1} \
	CONFIG.M00_HAS_REGSLICE {4} \
	CONFIG.M01_HAS_REGSLICE {4} \
	CONFIG.M02_HAS_REGSLICE {4} \
      CONFIG.M03_HAS_REGSLICE {4} \
	CONFIG.M04_HAS_REGSLICE {4} \
	CONFIG.M05_HAS_REGSLICE {4} \
      CONFIG.M06_HAS_REGSLICE {4} \
	CONFIG.S00_HAS_REGSLICE {3} ] $axi_ic_mcdma_mmio

  # Scatter-Gather ports (4) and S2MM ports (4) of all axi_nvme_qe_dma_* engines
  set axi_ic_nvme_qe_dma [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_nvme_qe_dma ]
  set_property -dict [ list CONFIG.NUM_MI {2} \
	CONFIG.NUM_SI {3} \
	CONFIG.M00_HAS_REGSLICE {4} \
      CONFIG.M01_HAS_REGSLICE {4} \
      CONFIG.ENABLE_ADVANCED_OPTIONS {1} \
      CONFIG.XBAR_DATA_WIDTH {512} \
      CONFIG.M01_HAS_DATA_FIFO {2} \
      ] $axi_ic_nvme_qe_dma

  # MM2S ports (4) of all axi_nvme_qe_dma_* engines

  set axi_ic_mcdma [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_ic_mcdma ]
  set_property -dict [ list CONFIG.NUM_MI {1} \
	CONFIG.NUM_SI {4} \
	CONFIG.M00_HAS_REGSLICE {4} ] $axi_ic_mcdma
        
  #==================================================
  # QDMA queue ID mapping to x86 host-end NVMe Queues
  # Total number of Admin Queues: 16
  # Total number of I/O   Queues: 48
  #     * NVMe Admin Submission Queues (QE):   H2C queue  0   - 15  (To QE MCDMA)
  #     * NVMe Admin Submission Queues (Data): C2H queue  112 - 127 (From Admin data MCDMA)
  #     * NVMe Admin Completion Queues (QE):   C2H queue  0   - 15  (From QE MCDMA)
  #     * NVMe I/O   Submission Queues (QE):   H2C queue  16  - 63 (To QE MCDMA)
  #     * NVMe I/O   Submission Queues (Write): H2C queue 64 - 111 (To PCIe RP)
  #     * NVMe I/O   Submission Queues (Read): C2H queue  64 - 111 (From PCIe RP)
  #     * NVMe I/O   Completion Queues (QE):   C2H queue  16  - 63 (From QE MCDMA)
  #==================================================

  set axis_ic_qdma_h2c_num_out 2
  set axis_ic_qdma_h2c_num_in 1
  set axis_ic_qdma_c2h_num_out 2
  set axis_ic_qdma_c2h_num_in 1

  if { ${mpsoc_bd_val::ar_bridge_en} == "1" } {
        incr axis_ic_qdma_c2h_num_out 1
        incr axis_ic_qdma_h2c_num_in 1
  }

  if { ${mpsoc_bd_val::r_bridge_en} == "1" } {
        incr axis_ic_qdma_h2c_num_out -1
  }

  #==================================================
  # AXI-Stream IC (AXIS-IC) of the H2C port of QDMA
  # S00 port: QDMA H2C data
  # S01 port: QDMA H2C bypass out, for debug (tdest 0x1F, tid 0x1F)
  # S02 port: QDMA C2H bypass out, for debug (tdest 0x1E, tid 0x22)
  # S03 port: XDMA w input (tdest 0x1F, tid 0x20)
  # S04 port: XDMA aw_req input (tdest 0x1F, tid 0x21)
  # S05 port: XDMA ar_req input, when ar_bridge_en is true
  # M00 port: (QE) to QE MCDMA
  # M01 port (tdest=0x26): (Data) to XDMA AXIB_R input, when r_bridge_en is false
  # When R bridge is enabled, data is also routed to QE MCDMA, thus only 1 M port
  # QID[1:0] is leveraged as the TDEST value of each transaction 
  #==================================================
  set axis_ic_qdma_h2c [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_ic_qdma_h2c ]
  set_property -dict [ list CONFIG.NUM_SI ${axis_ic_qdma_h2c_num_in} \
        CONFIG.NUM_MI ${axis_ic_qdma_h2c_num_out} \
        CONFIG.ENABLE_ADVANCED_OPTIONS {1} \
        CONFIG.XBAR_TDATA_NUM_BYTES.VALUE_SRC {USER} \
        CONFIG.XBAR_TDATA_NUM_BYTES {64} \
        CONFIG.M00_FIFO_MODE {1} \
        CONFIG.M00_FIFO_DEPTH {4096} ] $axis_ic_qdma_h2c 

  if { ${mpsoc_bd_val::r_bridge_en} == "1" } {
      set_property -dict [ list CONFIG.M00_AXIS_BASETDEST {0} \
            CONFIG.M00_AXIS_HIGHTDEST {0x000000FF} ] $axis_ic_qdma_h2c 
  } else {
      set_property -dict [ list CONFIG.M00_AXIS_BASETDEST {0} \
            CONFIG.M00_AXIS_HIGHTDEST {0x00000008} \
            CONFIG.M01_AXIS_BASETDEST {0x00000009} \
            CONFIG.M01_AXIS_HIGHTDEST {0x00000010} \
            CONFIG.M02_AXIS_BASETDEST {0x00000011} \
            CONFIG.M02_AXIS_HIGHTDEST {0x00000011} \
            CONFIG.M01_FIFO_MODE {1} \
            CONFIG.M01_FIFO_DEPTH {4096} ] $axis_ic_qdma_h2c 
  }

  #==================================================
  # AXI-Stream IC (AXIS-IC) of the C2H port of QDMA
  # S00 port: from QE MCDMA
  # M00 port (tdest<0x1e): to QDMA data
  # M01 port (tdest=0x1e): to XDMA AXIB_R input, when r_bridge_en is true
  # M02 port (tdest=0x1f): to QDMA H2C bypass in, when ar_bridge_en is true
  #    (0x16<=tdest<0x1e): to QDMA C2H data input, when w bridge is enabled
  # M03 port (tdest=0x21): to QDMA C2H bypass in, when aw_bridge is enabled
  #==================================================

  set axis_ic_qdma_c2h [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_ic_qdma_c2h ]
  set_property -dict [ list CONFIG.NUM_SI ${axis_ic_qdma_c2h_num_in} \
        CONFIG.NUM_MI ${axis_ic_qdma_c2h_num_out} \
        CONFIG.ENABLE_ADVANCED_OPTIONS {1} \
        CONFIG.XBAR_TDATA_NUM_BYTES.VALUE_SRC {USER} \
        CONFIG.XBAR_TDATA_NUM_BYTES {64} \
        CONFIG.ARB_ON_TLAST {1} \
        CONFIG.ARB_ON_MAX_XFERS {0} \
        CONFIG.M00_FIFO_MODE {1} \
        CONFIG.M00_FIFO_DEPTH {4096} \
        CONFIG.M01_FIFO_MODE {1} \
        CONFIG.M01_FIFO_DEPTH {4096} \
        CONFIG.S00_FIFO_MODE {1} \
        CONFIG.S00_FIFO_DEPTH {4096} ] $axis_ic_qdma_c2h 

if { ${mpsoc_bd_val::ar_bridge_en} == "1" } {
      set_property -dict [ list CONFIG.M00_AXIS_BASETDEST {0} \
            CONFIG.M02_FIFO_MODE {1} \
            CONFIG.M02_FIFO_DEPTH {4096} \
            CONFIG.M00_AXIS_HIGHTDEST {0x0000001D} \
            CONFIG.M01_AXIS_BASETDEST {0x0000001E} \
            CONFIG.M01_AXIS_HIGHTDEST {0x0000001E} \
            CONFIG.M02_AXIS_BASETDEST {0x0000001F} \
            CONFIG.M02_AXIS_HIGHTDEST {0x0000001F} ] $axis_ic_qdma_c2h 
} else {
      set_property -dict [ list CONFIG.M00_AXIS_BASETDEST {0} \
            CONFIG.M00_AXIS_HIGHTDEST {0x000000FF} \
            CONFIG.M01_AXIS_BASETDEST {0x00000100} \
            CONFIG.M01_AXIS_HIGHTDEST {0x00000100} ] $axis_ic_qdma_c2h 
}

  set axis_ic_qdma_c2h_data [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_ic_qdma_c2h_data ]
  set_property -dict [ list CONFIG.NUM_SI {2} \
        CONFIG.NUM_MI {1} \
        CONFIG.ENABLE_ADVANCED_OPTIONS {1} \
        CONFIG.XBAR_TDATA_NUM_BYTES.VALUE_SRC {USER} \
        CONFIG.XBAR_TDATA_NUM_BYTES {64} \
        CONFIG.ARB_ON_TLAST {1} \
        CONFIG.ARB_ON_MAX_XFERS {0} \
        CONFIG.M00_FIFO_MODE {1} \
        CONFIG.M00_FIFO_DEPTH {64} \
        CONFIG.M00_AXIS_BASETDEST {0x00000000} \
        CONFIG.M00_AXIS_HIGHTDEST {0x000000FF} \
        CONFIG.S00_FIFO_MODE {1} \
        CONFIG.S00_FIFO_DEPTH {64} \
        CONFIG.S01_FIFO_MODE {1} \
        CONFIG.S01_FIFO_DEPTH {64} ] $axis_ic_qdma_c2h_data 

  set axis_ic_qdma_h2c_byp_in [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_ic_qdma_h2c_byp_in ]
  set_property -dict [ list CONFIG.NUM_SI {2} \
        CONFIG.NUM_MI {1} \
        CONFIG.ARB_ON_TLAST {1} \
        CONFIG.ARB_ON_MAX_XFERS {0} ] $axis_ic_qdma_h2c_byp_in 

  set axis_ic_h2c_multi_r_out [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_ic_h2c_multi_r_out ]
  set_property -dict [ list CONFIG.NUM_SI {1} \
        CONFIG.NUM_MI {4} \
        CONFIG.ENABLE_ADVANCED_OPTIONS {1} \
        CONFIG.XBAR_TDATA_NUM_BYTES.VALUE_SRC {USER} \
        CONFIG.XBAR_TDATA_NUM_BYTES {64} \
        CONFIG.ARB_ON_TLAST {1} \
        CONFIG.ARB_ON_MAX_XFERS {0} \
        CONFIG.M00_FIFO_MODE {1} \
        CONFIG.M00_FIFO_DEPTH {64} \
        CONFIG.M01_FIFO_MODE {1} \
        CONFIG.M01_FIFO_DEPTH {64} \
        CONFIG.M02_FIFO_MODE {1} \
        CONFIG.M02_FIFO_DEPTH {64} \
        CONFIG.M03_FIFO_MODE {1} \
        CONFIG.M03_FIFO_DEPTH {64} \
        CONFIG.S00_FIFO_MODE {0} \
        CONFIG.S00_FIFO_DEPTH {64} ] $axis_ic_h2c_multi_r_out


  set axis_dwidth_converter_w_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_w_0 ]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES.VALUE_SRC USER CONFIG.TUSER_BITS_PER_BYTE.VALUE_SRC USER CONFIG.HAS_TLAST.VALUE_SRC USER CONFIG.HAS_TKEEP.VALUE_SRC USER] [get_bd_cells axis_dwidth_converter_w_0]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {64} CONFIG.M_TDATA_NUM_BYTES {16} CONFIG.TUSER_BITS_PER_BYTE {1} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.HAS_MI_TKEEP {1}] [get_bd_cells axis_dwidth_converter_w_0]

  set axis_dwidth_converter_w_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_w_1 ]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES.VALUE_SRC USER CONFIG.TUSER_BITS_PER_BYTE.VALUE_SRC USER CONFIG.HAS_TLAST.VALUE_SRC USER CONFIG.HAS_TKEEP.VALUE_SRC USER] [get_bd_cells axis_dwidth_converter_w_1]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {64} CONFIG.M_TDATA_NUM_BYTES {16} CONFIG.TUSER_BITS_PER_BYTE {1} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.HAS_MI_TKEEP {1}] [get_bd_cells axis_dwidth_converter_w_1]

  set axis_dwidth_converter_w_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_w_2 ]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES.VALUE_SRC USER CONFIG.TUSER_BITS_PER_BYTE.VALUE_SRC USER CONFIG.HAS_TLAST.VALUE_SRC USER CONFIG.HAS_TKEEP.VALUE_SRC USER] [get_bd_cells axis_dwidth_converter_w_2]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {64} CONFIG.M_TDATA_NUM_BYTES {16} CONFIG.TUSER_BITS_PER_BYTE {1} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.HAS_MI_TKEEP {1}] [get_bd_cells axis_dwidth_converter_w_2]

  set axis_dwidth_converter_w_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_w_3 ]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES.VALUE_SRC USER CONFIG.TUSER_BITS_PER_BYTE.VALUE_SRC USER CONFIG.HAS_TLAST.VALUE_SRC USER CONFIG.HAS_TKEEP.VALUE_SRC USER] [get_bd_cells axis_dwidth_converter_w_3]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {64} CONFIG.M_TDATA_NUM_BYTES {16} CONFIG.TUSER_BITS_PER_BYTE {1} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.HAS_MI_TKEEP {1}] [get_bd_cells axis_dwidth_converter_w_3]

  set axis_dwidth_converter_r_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_r_0 ]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES.VALUE_SRC USER CONFIG.TUSER_BITS_PER_BYTE.VALUE_SRC USER CONFIG.HAS_TLAST.VALUE_SRC USER CONFIG.HAS_TKEEP.VALUE_SRC USER] [get_bd_cells axis_dwidth_converter_r_0]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {16} CONFIG.M_TDATA_NUM_BYTES {64} CONFIG.TUSER_BITS_PER_BYTE {1} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.HAS_MI_TKEEP {1}] [get_bd_cells axis_dwidth_converter_r_0]

  set axis_dwidth_converter_r_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_r_1 ]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES.VALUE_SRC USER CONFIG.TUSER_BITS_PER_BYTE.VALUE_SRC USER CONFIG.HAS_TLAST.VALUE_SRC USER CONFIG.HAS_TKEEP.VALUE_SRC USER] [get_bd_cells axis_dwidth_converter_r_1]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {16} CONFIG.M_TDATA_NUM_BYTES {64} CONFIG.TUSER_BITS_PER_BYTE {1} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.HAS_MI_TKEEP {1}] [get_bd_cells axis_dwidth_converter_r_1]

  set axis_dwidth_converter_r_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_r_2 ]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES.VALUE_SRC USER CONFIG.TUSER_BITS_PER_BYTE.VALUE_SRC USER CONFIG.HAS_TLAST.VALUE_SRC USER CONFIG.HAS_TKEEP.VALUE_SRC USER] [get_bd_cells axis_dwidth_converter_r_2]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {16} CONFIG.M_TDATA_NUM_BYTES {64} CONFIG.TUSER_BITS_PER_BYTE {1} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.HAS_MI_TKEEP {1}] [get_bd_cells axis_dwidth_converter_r_2]

  set axis_dwidth_converter_r_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 axis_dwidth_converter_r_3 ]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES.VALUE_SRC USER CONFIG.TUSER_BITS_PER_BYTE.VALUE_SRC USER CONFIG.HAS_TLAST.VALUE_SRC USER CONFIG.HAS_TKEEP.VALUE_SRC USER] [get_bd_cells axis_dwidth_converter_r_3]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {16} CONFIG.M_TDATA_NUM_BYTES {64} CONFIG.TUSER_BITS_PER_BYTE {1} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.HAS_MI_TKEEP {1}] [get_bd_cells axis_dwidth_converter_r_3]

  # constant for ready signal
  set const_vcc [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_vcc ]
  set_property -dict [list CONFIG.CONST_WIDTH {1} \
        CONFIG.CONST_VAL {0x1} ] $const_vcc

  set const_zero [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_zero ]
  set_property -dict [list CONFIG.CONST_WIDTH {2} \
        CONFIG.CONST_VAL {0x0} ] $const_zero

  set const_one [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_one ]
  set_property -dict [list CONFIG.CONST_WIDTH {2} \
        CONFIG.CONST_VAL {0x1} ] $const_one

  set const_two [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_two ]
  set_property -dict [list CONFIG.CONST_WIDTH {2} \
        CONFIG.CONST_VAL {0x2} ] $const_two

  set const_three [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_three ]
  set_property -dict [list CONFIG.CONST_WIDTH {2} \
        CONFIG.CONST_VAL {0x3} ] $const_three

  # Create instance: IBUFDS_GTE for PCIe RP #1 reference clock
  set pcie_rc_ref_clk_buf_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf:2.1 pcie_rc_ref_clk_buf_1 ]
  set_property CONFIG.C_BUF_TYPE {IBUFDSGTE} $pcie_rc_ref_clk_buf_1

  # Create instance: IBUFDS_GTE for PCIe RP #2 reference clock
  set pcie_rc_ref_clk_buf_2 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf:2.1 pcie_rc_ref_clk_buf_2 ]
  set_property CONFIG.C_BUF_TYPE {IBUFDSGTE} $pcie_rc_ref_clk_buf_2

  # Create instance: IBUFDS_GTE for PCIe RP #3 reference clock
  set pcie_rc_ref_clk_buf_3 [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf:2.1 pcie_rc_ref_clk_buf_3 ]
  set_property CONFIG.C_BUF_TYPE {IBUFDSGTE} $pcie_rc_ref_clk_buf_3

  # Create instance: IBUFDS_GTE for PCIe EP reference clock
  set pcie_ep_ref_clk_buf [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf:2.1 pcie_ep_ref_clk_buf ]
  set_property CONFIG.C_BUF_TYPE {IBUFDSGTE} $pcie_ep_ref_clk_buf

  # Create instance: IBUFDS and BUFG for DDR4 MIG reference clock
#   set ddr4_mig_sys_clk_ibuf [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf:2.1 ddr4_mig_sys_clk_ibuf ]
#   set_property CONFIG.C_BUF_TYPE {IBUFDS} $ddr4_mig_sys_clk_ibuf

#   set ddr4_mig_sys_clk_bufg [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_ds_buf:2.1 ddr4_mig_sys_clk_bufg ]
#   set_property CONFIG.C_BUF_TYPE {BUFG} $ddr4_mig_sys_clk_bufg

  
      create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_pfch_tag_0
      set_property -dict [list CONFIG.C_ALL_INPUTS {1}] [get_bd_cells axi_gpio_pfch_tag_0]
      set_property -dict [list CONFIG.C_IS_DUAL {1} \
      CONFIG.C_ALL_INPUTS {0} \
      CONFIG.C_ALL_INPUTS_2 {0} \
      CONFIG.C_ALL_OUTPUTS {1} \
      CONFIG.C_ALL_OUTPUTS_2 {1} ] [get_bd_cells axi_gpio_pfch_tag_0]

      create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_pfch_tag_1
      set_property -dict [list CONFIG.C_ALL_INPUTS {1}] [get_bd_cells axi_gpio_pfch_tag_1]
      set_property -dict [list CONFIG.C_IS_DUAL {1} \
            CONFIG.C_ALL_INPUTS {0} \
            CONFIG.C_ALL_INPUTS_2 {0} \
            CONFIG.C_ALL_OUTPUTS {1} \
            CONFIG.C_ALL_OUTPUTS_2 {1} ] [get_bd_cells axi_gpio_pfch_tag_1]

      create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_pfch_tag_2
      set_property -dict [list CONFIG.C_ALL_INPUTS {1}] [get_bd_cells axi_gpio_pfch_tag_2]
      set_property -dict [list CONFIG.C_IS_DUAL {1} \
            CONFIG.C_ALL_INPUTS {0} \
            CONFIG.C_ALL_INPUTS_2 {0} \
            CONFIG.C_ALL_OUTPUTS {1} \
            CONFIG.C_ALL_OUTPUTS_2 {1} ] [get_bd_cells axi_gpio_pfch_tag_2]

      create_bd_cell -type ip -vlnv xilinx.com:ip:axi_gpio:2.0 axi_gpio_pfch_tag_3
      set_property -dict [list CONFIG.C_ALL_INPUTS {1}] [get_bd_cells axi_gpio_pfch_tag_3]
      set_property -dict [list CONFIG.C_IS_DUAL {1} \
            CONFIG.C_ALL_INPUTS {0} \
            CONFIG.C_ALL_INPUTS_2 {0} \
            CONFIG.C_ALL_OUTPUTS {1} \
            CONFIG.C_ALL_OUTPUTS_2 {1} ] [get_bd_cells axi_gpio_pfch_tag_3]


  set block_name qdma_h2c_byp_ctrl 
  set block_cell_name u_qdma_h2c_byp_ctrl
  if { [catch {set u_qdma_h2c_byp_ctrl [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_qdma_h2c_byp_ctrl eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name ar_to_bd_pktizer 
  set block_cell_name u_ar_to_bd_pktizer
  if { [catch {set u_ar_to_bd_pktizer [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_ar_to_bd_pktizer eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name ar_to_bd_pktizer 
  set block_cell_name u_aw_to_bd_pktizer_0
  if { [catch {set u_aw_to_bd_pktizer_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_aw_to_bd_pktizer_0 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

#   set block_name ar_to_bd_pktizer 
#   set block_cell_name u_aw_to_bd_pktizer_1
#   if { [catch {set u_aw_to_bd_pktizer_1 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
#         catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
#         return 1
#   } elseif { $u_aw_to_bd_pktizer_1 eq "" } {
#         catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
#         return 1
#   }

#   set block_name ar_to_bd_pktizer 
#   set block_cell_name u_aw_to_bd_pktizer_2
#   if { [catch {set u_aw_to_bd_pktizer_2 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
#         catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
#         return 1
#   } elseif { $u_aw_to_bd_pktizer_2 eq "" } {
#         catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
#         return 1
#   }

#   set block_name ar_to_bd_pktizer 
#   set block_cell_name u_aw_to_bd_pktizer_3
#   if { [catch {set u_aw_to_bd_pktizer_3 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
#         catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
#         return 1
#   } elseif { $u_aw_to_bd_pktizer_3 eq "" } {
#         catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
#         return 1
#   }

  # axis ic for multi drive on h2c data path
  #set axis_ic_h2c_multi_drive [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_ic_h2c_multi_drive ]
  #set_property -dict [ list CONFIG.NUM_SI {1} \
  #      CONFIG.NUM_MI {4} \
  #      CONFIG.ENABLE_ADVANCED_OPTIONS {1} \
  #      CONFIG.XBAR_TDATA_NUM_BYTES.VALUE_SRC {USER} \
  #      CONFIG.XBAR_TDATA_NUM_BYTES {64} \
  #      CONFIG.M00_FIFO_MODE {1} \
  #      CONFIG.M00_FIFO_DEPTH {4096} \
  #      CONFIG.M01_FIFO_MODE {1} \
  #      CONFIG.M01_FIFO_DEPTH {4096} \
  #      CONFIG.M02_FIFO_MODE {1} \
  #      CONFIG.M02_FIFO_DEPTH {4096} \
  #      CONFIG.M03_FIFO_MODE {1} \
  #      CONFIG.M03_FIFO_DEPTH {4096} ] $axis_ic_h2c_multi_drive

  #if { ${mpsoc_bd_val::r_bridge_en} == "1" } {
  #    set_property -dict [ list CONFIG.M00_AXIS_BASETDEST {0} \
  #          CONFIG.M00_AXIS_HIGHTDEST {0x000000FF} ] $axis_ic_h2c_multi_drive
  #} else {
  #    set_property -dict [ list CONFIG.M00_AXIS_BASETDEST {0} \
  #          CONFIG.M00_AXIS_HIGHTDEST {0x00000000} \
  #          CONFIG.M01_AXIS_BASETDEST {0x00000001} \
  #          CONFIG.M01_AXIS_HIGHTDEST {0x00000001} \
  #          CONFIG.M02_AXIS_BASETDEST {0x00000002} \
  #          CONFIG.M02_AXIS_HIGHTDEST {0x00000002} \
  #          CONFIG.M03_AXIS_BASETDEST {0x00000003} \
  #          CONFIG.M03_AXIS_HIGHTDEST {0x00000003} ] $axis_ic_h2c_multi_drive
  #}


  set block_name axis_tdest_width_converter 
  set block_cell_name u_axis_tdest_width_converter
  if { [catch {set u_axis_tdest_width_converter [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_axis_tdest_width_converter eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }


  set block_name accframework
  set block_cell_name u_accframework
  if { [catch {set u_accframework [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
       catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_accframework eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }  
  create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 xlconstant_0

  set block_cell_name axis_ic_split_nvmeq_and_compute
  create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 $block_cell_name
  set_property -dict [list CONFIG.M01_AXIS_BASETDEST {0x00000004} CONFIG.M00_AXIS_HIGHTDEST {0x00000003} CONFIG.M01_AXIS_HIGHTDEST {0x00000007} CONFIG.S00_HAS_REGSLICE {0} ] [get_bd_cells $block_cell_name]
  set_property -dict [list CONFIG.M00_HAS_REGSLICE {1} CONFIG.M00_FIFO_DEPTH {64}] [get_bd_cells axis_ic_split_nvmeq_and_compute]

  set block_cell_name axis_ic_merge_nvmeq_and_compute
  create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 $block_cell_name
  set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {1} CONFIG.M00_AXIS_HIGHTDEST {0x000000FF}] [get_bd_cells $block_cell_name]
  set_property -dict [list CONFIG.S01_HAS_REGSLICE {1} CONFIG.S01_FIFO_DEPTH {64}] [get_bd_cells axis_ic_merge_nvmeq_and_compute]
  
  create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 axis_h2c_data_fifo
  set_property -dict [list CONFIG.FIFO_DEPTH {256} CONFIG.IS_ACLK_ASYNC {1}] [get_bd_cells axis_h2c_data_fifo]
  

  set block_name xdma_rp_axi_bridge
  set block_cell_name u_xdma_rp_axi_bridge_0
  if { [catch {set u_xdma_rp_axi_bridge_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_xdma_rp_axi_bridge_0 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name xdma_rp_axi_bridge
  set block_cell_name u_xdma_rp_axi_bridge_1
  if { [catch {set u_xdma_rp_axi_bridge_1 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_xdma_rp_axi_bridge_1 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name xdma_rp_axi_bridge
  set block_cell_name u_xdma_rp_axi_bridge_2
  if { [catch {set u_xdma_rp_axi_bridge_2 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_xdma_rp_axi_bridge_2 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name xdma_rp_axi_bridge
  set block_cell_name u_xdma_rp_axi_bridge_3
  if { [catch {set u_xdma_rp_axi_bridge_3 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_xdma_rp_axi_bridge_3 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name axis_w_merger
  set block_cell_name u_axis_w_merger_0
  if { [catch {set u_axis_w_merger_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_axis_w_merger_0 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name axis_w_merger
  set block_cell_name u_axis_w_merger_1
  if { [catch {set u_axis_w_merger_1 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_axis_w_merger_1 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name axis_w_merger
  set block_cell_name u_axis_w_merger_2
  if { [catch {set u_axis_w_merger_2 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_axis_w_merger_2 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name axis_w_merger
  set block_cell_name u_axis_w_merger_3
  if { [catch {set u_axis_w_merger_3 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_axis_w_merger_3 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name axis_aw_w_splitter
  set block_cell_name u_axis_aw_w_splitter
  if { [catch {set u_axis_aw_w_splitter [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_axis_aw_w_splitter eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name qdma_c2h_byp_ctrl
  set block_cell_name u_qdma_c2h_byp_ctrl
  if { [catch {set u_qdma_c2h_byp_ctrl [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_qdma_c2h_byp_ctrl eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name w_data_connector
  set block_cell_name u_w_data_connector_0
  if { [catch {set u_w_data_connector_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_w_data_connector_0 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }
  
  
  
  
  

  set block_name axis_route_r_handler
  set block_cell_name u_axis_route_r_handler
  if { [catch {set u_axis_route_r_handler [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_axis_route_r_handler eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name fifo
  set block_cell_name u_len_fifo_0
  if { [catch {set u_len_fifo_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_len_fifo_0 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }
  
  set block_name fifo
  set block_cell_name u_len_fifo_1
  if { [catch {set u_len_fifo_1 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_len_fifo_1 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }
  
  set block_name fifo
  set block_cell_name u_len_fifo_2
  if { [catch {set u_len_fifo_2 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_len_fifo_2 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }
 
  set block_name fifo
  set block_cell_name u_len_fifo_3
  if { [catch {set u_len_fifo_3 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_len_fifo_3 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name fifo
  set block_cell_name u_wid_fifo_0
  if { [catch {set u_wid_fifo_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_wid_fifo_0 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name fifo
  set block_cell_name u_wid_fifo_1
  if { [catch {set u_wid_fifo_1 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_wid_fifo_1 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name fifo
  set block_cell_name u_wid_fifo_2
  if { [catch {set u_wid_fifo_2 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_wid_fifo_2 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name fifo
  set block_cell_name u_wid_fifo_3
  if { [catch {set u_wid_fifo_3 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_wid_fifo_3 eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name fifo
  set block_cell_name u_crdt_fifo
  if { [catch {set u_crdt_fifo [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_crdt_fifo eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name fifo
  set block_cell_name u_cmpt_fifo
  if { [catch {set u_cmpt_fifo [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_cmpt_fifo eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

  set block_name credit_manager
  set block_cell_name u_credit_manager
  if { [catch {set u_credit_manager [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
        catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  } elseif { $u_credit_manager eq "" } {
        catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
        return 1
  }

 

  set block_name axis_req_cnt
  set block_cell_name u_axis_req_in_cnt
  if { [catch {set u_axis_req_in_cnt [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $u_axis_req_in_cnt eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
   
  set axis_ic_w [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_ic_w ]
  set_property -dict [list CONFIG.ARB_ON_TLAST {1} \
      CONFIG.ARB_ON_MAX_XFERS {0} \
      CONFIG.NUM_SI {4} \
      CONFIG.NUM_MI {1} \
      CONFIG.S00_FIFO_MODE {1} \
      CONFIG.S01_FIFO_MODE {1} \
      CONFIG.S02_FIFO_MODE {1} \
      CONFIG.S03_FIFO_MODE {1} \
      CONFIG.M00_FIFO_MODE {1} \
      CONFIG.S00_FIFO_DEPTH {512} \
      CONFIG.S01_FIFO_DEPTH {512} \
      CONFIG.S02_FIFO_DEPTH {512} \
      CONFIG.S03_FIFO_DEPTH {512} \
      CONFIG.M00_FIFO_DEPTH {512} \
      CONFIG.M00_AXIS_HIGHTDEST {0x000000FF} ] $axis_ic_w

  set axis_ic_h2c_req [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_ic_h2c_req ]
  set_property -dict [ list CONFIG.NUM_SI {4} \
        CONFIG.NUM_MI {1} \
        CONFIG.ENABLE_ADVANCED_OPTIONS {1} \
        CONFIG.M00_AXIS_HIGHTDEST {0x000000FF} \
        CONFIG.M00_FIFO_MODE {0} \
        CONFIG.M00_FIFO_DEPTH {64} \
        CONFIG.S00_FIFO_MODE {0} \
        CONFIG.S00_FIFO_DEPTH {64} \
        CONFIG.S01_FIFO_MODE {0} \
        CONFIG.S01_FIFO_DEPTH {64} \
        CONFIG.S02_FIFO_MODE {0} \
        CONFIG.S02_FIFO_DEPTH {64} \
        CONFIG.S03_FIFO_MODE {0} \
        CONFIG.S03_FIFO_DEPTH {64} ] $axis_ic_h2c_req 
  

  set i 0
  while {$i < ${mpsoc_bd_val::nvme_qe_dma_num}} {
      set block_name axis_tid_as_tdest 
      set block_cell_name u_axis_tid_as_tdest_$i
      if { [catch {set u_axis_tid_as_tdest [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
            catch {common::send_msg_id "BD_TCL-105" "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
            return 1
      } elseif { $u_axis_tid_as_tdest eq "" } {
            catch {common::send_msg_id "BD_TCL-106" "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
            return 1
      }

      set_property CONFIG.FREQ_HZ 249997498 [get_bd_intf_pins u_axis_tid_as_tdest_${i}/s_axis]
      set_property CONFIG.FREQ_HZ 249997498 [get_bd_intf_pins u_axis_tid_as_tdest_${i}/m_axis]

      incr i 1
  }



# Create instance: axi_datamover_0, and set properties
  set axi_datamover_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_datamover:5.1 axi_datamover_0 ]
  set_property -dict [ list \
   CONFIG.c_addr_width {40} \
   CONFIG.c_dummy {1} \
   CONFIG.c_m_axi_mm2s_data_width {512} \
   CONFIG.c_m_axis_mm2s_tdata_width {512} \
   CONFIG.c_mm2s_burst_size {64} \
   CONFIG.c_mm2s_btt_used {23} \
   CONFIG.c_m_axi_s2mm_data_width {512} \
   CONFIG.c_m_axis_s2mm_tdata_width {512} \
   CONFIG.c_s2mm_burst_size {64} \
   CONFIG.c_s2mm_btt_used {23} \
   CONFIG.c_s2mm_support_indet_btt {true} \
 ] $axi_datamover_0
    

   create_bd_cell -type module -reference axis_tid_as_tdest u_datapath_to_acc_bridge
   set_property CONFIG.FREQ_HZ 249997498 [get_bd_intf_pins u_datapath_to_acc_bridge/s_axis]
   set_property CONFIG.FREQ_HZ 249997498 [get_bd_intf_pins u_datapath_to_acc_bridge/m_axis]
  


#   set_property CONFIG.FREQ_HZ 249997498 [get_bd_intf_pins u_xdma_rp_axi_bridge_0/s_axis_h2c]
#   set_property CONFIG.FREQ_HZ 249997498 [get_bd_intf_pins u_xdma_rp_axi_bridge_0/m_axis_ar_req]

#==================================================
# system ila
#==================================================

#=============================================
# Clock ports
#=============================================

  # gt differential reference clock for pcie rp #1
  set pcie_rc_gt_ref_clk_1 [ create_bd_intf_port -mode slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 pcie_rc_gt_ref_clk_1 ]
  set_property -dict [ list config.freq_hz {100000000} ] $pcie_rc_gt_ref_clk_1

  # gt differential reference clock for pcie rp #2
  set pcie_rc_gt_ref_clk_2 [ create_bd_intf_port -mode slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 pcie_rc_gt_ref_clk_2 ]
  set_property -dict [ list config.freq_hz {100000000} ] $pcie_rc_gt_ref_clk_2

  # gt differential reference clock for pcie rp #3
  set pcie_rc_gt_ref_clk_3 [ create_bd_intf_port -mode slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 pcie_rc_gt_ref_clk_3 ]
  set_property -dict [ list config.freq_hz {100000000} ] $pcie_rc_gt_ref_clk_3

  # gt differential reference clock for pcie ep
  set pcie_ep_gt_ref_clk [ create_bd_intf_port -mode slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 pcie_ep_gt_ref_clk ]
  set_property -dict [ list config.freq_hz {100000000} ] $pcie_ep_gt_ref_clk

  # Differential system clock for DDR4 MIG
#   set ddr4_mig_sys_clk [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:diff_clock_rtl:1.0 ddr4_mig_sys_clk ]
#   set_property -dict [ list CONFIG.FREQ_HZ {100000000} ] $ddr4_mig_sys_clk

#=============================================
# Reset ports
#=============================================

  # PCIe RC perst
  create_bd_port -dir O -type rst pcie_rc_perstn_1
  create_bd_port -dir O -type rst pcie_rc_perstn_2
  create_bd_port -dir O -type rst pcie_rc_perstn_3

  # PCIe EP perst
  create_bd_port -dir I -type rst pcie_ep_perstn


  # Create instance: PCIe RP 1 AXI sync. reset generation in pl_clk1 (250MHz) domain
  create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 pcie_rp_1_sync_reset

  # Create instance: PCIe RP 2 AXI sync. reset generation in pl_clk1 (250MHz) domain
  create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 pcie_rp_2_sync_reset

  # Create instance: PCIe RP 3 AXI sync. reset generation in pl_clk1 (250MHz) domain
  create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 pcie_rp_3_sync_reset

  # Create instance: PCIe RC AXI sync. reset generation in pl_clk1 (250MHz) domain
  create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 pcie_rc_sync_reset

  # Create instance: PL DDR MIG AXI sync. reset generation
#   create_bd_cell -type ip -vlnv xilinx.com:ip:proc_sys_reset:5.0 ddr4_mig_sync_reset

#=============================================
# GT ports
#=============================================

  # PCIe RP Slots

  create_bd_port -dir I -from 3 -to 0 pcie_rp_rxn_1
  create_bd_port -dir I -from 3 -to 0 pcie_rp_rxp_1
  create_bd_port -dir O -from 3 -to 0 pcie_rp_txn_1
  create_bd_port -dir O -from 3 -to 0 pcie_rp_txp_1

  create_bd_port -dir I -from 3 -to 0 pcie_rp_rxn_2
  create_bd_port -dir I -from 3 -to 0 pcie_rp_rxp_2
  create_bd_port -dir O -from 3 -to 0 pcie_rp_txn_2
  create_bd_port -dir O -from 3 -to 0 pcie_rp_txp_2

  create_bd_port -dir I -from 3 -to 0 pcie_rp_rxn_3
  create_bd_port -dir I -from 3 -to 0 pcie_rp_rxp_3
  create_bd_port -dir O -from 3 -to 0 pcie_rp_txn_3
  create_bd_port -dir O -from 3 -to 0 pcie_rp_txp_3

  # PCIe EP Slot
  create_bd_port -dir I -from 15 -to 0 pcie_ep_rxn
  create_bd_port -dir I -from 15 -to 0 pcie_ep_rxp
  create_bd_port -dir O -from 15 -to 0 pcie_ep_txn
  create_bd_port -dir O -from 15 -to 0 pcie_ep_txp

#=============================================
# DDR4 pins
#=============================================
  
  create_bd_intf_port -mode Master -vlnv xilinx.com:interface:ddr4_rtl:1.0 c0_ddr4

#=============================================
# MISC ports
#=============================================

  #create_bd_port -dir O ddr4_mig_calib_done

#=============================================
# System clock connection
#=============================================


  # PCIe RP #1 reference clock (100MHz)
  connect_bd_intf_net -intf_net pcie_rc_gt_ref_clk_1 \
      [get_bd_intf_pins pcie_rc_gt_ref_clk_1] \
      [get_bd_intf_pins pcie_rc_ref_clk_buf_1/CLK_IN_D]

  connect_bd_net -net pcie_rc_ref_clk_1 \
      [get_bd_pins pcie_rc_ref_clk_buf_1/IBUF_DS_ODIV2] \
      [get_bd_pins xdma_rp_1/sys_clk]

  connect_bd_net -net pcie_rc_sys_clk_1 \
      [get_bd_pins pcie_rc_ref_clk_buf_1/IBUF_OUT] \
      [get_bd_pins xdma_rp_1/sys_clk_gt]

  # PCIe RP #2 reference clock (100MHz)
  connect_bd_intf_net -intf_net pcie_rc_gt_ref_clk_2 \
      [get_bd_intf_pins pcie_rc_gt_ref_clk_2] \
      [get_bd_intf_pins pcie_rc_ref_clk_buf_2/CLK_IN_D]

  connect_bd_net -net pcie_rc_ref_clk_2 \
      [get_bd_pins pcie_rc_ref_clk_buf_2/IBUF_DS_ODIV2] \
      [get_bd_pins xdma_rp_2/sys_clk]

  connect_bd_net -net pcie_rc_sys_clk_2 \
      [get_bd_pins pcie_rc_ref_clk_buf_2/IBUF_OUT] \
      [get_bd_pins xdma_rp_2/sys_clk_gt]

  # PCIe RP #3 reference clock (100MHz)
  connect_bd_intf_net -intf_net pcie_rc_gt_ref_clk_3 \
      [get_bd_intf_pins pcie_rc_gt_ref_clk_3] \
      [get_bd_intf_pins pcie_rc_ref_clk_buf_3/CLK_IN_D]

  connect_bd_net -net pcie_rc_ref_clk_3 \
      [get_bd_pins pcie_rc_ref_clk_buf_3/IBUF_DS_ODIV2] \
      [get_bd_pins xdma_rp_3/sys_clk]

  connect_bd_net -net pcie_rc_sys_clk_3 \
      [get_bd_pins pcie_rc_ref_clk_buf_3/IBUF_OUT] \
      [get_bd_pins xdma_rp_3/sys_clk_gt]

  # PCIe EP reference clock (100MHz)
  connect_bd_intf_net -intf_net pcie_ep_gt_ref_clk \
      [get_bd_intf_pins pcie_ep_gt_ref_clk] \
      [get_bd_intf_pins pcie_ep_ref_clk_buf/CLK_IN_D]

  connect_bd_net -net pcie_ep_sys_clk \
      [get_bd_pins pcie_ep_ref_clk_buf/IBUF_DS_ODIV2] \
      [get_bd_pins u_qdma_ep/sys_clk]

  connect_bd_net -net pcie_ep_sys_clk_gt \
      [get_bd_pins pcie_ep_ref_clk_buf/IBUF_OUT] \
      [get_bd_pins u_qdma_ep/sys_clk_gt]

  # DDR4 memory controller reference clock (100MHz)
#   connect_bd_intf_net -intf_net ddr4_mig_sys_clk_in \
#       [get_bd_intf_pins ddr4_mig_sys_clk] \
#       [get_bd_intf_pins ddr4_mig_sys_clk_ibuf/CLK_IN_D]

#   connect_bd_net -net ddr4_mig_sys_clk_ibuf_out \
#       [get_bd_pins ddr4_mig_sys_clk_ibuf/IBUF_OUT] \
#       [get_bd_pins ddr4_mig_sys_clk_bufg/BUFG_I]

#   connect_bd_net -net ddr4_mig_sys_clk_bufg_out \
#       [get_bd_pins ddr4_mig_sys_clk_bufg/BUFG_O] \
#       [get_bd_pins ddr4_mig/C0_sys_clk_i]


  # PCIe RP #1 AXI clock (250MHz)
  connect_bd_net -net pcie_axi_clk1 [get_bd_pins xdma_rp_1/axi_aclk] \
      [get_bd_pins axi_ic_pcie_rp_1_dma/ACLK] \
      [get_bd_pins axi_ic_pcie_rp_1_dma/S*ACLK] \
      [get_bd_pins axi_ic_pcie_rc_bar/M01_ACLK] \
      [get_bd_pins axi_ic_pcie_rc_mmio/M01_ACLK] \
      [get_bd_pins axi_ic_pcie_rc_bar/M00_ACLK] \
      [get_bd_pins axi_ic_pcie_rc_mmio/M00_ACLK] 
      # [get_bd_pins axi_ic_pcie_rp_1_dma/M00_ACLK] \

  # PCIe RP #2 AXI clock (250MHz)
  connect_bd_net -net pcie_axi_clk2 [get_bd_pins xdma_rp_2/axi_aclk] \
      [get_bd_pins axi_ic_pcie_rp_2_dma/ACLK] \
      [get_bd_pins axi_ic_pcie_rp_2_dma/S*ACLK] \
      [get_bd_pins axi_ic_pcie_rc_bar/M02_ACLK] \
      [get_bd_pins axi_ic_pcie_rc_mmio/M02_ACLK]

  # PCIe RP #3 AXI clock (250MHz)
  connect_bd_net -net pcie_axi_clk3 [get_bd_pins xdma_rp_3/axi_aclk] \
      [get_bd_pins axi_ic_pcie_rp_3_dma/ACLK] \
      [get_bd_pins axi_ic_pcie_rp_3_dma/S*ACLK] \
      [get_bd_pins axi_ic_pcie_rc_bar/M03_ACLK] \
      [get_bd_pins axi_ic_pcie_rc_mmio/M03_ACLK]

  # DDR4 controller ui clock (200MHz) for AXI IC and AXI interface
#   connect_bd_net [get_bd_pins ddr4_mig/c0_ddr4_ui_clk] \
#       [get_bd_pins axi_ic_mcdma_mmio/M00_ACLK] \
#       [get_bd_pins ddr4_mig_sync_reset/slowest_sync_clk]

  # MPSoC pl_clk1 (250MHz) for PCIe RC S_AXI and M_AXI related interfaces
  connect_bd_net -net pl_clk1_out [get_bd_pins zynq_mpsoc/pl_clk1] \
      [get_bd_pins pcie_rp_1_sync_reset/slowest_sync_clk] \
      [get_bd_pins pcie_rp_2_sync_reset/slowest_sync_clk] \
      [get_bd_pins pcie_rp_3_sync_reset/slowest_sync_clk] \
      [get_bd_pins pcie_rc_sync_reset/slowest_sync_clk] 

  connect_bd_net -net pl_clk1_out \
      [get_bd_pins axi_ic_pcie_rc_dma/ACLK] \
      [get_bd_pins axi_ic_pcie_rc_dma/M00_ACLK] \
      [get_bd_pins axi_ic_pcie_rc_bar/ACLK] \
      [get_bd_pins axi_ic_pcie_rc_bar/S00_ACLK] \
      [get_bd_pins axi_ic_pcie_rc_mmio/ACLK] \
      [get_bd_pins axi_ic_pcie_rc_mmio/S00_ACLK] \
      [get_bd_pins axi_ic_nvme_qe_dma/ACLK] \
      [get_bd_pins axi_ic_nvme_qe_dma/S*_ACLK] \
      [get_bd_pins axi_ic_nvme_qe_dma/M00_ACLK] \
      [get_bd_pins axi_ic_mcdma_mmio/ACLK] \
      [get_bd_pins axi_ic_mcdma_mmio/S00_ACLK] \
	[get_bd_pins axi_ic_mcdma_mmio/M0*_ACLK] \
	[get_bd_pins axi_ic_mcdma_mmio/M10_ACLK] \
	[get_bd_pins axi_ic_mcdma_mmio/M11_ACLK] \
      [get_bd_pins axi_ic_nvme_qe_dma_mmio/*ACLK] \
      [get_bd_pins axi_nvme_qe_dma_*/s_axi_lite_aclk] \
      [get_bd_pins axi_nvme_qe_dma_*/m_axi_sg_aclk] \
      [get_bd_pins axi_nvme_qe_dma_*/m_axi_mm2s_aclk] \
      [get_bd_pins axi_nvme_qe_dma_*/m_axi_s2mm_aclk] \
      [get_bd_pins axi_gpio_*/s_axi_aclk] \
      [get_bd_pins axi_nvme_qe_dma_intc_*/s_axi_aclk] \
      [get_bd_pins axi_ic_mcdma/ACLK] \
      [get_bd_pins axi_ic_mcdma/M*_ACLK] \
      [get_bd_pins axi_ic_mcdma/S00_ACLK] \
      [get_bd_pins axi_ic_mcdma/S02_ACLK] \
      [get_bd_pins axi_ic_mcdma/S03_ACLK] \
      [get_bd_pins axi_mcdma_intr/s_axi_aclk] \
      [get_bd_pins zynq_mpsoc/maxihpm0_fpd_aclk] \
      [get_bd_pins zynq_mpsoc/maxihpm1_fpd_aclk] \
      [get_bd_pins zynq_mpsoc/maxihpm0_lpd_aclk] \
      [get_bd_pins zynq_mpsoc/saxihpc0_fpd_aclk] \
      [get_bd_pins zynq_mpsoc/saxihpc1_fpd_aclk] \
      [get_bd_pins zynq_mpsoc/saxihp0_fpd_aclk] \
      [get_bd_pins zynq_mpsoc/saxihp1_fpd_aclk] \
      [get_bd_pins zynq_mpsoc/saxihp2_fpd_aclk] \
      [get_bd_pins zynq_mpsoc/saxihp3_fpd_aclk] \
      [get_bd_pins axis_ic_qdma_h2c/ACLK] \
      [get_bd_pins axis_ic_qdma_h2c/M00_AXIS_ACLK] \
      [get_bd_pins axis_ic_qdma_c2h/ACLK] \
      [get_bd_pins axis_ic_qdma_c2h/S*_ACLK] \
      [get_bd_pins axi_ic_pcie_rp_*_dma/M00_ACLK] \
      [get_bd_pins axi_ic_pcie_rc_dma/S*_ACLK] \
      [get_bd_pins axis_ic_qdma_c2h/M01_AXIS_ACLK] \
      [get_bd_pins axis_ic_split_nvmeq_and_compute/*ACLK] \
      [get_bd_pins axis_ic_merge_nvmeq_and_compute/*ACLK] \
      [get_bd_pins u_accframework/clk] \
      [get_bd_pins axi_datamover_0/m_*_aclk] \
      [get_bd_pins axi_datamover_0/m_axis_s2mm_cmdsts_awclk] \
      [get_bd_pins axis_h2c_data_fifo/s_axis_aclk] \
      [get_bd_pins axi_ic_mcdma/S01_ACLK] 

  # PCIe EP BAR to PCIe RP BARs interfaces
  connect_bd_net [get_bd_pins u_qdma_ep/axi_aclk] \
      [get_bd_pins axi_ic_nvme_qe_dma/M01_ACLK] \
      [get_bd_pins u_qdma_ep_axis_wrapper/axi_aclk] \
      [get_bd_pins u_qdma_h2c_byp_ctrl/axi_aclk] \
      [get_bd_pins u_qdma_c2h_byp_ctrl/axi_aclk] \
      [get_bd_pins axi_ic_pcie_rc_bar/S01_ACLK] \
      [get_bd_pins axis_ic_qdma_h2c/S00_AXIS_ACLK] \
      [get_bd_pins axis_ic_qdma_h2c/M01_AXIS_ACLK] \
      [get_bd_pins axis_ic_qdma_c2h/M00_AXIS_ACLK] \
      [get_bd_pins axis_ic_qdma_c2h_data/ACLK] \
      [get_bd_pins axis_ic_qdma_c2h_data/M*_AXIS_ACLK] \
      [get_bd_pins axis_ic_qdma_c2h_data/S*_AXIS_ACLK] \
      [get_bd_pins axis_ic_qdma_h2c_byp_in/ACLK] \
      [get_bd_pins axis_ic_qdma_h2c_byp_in/M*_AXIS_ACLK] \
      [get_bd_pins axis_ic_qdma_h2c_byp_in/S*_AXIS_ACLK] \
      [get_bd_pins u_xdma_rp_axi_bridge_*/s_axib_aclk] \
      [get_bd_pins u_axis_w_merger_*/axi_aclk] \
      [get_bd_pins u_axis_aw_w_splitter/aclk] \
      [get_bd_pins axi_ic_pcie_rp_*_dma/M01_ACLK] \
      [get_bd_pins axis_dwidth_converter_w_0/aclk] \
      [get_bd_pins axis_dwidth_converter_w_1/aclk] \
      [get_bd_pins axis_dwidth_converter_w_2/aclk] \
      [get_bd_pins axis_dwidth_converter_w_3/aclk] \
      [get_bd_pins axis_dwidth_converter_r_*/aclk] \
      [get_bd_pins u_len_fifo_*/clk] \
      [get_bd_pins u_wid_fifo_*/clk] \
      [get_bd_pins u_crdt_fifo/clk] \
      [get_bd_pins u_cmpt_fifo/clk] \
      [get_bd_pins u_w_data_connector_0/aclk] \
      [get_bd_pins u_axis_route_r_handler/aclk] \
      [get_bd_pins axis_ic_w/aclk] \
      [get_bd_pins axis_ic_w/S*_AXIS_ACLK] \
      [get_bd_pins axis_ic_w/M*_AXIS_ACLK] \
      [get_bd_pins axis_ic_h2c_req/aclk] \
      [get_bd_pins axis_ic_h2c_req/S*_AXIS_aclk] \
      [get_bd_pins axis_ic_h2c_req/M*_AXIS_aclk] \
      [get_bd_pins u_credit_manager/axi_aclk] \
      [get_bd_pins axis_ic_h2c_multi_r_out/S*_AXIS_ACLK] \
      [get_bd_pins axis_ic_h2c_multi_r_out/M*_AXIS_ACLK] \
      [get_bd_pins axis_h2c_data_fifo/m_axis_aclk] \
      [get_bd_pins u_axis_req_in_cnt/aclk] \
      [get_bd_pins axis_ic_h2c_multi_r_out/ACLK] 

if { ${mpsoc_bd_val::ar_bridge_en} == "1" } {
      connect_bd_net [get_bd_pins u_qdma_ep/axi_aclk] [get_bd_pins axis_ic_qdma_c2h/M02_AXIS_ACLK]
}

#=============================================
# System reset connection
#=============================================

  connect_bd_net [get_bd_pins const_vcc/dout] \
      [get_bd_pins pcie_rp_1_sync_reset/dcm_locked] \
      [get_bd_pins pcie_rp_2_sync_reset/dcm_locked] \
      [get_bd_pins pcie_rp_3_sync_reset/dcm_locked]

  # System reset (active low) for AXI PCIe RC bridge and system reset firmware
  connect_bd_net -net pl_resetn0 [get_bd_pins zynq_mpsoc/pl_resetn0] \
      [get_bd_pins xdma_rp_1/sys_rst_n] \
      [get_bd_pins xdma_rp_2/sys_rst_n] \
      [get_bd_pins xdma_rp_3/sys_rst_n] \
      [get_bd_pins pcie_rc_sync_reset/ext_reset_in]
      # [get_bd_pins ddr4_mig_sync_reset/ext_reset_in]

  # Reset for AXI interface of PCIe RP #0
  ## AXI interface reset
  ## PHY ready signal

  # Reset for AXI interface of PCIe RP #1
  ## AXI interface reset
  connect_bd_net -net pcie_rp_1_axi_aresetn [get_bd_pins xdma_rp_1/axi_aresetn] \
          [get_bd_pins axi_ic_pcie_rp_1_dma/ARESETN] \
          [get_bd_pins axi_ic_pcie_rp_1_dma/S*ARESETN] \
          [get_bd_pins axi_ic_pcie_rc_bar/M01_ARESETN]

  ## PHY ready signal
  connect_bd_net -net pcie_rp_1_axi_ctl_aresetn [get_bd_pins xdma_rp_1/axi_ctl_aresetn] \
          [get_bd_pins axi_ic_pcie_rc_mmio/M01_ARESETN] \
          [get_bd_pins pcie_rp_1_sync_reset/ext_reset_in]
          

  # Reset for AXI interface of PCIe RP #2
  ## AXI interface reset
  connect_bd_net -net pcie_rp_2_axi_aresetn [get_bd_pins xdma_rp_2/axi_aresetn] \
          [get_bd_pins axi_ic_pcie_rp_2_dma/ARESETN] \
          [get_bd_pins axi_ic_pcie_rp_2_dma/S*ARESETN] \
          [get_bd_pins axi_ic_pcie_rc_bar/M02_ARESETN]

  ## PHY ready signal
  connect_bd_net -net pcie_rp_2_axi_ctl_aresetn [get_bd_pins xdma_rp_2/axi_ctl_aresetn] \
          [get_bd_pins axi_ic_pcie_rc_mmio/M02_ARESETN] \
          [get_bd_pins pcie_rp_2_sync_reset/ext_reset_in]

  # Reset for AXI interface of PCIe RP #3
  ## AXI interface reset
  connect_bd_net -net pcie_rp_3_axi_aresetn [get_bd_pins xdma_rp_3/axi_aresetn] \
          [get_bd_pins axi_ic_pcie_rp_3_dma/ARESETN] \
          [get_bd_pins axi_ic_pcie_rp_3_dma/S*ARESETN] \
          [get_bd_pins axi_ic_pcie_rc_bar/M03_ARESETN]

  ## PHY ready signal
  connect_bd_net -net pcie_rp_3_axi_ctl_aresetn [get_bd_pins xdma_rp_3/axi_ctl_aresetn] \
          [get_bd_pins axi_ic_pcie_rc_mmio/M03_ARESETN] \
          [get_bd_pins pcie_rp_3_sync_reset/ext_reset_in]

  # Reset sync. in ARM-side AXI interface of PCIe RP-related functions
  ## Create instance: pcie_rc_dcm_locked_gen 
  set pcie_rc_dcm_locked_gen [ create_bd_cell -type ip -vlnv xilinx.com:ip:util_reduced_logic:2.0 pcie_rc_dcm_locked_gen ]
  set_property -dict [list CONFIG.C_SIZE {4}] $pcie_rc_dcm_locked_gen

  set xlconcat_pcie_rp_perstn [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 xlconcat_pcie_rp_perstn ]
  set_property -dict [list CONFIG.NUM_PORTS {4}] $xlconcat_pcie_rp_perstn

  connect_bd_net [get_bd_pins xlconcat_pcie_rp_perstn/In0] \
        [get_bd_pins pcie_rp_1_sync_reset/peripheral_aresetn]

  connect_bd_net [get_bd_pins xlconcat_pcie_rp_perstn/In1] \
        [get_bd_ports pcie_rc_perstn_1] \
        [get_bd_pins pcie_rp_1_sync_reset/peripheral_aresetn]

  connect_bd_net [get_bd_pins xlconcat_pcie_rp_perstn/In2] \
        [get_bd_ports pcie_rc_perstn_2] \
        [get_bd_pins pcie_rp_2_sync_reset/peripheral_aresetn]

  connect_bd_net [get_bd_pins xlconcat_pcie_rp_perstn/In3] \
        [get_bd_ports pcie_rc_perstn_3] \
        [get_bd_pins pcie_rp_3_sync_reset/peripheral_aresetn]

  connect_bd_net [get_bd_pins pcie_rc_dcm_locked_gen/Op1] \
        [get_bd_pins xlconcat_pcie_rp_perstn/dout]

  connect_bd_net [get_bd_pins pcie_rc_dcm_locked_gen/Res] \
        [get_bd_pins pcie_rc_sync_reset/dcm_locked]

  connect_bd_net [get_bd_pins pcie_rc_sync_reset/peripheral_aresetn] \
        [get_bd_pins axi_ic_pcie_rc_dma/M00_ARESETN] \
        [get_bd_pins axi_ic_pcie_rc_bar/S00_ARESETN] \
        [get_bd_pins axi_ic_pcie_rc_mmio/S00_ARESETN] \
        [get_bd_pins axi_ic_mcdma/M*_ARESETN] \
        [get_bd_pins axi_ic_mcdma/S00_ARESETN] \
        [get_bd_pins axi_ic_mcdma/S02_ARESETN] \
        [get_bd_pins axi_ic_mcdma/S03_ARESETN] \
	[get_bd_pins axi_ic_nvme_qe_dma/S*_ARESETN] \
      [get_bd_pins axi_ic_nvme_qe_dma/M00_ARESETN] \
	[get_bd_pins axi_ic_mcdma_mmio/S00_ARESETN] \
	[get_bd_pins axi_ic_mcdma_mmio/M0*_ARESETN] \
	[get_bd_pins axi_ic_mcdma_mmio/M10_ARESETN] \
	[get_bd_pins axi_ic_mcdma_mmio/M11_ARESETN] \
      [get_bd_pins axi_gpio_*/s_axi_aresetn] \
        [get_bd_pins axi_ic_nvme_qe_dma_mmio/*_ARESETN] \
	[get_bd_pins axi_nvme_qe_dma_*/axi_resetn] \
        [get_bd_pins axi_nvme_qe_dma_intc_*/s_axi_aresetn] \
        [get_bd_pins axi_mcdma_intr/s_axi_aresetn] \
        [get_bd_pins axis_ic_qdma_h2c/M00_AXIS_ARESETN] \
        [get_bd_pins axis_ic_qdma_c2h/S*_ARESETN] \
        [get_bd_pins axi_ic_pcie_rp_*_dma/M00_ARESETN] \
        [get_bd_pins axi_ic_pcie_rc_dma/S00_ARESETN] \
        [get_bd_pins axi_ic_pcie_rc_dma/S01_ARESETN] \
        [get_bd_pins axi_ic_pcie_rc_dma/S02_ARESETN] \
        [get_bd_pins axi_ic_pcie_rc_dma/S03_ARESETN] \
        [get_bd_pins axis_ic_split_nvmeq_and_compute/*ARESETN]  \
        [get_bd_pins axis_ic_merge_nvmeq_and_compute/*ARESETN] \
        [get_bd_pins axi_datamover_0/m_*_aresetn] \
        [get_bd_pins u_accframework/resetn] \
        [get_bd_pins axis_h2c_data_fifo/s_axis_aresetn] \
        [get_bd_pins axi_ic_mcdma/S01_ARESETN] 


if { ${mpsoc_bd_val::r_bridge_en} == "1" } {
      connect_bd_net [get_bd_pins pcie_rc_sync_reset/peripheral_aresetn] [get_bd_pins axis_ic_qdma_c2h/M01_AXIS_ARESETN]
}

  connect_bd_net [get_bd_pins pcie_rc_sync_reset/interconnect_aresetn] \
        [get_bd_pins axi_ic_pcie_rc_dma/ARESETN] \
        [get_bd_pins axi_ic_pcie_rc_bar/ARESETN] \
	  [get_bd_pins axi_ic_pcie_rc_mmio/ARESETN] \
        [get_bd_pins axi_ic_mcdma/ARESETN] \
	  [get_bd_pins axi_ic_nvme_qe_dma/ARESETN] \
	  [get_bd_pins axi_ic_mcdma_mmio/ARESETN] \
        [get_bd_pins axi_ic_nvme_qe_dma_mmio/ARESETN] \
        [get_bd_pins axis_ic_qdma_h2c/ARESETN] \
        [get_bd_pins axis_ic_qdma_c2h/ARESETN] 

  # Reset signals for DDR4 MIG related AXI interfaces in MIG ui clock domain
#   connect_bd_net -net mig_calib_done [get_bd_pins ddr4_mig/c0_init_calib_complete] \
#         [get_bd_pins ddr4_mig_sync_reset/dcm_locked]

#   connect_bd_net [get_bd_pins ddr4_mig_sync_reset/peripheral_aresetn] \
#         [get_bd_pins ddr4_mig/c0_ddr4_aresetn] \
# 	[get_bd_pins axi_ic_mcdma_mmio/M00_ARESETN]

  # PCIe EP reset
  connect_bd_net [get_bd_pins u_qdma_ep/sys_rst_n] \
        [get_bd_ports pcie_ep_perstn]

  connect_bd_net [get_bd_pins u_qdma_ep/axi_aresetn] \
      [get_bd_pins axi_ic_nvme_qe_dma/M01_ARESETN] \
      [get_bd_pins u_qdma_ep_axis_wrapper/axi_aresetn] \
      [get_bd_pins u_qdma_h2c_byp_ctrl/axi_aresetn] \
      [get_bd_pins u_qdma_c2h_byp_ctrl/axi_aresetn] \
      [get_bd_pins axi_ic_pcie_rc_bar/S01_ARESETN] \
      [get_bd_pins axis_ic_qdma_h2c/S00_AXIS_ARESETN] \
      [get_bd_pins axis_ic_qdma_h2c/M01_AXIS_ARESETN] \
      [get_bd_pins axis_ic_qdma_c2h/M00_AXIS_ARESETN] \
      [get_bd_pins axis_ic_qdma_c2h_data/ARESETN] \
      [get_bd_pins axis_ic_qdma_c2h_data/M*_AXIS_ARESETN] \
      [get_bd_pins axis_ic_qdma_c2h_data/S*_AXIS_ARESETN] \
      [get_bd_pins axis_ic_qdma_h2c_byp_in/ARESETN] \
      [get_bd_pins axis_ic_qdma_h2c_byp_in/M*_AXIS_ARESETN] \
      [get_bd_pins axis_ic_qdma_h2c_byp_in/S*_AXIS_ARESETN] \
      [get_bd_pins u_xdma_rp_axi_bridge_*/s_axib_aresetn] \
      [get_bd_pins u_axis_w_merger_*/axi_aresetn] \
      [get_bd_pins u_axis_aw_w_splitter/aresetn] \
      [get_bd_pins axi_ic_pcie_rp_*_dma/M01_ARESETN] \
      [get_bd_pins axis_dwidth_converter_w_0/aresetn] \
      [get_bd_pins axis_dwidth_converter_w_1/aresetn] \
      [get_bd_pins axis_dwidth_converter_w_2/aresetn] \
      [get_bd_pins axis_dwidth_converter_w_3/aresetn] \
      [get_bd_pins axis_dwidth_converter_r_*/aresetn] \
      [get_bd_pins u_len_fifo_*/resetn] \
      [get_bd_pins u_wid_fifo_*/resetn] \
      [get_bd_pins u_crdt_fifo/resetn] \
      [get_bd_pins u_cmpt_fifo/resetn] \
      [get_bd_pins u_w_data_connector_0/aresetn] \
      [get_bd_pins u_axis_route_r_handler/aresetn] \
      [get_bd_pins axis_ic_w/aresetn] \
      [get_bd_pins axis_ic_w/S*_AXIS_aresetn] \
      [get_bd_pins axis_ic_w/M*_AXIS_aresetn] \
      [get_bd_pins axis_ic_h2c_req/aresetn] \
      [get_bd_pins axis_ic_h2c_req/S*_AXIS_aresetn] \
      [get_bd_pins axis_ic_h2c_req/M*_AXIS_aresetn] \
      [get_bd_pins u_credit_manager/axi_aresetn] \
      [get_bd_pins axis_ic_h2c_multi_r_out/S*_AXIS_ARESETN] \
      [get_bd_pins axis_ic_h2c_multi_r_out/M*_AXIS_ARESETN] \
      [get_bd_pins u_axis_req_in_cnt/aresetn] \
      [get_bd_pins axis_ic_h2c_multi_r_out/ARESETN] 

if { ${mpsoc_bd_val::ar_bridge_en} == "1" } {
      connect_bd_net [get_bd_pins u_qdma_ep/axi_aresetn] [get_bd_pins axis_ic_qdma_c2h/M02_AXIS_ARESETN]
}

#=============================================
# AXI interface connection
#=============================================

  # PCIe RP BAR 
  connect_bd_intf_net [get_bd_intf_pins zynq_mpsoc/M_AXI_HPM0_FPD] \
	[get_bd_intf_pins axi_ic_pcie_rc_bar/S00_AXI]

  connect_bd_intf_net [get_bd_intf_pins xdma_rp_1/S_AXI_B] \
	[get_bd_intf_pins axi_ic_pcie_rc_bar/M01_AXI]

  connect_bd_intf_net [get_bd_intf_pins xdma_rp_2/S_AXI_B] \
	[get_bd_intf_pins axi_ic_pcie_rc_bar/M02_AXI]

  connect_bd_intf_net [get_bd_intf_pins xdma_rp_3/S_AXI_B] \
	[get_bd_intf_pins axi_ic_pcie_rc_bar/M03_AXI]

  connect_bd_intf_net [get_bd_intf_pins u_qdma_ep/M_AXI_BRIDGE_0] \
	[get_bd_intf_pins axi_ic_pcie_rc_bar/S01_AXI]

  # PCIe RP MMIO 
  connect_bd_intf_net [get_bd_intf_pins zynq_mpsoc/M_AXI_HPM0_LPD] \
	[get_bd_intf_pins axi_ic_pcie_rc_mmio/S00_AXI]

  connect_bd_intf_net [get_bd_intf_pins xdma_rp_1/S_AXI_LITE] \
	[get_bd_intf_pins axi_ic_pcie_rc_mmio/M01_AXI]

  connect_bd_net [get_bd_pins axi_ic_pcie_rc_mmio/M01_AXI_araddr] [get_bd_pins xlconcat_rp1_ar/In0]
  connect_bd_net [get_bd_pins axi_ic_pcie_rc_mmio/M01_AXI_awaddr] [get_bd_pins xlconcat_rp1_aw/In0]

  connect_bd_net [get_bd_pins xlconcat_rp1_ar/dout] [get_bd_pins xdma_rp_1/s_axil_araddr]
  connect_bd_net [get_bd_pins xlconcat_rp1_aw/dout] [get_bd_pins xdma_rp_1/s_axil_awaddr]

  connect_bd_intf_net [get_bd_intf_pins xdma_rp_2/S_AXI_LITE] \
	[get_bd_intf_pins axi_ic_pcie_rc_mmio/M02_AXI]

  connect_bd_net [get_bd_pins axi_ic_pcie_rc_mmio/M02_AXI_araddr] [get_bd_pins xlconcat_rp2_ar/In0]
  connect_bd_net [get_bd_pins axi_ic_pcie_rc_mmio/M02_AXI_awaddr] [get_bd_pins xlconcat_rp2_aw/In0]

  connect_bd_net [get_bd_pins xlconcat_rp2_ar/dout] [get_bd_pins xdma_rp_2/s_axil_araddr]
  connect_bd_net [get_bd_pins xlconcat_rp2_aw/dout] [get_bd_pins xdma_rp_2/s_axil_awaddr]

  connect_bd_intf_net [get_bd_intf_pins xdma_rp_3/S_AXI_LITE] \
	[get_bd_intf_pins axi_ic_pcie_rc_mmio/M03_AXI]

  connect_bd_net [get_bd_pins axi_ic_pcie_rc_mmio/M03_AXI_araddr] [get_bd_pins xlconcat_rp3_ar/In0]
  connect_bd_net [get_bd_pins axi_ic_pcie_rc_mmio/M03_AXI_awaddr] [get_bd_pins xlconcat_rp3_aw/In0]

  connect_bd_net [get_bd_pins xlconcat_rp3_ar/dout] [get_bd_pins xdma_rp_3/s_axil_araddr]
  connect_bd_net [get_bd_pins xlconcat_rp3_aw/dout] [get_bd_pins xdma_rp_3/s_axil_awaddr]

  # PCIe RP DMA 
  connect_bd_intf_net [get_bd_intf_pins zynq_mpsoc/S_AXI_HPC0_FPD] \
	[get_bd_intf_pins axi_ic_pcie_rc_dma/M00_AXI]

  # Cache-coherence signal to all PCIe DMA transactions
  ## constant for axcache signal
  set const_axcache [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_axcache ]
  set_property -dict [list CONFIG.CONST_WIDTH {4} \
        CONFIG.CONST_VAL {0xb} ] $const_axcache

  ## constant for axprot signal
  set const_axprot [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 const_axprot ]
  set_property -dict [list CONFIG.CONST_WIDTH {3} \
        CONFIG.CONST_VAL {0x2} ] $const_axprot

  connect_bd_net [get_bd_pins const_zero/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/drive_id]

  connect_bd_intf_net [get_bd_intf_pins xdma_rp_1/M_AXI_B] \
        [get_bd_intf_pins axi_ic_pcie_rp_1_dma/S00_AXI]

  connect_bd_intf_net [get_bd_intf_pins axi_ic_pcie_rp_1_dma/M00_AXI] \
	[get_bd_intf_pins axi_ic_pcie_rc_dma/S01_AXI]
  
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_ic_nvme_qe_dma/M01_AXI] \
       [get_bd_intf_pins u_qdma_ep/S_AXI_BRIDGE_0]
  create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_0
  create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 xlslice_1
  set_property -dict [list CONFIG.DIN_FROM {47} CONFIG.DIN_WIDTH {64} CONFIG.DOUT_WIDTH {48}] [get_bd_cells xlslice_0]
  set_property -dict [list CONFIG.DIN_FROM {47} CONFIG.DIN_WIDTH {64} CONFIG.DOUT_WIDTH {48}] [get_bd_cells xlslice_1]
  connect_bd_net [get_bd_pins axi_ic_nvme_qe_dma/M01_AXI_araddr] [get_bd_pins xlslice_1/Din]
  connect_bd_net [get_bd_pins axi_ic_nvme_qe_dma/M01_AXI_awaddr] [get_bd_pins xlslice_0/Din]
  connect_bd_net [get_bd_pins xlslice_0/Dout] [get_bd_pins u_qdma_ep/S_AXI_BRIDGE_0_awaddr]
  connect_bd_net [get_bd_pins xlslice_1/Dout] [get_bd_pins u_qdma_ep/S_AXI_BRIDGE_0_araddr]



  connect_bd_net [get_bd_pins const_one/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/drive_id]

  connect_bd_intf_net [get_bd_intf_pins axi_ic_pcie_rp_1_dma/M01_AXI] \
	[get_bd_intf_pins u_xdma_rp_axi_bridge_1/S_AXIB]

  connect_bd_intf_net [get_bd_intf_pins xdma_rp_2/M_AXI_B] \
        [get_bd_intf_pins axi_ic_pcie_rp_2_dma/S00_AXI]

  connect_bd_intf_net [get_bd_intf_pins axi_ic_pcie_rp_2_dma/M00_AXI] \
	[get_bd_intf_pins axi_ic_pcie_rc_dma/S02_AXI]

  connect_bd_net [get_bd_pins const_two/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/drive_id]

  connect_bd_intf_net [get_bd_intf_pins axi_ic_pcie_rp_2_dma/M01_AXI] \
	[get_bd_intf_pins u_xdma_rp_axi_bridge_2/S_AXIB]

  connect_bd_intf_net [get_bd_intf_pins xdma_rp_3/M_AXI_B] \
        [get_bd_intf_pins axi_ic_pcie_rp_3_dma/S00_AXI]

  connect_bd_intf_net [get_bd_intf_pins axi_ic_pcie_rp_3_dma/M00_AXI] \
	[get_bd_intf_pins axi_ic_pcie_rc_dma/S03_AXI]

  connect_bd_net [get_bd_pins const_three/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/drive_id]

  connect_bd_intf_net [get_bd_intf_pins axi_ic_pcie_rp_3_dma/M01_AXI] \
	[get_bd_intf_pins u_xdma_rp_axi_bridge_3/S_AXIB]

  connect_bd_net -net const_axcache_dout [get_bd_pins axi_ic_pcie_rc_dma/S*_AXI_arcache] \
        [get_bd_pins axi_ic_pcie_rc_dma/S*_AXI_awcache] \
        [get_bd_pins axi_ic_mcdma/S*_AXI_arcache] \
        [get_bd_pins axi_ic_mcdma/S*_AXI_awcache] \
        [get_bd_pins const_axcache/dout]

  connect_bd_net -net const_axprot_dout [get_bd_pins axi_ic_pcie_rc_dma/S*_AXI_arprot] \
        [get_bd_pins axi_ic_pcie_rc_dma/S*_AXI_awprot] \
        [get_bd_pins axi_ic_mcdma/S*_AXI_arprot] \
        [get_bd_pins axi_ic_mcdma/S*_AXI_awprot] \
        [get_bd_pins const_axprot/dout]

  # DDR4 MIG
#   connect_bd_intf_net [get_bd_intf_pins ddr4_mig/C0_DDR4_S_AXI] \
#         [get_bd_intf_pins axi_ic_mcdma_mmio/M00_AXI]

  # AXI DMA of NVMe Queue elements
  connect_bd_intf_net [get_bd_intf_pins zynq_mpsoc/S_AXI_HPC1_FPD] \
        [get_bd_intf_pins axi_ic_mcdma/M00_AXI]

  connect_bd_intf_net [get_bd_intf_pins axi_ic_nvme_qe_dma/M00_AXI] \
        [get_bd_intf_pins axi_ic_mcdma/S00_AXI]

  connect_bd_intf_net [get_bd_intf_pins axi_datamover_0/m_axi_mm2s] \
        [get_bd_intf_pins axi_ic_mcdma/S02_AXI]

  connect_bd_intf_net [get_bd_intf_pins axi_datamover_0/m_axi_s2mm] \
        [get_bd_intf_pins axi_ic_mcdma/S03_AXI]      

  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_ic_mcdma_mmio/M02_AXI] \
        [get_bd_intf_pins u_accframework/s_axi_Manager]
  
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_ic_mcdma/S01_AXI] \
        [get_bd_intf_pins u_accframework/axi_mem_access]

  connect_bd_intf_net [get_bd_intf_pins axi_datamover_0/S_AXIS_S2MM] \
        [get_bd_intf_pins u_accframework/write_mem_data]

  connect_bd_intf_net [get_bd_intf_pins u_accframework/write_mem_cmd] \
        [get_bd_intf_pins axi_datamover_0/S_AXIS_S2MM_CMD]

  connect_bd_intf_net [get_bd_intf_pins axi_datamover_0/S_AXIS_MM2S_CMD] \
        [get_bd_intf_pins u_accframework/read_mem_cmd]

  connect_bd_intf_net [get_bd_intf_pins u_accframework/read_mem_data] \
        [get_bd_intf_pins axi_datamover_0/M_AXIS_MM2S]

  
  connect_bd_intf_net [get_bd_intf_pins axis_h2c_data_fifo/S_AXIS] \
        [get_bd_intf_pins u_accframework/c2h_data]

  connect_bd_intf_net [get_bd_intf_pins u_accframework/m_ext_axis] \
        [get_bd_intf_pins axis_ic_merge_nvmeq_and_compute/S01_AXIS]

  
        
  
  set i 0
  set j 0
  set k 0
  set m 0
  while {$i < ${mpsoc_bd_val::nvme_qe_dma_num}} {
	  if {$j < 10} {
		  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_$i/m_axi_sg] \
	                [get_bd_intf_pins axi_ic_nvme_qe_dma/S0${j}_AXI]
	  } else {
		  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_$i/m_axi_sg] \
	                [get_bd_intf_pins axi_ic_nvme_qe_dma/S${j}_AXI]
	  }
	  incr j 1

	  if {$j < 10} {
		  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_$i/m_axi_s2mm] \
	                [get_bd_intf_pins axi_ic_nvme_qe_dma/S0${j}_AXI]
	  } else {
		  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_$i/m_axi_s2mm] \
	                [get_bd_intf_pins axi_ic_nvme_qe_dma/S${j}_AXI]
	  }
	  incr j 1

	  if {$j < 10} {
		  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_$i/m_axi_mm2s] \
	                [get_bd_intf_pins axi_ic_nvme_qe_dma/S0${j}_AXI]
	  } else {
		  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_$i/m_axi_mm2s] \
	                [get_bd_intf_pins axi_ic_nvme_qe_dma/S${j}_AXI]
	  }
	  incr j 1
	  
          ## AXI-Lite MMIO interface
	  if {$k < 10} {
                  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_$i/s_axi_lite] \
	                [get_bd_intf_pins axi_ic_nvme_qe_dma_mmio/M0${k}_AXI]
	  } else {
                  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_$i/s_axi_lite] \
	                [get_bd_intf_pins axi_ic_nvme_qe_dma_mmio/M${k}_AXI]
	  }
          incr k 1

	  if {$k < 10} {
                  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_intc_$i/s_axi] \
                      [get_bd_intf_pins axi_ic_nvme_qe_dma_mmio/M0${k}_AXI]
	  } else {
                  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_intc_$i/s_axi] \
                      [get_bd_intf_pins axi_ic_nvme_qe_dma_mmio/M${k}_AXI]
	  }
          incr k 1

	  incr i 1
  }

  connect_bd_intf_net [get_bd_intf_pins zynq_mpsoc/M_AXI_HPM1_FPD] \
        [get_bd_intf_pins axi_ic_mcdma_mmio/S00_AXI]

  connect_bd_intf_net [get_bd_intf_pins axi_ic_nvme_qe_dma_mmio/S00_AXI] \
        [get_bd_intf_pins axi_ic_mcdma_mmio/M00_AXI]

  connect_bd_intf_net [get_bd_intf_pins axi_mcdma_intr/s_axi] \
        [get_bd_intf_pins axi_ic_mcdma_mmio/M01_AXI]


#==============================================
# AXI-STREAM connection
#==============================================


  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_qid] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_qid]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_tdata] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_tdata]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_tlast] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_tlast]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_tvalid] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_tvalid]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_tready] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_tready]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_err] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_err]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_mdata] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_mdata]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_mty] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_mty]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_port_id] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_port_id]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_zero_byte] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_zero_byte]

  connect_bd_net [get_bd_pins u_qdma_ep/m_axis_h2c_0_tcrc] \
        [get_bd_pins u_qdma_ep_axis_wrapper/s_axis_h2c_0_tcrc]

  connect_bd_intf_net [get_bd_intf_pins u_qdma_ep_axis_wrapper/m_axis_h2c] \
        [get_bd_intf_pins axis_ic_qdma_h2c/S00_AXIS]

  # H2C bypass out interface of QDMA
  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_dsc] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_dsc]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_st_mm] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_st_mm]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_dsc_sz] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_dsc_sz]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_qid] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_qid]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_error] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_error]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_func] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_func]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_cidx] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_cidx]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_port_id] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_port_id]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_fmt] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_fmt]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_valid] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_valid]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_out_0_ready] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_out_0_ready]

  # H2C AR request to AXIS-IC
  connect_bd_intf_net [get_bd_intf_pins axis_ic_h2c_req/M00_AXIS] -boundary_type upper \
      [get_bd_intf_pins u_axis_req_in_cnt/s_axis]

  connect_bd_intf_net [get_bd_intf_pins u_axis_req_in_cnt/m_axis] \
      [get_bd_intf_pins u_axis_tdest_width_converter/S_AXIS]
  
  # H2C Data to XDMA
if { ${mpsoc_bd_val::r_bridge_en} == "1" } {
      connect_bd_intf_net [get_bd_intf_pins axis_ic_qdma_c2h/M01_AXIS] \
            [get_bd_intf_pins axis_dwidth_converter_w_0/S_AXIS]
} else {
      connect_bd_intf_net [get_bd_intf_pins axis_ic_qdma_h2c/M01_AXIS] \
            [get_bd_intf_pins u_axis_route_r_handler/s_axis]
}

  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_0/s_axis_h2c] \
        [get_bd_intf_pins axis_dwidth_converter_w_0/M_AXIS]

  connect_bd_intf_net [get_bd_intf_pins axis_ic_h2c_multi_r_out/S00_AXIS] \
        [get_bd_intf_pins u_axis_route_r_handler/M_AXIS]

  connect_bd_intf_net [get_bd_intf_pins axis_ic_h2c_multi_r_out/M00_AXIS] \
        [get_bd_intf_pins axis_dwidth_converter_w_0/S_AXIS]

  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_1/s_axis_h2c] \
        [get_bd_intf_pins axis_dwidth_converter_w_1/M_AXIS]

  connect_bd_intf_net [get_bd_intf_pins axis_ic_h2c_multi_r_out/M01_AXIS] \
        [get_bd_intf_pins axis_dwidth_converter_w_1/S_AXIS]

  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_2/s_axis_h2c] \
        [get_bd_intf_pins axis_dwidth_converter_w_2/M_AXIS]

  connect_bd_intf_net [get_bd_intf_pins axis_ic_h2c_multi_r_out/M02_AXIS] \
        [get_bd_intf_pins axis_dwidth_converter_w_2/S_AXIS]

  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_3/s_axis_h2c] \
        [get_bd_intf_pins axis_dwidth_converter_w_3/M_AXIS]

  connect_bd_intf_net [get_bd_intf_pins axis_ic_h2c_multi_r_out/M03_AXIS] \
        [get_bd_intf_pins axis_dwidth_converter_w_3/S_AXIS]

#   set_property CONFIG.FREQ_HZ 250000000 [get_bd_intf_pins u_axis_tdest_width_converter/M_AXIS]
if { ${mpsoc_bd_val::ar_bridge_en} == "1" } {
      connect_bd_intf_net [get_bd_intf_pins u_axis_tdest_width_converter/M_AXIS] \
            [get_bd_intf_pins axis_ic_qdma_h2c/s04_AXIS]


      connect_bd_intf_net [get_bd_intf_pins u_ar_to_bd_pktizer/s_axis_ar_req] \
            [get_bd_intf_pins axis_ic_qdma_c2h/M02_AXIS]
} else {
      # set_property CONFIG.FREQ_HZ 250000000 [get_bd_intf_pins u_ar_to_bd_pktizer/s_axis_ar_req]
      connect_bd_intf_net [get_bd_intf_pins u_axis_tdest_width_converter/M_AXIS] \
            [get_bd_intf_pins u_ar_to_bd_pktizer/s_axis_ar_req]
}

  connect_bd_intf_net [get_bd_intf_pins axis_ic_qdma_h2c_byp_in/s00_axis] \
        [get_bd_intf_pins u_ar_to_bd_pktizer/m_axis_h2c_byp_st]

  connect_bd_intf_net [get_bd_intf_pins u_qdma_h2c_byp_ctrl/s_axis_h2c_byp_in] \
        [get_bd_intf_pins axis_ic_qdma_h2c_byp_in/m00_axis]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_addr] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_addr]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_len] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_len]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_sop] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_sop]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_eop] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_eop]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_sdi] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_sdi]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_mrkr_req] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_mrkr_req]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_no_dma] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_no_dma]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_qid] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_qid]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_error] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_error]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_func] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_func]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_cidx] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_cidx]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_port_id] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_port_id]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_valid] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_valid]

  connect_bd_net [get_bd_pins u_qdma_ep/h2c_byp_in_st_0_ready] \
        [get_bd_pins u_qdma_h2c_byp_ctrl/h2c_byp_in_st_0_ready]

  set qid_slice_to_tdest [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlslice:1.0 qid_slice_to_tdest ]
  set_property -dict [ list CONFIG.DIN_WIDTH {8} \
       CONFIG.DIN_FROM {1} \
       CONFIG.DIN_TO {0} \
       CONFIG.DOUT_WIDTH {2} ] $qid_slice_to_tdest
  
  # C2H interface of QDMA
  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_ctrl_qid] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_ctrl_qid]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_tdata] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_tdata]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_tlast] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_tlast]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_tvalid] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_tvalid]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_tready] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_tready]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_ctrl_has_cmpt] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_ctrl_has_cmpt]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_ctrl_len] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_ctrl_len]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_ctrl_marker] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_ctrl_marker]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_ctrl_port_id] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_ctrl_port_id]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_mty] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_mty]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_tcrc] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_tcrc]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_0_ecc] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_0_ecc]
  
  # CMPT FIFO
  connect_bd_net [get_bd_pins u_cmpt_fifo/wr_en] \
        [get_bd_pins u_qdma_ep_axis_wrapper/cmpt_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_cmpt_fifo/din] \
        [get_bd_pins u_qdma_ep_axis_wrapper/cmpt_fifo_din]
  
  connect_bd_net [get_bd_pins u_cmpt_fifo/rd_en] \
        [get_bd_pins u_qdma_ep_axis_wrapper/cmpt_fifo_rd_en]
  
  connect_bd_net [get_bd_pins u_cmpt_fifo/dout] \
        [get_bd_pins u_qdma_ep_axis_wrapper/cmpt_fifo_dout]
  
  connect_bd_net [get_bd_pins u_cmpt_fifo/empty] \
        [get_bd_pins u_qdma_ep_axis_wrapper/cmpt_fifo_empty]
  
  connect_bd_net [get_bd_pins u_cmpt_fifo/full] \
        [get_bd_pins u_qdma_ep_axis_wrapper/cmpt_fifo_full]
  
  # Traffic manager out interface of QDMA
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_valid] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_vld]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_rdy] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_rdy]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_byp] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_byp]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_dir] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_dir]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_mm] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_mm]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_qid] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_qid]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_avl] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_avl]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_qinv] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_qinv]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_qen] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_qen]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_irq_arm] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_irq_arm]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_error] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_error]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_pidx] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_pidx]
  
  connect_bd_net [get_bd_pins u_qdma_ep/tm_dsc_sts_0_port_id] \
        [get_bd_pins u_credit_manager/tm_dsc_sts_0_port_id]
  
#   connect_bd_intf_net [get_bd_intf_pins axis_ic_qdma_h2c/S01_AXIS] \
#         [get_bd_intf_pins u_credit_manager/m_axis_crdt]
  
  # Desc credit input interface of QDMA
  connect_bd_net [get_bd_pins u_qdma_ep/dsc_crdt_in_0_valid] \
        [get_bd_pins u_credit_manager/dsc_crdt_in_0_vld]
  
  connect_bd_net [get_bd_pins u_qdma_ep/dsc_crdt_in_0_rdy] \
        [get_bd_pins u_credit_manager/dsc_crdt_in_0_rdy]
  
  connect_bd_net [get_bd_pins u_qdma_ep/dsc_crdt_in_0_dir] \
        [get_bd_pins u_credit_manager/dsc_crdt_in_0_dir]
  
  connect_bd_net [get_bd_pins u_qdma_ep/dsc_crdt_in_0_fence] \
        [get_bd_pins u_credit_manager/dsc_crdt_in_0_fence]
  
  connect_bd_net [get_bd_pins u_qdma_ep/dsc_crdt_in_0_qid] \
        [get_bd_pins u_credit_manager/dsc_crdt_in_0_qid]
  
  connect_bd_net [get_bd_pins u_qdma_ep/dsc_crdt_in_0_crdt] \
        [get_bd_pins u_credit_manager/dsc_crdt_in_0_crdt]
  
  # Credit manager FIFO
  connect_bd_net [get_bd_pins u_crdt_fifo/wr_en] \
        [get_bd_pins u_credit_manager/crdt_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_crdt_fifo/din] \
        [get_bd_pins u_credit_manager/crdt_fifo_din]
  
  connect_bd_net [get_bd_pins u_crdt_fifo/rd_en] \
        [get_bd_pins u_credit_manager/crdt_fifo_rd_en]
  
  connect_bd_net [get_bd_pins u_crdt_fifo/dout] \
        [get_bd_pins u_credit_manager/crdt_fifo_dout]
  
  connect_bd_net [get_bd_pins u_crdt_fifo/empty] \
        [get_bd_pins u_credit_manager/crdt_fifo_empty]
  
  connect_bd_net [get_bd_pins u_crdt_fifo/full] \
        [get_bd_pins u_credit_manager/crdt_fifo_full]
  
  # C2H bypass out interfacce of QDMA
  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_dsc] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_dsc]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_st_mm] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_st_mm]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_dsc_sz] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_dsc_sz]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_qid] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_qid]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_error] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_error]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_func] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_func]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_cidx] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_cidx]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_port_id] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_port_id]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_pfch_tag] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_pfch_tag]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_fmt] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_fmt]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_valid] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_valid]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_out_0_ready] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_out_0_ready]
  
  # C2H bypass in interface of QDMA
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_0/m_axis_aw_req] \
        [get_bd_intf_pins u_axis_w_merger_0/s_axis_aw_req]
  
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_0/m_axis_w] \
        [get_bd_intf_pins u_axis_w_merger_0/s_axis_w]

  connect_bd_intf_net [get_bd_intf_pins u_axis_w_merger_0/m_axis] \
        [get_bd_intf_pins axis_dwidth_converter_r_0/S_AXIS]
  
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_1/m_axis_aw_req] \
        [get_bd_intf_pins u_axis_w_merger_1/s_axis_aw_req]
  
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_1/m_axis_w] \
        [get_bd_intf_pins u_axis_w_merger_1/s_axis_w]

  connect_bd_intf_net [get_bd_intf_pins u_axis_w_merger_1/m_axis] \
        [get_bd_intf_pins axis_dwidth_converter_r_1/S_AXIS]
  
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_2/m_axis_aw_req] \
        [get_bd_intf_pins u_axis_w_merger_2/s_axis_aw_req]
  
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_2/m_axis_w] \
        [get_bd_intf_pins u_axis_w_merger_2/s_axis_w]

  connect_bd_intf_net [get_bd_intf_pins u_axis_w_merger_2/m_axis] \
        [get_bd_intf_pins axis_dwidth_converter_r_2/S_AXIS]
  
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_3/m_axis_aw_req] \
        [get_bd_intf_pins u_axis_w_merger_3/s_axis_aw_req]
  
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_3/m_axis_w] \
        [get_bd_intf_pins u_axis_w_merger_3/s_axis_w]

  connect_bd_intf_net [get_bd_intf_pins u_axis_w_merger_3/m_axis] \
        [get_bd_intf_pins axis_dwidth_converter_r_3/S_AXIS]

  # H2C bypass in interface of QDMA
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_0/m_axis_ar_req] \
        [get_bd_intf_pins axis_ic_h2c_req/S00_AXIS]
  
  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_1/m_axis_ar_req] \
        [get_bd_intf_pins axis_ic_h2c_req/S01_AXIS]

  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_2/m_axis_ar_req] \
        [get_bd_intf_pins axis_ic_h2c_req/S02_AXIS]

  connect_bd_intf_net [get_bd_intf_pins u_xdma_rp_axi_bridge_3/m_axis_ar_req] \
        [get_bd_intf_pins axis_ic_h2c_req/S03_AXIS]

  connect_bd_intf_net [get_bd_intf_pins u_qdma_c2h_byp_ctrl/s_axis_c2h_byp_in] \
        [get_bd_intf_pins u_aw_to_bd_pktizer_0/m_axis_h2c_byp_st]
  
  connect_bd_intf_net [get_bd_intf_pins axis_dwidth_converter_r_0/M_AXIS] \
        [get_bd_intf_pins axis_ic_w/S00_AXIS]

  connect_bd_intf_net [get_bd_intf_pins axis_dwidth_converter_r_1/M_AXIS] \
        [get_bd_intf_pins axis_ic_w/S01_AXIS]
  
  connect_bd_intf_net [get_bd_intf_pins axis_dwidth_converter_r_2/M_AXIS] \
        [get_bd_intf_pins axis_ic_w/S02_AXIS]
  
  connect_bd_intf_net [get_bd_intf_pins axis_dwidth_converter_r_3/M_AXIS] \
        [get_bd_intf_pins axis_ic_w/S03_AXIS]
  
  connect_bd_intf_net [get_bd_intf_pins axis_ic_w/M00_AXIS] \
        [get_bd_intf_pins u_axis_aw_w_splitter/s_axis]

  connect_bd_intf_net [get_bd_intf_pins u_axis_aw_w_splitter/m_axis_ar_req] \
        [get_bd_intf_pins u_aw_to_bd_pktizer_0/s_axis_ar_req]

  connect_bd_intf_net [get_bd_intf_pins u_axis_aw_w_splitter/m_axis] \
        [get_bd_intf_pins u_w_data_connector_0/s_axis]

#   set_property CONFIG.FREQ_HZ 250000000 [get_bd_intf_pins u_w_data_connector_0/s_axis]
#   set_property CONFIG.FREQ_HZ 250000000 [get_bd_intf_pins u_w_data_connector_0/m_axis]

 connect_bd_intf_net [get_bd_intf_pins u_w_data_connector_0/m_axis] \
       [get_bd_intf_pins axis_ic_qdma_c2h_data/S00_AXIS]
        
  
  connect_bd_net [get_bd_pins u_len_fifo_0/full] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/len_fifo_full]
  
  connect_bd_net [get_bd_pins u_len_fifo_0/empty] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/len_fifo_empty]
  
  connect_bd_net [get_bd_pins u_len_fifo_0/din] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/len_fifo_wr_data]
  
  connect_bd_net [get_bd_pins u_len_fifo_0/wr_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/len_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_len_fifo_0/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/len_fifo_rd_data]
  
  connect_bd_net [get_bd_pins u_len_fifo_0/rd_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/len_fifo_rd_en]

  connect_bd_net [get_bd_pins u_wid_fifo_0/full] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/wid_fifo_full]
  
  connect_bd_net [get_bd_pins u_wid_fifo_0/empty] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/wid_fifo_empty]
  
  connect_bd_net [get_bd_pins u_wid_fifo_0/din] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/wid_fifo_wr_data]
  
  connect_bd_net [get_bd_pins u_wid_fifo_0/wr_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/wid_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_wid_fifo_0/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/wid_fifo_rd_data]
  
  connect_bd_net [get_bd_pins u_wid_fifo_0/rd_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_0/wid_fifo_rd_en]
  
  connect_bd_net [get_bd_pins u_len_fifo_1/full] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/len_fifo_full]
  
  connect_bd_net [get_bd_pins u_len_fifo_1/empty] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/len_fifo_empty]
  
  connect_bd_net [get_bd_pins u_len_fifo_1/din] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/len_fifo_wr_data]
  
  connect_bd_net [get_bd_pins u_len_fifo_1/wr_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/len_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_len_fifo_1/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/len_fifo_rd_data]
  
  connect_bd_net [get_bd_pins u_len_fifo_1/rd_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/len_fifo_rd_en]

  connect_bd_net [get_bd_pins u_wid_fifo_1/full] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/wid_fifo_full]
  
  connect_bd_net [get_bd_pins u_wid_fifo_1/empty] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/wid_fifo_empty]
  
  connect_bd_net [get_bd_pins u_wid_fifo_1/din] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/wid_fifo_wr_data]
  
  connect_bd_net [get_bd_pins u_wid_fifo_1/wr_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/wid_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_wid_fifo_1/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/wid_fifo_rd_data]
  
  connect_bd_net [get_bd_pins u_wid_fifo_1/rd_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_1/wid_fifo_rd_en]
  
  connect_bd_net [get_bd_pins u_len_fifo_2/full] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/len_fifo_full]
  
  connect_bd_net [get_bd_pins u_len_fifo_2/empty] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/len_fifo_empty]
  
  connect_bd_net [get_bd_pins u_len_fifo_2/din] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/len_fifo_wr_data]
  
  connect_bd_net [get_bd_pins u_len_fifo_2/wr_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/len_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_len_fifo_2/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/len_fifo_rd_data]
  
  connect_bd_net [get_bd_pins u_len_fifo_2/rd_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/len_fifo_rd_en]

  connect_bd_net [get_bd_pins u_wid_fifo_2/full] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/wid_fifo_full]
  
  connect_bd_net [get_bd_pins u_wid_fifo_2/empty] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/wid_fifo_empty]
  
  connect_bd_net [get_bd_pins u_wid_fifo_2/din] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/wid_fifo_wr_data]
  
  connect_bd_net [get_bd_pins u_wid_fifo_2/wr_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/wid_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_wid_fifo_2/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/wid_fifo_rd_data]
  
  connect_bd_net [get_bd_pins u_wid_fifo_2/rd_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_2/wid_fifo_rd_en]
  
  connect_bd_net [get_bd_pins u_len_fifo_3/full] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/len_fifo_full]
  
  connect_bd_net [get_bd_pins u_len_fifo_3/empty] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/len_fifo_empty]
  
  connect_bd_net [get_bd_pins u_len_fifo_3/din] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/len_fifo_wr_data]
  
  connect_bd_net [get_bd_pins u_len_fifo_3/wr_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/len_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_len_fifo_3/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/len_fifo_rd_data]
  
  connect_bd_net [get_bd_pins u_len_fifo_3/rd_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/len_fifo_rd_en]

  connect_bd_net [get_bd_pins u_wid_fifo_3/full] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/wid_fifo_full]
  
  connect_bd_net [get_bd_pins u_wid_fifo_3/empty] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/wid_fifo_empty]
  
  connect_bd_net [get_bd_pins u_wid_fifo_3/din] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/wid_fifo_wr_data]
  
  connect_bd_net [get_bd_pins u_wid_fifo_3/wr_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/wid_fifo_wr_en]
  
  connect_bd_net [get_bd_pins u_wid_fifo_3/dout] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/wid_fifo_rd_data]
  
  connect_bd_net [get_bd_pins u_wid_fifo_3/rd_en] \
        [get_bd_pins u_xdma_rp_axi_bridge_3/wid_fifo_rd_en]
  
  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_in_st_0_addr] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_in_st_0_addr]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_in_st_0_qid] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_in_st_0_qid]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_in_st_0_error] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_in_st_0_error]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_in_st_0_func] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_in_st_0_func]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_in_st_0_port_id] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_in_st_0_port_id]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_in_st_0_pfch_tag] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_in_st_0_pfch_tag]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_in_st_0_valid] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_in_st_0_valid]

  connect_bd_net [get_bd_pins u_qdma_ep/c2h_byp_in_st_0_ready] \
        [get_bd_pins u_qdma_c2h_byp_ctrl/c2h_byp_in_st_0_ready]
  
  # CMPT interface of QDMA
  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_data] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_tdata]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_size] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_size]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_dpar] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_dpar]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_qid] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_ctrl_qid] 

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_marker] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_ctrl_marker]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_user_trig] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_ctrl_user_trig]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_cmpt_type] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_ctrl_cmpt_type]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_wait_pld_pkt_id] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_ctrl_wait_pld_pkt_id]

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_tvalid] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_tvalid] 

  connect_bd_net [get_bd_pins u_qdma_ep/s_axis_c2h_cmpt_0_tready] \
        [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_c2h_cmpt_tready] 

  connect_bd_intf_net [get_bd_intf_pins axis_ic_qdma_c2h_data/S01_AXIS] \
        [get_bd_intf_pins axis_ic_qdma_c2h/M00_AXIS]

  connect_bd_intf_net [get_bd_intf_pins u_qdma_ep_axis_wrapper/s_axis_c2h] \
        [get_bd_intf_pins axis_ic_qdma_c2h_data/M00_AXIS]
  # AXI MCDMA for NVMe QE delivery
  set i 0
  set j 0
  while {$i < ${mpsoc_bd_val::nvme_qe_dma_num}} {
        connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_${i}/S_AXIS_S2MM] \
            [get_bd_intf_pins axis_ic_qdma_h2c/M0${j}_AXIS]

        connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_${i}/M_AXIS_MM2S] \
            [get_bd_intf_pins u_axis_tid_as_tdest_${i}/S_AXIS]

        connect_bd_intf_net [get_bd_intf_pins u_axis_tid_as_tdest_${i}/M_AXIS] \
            [get_bd_intf_pins axis_ic_qdma_c2h/S0${j}_AXIS]

        incr i 1
        incr j 1
  }

  # Special Control For Compute
  delete_bd_objs [get_bd_intf_nets axi_nvme_qe_dma_0_M_AXIS_MM2S]
  connect_bd_intf_net [get_bd_intf_pins axi_nvme_qe_dma_0/M_AXIS_MM2S] [get_bd_intf_pins axis_ic_split_nvmeq_and_compute/S00_AXIS]
  connect_bd_intf_net [get_bd_intf_pins axis_ic_split_nvmeq_and_compute/M00_AXIS] [get_bd_intf_pins u_axis_tid_as_tdest_0/s_axis]
  delete_bd_objs [get_bd_intf_nets axis_ic_qdma_h2c_M00_AXIS]
  connect_bd_intf_net [get_bd_intf_pins axis_ic_qdma_h2c/M00_AXIS] [get_bd_intf_pins axis_ic_merge_nvmeq_and_compute/S00_AXIS]
  connect_bd_intf_net [get_bd_intf_pins axis_ic_merge_nvmeq_and_compute/M00_AXIS] [get_bd_intf_pins axi_nvme_qe_dma_0/S_AXIS_S2MM]
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axis_ic_split_nvmeq_and_compute/M01_AXIS] [get_bd_intf_pins u_datapath_to_acc_bridge/s_axis]
  create_bd_cell -type ip -vlnv xilinx.com:ip:axis_dwidth_converter:1.1 u_acc_width_converter
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES.VALUE_SRC USER CONFIG.TID_WIDTH.VALUE_SRC USER CONFIG.TDEST_WIDTH.VALUE_SRC USER CONFIG.TUSER_BITS_PER_BYTE.VALUE_SRC USER CONFIG.HAS_TLAST.VALUE_SRC USER CONFIG.HAS_TSTRB.VALUE_SRC USER CONFIG.HAS_TKEEP.VALUE_SRC USER] \
  [get_bd_cells u_acc_width_converter]
  set_property -dict [list CONFIG.S_TDATA_NUM_BYTES {16} CONFIG.M_TDATA_NUM_BYTES {64} CONFIG.TID_WIDTH {8} CONFIG.TDEST_WIDTH {8} CONFIG.HAS_TLAST {1} CONFIG.HAS_TKEEP {1} CONFIG.HAS_MI_TKEEP {1}] [get_bd_cells u_acc_width_converter]
  set_property -dict [list CONFIG.TUSER_BITS_PER_BYTE {8} ] [get_bd_cells u_acc_width_converter]
  connect_bd_intf_net [get_bd_intf_pins u_datapath_to_acc_bridge/m_axis] [get_bd_intf_pins u_acc_width_converter/S_AXIS]
  connect_bd_intf_net [get_bd_intf_pins u_acc_width_converter/M_AXIS] [get_bd_intf_pins u_accframework/s_ext_axis]
  connect_bd_net [get_bd_pins u_acc_width_converter/aclk] [get_bd_pins zynq_mpsoc/pl_clk1]
  connect_bd_net [get_bd_pins u_acc_width_converter/aresetn] [get_bd_pins pcie_rc_sync_reset/peripheral_aresetn]

  connect_bd_net [get_bd_pins u_qdma_c2h_byp_ctrl/pfch_tag_0_lsb] [get_bd_pins axi_gpio_pfch_tag_0/gpio_io_o]
  connect_bd_net [get_bd_pins u_qdma_c2h_byp_ctrl/pfch_tag_0_msb] [get_bd_pins axi_gpio_pfch_tag_0/gpio2_io_o]
  connect_bd_net [get_bd_pins u_qdma_c2h_byp_ctrl/pfch_tag_1_lsb] [get_bd_pins axi_gpio_pfch_tag_1/gpio_io_o]
  connect_bd_net [get_bd_pins u_qdma_c2h_byp_ctrl/pfch_tag_1_msb] [get_bd_pins axi_gpio_pfch_tag_1/gpio2_io_o]
  connect_bd_net [get_bd_pins u_qdma_c2h_byp_ctrl/pfch_tag_2_lsb] [get_bd_pins axi_gpio_pfch_tag_2/gpio_io_o]
  connect_bd_net [get_bd_pins u_qdma_c2h_byp_ctrl/pfch_tag_2_msb] [get_bd_pins axi_gpio_pfch_tag_2/gpio2_io_o]
  connect_bd_net [get_bd_pins u_qdma_c2h_byp_ctrl/pfch_tag_3_lsb] [get_bd_pins axi_gpio_pfch_tag_3/gpio_io_o]
  connect_bd_net [get_bd_pins u_qdma_c2h_byp_ctrl/pfch_tag_3_msb] [get_bd_pins axi_gpio_pfch_tag_3/gpio2_io_o]

  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_ic_mcdma_mmio/M03_AXI] [get_bd_intf_pins axi_gpio_pfch_tag_0/S_AXI]
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_ic_mcdma_mmio/M04_AXI] [get_bd_intf_pins axi_gpio_pfch_tag_1/S_AXI]
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_ic_mcdma_mmio/M05_AXI] [get_bd_intf_pins axi_gpio_pfch_tag_2/S_AXI]
  connect_bd_intf_net -boundary_type upper [get_bd_intf_pins axi_ic_mcdma_mmio/M06_AXI] [get_bd_intf_pins axi_gpio_pfch_tag_3/S_AXI]


#==============================================
# GT Port connection
#==============================================


  # PCIe RP #1 slot
  connect_bd_net [get_bd_ports pcie_rp_rxn_1] [get_bd_pins xdma_rp_1/pci_exp_rxn]
  connect_bd_net [get_bd_ports pcie_rp_rxp_1] [get_bd_pins xdma_rp_1/pci_exp_rxp]
  connect_bd_net [get_bd_ports pcie_rp_txn_1] [get_bd_pins xdma_rp_1/pci_exp_txn]
  connect_bd_net [get_bd_ports pcie_rp_txp_1] [get_bd_pins xdma_rp_1/pci_exp_txp]

  # PCIe RP #2 slot
  connect_bd_net [get_bd_ports pcie_rp_rxn_2] [get_bd_pins xdma_rp_2/pci_exp_rxn]
  connect_bd_net [get_bd_ports pcie_rp_rxp_2] [get_bd_pins xdma_rp_2/pci_exp_rxp]
  connect_bd_net [get_bd_ports pcie_rp_txn_2] [get_bd_pins xdma_rp_2/pci_exp_txn]
  connect_bd_net [get_bd_ports pcie_rp_txp_2] [get_bd_pins xdma_rp_2/pci_exp_txp]

  # PCIe RP #3 slot
  connect_bd_net [get_bd_ports pcie_rp_rxn_3] [get_bd_pins xdma_rp_3/pci_exp_rxn]
  connect_bd_net [get_bd_ports pcie_rp_rxp_3] [get_bd_pins xdma_rp_3/pci_exp_rxp]
  connect_bd_net [get_bd_ports pcie_rp_txn_3] [get_bd_pins xdma_rp_3/pci_exp_txn]
  connect_bd_net [get_bd_ports pcie_rp_txp_3] [get_bd_pins xdma_rp_3/pci_exp_txp]

  # PCIe EP slot
  connect_bd_net [get_bd_ports pcie_ep_rxn] [get_bd_pins u_qdma_ep/pcie_ep_rxn]
  connect_bd_net [get_bd_ports pcie_ep_rxp] [get_bd_pins u_qdma_ep/pcie_ep_rxp]
  connect_bd_net [get_bd_ports pcie_ep_txn] [get_bd_pins u_qdma_ep/pcie_ep_txn]
  connect_bd_net [get_bd_ports pcie_ep_txp] [get_bd_pins u_qdma_ep/pcie_ep_txp]

#==============================================
# DDR4 memory connection
#==============================================

#   connect_bd_intf_net [get_bd_intf_pins ddr4_mig/C0_DDR4] [get_bd_intf_ports c0_ddr4]

#=============================================
# Interrupt signal connection
#=============================================

  set axi_mcdma_intr_concat [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 axi_mcdma_intr_concat ]
        set_property -dict [ list CONFIG.NUM_PORTS {5} ] $axi_mcdma_intr_concat

  set i 0
  set m 0
  while {$i < ${mpsoc_bd_val::nvme_qe_dma_num}} {
        set j 1
        set k 0

        set intr_concat_name axi_nvme_qe_dma_intr_concat_$i
        set intr_concat_ctrl [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 $intr_concat_name ]
        set_property -dict [ list CONFIG.NUM_PORTS {32} ] $intr_concat_ctrl

        while {$j <= 8} {
            connect_bd_net [get_bd_pins axi_nvme_qe_dma_$i/mm2s_ch${j}_introut] \
                [get_bd_pins axi_nvme_qe_dma_intr_concat_$i/In${k}]

            incr j 1
            incr k 1
        }

        set j 1
        while {$j <= 8} {
            connect_bd_net [get_bd_pins axi_nvme_qe_dma_$i/s2mm_ch${j}_introut] \
                [get_bd_pins axi_nvme_qe_dma_intr_concat_$i/In${k}]

            incr j 1
            incr k 1
        }

        connect_bd_net [get_bd_pins axi_nvme_qe_dma_intr_concat_$i/dout] \
            [get_bd_pins axi_nvme_qe_dma_intc_$i/intr]

        connect_bd_net [get_bd_pins axi_nvme_qe_dma_intc_$i/irq] \
            [get_bd_pins axi_mcdma_intr_concat/In${m}] 

        incr i 1
        incr m 1
  }

  connect_bd_net [get_bd_pins axi_mcdma_intr_concat/dout] \
        [get_bd_pins axi_mcdma_intr/intr]

  # Create instance: concat_intr, and set properties
  set concat_intr [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 concat_intr ]
  set_property -dict [ list CONFIG.NUM_PORTS {7} ] $concat_intr
	  
  connect_bd_net [get_bd_pins xdma_rp_1/interrupt_out] [get_bd_pins concat_intr/In3]
  connect_bd_net [get_bd_pins xdma_rp_1/interrupt_out_msi_vec0to31] [get_bd_pins concat_intr/In4]
  connect_bd_net [get_bd_pins xdma_rp_1/interrupt_out_msi_vec32to63] [get_bd_pins concat_intr/In5]
  connect_bd_net [get_bd_pins axi_mcdma_intr/irq] [get_bd_pins concat_intr/In6]
  connect_bd_net [get_bd_pins concat_intr/dout] [get_bd_pins zynq_mpsoc/pl_ps_irq0]

  # Create instance: concat_intr, and set properties
  set concat_intr_high [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 concat_intr_high ]
  set_property -dict [ list CONFIG.NUM_PORTS {6} ] $concat_intr_high
	  
  connect_bd_net [get_bd_pins xdma_rp_2/interrupt_out] [get_bd_pins concat_intr_high/In0]
  connect_bd_net [get_bd_pins xdma_rp_2/interrupt_out_msi_vec0to31] [get_bd_pins concat_intr_high/In1]
  connect_bd_net [get_bd_pins xdma_rp_2/interrupt_out_msi_vec32to63] [get_bd_pins concat_intr_high/In2]
  connect_bd_net [get_bd_pins xdma_rp_3/interrupt_out] [get_bd_pins concat_intr_high/In3]
  connect_bd_net [get_bd_pins xdma_rp_3/interrupt_out_msi_vec0to31] [get_bd_pins concat_intr_high/In4]
  connect_bd_net [get_bd_pins xdma_rp_3/interrupt_out_msi_vec32to63] [get_bd_pins concat_intr_high/In5]
  connect_bd_net [get_bd_pins concat_intr_high/dout] [get_bd_pins zynq_mpsoc/pl_ps_irq1]


#============================================
# Just for test
#============================================
  connect_bd_net [get_bd_pins axis_ic_qdma_c2h/M01_AXIS_tready] [get_bd_pins xlconstant_0/dout]

#============================================
# Device Tree Debug
#============================================


  create_bd_cell -type module -reference axis_w_merger_fake axis_w_merger_fake_0
  replace_bd_cell [get_bd_cells u_axis_w_merger_0] axis_w_merger_fake_0
  delete_bd_objs [get_bd_cells u_axis_w_merger_0]
#=============================================
# Address segments
#=============================================
  
  # AXI PCIe RP #0 M_AXI (DMA)
 
  create_bd_addr_seg -range 0x8000000000000000 -offset 0x8000000000000000 [get_bd_addr_spaces xdma_rp_1/M_AXI_B] [get_bd_addr_segs u_xdma_rp_axi_bridge_1/s_axib/reg0] XDMA_RP_AXI_BRIDGE_1
  create_bd_addr_seg -range 0x8000000000000000 -offset 0x8000000000000000 [get_bd_addr_spaces xdma_rp_2/M_AXI_B] [get_bd_addr_segs u_xdma_rp_axi_bridge_2/s_axib/reg0] XDMA_RP_AXI_BRIDGE_2
  create_bd_addr_seg -range 0x8000000000000000 -offset 0x8000000000000000 [get_bd_addr_spaces xdma_rp_3/M_AXI_B] [get_bd_addr_segs u_xdma_rp_axi_bridge_3/s_axib/reg0] XDMA_RP_AXI_BRIDGE_3

  # AXI PCIe RP #1 M_AXI (DMA)
  create_bd_addr_seg -range 0x80000000 -offset 0x0 [get_bd_addr_spaces xdma_rp_1/M_AXI_B] [get_bd_addr_segs zynq_mpsoc/SAXIGP0/HPC0_DDR_LOW] HP0_DDR_LOW_1
  create_bd_addr_seg -range 0x400000000 -offset 0x800000000 [get_bd_addr_spaces xdma_rp_1/M_AXI_B] [get_bd_addr_segs zynq_mpsoc/SAXIGP0/HPC0_DDR_HIGH] HP0_DDR_HIGH_1

  # AXI PCIe RP #2 M_AXI (DMA)
  create_bd_addr_seg -range 0x80000000 -offset 0x0 [get_bd_addr_spaces xdma_rp_2/M_AXI_B] [get_bd_addr_segs zynq_mpsoc/SAXIGP0/HPC0_DDR_LOW] HP0_DDR_LOW_2
  create_bd_addr_seg -range 0x400000000 -offset 0x800000000 [get_bd_addr_spaces xdma_rp_2/M_AXI_B] [get_bd_addr_segs zynq_mpsoc/SAXIGP0/HPC0_DDR_HIGH] HP0_DDR_HIGH_2

  # AXI PCIe RP #3 M_AXI (DMA)
  create_bd_addr_seg -range 0x80000000 -offset 0x0 [get_bd_addr_spaces xdma_rp_3/M_AXI_B] [get_bd_addr_segs zynq_mpsoc/SAXIGP0/HPC0_DDR_LOW] HP0_DDR_LOW_3
  create_bd_addr_seg -range 0x400000000 -offset 0x800000000 [get_bd_addr_spaces xdma_rp_3/M_AXI_B] [get_bd_addr_segs zynq_mpsoc/SAXIGP0/HPC0_DDR_HIGH] HP0_DDR_HIGH_3

  ## PCIe RP BAR in ARMv8 address space
  create_bd_addr_seg -range 0x00100000 -offset 0xA0100000 [get_bd_addr_spaces zynq_mpsoc/Data] [get_bd_addr_segs xdma_rp_1/S_AXI_B/BAR0] PCIE_RC_BAR0_1
  create_bd_addr_seg -range 0x00100000 -offset 0xA0200000 [get_bd_addr_spaces zynq_mpsoc/Data] [get_bd_addr_segs xdma_rp_2/S_AXI_B/BAR0] PCIE_RC_BAR0_2
  create_bd_addr_seg -range 0x00100000 -offset 0xA0300000 [get_bd_addr_spaces zynq_mpsoc/Data] [get_bd_addr_segs xdma_rp_3/S_AXI_B/BAR0] PCIE_RC_BAR0_3

  # PCIe RP BAR in x86 address space via PCIe EP
  create_bd_addr_seg -range 0x00100000 -offset 0xA0100000 [get_bd_addr_spaces u_qdma_ep/M_AXI_BRIDGE_0] [get_bd_addr_segs xdma_rp_1/S_AXI_B/BAR0] X86_PCIE_RC_BAR0_1

  ## PCIe RC MMIO controller in ARMv8 address space
  create_bd_addr_seg -range 0x800000 -offset 0x80800000 [get_bd_addr_spaces zynq_mpsoc/Data] [get_bd_addr_segs xdma_rp_1/S_AXI_LITE/CTL0] PCIE_RC_CTL_1
  create_bd_addr_seg -range 0x800000 -offset 0x81000000 [get_bd_addr_spaces zynq_mpsoc/Data] [get_bd_addr_segs xdma_rp_2/S_AXI_LITE/CTL0] PCIE_RC_CTL_2
  create_bd_addr_seg -range 0x800000 -offset 0x81800000 [get_bd_addr_spaces zynq_mpsoc/Data] [get_bd_addr_segs xdma_rp_3/S_AXI_LITE/CTL0] PCIE_RC_CTL_3

  
  ## DMA engine of NVMe Queue elements in ARMv8 address space
  # if you use real dma,the address is Reg not reg0
  # and dma_$i/M_AXI_SG changes to dma_$i/DATA_SG
  # M_AXI_MM2S changes to DATA_MM2S
  # M_AXI_S2MM changes to DATA_S2MM
  set i 0
  set addr_base 0xB0000000
  set intc_addr_base 0xB1000000
  
  while {$i < ${mpsoc_bd_val::nvme_qe_dma_num}} {
	  set addr [expr $addr_base + 0x10000]
	  create_bd_addr_seg -range 4K -offset $addr_base [get_bd_addr_spaces zynq_mpsoc/Data] \
	          [get_bd_addr_segs axi_nvme_qe_dma_$i/S_AXI_LITE/reg0] SEG_NVME_QE_DMA_$i 
	  set addr_base $addr

          set addr [expr $intc_addr_base + 0x1000]
          create_bd_addr_seg -range 0x1000 -offset $intc_addr_base [get_bd_addr_spaces zynq_mpsoc/Data] \
                  [get_bd_addr_segs axi_nvme_qe_dma_intc_$i/s_axi/Reg] QE_DMA_INTC_$i
          set intc_addr_base $addr
  
	  ## DMA interface
	  create_bd_addr_seg -range 0x80000000 -offset 0x0 [get_bd_addr_spaces axi_nvme_qe_dma_$i/M_AXI_SG] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_LOW] HP1_DDR_LOW_SG_$i
	  create_bd_addr_seg -range 0x400000000 -offset 0x800000000 [get_bd_addr_spaces axi_nvme_qe_dma_$i/M_AXI_SG] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_HIGH] HP1_DDR_HIGH_SG_$i

	  create_bd_addr_seg -range 0x80000000 -offset 0x0 [get_bd_addr_spaces axi_nvme_qe_dma_$i/M_AXI_S2MM] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_LOW] HP1_DDR_LOW_S2MM_$i
	  create_bd_addr_seg -range 0x400000000 -offset 0x800000000 [get_bd_addr_spaces axi_nvme_qe_dma_$i/M_AXI_S2MM] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_HIGH] HP1_DDR_HIGH_S2MM_$i

        create_bd_addr_seg -range 0x80000000 -offset 0x0 [get_bd_addr_spaces axi_nvme_qe_dma_$i/M_AXI_MM2S] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_LOW] HP1_DDR_LOW_MM2S_$i
        create_bd_addr_seg -range 0x400000000 -offset 0x800000000 [get_bd_addr_spaces axi_nvme_qe_dma_$i/M_AXI_MM2S] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_HIGH] HP1_DDR_HIGH_MM2S_$i

	  
	  incr i 1
  }
  

  create_bd_addr_seg -range 0x1000 -offset $intc_addr_base [get_bd_addr_spaces zynq_mpsoc/Data] \
        [get_bd_addr_segs axi_mcdma_intr/s_axi/Reg] MCDMA_INTR

assign_bd_address -target_address_space /axi_nvme_qe_dma_0/M_AXI_MM2S [get_bd_addr_segs u_qdma_ep/S_AXI_BRIDGE_0/reg0] -force
assign_bd_address -target_address_space /axi_nvme_qe_dma_0/M_AXI_S2MM [get_bd_addr_segs u_xdma_ep/S_AXI_BRIDGE_0/reg0] -force



create_bd_addr_seg -range 0x80000000 -offset 0x0 [get_bd_addr_spaces axi_datamover_0/Data_MM2S] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_LOW] HP1_DDR_LOW_CE
create_bd_addr_seg -range 0x400000000 -offset 0x800000000 [get_bd_addr_spaces axi_datamover_0/Data_MM2S] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_HIGH] HP1_DDR_HIGH_CE

create_bd_addr_seg -range 0x80000000 -offset 0x0 [get_bd_addr_spaces axi_datamover_0/Data_S2MM] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_LOW] HP1_DDR_LOW_CE
create_bd_addr_seg -range 0x400000000 -offset 0x800000000 [get_bd_addr_spaces axi_datamover_0/Data_S2MM] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_HIGH] HP1_DDR_HIGH_CE

create_bd_addr_seg -range 0x80000000 -offset 0x0 [get_bd_addr_spaces u_accframework/axi_mem_access] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_LOW] HP1_DDR_LOW_CE
create_bd_addr_seg -range 0x400000000 -offset 0x800000000 [get_bd_addr_spaces u_accframework/axi_mem_access] [get_bd_addr_segs zynq_mpsoc/SAXIGP1/HPC1_DDR_HIGH] HP1_DDR_HIGH_CE

create_bd_addr_seg -range 64K -offset 0xB0070000 [get_bd_addr_spaces zynq_mpsoc/Data] [get_bd_addr_segs u_accframework/s_axi_Manager/reg0] ACCFRAMEWORK_REG0

assign_bd_address -offset 0xB1030000 -range 64K [get_bd_addr_segs axi_gpio_pfch_tag_0/S_AXI/Reg]
assign_bd_address -offset 0xB1040000 -range 64K [get_bd_addr_segs axi_gpio_pfch_tag_1/S_AXI/Reg]
assign_bd_address -offset 0xB1050000 -range 64K [get_bd_addr_segs axi_gpio_pfch_tag_2/S_AXI/Reg]
assign_bd_address -offset 0xB1060000 -range 64K [get_bd_addr_segs axi_gpio_pfch_tag_3/S_AXI/Reg]

# Must be added after bd is validated: https://support.xilinx.com/s/article/72995?language=en_US




validate_bd_design



connect_bd_net [get_bd_pins axis_ic_qdma_c2h_data/M00_axis_tid] \
      [get_bd_pins u_qdma_ep_axis_wrapper/S_AXIS_C2H_tid] 

connect_bd_net [get_bd_pins axis_ic_qdma_c2h_data/M00_axis_tready] \
      [get_bd_pins u_qdma_ep_axis_wrapper/S_AXIS_C2H_tready] 

connect_bd_net [get_bd_pins axis_ic_qdma_c2h_data/M00_axis_tvalid] \
      [get_bd_pins u_qdma_ep_axis_wrapper/S_AXIS_C2H_tvalid] 

connect_bd_net [get_bd_pins axis_ic_qdma_c2h_data/M00_axis_tlast] \
      [get_bd_pins u_qdma_ep_axis_wrapper/S_AXIS_C2H_tlast] 

connect_bd_net [get_bd_pins u_qdma_ep_axis_wrapper/m_axis_h2c_tuser] \
      [get_bd_pins axis_ic_qdma_h2c/S00_AXIS_tuser]

connect_bd_net [get_bd_pins axis_ic_qdma_h2c/M00_AXIS_tdest] \
      [get_bd_pins qid_slice_to_tdest/Din]


connect_bd_net [get_bd_pins axis_ic_merge_nvmeq_and_compute/S00_AXIS_tdest] \
      [get_bd_pins qid_slice_to_tdest/Dout]




#=============================================
# Finish BD creation 
#=============================================

  # Restore current instance
  current_bd_instance $oldCurInst

  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""


