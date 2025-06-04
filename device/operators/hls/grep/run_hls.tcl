# run_hls.tcl - HLS脚本用于项目创建和RTL生成

# 获取命令行参数
set target_name [lindex $argv 2]
set mode [lindex $argv 3]

# 设置项目和解决方案名称
set project_name ${target_name}
set solution_name "solution1"

# 设置共享头文件目录的路径
set top_dir [file normalize [file join [file dirname [info script]] "../.."]]
set shared_include_dir [file join $top_dir "../../../shared_components/hls"]

# 设置编译标志
set compile_flags "-I${shared_include_dir} -DUSING_XILINX_STREAM"

# 创建项目
puts "Creating/opening project: ${project_name}"
open_project -reset ${project_name}

# 添加设计文件
set source_file "${target_name}.cpp"
if {[file exists $source_file]} {
    puts "Adding design file: ${source_file}"
    add_files ${source_file} -cflags "${compile_flags}"
} else {
    puts "Error: Source file ${source_file} not found!"
    exit 1
}

# 添加测试文件
set testbench_file "test.cpp"
if {[file exists $testbench_file]} {
    puts "Adding testbench file: ${testbench_file}"
    add_files -tb ${testbench_file} -cflags "${compile_flags}"
} else {
    puts "Warning: Testbench file ${testbench_file} not found. Continuing without testbench."
}

# 设置顶层函数
puts "Setting top function to: ${target_name}"
set_top SimpleGrep

# 创建解决方案
puts "Creating solution: ${solution_name}"
open_solution -reset ${solution_name}

# 定义技术和时钟速率
set_part {xczu19eg-ffvc1760-2-e}
create_clock -period "250MHz"

# 配置接口和RTL设置
config_interface -m_axi_max_bitwidth 512
config_rtl -reset all

# 基于模式执行不同操作
if {$mode eq "prj_gen"} {
    # 项目生成模式：运行C仿真和综合
    puts "Mode: Project Generation - Only Gen"
    #csim_design
    #csynth_design
    
} elseif {$mode eq "rtl_gen"} {
    # RTL生成模式：运行综合并导出设计
    puts "Mode: RTL Generation - Running synthesis and exporting design"
    csynth_design
    export_design -format ip_catalog -output ./${target_name}_ip.zip
    puts "RTL generation completed. IP catalog exported to ${target_name}_ip.zip"
    
} else {
    puts "Error: Unknown mode '${mode}'. Valid modes are 'prj_gen' or 'rtl_gen'."
    exit 1
}

# 完成
puts "HLS processing for ${target_name} completed successfully."
exit