.PHONY: all setup build clean run-help

BUILD_DIR ?= build

all: build

setup:
	meson setup $(BUILD_DIR) --wipe

build:
	@if [ ! -d "$(BUILD_DIR)" ]; then meson setup $(BUILD_DIR); fi
	ninja -C $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run-help: build
	./$(BUILD_DIR)/l2fwd-lite --no-pci --vdev=net_null0 -- --help
