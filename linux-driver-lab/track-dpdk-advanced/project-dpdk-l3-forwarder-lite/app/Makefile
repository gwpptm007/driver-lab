APP := dpdk-l3-forwarder-lite
SRCS := main.c
BUILD_DIR := build
BIN := $(BUILD_DIR)/$(APP)
PKGCONF ?= pkg-config
CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra

DPDK_CFLAGS := $(shell $(PKGCONF) --cflags libdpdk 2>/dev/null)
DPDK_LIBS := $(shell $(PKGCONF) --libs libdpdk 2>/dev/null)

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRCS)
	@if ! $(PKGCONF) --exists libdpdk; then echo "[ERR] pkg-config cannot find libdpdk"; exit 1; fi
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DPDK_CFLAGS) $(SRCS) -o $@ $(DPDK_LIBS)

clean:
	rm -rf $(BUILD_DIR)

