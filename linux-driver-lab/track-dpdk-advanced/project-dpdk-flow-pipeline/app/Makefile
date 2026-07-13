# 通过 pkg-config 获取当前主机 DPDK 的编译和链接参数。
APP := dpdk-flow-pipeline
SRCS := main.c flow_key.c flow_table.c flow_pipeline.c flow_capability.c flow_worker.c
BUILD_DIR := build
BIN := $(BUILD_DIR)/$(APP)
PKGCONF ?= pkg-config
CC ?= cc
CFLAGS ?= -O2 -g -std=gnu11 -Wall -Wextra

DPDK_CFLAGS := $(shell $(PKGCONF) --cflags libdpdk 2>/dev/null)
DPDK_LIBS := $(shell $(PKGCONF) --libs libdpdk 2>/dev/null)

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRCS) flow_key.h flow_table.h flow_pipeline.h flow_capability.h flow_worker.h
	@if ! $(PKGCONF) --exists libdpdk; then echo "[ERR] pkg-config cannot find libdpdk"; exit 1; fi
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DPDK_CFLAGS) $(SRCS) -o $@ $(DPDK_LIBS)

clean:
	rm -rf $(BUILD_DIR)
