#========================================================
# FSBL auto generation and compiling script running in 
# the Vivado 2016.4 SDK HSI environment
# Author: Yisong Chang (changyisong@ict.ac.cn)
# Date: 13/11/2017
#========================================================

set hdf_file [lindex $argv 0]

set prj_dir [lindex $argv 1]

set fpga_arch [lindex $argv 2]

set fpga_proc [lindex $argv 3]

# Step 0: setup Vitis patch
exec mkdir -p ${prj_dir}
setws ${prj_dir}
#repo -set ${prj_dir}/../patches/AR76032/Vitis/data/embeddedsw

# Step 1: open hardware definitiion file
set hw_design [ hsi::open_hw_design ${hdf_file} ]

# Step 2: automatic generation of FSBL project
hsi::generate_app -hw $hw_design -os standalone -proc ${fpga_proc} -app ${fpga_arch}_fsbl -sw fsbl -dir ${prj_dir}

