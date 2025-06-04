# 定义目标名称
TARGETS := hlsacc_assscheduler

# 顶层目录路径
TOP_DIR := $(shell pwd)
SCRIPTS_DIR := $(TOP_DIR)/scripts

# 默认目标
.PHONY: all clean $(TARGETS) prj_gen rtl_gen

all: prj_gen

# 清理所有项目
clean:
	@echo "Cleaning all projects..."
	@for target in $(TARGETS); do \
		if [ -d hls/$$target ]; then \
			rm -rf hls/$$target/solution1; \
			echo "Cleaned hls/$$target"; \
		fi; \
	done

# 为所有目标生成项目
prj_gen: $(addsuffix _prj, $(TARGETS))
	@echo "All projects created."

# 为所有目标生成RTL
rtl_gen: $(addsuffix _rtl, $(TARGETS))
	@echo "All RTL generation completed."

# 为每个目标创建项目的规则
%_prj:
	@target=$$(echo $@ | sed 's/_prj//'); \
	if [ -d hls/$$target ]; then \
		echo "Creating project for $$target..."; \
		if [ -f hls/$$target/run_hls.tcl ]; then \
			cd hls/$$target && \
			$(MAKE) -f $(SCRIPTS_DIR)/toolset.mk TARGET=$$target MODE=prj_gen run_hls; \
		else \
			echo "Warning: run_hls.tcl not found in hls/$$target, skipping..."; \
		fi; \
	else \
		echo "Warning: Directory hls/$$target does not exist, skipping..."; \
	fi

# 为每个目标生成RTL的规则
%_rtl:
	@target=$$(echo $@ | sed 's/_rtl//'); \
	if [ -d hls/$$target ]; then \
		echo "Generating RTL for $$target..."; \
		if [ -f hls/$$target/run_hls.tcl ]; then \
			cd hls/$$target && \
			$(MAKE) -f $(SCRIPTS_DIR)/toolset.mk TARGET=$$target MODE=rtl_gen run_hls; \
		else \
			echo "Warning: run_hls.tcl not found in hls/$$target, skipping..."; \
		fi; \
	else \
		echo "Warning: Directory hls/$$target does not exist, skipping..."; \
	fi