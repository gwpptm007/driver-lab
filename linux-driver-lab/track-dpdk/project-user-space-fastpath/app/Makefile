APP := fastpath-lite
BUILD_DIR := build

.PHONY: all clean rebuild

all:
	meson setup $(BUILD_DIR) --wipe || meson setup $(BUILD_DIR)
	ninja -C $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all
