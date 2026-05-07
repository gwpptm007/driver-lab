APP := media-gateway-lite
SRCS := main.c gateway_config.c gateway_rule.c gateway_packet.c gateway_port.c gateway_stats.c
BUILD_DIR := build
BIN := $(BUILD_DIR)/$(APP)
PKGCONF ?= pkg-config
CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra

DPDK_CFLAGS := $(shell $(PKGCONF) --cflags libdpdk 2>/dev/null)
DPDK_LIBS := $(shell $(PKGCONF) --libs libdpdk 2>/dev/null)

.PHONY: all clean meson

all: $(BIN)

$(BIN): $(SRCS)
	@if ! $(PKGCONF) --exists libdpdk; then \
		echo "[ERR] pkg-config cannot find libdpdk"; \
		echo "Install DPDK dev package or use meson with a configured DPDK environment."; \
		exit 1; \
	fi
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DPDK_CFLAGS) $(SRCS) -o $@ $(DPDK_LIBS)

meson:
	meson setup $(BUILD_DIR) --wipe
	ninja -C $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
