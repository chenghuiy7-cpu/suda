# 获取当前Makefile的目录路径
CURRENT_DIR := $(shell pwd)
# 或者使用MAKEFILE_LIST变量
MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(MAKEFILE_PATH))

.PHONY: all

all:
	@echo "Current directory: $(CURRENT_DIR)"
	@echo "Makefile directory: $(MAKEFILE_DIR)"
	@echo "Building API libraries..."
	$(MAKE) -C ./liburing
	
	# 正确使用bash -c (小写c)并使用绝对路径
	@bash -c "cd $(CURRENT_DIR)/libnvme && mkdir -p .build && meson setup --default-library=static .build && meson compile -C .build"
