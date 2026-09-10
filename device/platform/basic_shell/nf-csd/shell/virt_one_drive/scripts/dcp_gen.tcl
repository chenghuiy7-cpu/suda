source [file join $script_dir "prj_setup.tcl"]

add_files -norecurse -fileset sources_1 ${design_dir}/../fpga/sources/hdl/

set_property ip_repo_paths ${design_dir}/../fpga/sources/vscode-op [current_fileset]
#set_property ip_repo_paths ${design_dir}/../fpga/sources/hlsaccframework [current_fileset]

# 只收集各算子目录当前层的 Verilog。不能递归加入整个目录，否则放在
# hlsaccframework 下的 RTL 备份也会参与综合并覆盖当前模块定义。
set hlsacc_src_root ${design_dir}/../fpga/sources/hlsaccframework
set hlsacc_hdl_files {}
foreach src_dir [glob -nocomplain -types d ${hlsacc_src_root}/*] {
    set src_dir_name [file tail ${src_dir}]
    if {[regexp -nocase {(^|[._-])(backup|bak)([._-]|$)} ${src_dir_name}]} {
        puts "\[dcp_gen\] 跳过备份 RTL 目录：${src_dir}"
        continue
    }

    foreach hdl_file [glob -nocomplain -types f ${src_dir}/*.v] {
        lappend hlsacc_hdl_files ${hdl_file}
    }
}

set hlsacc_hdl_files [lsort -unique ${hlsacc_hdl_files}]
if {[llength ${hlsacc_hdl_files}] == 0} {
    error "未在 ${hlsacc_src_root} 下找到可综合的 HLS accelerator Verilog"
}

puts "\[dcp_gen\] 加入 [llength ${hlsacc_hdl_files}] 个 HLS accelerator Verilog 文件"
add_files -norecurse -fileset sources_1 ${hlsacc_hdl_files}
update_compile_order -fileset sources_1
update_ip_catalog -rebuild

set bd_design ${prj_design}

source ${design_dir}/../fpga/scripts/${bd_design}.tcl

set bd_file_gen_loc \
    ./${vivado_prj_name}/${vivado_prj_name}.srcs/sources_1/bd
set bd_file_gen \
    ./${bd_file_gen_loc}/${bd_design}/${bd_design}.bd
	
set_property synth_checkpoint_mode None [get_files ${bd_file_gen}] 
generate_target all [get_files ${bd_file_gen}] 
	
make_wrapper -files [get_files ${bd_file_gen}] -top
exec cp -r ./${vivado_prj_name}/${vivado_prj_name}.gen/sources_1/bd/${bd_design} \
    ${bd_file_gen_loc}/
import_files -force -norecurse -fileset sources_1 \
    ./${bd_file_gen_loc}/${bd_design}/hdl/${bd_design}_wrapper.v
	
validate_bd_design
save_bd_design
close_bd_design ${bd_design}
	
# synthesizing role design
synth_design -top ${bd_design}_wrapper -part ${device} -mode out_of_context
	
# write checkpoint
write_checkpoint -force ${dcp_dir}/${bd_design}.dcp
