# day22/tools/Makefile
#
# 作用：
#   1. 编译 guest 侧辅助 C 工具。
#   2. 这些工具会被打进 day22 自己的 initramfs，不依赖前面任何 day 的代码。
#
# 默认按 arm64 静态程序编译，方便塞进最小 rootfs。

CROSS_COMPILE ?= aarch64-linux-gnu-
CC            ?= $(CROSS_COMPILE)gcc
STRIP         ?= $(CROSS_COMPILE)strip
CFLAGS        ?= -O2 -static -Wall -Wextra
LDFLAGS       ?= -static

OUT_DIR ?= ../workdir/tools/aarch64

TOOLS := pci_sysfs_dump

all: $(TOOLS:%=$(OUT_DIR)/%)

$(OUT_DIR):
	mkdir -p $@

$(OUT_DIR)/pci_sysfs_dump: pci_sysfs_dump.c | $(OUT_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	$(STRIP) $@ || true

clean:
	rm -rf $(OUT_DIR)

.PHONY: all clean
