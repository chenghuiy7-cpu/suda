.PHONY: all

all:
	@echo "Building API libraries..."
	$(MAKE) -C ./liburing
	@bash -C "cd ./libnvme && mkdir -p .build && meson setup --default-library=static .build . && meson compile -C .build