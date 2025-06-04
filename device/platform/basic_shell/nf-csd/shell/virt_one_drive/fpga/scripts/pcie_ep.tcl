
################################################################
# This is a generated script based on design: pcie_ep
#
# Though there are limitations about the generated script,
# the main purpose of this utility is to make learning
# IP Integrator Tcl commands easier.
################################################################

namespace eval _tcl {
proc get_script_folder {} {
   set script_path [file normalize [info script]]
   set script_folder [file dirname $script_path]
   return $script_folder
}
}
variable script_folder
set script_folder [_tcl::get_script_folder]

################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2020.2
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   catch {common::send_gid_msg -ssname BD::TCL -id 2041 -severity "ERROR" "This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Please run the script in Vivado <$scripts_vivado_version> then open the design in Vivado <$current_vivado_version>. Upgrade the design by running \"Tools => Report => Report IP Status...\", then run write_bd_tcl to create an updated script."}

   return 1
}

################################################################
# START
################################################################

# To test this script, run the following commands from Vivado Tcl console:
# source pcie_ep_script.tcl

# If there is no project opened, this script will create a
# project, but make sure you do not have an existing project
# <./myproj/project_1.xpr> in the current working folder.

set list_projs [get_projects -quiet]
if { $list_projs eq "" } {
   create_project project_1 myproj -part xczu19eg-ffvc1760-2-i
   set_property BOARD_PART fidus:none:part0:2.0 [current_project]
}


# CHANGE DESIGN NAME HERE
variable design_name
set design_name pcie_ep

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

if { ${design_name} eq "" } {
   # USE CASES:
   #    1) Design_name not set

   set errMsg "Please set the variable <design_name> to a non-empty value."
   set nRet 1

} elseif { ${cur_design} ne "" && ${list_cells} eq "" } {
   # USE CASES:
   #    2): Current design opened AND is empty AND names same.
   #    3): Current design opened AND is empty AND names diff; design_name NOT in project.
   #    4): Current design opened AND is empty AND names diff; design_name exists in project.

   if { $cur_design ne $design_name } {
      common::send_gid_msg -ssname BD::TCL -id 2001 -severity "INFO" "Changing value of <design_name> from <$design_name> to <$cur_design> since current design is empty."
      set design_name [get_property NAME $cur_design]
   }
   common::send_gid_msg -ssname BD::TCL -id 2002 -severity "INFO" "Constructing design in IPI design <$cur_design>..."

} elseif { ${cur_design} ne "" && $list_cells ne "" && $cur_design eq $design_name } {
   # USE CASES:
   #    5) Current design opened AND has components AND same names.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 1
} elseif { [get_files -quiet ${design_name}.bd] ne "" } {
   # USE CASES: 
   #    6) Current opened design, has components, but diff names, design_name exists in project.
   #    7) No opened design, design_name exists in project.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 2

} else {
   # USE CASES:
   #    8) No opened design, design_name not in project.
   #    9) Current opened design, has components, but diff names, design_name not in project.

   common::send_gid_msg -ssname BD::TCL -id 2003 -severity "INFO" "Currently there is no design <$design_name> in project, so creating one..."

   create_bd_design $design_name

   common::send_gid_msg -ssname BD::TCL -id 2004 -severity "INFO" "Making design <$design_name> as current_bd_design."
   current_bd_design $design_name

}

common::send_gid_msg -ssname BD::TCL -id 2005 -severity "INFO" "Currently the variable <design_name> is equal to \"$design_name\"."

if { $nRet != 0 } {
   catch {common::send_gid_msg -ssname BD::TCL -id 2006 -severity "ERROR" $errMsg}
   return $nRet
}

set bCheckIPsPassed 1
##################################################################
# CHECK IPs
##################################################################
set bCheckIPs 1
if { $bCheckIPs == 1 } {
   set list_check_ips "\ 
xilinx.com:ip:qdma:4.0\
xilinx.com:ip:xlconstant:1.1\
"

   set list_ips_missing ""
   common::send_gid_msg -ssname BD::TCL -id 2011 -severity "INFO" "Checking if the following IPs exist in the project's IP catalog: $list_check_ips ."

   foreach ip_vlnv $list_check_ips {
      set ip_obj [get_ipdefs -all $ip_vlnv]
      if { $ip_obj eq "" } {
         lappend list_ips_missing $ip_vlnv
      }
   }

   if { $list_ips_missing ne "" } {
      catch {common::send_gid_msg -ssname BD::TCL -id 2012 -severity "ERROR" "The following IPs are not found in the IP Catalog:\n  $list_ips_missing\n\nResolution: Please add the repository containing the IP(s) to the project." }
      set bCheckIPsPassed 0
   }

}

if { $bCheckIPsPassed != 1 } {
  common::send_gid_msg -ssname BD::TCL -id 2023 -severity "WARNING" "Will not continue with creation of design due to the error(s) above."
  return 3
}

##################################################################
# DESIGN PROCs
##################################################################



# Procedure to create entire design; Provide argument to make
# procedure reusable. If parentCell is "", will use root.
proc create_root_design { parentCell } {

  variable script_folder
  variable design_name

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2090 -severity "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2091 -severity "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports
  set M_AXI_BRIDGE_0 [ create_bd_intf_port -mode Master -vlnv xilinx.com:interface:aximm_rtl:1.0 M_AXI_BRIDGE_0 ]
  set_property -dict [ list \
   CONFIG.ADDR_WIDTH {64} \
   CONFIG.DATA_WIDTH {512} \
   CONFIG.HAS_BURST {0} \
   CONFIG.HAS_QOS {0} \
   CONFIG.HAS_REGION {0} \
   CONFIG.NUM_READ_OUTSTANDING {32} \
   CONFIG.NUM_WRITE_OUTSTANDING {32} \
   CONFIG.PROTOCOL {AXI4} \
   ] $M_AXI_BRIDGE_0

  set S_AXI_BRIDGE_0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:interface:aximm_rtl:1.0 S_AXI_BRIDGE_0 ]
  set_property -dict [ list \
   CONFIG.ADDR_WIDTH {64} \
   CONFIG.ARUSER_WIDTH {12} \
   CONFIG.AWUSER_WIDTH {12} \
   CONFIG.BUSER_WIDTH {0} \
   CONFIG.DATA_WIDTH {512} \
   CONFIG.HAS_BRESP {1} \
   CONFIG.HAS_BURST {1} \
   CONFIG.HAS_CACHE {0} \
   CONFIG.HAS_LOCK {0} \
   CONFIG.HAS_PROT {0} \
   CONFIG.HAS_QOS {0} \
   CONFIG.HAS_REGION {1} \
   CONFIG.HAS_RRESP {1} \
   CONFIG.HAS_WSTRB {1} \
   CONFIG.ID_WIDTH {4} \
   CONFIG.MAX_BURST_LENGTH {256} \
   CONFIG.NUM_READ_OUTSTANDING {8} \
   CONFIG.NUM_READ_THREADS {1} \
   CONFIG.NUM_WRITE_OUTSTANDING {8} \
   CONFIG.NUM_WRITE_THREADS {1} \
   CONFIG.PROTOCOL {AXI4} \
   CONFIG.READ_WRITE_MODE {READ_WRITE} \
   CONFIG.RUSER_BITS_PER_BYTE {0} \
   CONFIG.RUSER_WIDTH {64} \
   CONFIG.SUPPORTS_NARROW_BURST {0} \
   CONFIG.WUSER_BITS_PER_BYTE {0} \
   CONFIG.WUSER_WIDTH {64} \
   ] $S_AXI_BRIDGE_0

  set c2h_byp_in_st_0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:display_eqdma:c2h_byp_in_st_rtl:1.0 c2h_byp_in_st_0 ]

  set c2h_byp_out_0 [ create_bd_intf_port -mode Master -vlnv xilinx.com:display_eqdma:c2h_byp_out_rtl:1.0 c2h_byp_out_0 ]

  set dsc_crdt_in_0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:display_eqdma:dsc_crdt_in_rtl:1.0 dsc_crdt_in_0 ]

  set h2c_byp_in_st_0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:display_eqdma:h2c_byp_in_st_rtl:1.0 h2c_byp_in_st_0 ]

  set h2c_byp_out_0 [ create_bd_intf_port -mode Master -vlnv xilinx.com:display_eqdma:h2c_byp_out_rtl:1.0 h2c_byp_out_0 ]

  set m_axis_h2c_0 [ create_bd_intf_port -mode Master -vlnv xilinx.com:display_eqdma:m_axis_h2c_rtl:1.0 m_axis_h2c_0 ]

  set s_axis_c2h_0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:display_eqdma:s_axis_c2h_rtl:1.0 s_axis_c2h_0 ]

  set s_axis_c2h_cmpt_0 [ create_bd_intf_port -mode Slave -vlnv xilinx.com:display_eqdma:s_axis_c2h_cmpt_rtl:1.0 s_axis_c2h_cmpt_0 ]

  set tm_dsc_sts_0 [ create_bd_intf_port -mode Master -vlnv xilinx.com:display_eqdma:tm_dsc_sts_rtl:1.0 tm_dsc_sts_0 ]


  # Create ports
  set axi_aclk [ create_bd_port -dir O -type clk axi_aclk ]
  set_property -dict [ list \
   CONFIG.ASSOCIATED_BUSIF {M_AXI_BRIDGE_0:S_AXI_BRIDGE_0} \
   CONFIG.ASSOCIATED_RESET {axi_aresetn} \
 ] $axi_aclk
  set_property CONFIG.ASSOCIATED_BUSIF.VALUE_SRC DEFAULT $axi_aclk

  set axi_aresetn [ create_bd_port -dir O -type rst axi_aresetn ]
  set pcie_ep_rxn [ create_bd_port -dir I -from 15 -to 0 pcie_ep_rxn ]
  set pcie_ep_rxp [ create_bd_port -dir I -from 15 -to 0 pcie_ep_rxp ]
  set pcie_ep_txn [ create_bd_port -dir O -from 15 -to 0 pcie_ep_txn ]
  set pcie_ep_txp [ create_bd_port -dir O -from 15 -to 0 pcie_ep_txp ]
  set sys_clk [ create_bd_port -dir I -type clk sys_clk ]
  set sys_clk_gt [ create_bd_port -dir I -type clk sys_clk_gt ]
  set sys_rst_n [ create_bd_port -dir I -type rst sys_rst_n ]

  # Create instance: qdma_ep, and set properties
  set qdma_ep [ create_bd_cell -type ip -vlnv xilinx.com:ip:qdma:4.0 qdma_ep ]
  set_property -dict [ list \
   CONFIG.PF0_MSIX_CAP_TABLE_SIZE_qdma {01f} \
   CONFIG.axibar_notranslate {true} \
   CONFIG.axilite_master_en {true} \
   CONFIG.axist_bypass_en {true} \
   CONFIG.cfg_ext_if {false} \
   CONFIG.cfg_mgmt_if {true} \
   CONFIG.csr_axilite_slave {false} \
   CONFIG.dma_intf_sel_qdma {AXI_Stream_with_Completion} \
   CONFIG.dsc_byp_mode {Descriptor_bypass_and_internal} \
   CONFIG.en_axi_mm_qdma {false} \
   CONFIG.en_bridge_slv {true} \
   CONFIG.en_gt_selection {true} \
   CONFIG.mode_selection {Advanced} \
   CONFIG.pcie_blk_locn {X1Y0} \
   CONFIG.pcie_extended_tag {true} \
   CONFIG.pf0_bar0_64bit_qdma {false} \
   CONFIG.pf0_bar1_64bit_qdma {false} \
   CONFIG.pf0_bar1_enabled_qdma {true} \
   CONFIG.pf0_bar1_scale_qdma {Megabytes} \
   CONFIG.pf0_bar1_size_qdma {4} \
   CONFIG.pf0_bar1_type_qdma {AXI_Lite_Master} \
   CONFIG.pf0_bar2_64bit_qdma {true} \
   CONFIG.pf0_bar2_scale_qdma {Megabytes} \
   CONFIG.pf0_bar2_size_qdma {4} \
   CONFIG.pf0_bar2_type_qdma {AXI_Bridge_Master} \
   CONFIG.pf0_base_class_menu_qdma {Processing_accelerators} \
   CONFIG.pf0_class_code_base_qdma {12} \
   CONFIG.pf0_pciebar2axibar_1 {0x0000000080000000} \
   CONFIG.pf0_pciebar2axibar_2 {0x00000000A0000000} \
   CONFIG.select_quad {GTH_Quad_227} \
   CONFIG.testname {st} \
   CONFIG.vdm_en {true} \
   CONFIG.xdma_axilite_slave {false} \
 ] $qdma_ep

  # Create instance: qdma_ep_soft_rst_n, and set properties
  set qdma_ep_soft_rst_n [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 qdma_ep_soft_rst_n ]
  set_property -dict [ list \
   CONFIG.CONST_VAL {0x1} \
   CONFIG.CONST_WIDTH {1} \
 ] $qdma_ep_soft_rst_n

  # Create instance: ready_set, and set properties
  set ready_set [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 ready_set ]
  set_property -dict [ list \
   CONFIG.CONST_VAL {0x1} \
   CONFIG.CONST_WIDTH {1} \
 ] $ready_set

  # Create interface connections
  connect_bd_intf_net -intf_net S_AXI_BRIDGE_0_1 [get_bd_intf_ports S_AXI_BRIDGE_0] [get_bd_intf_pins qdma_ep/S_AXI_BRIDGE]
  connect_bd_intf_net -intf_net c2h_byp_in_st_0_1 [get_bd_intf_ports c2h_byp_in_st_0] [get_bd_intf_pins qdma_ep/c2h_byp_in_st]
  connect_bd_intf_net -intf_net dsc_crdt_in_0_1 [get_bd_intf_ports dsc_crdt_in_0] [get_bd_intf_pins qdma_ep/dsc_crdt_in]
  connect_bd_intf_net -intf_net h2c_byp_in_st_0_1 [get_bd_intf_ports h2c_byp_in_st_0] [get_bd_intf_pins qdma_ep/h2c_byp_in_st]
  connect_bd_intf_net -intf_net qdma_ep_M_AXI_BRIDGE [get_bd_intf_ports M_AXI_BRIDGE_0] [get_bd_intf_pins qdma_ep/M_AXI_BRIDGE]
  connect_bd_intf_net -intf_net qdma_ep_c2h_byp_out [get_bd_intf_ports c2h_byp_out_0] [get_bd_intf_pins qdma_ep/c2h_byp_out]
  connect_bd_intf_net -intf_net qdma_ep_h2c_byp_out [get_bd_intf_ports h2c_byp_out_0] [get_bd_intf_pins qdma_ep/h2c_byp_out]
  connect_bd_intf_net -intf_net qdma_ep_m_axis_h2c [get_bd_intf_ports m_axis_h2c_0] [get_bd_intf_pins qdma_ep/m_axis_h2c]
  connect_bd_intf_net -intf_net qdma_ep_tm_dsc_sts [get_bd_intf_ports tm_dsc_sts_0] [get_bd_intf_pins qdma_ep/tm_dsc_sts]
  connect_bd_intf_net -intf_net s_axis_c2h_0_1 [get_bd_intf_ports s_axis_c2h_0] [get_bd_intf_pins qdma_ep/s_axis_c2h]
  connect_bd_intf_net -intf_net s_axis_c2h_cmpt_0_1 [get_bd_intf_ports s_axis_c2h_cmpt_0] [get_bd_intf_pins qdma_ep/s_axis_c2h_cmpt]

  # Create port connections
  connect_bd_net -net pcie_ep_rxn_1 [get_bd_ports pcie_ep_rxn] [get_bd_pins qdma_ep/pci_exp_rxn]
  connect_bd_net -net pcie_ep_rxp_1 [get_bd_ports pcie_ep_rxp] [get_bd_pins qdma_ep/pci_exp_rxp]
  connect_bd_net -net qdma_ep_axi_aclk [get_bd_ports axi_aclk] [get_bd_pins qdma_ep/axi_aclk]
  connect_bd_net -net qdma_ep_axi_aresetn [get_bd_ports axi_aresetn] [get_bd_pins qdma_ep/axi_aresetn]
  connect_bd_net -net qdma_ep_pci_exp_txn [get_bd_ports pcie_ep_txn] [get_bd_pins qdma_ep/pci_exp_txn]
  connect_bd_net -net qdma_ep_pci_exp_txp [get_bd_ports pcie_ep_txp] [get_bd_pins qdma_ep/pci_exp_txp]
  connect_bd_net -net qdma_ep_soft_rst_n_dout [get_bd_pins qdma_ep/soft_reset_n] [get_bd_pins qdma_ep_soft_rst_n/dout]
  connect_bd_net -net ready_set_dout [get_bd_pins qdma_ep/qsts_out_rdy] [get_bd_pins ready_set/dout]
  connect_bd_net -net sys_clk_1 [get_bd_ports sys_clk] [get_bd_pins qdma_ep/sys_clk]
  connect_bd_net -net sys_clk_gt_1 [get_bd_ports sys_clk_gt] [get_bd_pins qdma_ep/sys_clk_gt]
  connect_bd_net -net sys_rst_n_1 [get_bd_ports sys_rst_n] [get_bd_pins qdma_ep/sys_rst_n]

  # Create address segments
  assign_bd_address -offset 0x00000000 -range 0x00010000000000000000 -target_address_space [get_bd_addr_spaces S_AXI_BRIDGE_0] [get_bd_addr_segs qdma_ep/S_AXI_BRIDGE/BAR0] -force


  # Restore current instance
  current_bd_instance $oldCurInst

  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""


common::send_gid_msg -ssname BD::TCL -id 2053 -severity "WARNING" "This Tcl script was generated from a block design that has not been validated. It is possible that design <$design_name> may result in errors during validation."

