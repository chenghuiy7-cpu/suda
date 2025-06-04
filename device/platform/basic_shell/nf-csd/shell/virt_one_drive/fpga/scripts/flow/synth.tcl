# synthesizing full design
synth_design -top mpsoc_wrapper -part ${device} \
    -directive default -flatten_hierarchy rebuilt

# open PCIe endpoint checkpoint
set pcie_ep_blank [ get_property is_blackbox [get_cells mpsoc_i/u_qdma_ep/inst] ] 
if {${pcie_ep_blank} == 1} {
    read_checkpoint -cell [get_cells mpsoc_i/u_qdma_ep/inst] ${dcp_dir}/pcie_ep.dcp
}

# open ACCFRAMEWORK checkpoint
set accframework_blank [ get_property is_blackbox [get_cells mpsoc_i/u_accframework/inst] ]
if {${pcie_ep_blank} == 1} {
    read_checkpoint -cell [get_cells mpsoc_i/u_accframework/inst] ${dcp_dir}/accframework.dcp
}

read_xdc ${design_dir}/../fpga/constraints/pcie_ep_exception.xdc

# setup output logs and reports
report_timing_summary -file ${synth_rpt_dir}/synth_timing.rpt -delay_type max -max_paths 1000

# setup output logs and reports
report_utilization -hierarchical -file ${synth_rpt_dir}/synth_util_hier.rpt
report_utilization -file ${synth_rpt_dir}/synth_util.rpt
    
# write checkpoint
write_checkpoint -force ${dcp_dir}/synth.dcp

