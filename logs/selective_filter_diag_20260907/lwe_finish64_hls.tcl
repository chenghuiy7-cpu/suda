catch {unset env(DEBUG)}
open_project -reset project
add_files /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/lwe_encrypt.cpp -cflags {-DUSING_XILINX_STREAM -I/home/yangchenghui/suda/device/shared_components/hls -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include -I/usr/include/x86_64-linux-gnu}
add_files -tb /home/yangchenghui/suda/device/operators/hls/lwe_encrypt/test.cpp -cflags {-DUSING_XILINX_STREAM -I/home/yangchenghui/suda/device/shared_components/hls -I/opt/Xilinx_2020.2/Vitis_HLS/2020.2/include -I/usr/include/x86_64-linux-gnu}
set_top lwe_encrypt
open_solution -reset solution1
set_part {xczu19eg-ffvc1760-2-e}
create_clock -period {250MHz}
config_interface -m_axi_max_bitwidth 512
config_rtl -reset all
csim_design
csynth_design
exit
