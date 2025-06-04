# toolset.mk - 设置Vitis环境并运行HLS tcl脚本

# 确保MODE参数已设置，默认为prj_gen
MODE ?= prj_gen

# 默认目标
.PHONY: run_hls

# 运行HLS脚本
run_hls:
	@echo "Setting up Vitis environment..."
	@bash -c 'source /opt/Xilinx_2022.2/Vitis_HLS/2022.2/settings64.sh && \
	source /opt/Xilinx_2022.2/Vivado/2022.2/settings64.sh && \
	echo "Running HLS for $(TARGET) with mode $(MODE)..." && \
	vitis_hls -f run_hls.tcl -tclargs $(TARGET) $(MODE)'