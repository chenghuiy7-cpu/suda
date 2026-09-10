# run_hls.tcl - selective_filter HLS build and simulation script.

# 避免外部 DEBUG=release 被 Vitis HLS 2020.2 当作裸编译参数传给 g++。
catch {unset env(DEBUG)}

if {[llength $argv] >= 4} {
    set target_name [lindex $argv 2]
    set mode [lindex $argv 3]
} else {
    set target_name [lindex $argv 0]
    set mode [lindex $argv 1]
}

set project_name ${target_name}
set solution_name "solution1"

set top_dir [file normalize [file join [file dirname [info script]] "../.."]]
set shared_include_dir [file join $top_dir "../shared_components/hls"]
if {[info exists env(VITIS_HLS_ROOT)]} {
    set vitis_hls_include "$env(VITIS_HLS_ROOT)/include"
} else {
    set vitis_hls_include "/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include"
}
if {[info exists env(HLS_HOST_ARCH_INCLUDE)]} {
    set linux_arch_include $env(HLS_HOST_ARCH_INCLUDE)
} else {
    set linux_arch_include "/usr/include/x86_64-linux-gnu"
}
set compile_flags "-I${shared_include_dir} -I${vitis_hls_include} -I${linux_arch_include} -DUSING_XILINX_STREAM"

open_project -reset ${project_name}
add_files ${target_name}.cpp -cflags "${compile_flags}"
add_files -tb test.cpp -cflags "${compile_flags}"
# The LWE implementation is testbench-only. It proves the stream contract
# without adding or modifying the LWE core in the selective_filter RTL.
add_files -tb ../lwe_encrypt/lwe_encrypt.cpp -cflags "${compile_flags}"
set_top selective_filter

open_solution -reset ${solution_name}
set_part {xczu19eg-ffvc1760-2-e}
create_clock -period "250MHz"
config_interface -m_axi_max_bitwidth 512
config_rtl -reset all

if {$mode eq "csim"} {
    csim_design
} elseif {$mode eq "csynth"} {
    csynth_design
} elseif {$mode eq "cosim"} {
    csynth_design
    cosim_design
} elseif {$mode eq "rtl_gen"} {
    csynth_design
    export_design -format ip_catalog -output ./${target_name}_ip.zip
} else {
    error "Unknown mode '${mode}'; use csim, csynth, cosim, or rtl_gen"
}

puts "selective_filter ${mode} completed successfully"
exit
