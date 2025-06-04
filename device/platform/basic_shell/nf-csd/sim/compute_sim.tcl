
################################################################
# This is a generated script based on design: compute_sim
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
# source compute_sim_script.tcl


# The design that will be created by this Tcl script contains the following 
# module references:
# ar_to_bd_pktizer, axis_aw_w_splitter, axis_debug_reg, compute_c2h_merger, compute_engine, context_manager, context_manager, context_manager, context_manager, dram_reader, exec_engine, op_incr_1, op_incr_2, op_incr_4, prp_fetcher, qdma_c2h_byp_ctrl, qdma_ep_sim, qdma_h2c_byp_ctrl

# Please add the sources of those modules before sourcing this Tcl script.

# If there is no project opened, this script will create a
# project, but make sure you do not have an existing project
# <./myproj/project_1.xpr> in the current working folder.

set list_projs [get_projects -quiet]
if { $list_projs eq "" } {
   create_project project_1 myproj -part xc7vx485tffg1157-1
}


# CHANGE DESIGN NAME HERE
variable design_name
set design_name compute_sim

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

add_files -norecurse -fileset sources_1 $script_folder/../shell/virt_one_drive/fpga/sources/hdl/
add_files -norecurse $script_folder/../sim/qdma_ep_sim.v

set_property SOURCE_SET sources_1 [get_filesets sim_1]
add_files -norecurse -fileset sim_1 $script_folder/../sim/nvme_sim_tb.sv

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
xilinx.com:ip:axi_datamover:5.1\
xilinx.com:ip:axi_vip:1.1\
xilinx.com:ip:axis_data_fifo:2.0\
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

##################################################################
# CHECK Modules
##################################################################
set bCheckModules 1
if { $bCheckModules == 1 } {
   set list_check_mods "\ 
ar_to_bd_pktizer\
axis_aw_w_splitter\
axis_debug_reg\
compute_c2h_merger\
compute_engine\
context_manager\
context_manager\
context_manager\
context_manager\
dram_reader\
dram_writer\
exec_engine\
op_incr_1\
op_incr_2\
op_incr_4\
prp_fetcher\
qdma_c2h_byp_ctrl\
qdma_ep_sim\
qdma_h2c_byp_ctrl\
"

   set list_mods_missing ""
   common::send_gid_msg -ssname BD::TCL -id 2020 -severity "INFO" "Checking if the following modules exist in the project's sources: $list_check_mods ."

   foreach mod_vlnv $list_check_mods {
      if { [can_resolve_reference $mod_vlnv] == 0 } {
         lappend list_mods_missing $mod_vlnv
      }
   }

   if { $list_mods_missing ne "" } {
      catch {common::send_gid_msg -ssname BD::TCL -id 2021 -severity "ERROR" "The following module(s) are not found in the project: $list_mods_missing" }
      common::send_gid_msg -ssname BD::TCL -id 2022 -severity "INFO" "Please add source files for the missing module(s) above."
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

  # Create ports
  set aclk_0 [ create_bd_port -dir I -type clk aclk_0 ]
  set aresetn_0 [ create_bd_port -dir I -type rst aresetn_0 ]

  # Create instance: ar_to_bd_pktizer_0, and set properties
  set block_name ar_to_bd_pktizer
  set block_cell_name ar_to_bd_pktizer_0
  if { [catch {set ar_to_bd_pktizer_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $ar_to_bd_pktizer_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: axi_datamover_0, and set properties
  set axi_datamover_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_datamover:5.1 axi_datamover_0 ]
  set_property -dict [ list \
   CONFIG.c_addr_width {40} \
   CONFIG.c_dummy {1} \
   CONFIG.c_m_axi_mm2s_data_width {512} \
   CONFIG.c_m_axis_mm2s_tdata_width {512} \
   CONFIG.c_s_axi_s2mm_data_width {512} \
   CONFIG.c_s_axis_s2mm_tdata_width {512} \
   CONFIG.c_mm2s_btt_used {23} \
   CONFIG.c_mm2s_burst_size {64} \
   CONFIG.c_s2mm_btt_used {23} \
   CONFIG.c_s2mm_burst_size {64} \
 ] $axi_datamover_0

  # Create instance: axi_interconnect_0, and set properties
  set axi_interconnect_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_0 ]
  set_property -dict [ list \
   CONFIG.NUM_MI {1} \
   CONFIG.NUM_SI {3} \
 ] $axi_interconnect_0

  # Create instance: axi_vip_0, and set properties
  set axi_vip_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_vip:1.1 axi_vip_0 ]
  set_property -dict [ list \
   CONFIG.DATA_WIDTH {128} \
   CONFIG.INTERFACE_MODE {SLAVE} \
 ] $axi_vip_0

  # Create instance: axi_vip_1, and set properties
  set axi_vip_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_vip:1.1 axi_vip_1 ]
  set_property -dict [ list \
   CONFIG.ADDR_WIDTH {16} \
   CONFIG.ARUSER_WIDTH {0} \
   CONFIG.AWUSER_WIDTH {0} \
   CONFIG.BUSER_WIDTH {0} \
   CONFIG.DATA_WIDTH {64} \
   CONFIG.HAS_BRESP {1} \
   CONFIG.HAS_BURST {0} \
   CONFIG.HAS_CACHE {0} \
   CONFIG.HAS_LOCK {0} \
   CONFIG.HAS_PROT {1} \
   CONFIG.HAS_QOS {0} \
   CONFIG.HAS_REGION {0} \
   CONFIG.HAS_RRESP {1} \
   CONFIG.HAS_WSTRB {1} \
   CONFIG.ID_WIDTH {0} \
   CONFIG.INTERFACE_MODE {MASTER} \
   CONFIG.PROTOCOL {AXI4LITE} \
   CONFIG.READ_WRITE_MODE {READ_WRITE} \
   CONFIG.RUSER_BITS_PER_BYTE {0} \
   CONFIG.RUSER_WIDTH {0} \
   CONFIG.SUPPORTS_NARROW {0} \
   CONFIG.WUSER_BITS_PER_BYTE {0} \
   CONFIG.WUSER_WIDTH {0} \
 ] $axi_vip_1

  # Create instance: axis_aw_w_splitter_0, and set properties
  set block_name axis_aw_w_splitter
  set block_cell_name axis_aw_w_splitter_0
  if { [catch {set axis_aw_w_splitter_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $axis_aw_w_splitter_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: axis_data_fifo_0, and set properties
  set axis_data_fifo_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 axis_data_fifo_0 ]
  set_property -dict [ list \
   CONFIG.FIFO_DEPTH {16} \
 ] $axis_data_fifo_0

  set sqe_finished_concat [ create_bd_cell -type ip -vlnv xilinx.com:ip:xlconcat:2.1 sqe_finished_concat ]
  set_property -dict [list CONFIG.NUM_PORTS {4}] $sqe_finished_concat

  # Create instance: axis_debug_reg_0, and set properties
  set block_name axis_debug_reg
  set block_cell_name axis_debug_reg_0
  if { [catch {set axis_debug_reg_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $axis_debug_reg_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: axis_interconnect_0, and set properties
  set axis_interconnect_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axis_interconnect:2.1 axis_interconnect_0 ]
  set_property -dict [ list \
   CONFIG.NUM_MI {5} \
   CONFIG.NUM_SI {4} \
   CONFIG.S00_FIFO_DEPTH {64} \
   CONFIG.S01_FIFO_DEPTH {64} \
   CONFIG.S02_FIFO_DEPTH {64} \
   CONFIG.S03_FIFO_DEPTH {64} \
 ] $axis_interconnect_0

  # Create instance: compute_c2h_merger_0, and set properties
  set block_name compute_c2h_merger
  set block_cell_name compute_c2h_merger_0
  if { [catch {set compute_c2h_merger_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $compute_c2h_merger_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: compute_engine_0, and set properties
  set block_name compute_engine
  set block_cell_name compute_engine_0
  if { [catch {set compute_engine_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $compute_engine_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
    set_property -dict [ list \
   CONFIG.DATA_WIDTH_SLAVE {64} \
 ] $compute_engine_0

  # Create instance: context_manager_0, and set properties
  set block_name context_manager
  set block_cell_name context_manager_0
  if { [catch {set context_manager_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $context_manager_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: context_manager_1, and set properties
  set block_name context_manager
  set block_cell_name context_manager_1
  if { [catch {set context_manager_1 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $context_manager_1 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: context_manager_2, and set properties
  set block_name context_manager
  set block_cell_name context_manager_2
  if { [catch {set context_manager_2 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $context_manager_2 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: context_manager_3, and set properties
  set block_name context_manager
  set block_cell_name context_manager_3
  if { [catch {set context_manager_3 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $context_manager_3 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: dram_reader_0, and set properties
  set block_name dram_reader
  set block_cell_name dram_reader_0
  if { [catch {set dram_reader_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $dram_reader_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: dram_writer_0, and set properties
  set block_name dram_writer
  set block_cell_name dram_writer_0
  if { [catch {set dram_writer_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
    catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
    return 1
  } elseif { $dram_writer_0 eq "" } {
    catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
    return 1
  }

  # Create instance: exec_engine_0, and set properties
  set block_name exec_engine
  set block_cell_name exec_engine_0
  if { [catch {set exec_engine_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $exec_engine_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: op_incr_1_0, and set properties
  set block_name op_incr_1
  set block_cell_name op_incr_1_0
  if { [catch {set op_incr_1_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $op_incr_1_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: op_incr_2_0, and set properties
  set block_name op_incr_2
  set block_cell_name op_incr_2_0
  if { [catch {set op_incr_2_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $op_incr_2_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: op_incr_4_0, and set properties
  set block_name op_incr_4
  set block_cell_name op_incr_4_0
  if { [catch {set op_incr_4_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $op_incr_4_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: prp_fetcher_0, and set properties
  set block_name prp_fetcher
  set block_cell_name prp_fetcher_0
  if { [catch {set prp_fetcher_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $prp_fetcher_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: qdma_c2h_byp_ctrl_0, and set properties
  set block_name qdma_c2h_byp_ctrl
  set block_cell_name qdma_c2h_byp_ctrl_0
  if { [catch {set qdma_c2h_byp_ctrl_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $qdma_c2h_byp_ctrl_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: qdma_ep_sim_0, and set properties
  set block_name qdma_ep_sim
  set block_cell_name qdma_ep_sim_0
  if { [catch {set qdma_ep_sim_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $qdma_ep_sim_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create instance: qdma_h2c_byp_ctrl_0, and set properties
  set block_name qdma_h2c_byp_ctrl
  set block_cell_name qdma_h2c_byp_ctrl_0
  if { [catch {set qdma_h2c_byp_ctrl_0 [create_bd_cell -type module -reference $block_name $block_cell_name] } errmsg] } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2095 -severity "ERROR" "Unable to add referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   } elseif { $qdma_h2c_byp_ctrl_0 eq "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2096 -severity "ERROR" "Unable to referenced block <$block_name>. Please add the files for ${block_name}'s definition into the project."}
     return 1
   }
  
  # Create interface connections
  connect_bd_intf_net -intf_net S00_AXI_1 [get_bd_intf_pins axi_datamover_0/M_AXI_MM2S] [get_bd_intf_pins axi_interconnect_0/S00_AXI]
  connect_bd_intf_net -intf_net S01_AXIS_1 [get_bd_intf_pins axis_interconnect_0/S01_AXIS] [get_bd_intf_pins context_manager_1/m_axis]
  connect_bd_intf_net -intf_net S01_AXI_1 [get_bd_intf_pins axi_interconnect_0/S01_AXI] [get_bd_intf_pins compute_engine_0/m_axi]
  connect_bd_intf_net -intf_net S02_AXIS_1 [get_bd_intf_pins axis_interconnect_0/S02_AXIS] [get_bd_intf_pins context_manager_2/m_axis]
  connect_bd_intf_net -intf_net ar_to_bd_pktizer_0_m_axis_h2c_byp_st [get_bd_intf_pins ar_to_bd_pktizer_0/m_axis_h2c_byp_st] [get_bd_intf_pins qdma_c2h_byp_ctrl_0/s_axis_c2h_byp_in]
  connect_bd_intf_net -intf_net axi_datamover_0_M_AXIS_MM2S [get_bd_intf_pins axi_datamover_0/M_AXIS_MM2S] [get_bd_intf_pins dram_reader_0/s_axis_mm2s]
  connect_bd_intf_net -intf_net axi_datamover_0_M_AXIS_MM2S_STS [get_bd_intf_pins axi_datamover_0/M_AXIS_MM2S_STS] [get_bd_intf_pins dram_reader_0/s_axis_mm2s_sts]
  connect_bd_intf_net -intf_net axi_interconnect_0_M00_AXI [get_bd_intf_pins axi_interconnect_0/M00_AXI] [get_bd_intf_pins axi_vip_0/S_AXI]
  connect_bd_intf_net -intf_net axi_vip_1_M_AXI [get_bd_intf_pins axi_vip_1/M_AXI] [get_bd_intf_pins compute_engine_0/s_axi]
  connect_bd_intf_net -intf_net axis_aw_w_splitter_0_m_axis [get_bd_intf_pins axis_aw_w_splitter_0/m_axis] [get_bd_intf_pins qdma_ep_sim_0/s_axis_c2h_0]
  connect_bd_intf_net -intf_net axis_aw_w_splitter_0_m_axis_ar_req [get_bd_intf_pins ar_to_bd_pktizer_0/s_axis_ar_req] [get_bd_intf_pins axis_aw_w_splitter_0/m_axis_ar_req]
  connect_bd_intf_net -intf_net axis_data_fifo_0_M_AXIS [get_bd_intf_pins axis_aw_w_splitter_0/s_axis] [get_bd_intf_pins axis_data_fifo_0/M_AXIS]
  connect_bd_intf_net -intf_net axis_interconnect_0_M00_AXIS [get_bd_intf_pins axis_interconnect_0/M00_AXIS] [get_bd_intf_pins compute_c2h_merger_0/s_axis_c2h]
  connect_bd_intf_net -intf_net axis_interconnect_0_M01_AXIS [get_bd_intf_pins axis_interconnect_0/M01_AXIS] [get_bd_intf_pins op_incr_1_0/s_axis]
  connect_bd_intf_net -intf_net axis_interconnect_0_M02_AXIS [get_bd_intf_pins axis_interconnect_0/M02_AXIS] [get_bd_intf_pins op_incr_2_0/s_axis]
  connect_bd_intf_net -intf_net axis_interconnect_0_M03_AXIS [get_bd_intf_pins axis_interconnect_0/M03_AXIS] [get_bd_intf_pins op_incr_4_0/s_axis]
  connect_bd_intf_net -intf_net axis_interconnect_0_M04_AXIS [get_bd_intf_pins axis_interconnect_0/M04_AXIS] [get_bd_intf_pins dram_writer_0/s_axis_data]
  connect_bd_intf_net -intf_net compute_c2h_merger_0_m_axis_c2h [get_bd_intf_pins axis_data_fifo_0/S_AXIS] [get_bd_intf_pins compute_c2h_merger_0/m_axis_c2h]
  connect_bd_intf_net -intf_net compute_engine_0_m_axis_sqe [get_bd_intf_pins compute_engine_0/m_axis_sqe] [get_bd_intf_pins exec_engine_0/s_axis_sqe]
  connect_bd_intf_net -intf_net context_manager_0_m_axis [get_bd_intf_pins axis_interconnect_0/S00_AXIS] [get_bd_intf_pins context_manager_0/m_axis]
  connect_bd_intf_net -intf_net context_manager_3_m_axis [get_bd_intf_pins axis_interconnect_0/S03_AXIS] [get_bd_intf_pins context_manager_3/m_axis]
  connect_bd_intf_net -intf_net dram_reader_0_m_axis_data [get_bd_intf_pins context_manager_0/s_axis] [get_bd_intf_pins dram_reader_0/m_axis_data]
  connect_bd_intf_net -intf_net dram_reader_0_m_axis_mm2s_cmd [get_bd_intf_pins axi_datamover_0/S_AXIS_MM2S_CMD] [get_bd_intf_pins dram_reader_0/m_axis_mm2s_cmd]
  connect_bd_intf_net -intf_net exec_engine_0_m_axis_cqe [get_bd_intf_pins compute_engine_0/s_axis_cqe] [get_bd_intf_pins exec_engine_0/m_axis_cqe]
  connect_bd_intf_net -intf_net exec_engine_0_m_axis_mem_rd_req [get_bd_intf_pins dram_reader_0/s_axis_req] [get_bd_intf_pins exec_engine_0/m_axis_mem_rd_req]
  connect_bd_intf_net -intf_net exec_engine_0_m_axis_prp_fetch [get_bd_intf_pins exec_engine_0/m_axis_prp_fetch] [get_bd_intf_pins prp_fetcher_0/s_axis_prp_fetch]
  connect_bd_intf_net -intf_net op_incr_1_0_m_axis [get_bd_intf_pins context_manager_1/s_axis] [get_bd_intf_pins op_incr_1_0/m_axis]
  connect_bd_intf_net -intf_net op_incr_2_0_m_axis [get_bd_intf_pins context_manager_2/s_axis] [get_bd_intf_pins op_incr_2_0/m_axis]
  connect_bd_intf_net -intf_net op_incr_4_0_m_axis [get_bd_intf_pins context_manager_3/s_axis] [get_bd_intf_pins op_incr_4_0/m_axis]
  connect_bd_intf_net -intf_net prp_fetcher_0_m_axis_h2c_byp_in [get_bd_intf_pins prp_fetcher_0/m_axis_h2c_byp_in] [get_bd_intf_pins qdma_h2c_byp_ctrl_0/s_axis_h2c_byp_in]
  connect_bd_intf_net -intf_net prp_fetcher_0_m_axis_prp_out [get_bd_intf_pins compute_c2h_merger_0/s_axis_prp_out] [get_bd_intf_pins prp_fetcher_0/m_axis_prp_out]
  connect_bd_intf_net -intf_net qdma_ep_sim_0_m_axis_h2c_0 [get_bd_intf_pins prp_fetcher_0/s_axis_h2c] [get_bd_intf_pins qdma_ep_sim_0/m_axis_h2c_0]
  connect_bd_intf_net [get_bd_intf_pins exec_engine_0/m_axis_mem_wr_req] [get_bd_intf_pins dram_writer_0/s_axis_req]
  connect_bd_intf_net [get_bd_intf_pins dram_writer_0/m_axis_s2mm_cmd] [get_bd_intf_pins axi_datamover_0/S_AXIS_S2MM_CMD]
  connect_bd_intf_net [get_bd_intf_pins dram_writer_0/m_axis_s2mm] [get_bd_intf_pins axi_datamover_0/S_AXIS_S2MM]
  connect_bd_intf_net [get_bd_intf_pins axi_datamover_0/M_AXI_S2MM] [get_bd_intf_pins axi_interconnect_0/S02_AXI]
  connect_bd_intf_net [get_bd_intf_pins axi_datamover_0/M_AXIS_S2MM_STS] [get_bd_intf_pins dram_writer_0/s_axis_s2mm_sts]

  # Create port connections
  connect_bd_net -net aclk_0_1 [get_bd_ports aclk_0] [get_bd_pins axi_datamover_0/m_axi_mm2s_aclk] [get_bd_pins axi_datamover_0/m_axi_s2mm_aclk] [get_bd_pins axi_datamover_0/m_axis_mm2s_cmdsts_aclk] [get_bd_pins axi_datamover_0/m_axis_s2mm_cmdsts_awclk] [get_bd_pins axi_interconnect_0/ACLK] [get_bd_pins axi_interconnect_0/M00_ACLK] [get_bd_pins axi_interconnect_0/S00_ACLK] [get_bd_pins axi_interconnect_0/S01_ACLK] [get_bd_pins axi_interconnect_0/S02_ACLK] [get_bd_pins axi_vip_0/aclk] [get_bd_pins axi_vip_1/aclk] [get_bd_pins axis_aw_w_splitter_0/aclk] [get_bd_pins axis_data_fifo_0/s_axis_aclk] [get_bd_pins axis_debug_reg_0/aclk] [get_bd_pins axis_interconnect_0/ACLK] [get_bd_pins axis_interconnect_0/M00_AXIS_ACLK] [get_bd_pins axis_interconnect_0/M01_AXIS_ACLK] [get_bd_pins axis_interconnect_0/M02_AXIS_ACLK] [get_bd_pins axis_interconnect_0/M03_AXIS_ACLK] [get_bd_pins axis_interconnect_0/M04_AXIS_ACLK] [get_bd_pins axis_interconnect_0/S00_AXIS_ACLK] [get_bd_pins axis_interconnect_0/S01_AXIS_ACLK] [get_bd_pins axis_interconnect_0/S02_AXIS_ACLK] [get_bd_pins axis_interconnect_0/S03_AXIS_ACLK] [get_bd_pins compute_c2h_merger_0/aclk] [get_bd_pins compute_engine_0/aclk] [get_bd_pins context_manager_0/aclk] [get_bd_pins context_manager_1/aclk] [get_bd_pins context_manager_2/aclk] [get_bd_pins context_manager_3/aclk] [get_bd_pins dram_reader_0/aclk] [get_bd_pins dram_writer_0/aclk] [get_bd_pins exec_engine_0/aclk] [get_bd_pins op_incr_1_0/aclk] [get_bd_pins op_incr_2_0/aclk] [get_bd_pins op_incr_4_0/aclk] [get_bd_pins prp_fetcher_0/aclk] [get_bd_pins qdma_c2h_byp_ctrl_0/axi_aclk] [get_bd_pins qdma_ep_sim_0/sys_clk] [get_bd_pins qdma_h2c_byp_ctrl_0/axi_aclk]
  connect_bd_net -net aresetn_0_1 [get_bd_ports aresetn_0] [get_bd_pins axi_datamover_0/m_axi_mm2s_aresetn] [get_bd_pins axi_datamover_0/m_axi_s2mm_aresetn] [get_bd_pins axi_datamover_0/m_axis_mm2s_cmdsts_aresetn] [get_bd_pins axi_datamover_0/m_axis_s2mm_cmdsts_aresetn] [get_bd_pins axi_interconnect_0/ARESETN] [get_bd_pins axi_interconnect_0/M00_ARESETN] [get_bd_pins axi_interconnect_0/S00_ARESETN] [get_bd_pins axi_interconnect_0/S01_ARESETN] [get_bd_pins axi_interconnect_0/S02_ARESETN] [get_bd_pins axi_vip_0/aresetn] [get_bd_pins axi_vip_1/aresetn] [get_bd_pins axis_aw_w_splitter_0/aresetn] [get_bd_pins axis_data_fifo_0/s_axis_aresetn] [get_bd_pins axis_debug_reg_0/aresetn] [get_bd_pins axis_interconnect_0/ARESETN] [get_bd_pins axis_interconnect_0/M00_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/M01_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/M02_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/M03_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/M04_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/S00_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/S01_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/S02_AXIS_ARESETN] [get_bd_pins axis_interconnect_0/S03_AXIS_ARESETN] [get_bd_pins compute_c2h_merger_0/aresetn] [get_bd_pins compute_engine_0/aresetn] [get_bd_pins context_manager_0/aresetn] [get_bd_pins context_manager_1/aresetn] [get_bd_pins context_manager_2/aresetn] [get_bd_pins context_manager_3/aresetn] [get_bd_pins dram_reader_0/aresetn] [get_bd_pins dram_writer_0/aresetn] [get_bd_pins exec_engine_0/aresetn] [get_bd_pins op_incr_1_0/aresetn] [get_bd_pins op_incr_2_0/aresetn] [get_bd_pins op_incr_4_0/aresetn] [get_bd_pins prp_fetcher_0/aresetn] [get_bd_pins qdma_c2h_byp_ctrl_0/axi_aresetn] [get_bd_pins qdma_ep_sim_0/sys_rst_n] [get_bd_pins qdma_h2c_byp_ctrl_0/axi_aresetn]
  connect_bd_net -net axis_debug_reg_0_data_out [get_bd_pins axis_debug_reg_0/data_out] [get_bd_pins compute_engine_0/debug_in]
  connect_bd_net -net compute_c2h_merger_0_prp_out_tag [get_bd_pins compute_c2h_merger_0/prp_out_tag] [get_bd_pins prp_fetcher_0/prp_out_tag]
  connect_bd_net -net compute_engine_0_user_reset [get_bd_pins axis_debug_reg_0/user_reset] [get_bd_pins compute_engine_0/user_reset] [get_bd_pins dram_reader_0/user_reset] [get_bd_pins dram_writer_0/user_reset] [get_bd_pins exec_engine_0/user_reset] [get_bd_pins prp_fetcher_0/user_reset]
  connect_bd_net -net exec_engine_0_current_sqe [get_bd_pins context_manager_0/current_sqe] [get_bd_pins context_manager_1/current_sqe] [get_bd_pins context_manager_2/current_sqe] [get_bd_pins context_manager_3/current_sqe] [get_bd_pins exec_engine_0/current_sqe]
  connect_bd_net -net qdma_c2h_byp_ctrl_0_c2h_byp_in_st_0_addr [get_bd_pins qdma_c2h_byp_ctrl_0/c2h_byp_in_st_0_addr] [get_bd_pins qdma_ep_sim_0/c2h_byp_in_st_0_addr]
  connect_bd_net -net qdma_c2h_byp_ctrl_0_c2h_byp_in_st_0_error [get_bd_pins qdma_c2h_byp_ctrl_0/c2h_byp_in_st_0_error] [get_bd_pins qdma_ep_sim_0/c2h_byp_in_st_0_error]
  connect_bd_net -net qdma_c2h_byp_ctrl_0_c2h_byp_in_st_0_func [get_bd_pins qdma_c2h_byp_ctrl_0/c2h_byp_in_st_0_func] [get_bd_pins qdma_ep_sim_0/c2h_byp_in_st_0_func]
  connect_bd_net -net qdma_c2h_byp_ctrl_0_c2h_byp_in_st_0_pfch_tag [get_bd_pins qdma_c2h_byp_ctrl_0/c2h_byp_in_st_0_pfch_tag] [get_bd_pins qdma_ep_sim_0/c2h_byp_in_st_0_pfch_tag]
  connect_bd_net -net qdma_c2h_byp_ctrl_0_c2h_byp_in_st_0_port_id [get_bd_pins qdma_c2h_byp_ctrl_0/c2h_byp_in_st_0_port_id] [get_bd_pins qdma_ep_sim_0/c2h_byp_in_st_0_port_id]
  connect_bd_net -net qdma_c2h_byp_ctrl_0_c2h_byp_in_st_0_qid [get_bd_pins qdma_c2h_byp_ctrl_0/c2h_byp_in_st_0_qid] [get_bd_pins qdma_ep_sim_0/c2h_byp_in_st_0_qid]
  connect_bd_net -net qdma_c2h_byp_ctrl_0_c2h_byp_in_st_0_valid [get_bd_pins qdma_c2h_byp_ctrl_0/c2h_byp_in_st_0_valid] [get_bd_pins qdma_ep_sim_0/c2h_byp_in_st_0_valid]
  connect_bd_net -net qdma_ep_sim_0_c2h_byp_in_st_0_ready [get_bd_pins qdma_c2h_byp_ctrl_0/c2h_byp_in_st_0_ready] [get_bd_pins qdma_ep_sim_0/c2h_byp_in_st_0_ready]
  connect_bd_net -net qdma_ep_sim_0_h2c_byp_in_st_0_ready [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_ready] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_ready]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_addr [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_addr] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_addr]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_cidx [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_cidx] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_cidx]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_eop [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_eop] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_eop]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_error [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_error] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_error]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_func [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_func] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_func]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_len [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_len] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_len]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_mrkr_req [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_mrkr_req] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_mrkr_req]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_no_dma [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_no_dma] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_no_dma]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_port_id [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_port_id] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_port_id]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_qid [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_qid] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_qid]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_sdi [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_sdi] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_sdi]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_sop [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_sop] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_sop]
  connect_bd_net -net qdma_h2c_byp_ctrl_0_h2c_byp_in_st_0_valid [get_bd_pins qdma_ep_sim_0/h2c_byp_in_st_0_valid] [get_bd_pins qdma_h2c_byp_ctrl_0/h2c_byp_in_st_0_valid]
  connect_bd_net [get_bd_pins context_manager_0/sqe_finished] \
        [get_bd_pins sqe_finished_concat/In0]
  connect_bd_net [get_bd_pins context_manager_1/sqe_finished] \
        [get_bd_pins sqe_finished_concat/In1]
  connect_bd_net [get_bd_pins context_manager_2/sqe_finished] \
        [get_bd_pins sqe_finished_concat/In2]
  connect_bd_net [get_bd_pins context_manager_3/sqe_finished] \
        [get_bd_pins sqe_finished_concat/In3]
  connect_bd_net [get_bd_pins sqe_finished_concat/dout] \
        [get_bd_pins exec_engine_0/sqe_finished]

  # Create address segments
  assign_bd_address -offset 0x00000000 -range 0x000400000000 -target_address_space [get_bd_addr_spaces axi_datamover_0/Data_MM2S] [get_bd_addr_segs axi_vip_0/S_AXI/Reg] -force
  assign_bd_address -offset 0x00000000 -range 0x00010000 -target_address_space [get_bd_addr_spaces axi_vip_1/Master_AXI] [get_bd_addr_segs compute_engine_0/s_axi/reg0] -force
  assign_bd_address -offset 0x00000000 -range 0x000400000000 -target_address_space [get_bd_addr_spaces compute_engine_0/m_axi] [get_bd_addr_segs axi_vip_0/S_AXI/Reg] -force


  # Restore current instance
  current_bd_instance $oldCurInst

  validate_bd_design
  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""

set_property top nvme_sim_tb [get_filesets sim_1]
set_property top_lib xil_defaultlib [get_filesets sim_1]

