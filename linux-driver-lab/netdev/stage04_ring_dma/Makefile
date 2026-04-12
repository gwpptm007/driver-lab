# SPDX-License-Identifier: GPL-2.0
#
# Makefile — stage04_ring_dma 一站式构建
#
# 【目标说明】
#   make build          → 编译 userspace 工具 + 内核模块
#   make build-userspace → 编译 send_stage04_frame 和 recv_stage04_frame
#   make build-module    → 编译 netdev_stage04.ko（需要内核头文件）
#   make load           → 加载模块（调用 scripts/load_module.sh）
#   make unload         → 卸载模块（调用 scripts/unload_module.sh）
#   make smoke          → 运行 smoke test（调用 scripts/smoke.sh）
#   make clean          → 清理构建产物
#   make distclean      → 清理所有生成文件
#
# 【环境依赖】
#   KDIR   → 内核头文件目录（默认 /lib/modules/$(uname -r)/build）
#   HOST_CC → C 编译器（默认 gcc）
#
# 【stage04 核心文件】
#   driver/netdev_stage04.c     — 内核模块源码
#   tools/send_stage04_frame.c   — 发帧工具
#   tools/recv_stage04_frame.c   — 收帧工具
#
# 【smoke test 预期结果】
#   dmesg 输出中应看到：
#     [stage04] TX RXIDX=xx SKBLEN=xx CPYLEN=xx ETH=88b7
#     [stage04] POLL IDX=xx LEN=xx PROTO=88b7 RC=0

STAGE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include env/stage04_ring_dma.env

MODULE_NAME := netdev_stage04
DRIVER_DIR := $(STAGE_DIR)/driver
TOOLS_DIR  := $(STAGE_DIR)/tools
OUTPUT_DIR := $(STAGE_DIR)/output
RECORDS_DIR := $(STAGE_DIR)/records
WORKDIR_DIR := $(STAGE_DIR)/workdir
REPORT_FILE := $(OUTPUT_DIR)/stage04_report.md
SEND_TOOL := $(TOOLS_DIR)/send_stage04_frame
RECV_TOOL := $(TOOLS_DIR)/recv_stage04_frame

KDIR ?= /lib/modules/$(shell uname -r)/build
HOST_CC ?= gcc

.PHONY: help prepare report build-userspace build-module build load unload smoke clean distclean

help:
	@echo "stage04_ring_dma targets:"
	@echo "  make report          - 生成阶段环境报告"
	@echo "  make build-userspace - 编译 sender / receiver 工具"
	@echo "  make build-module    - 编译内核模块（需要内核头文件）"
	@echo "  make build           - 同时执行 userspace + module 构建"
	@echo "  make load            - 加载模块（支持 RING_SIZE/NAPI_WEIGHT/RX_BUF_SIZE）"
	@echo "  make unload          - 卸载模块"
	@echo "  make smoke           - 运行 stage04 smoke"
	@echo "  make clean           - 清理构建产物"

prepare:
	@mkdir -p $(OUTPUT_DIR) $(RECORDS_DIR) $(WORKDIR_DIR)
	@chmod +x scripts/*.sh 2>/dev/null || true

report: prepare
	@./scripts/check_host_env.sh
	@./scripts/generate_stage04_report.sh
	@echo "[stage04] report -> $(REPORT_FILE)"

build-userspace: prepare
	$(HOST_CC) -O2 -Wall -Wextra -std=c11 -o $(SEND_TOOL) $(TOOLS_DIR)/send_stage04_frame.c
	$(HOST_CC) -O2 -Wall -Wextra -std=c11 -o $(RECV_TOOL) $(TOOLS_DIR)/recv_stage04_frame.c
	@echo "[stage04] built userspace tools: $(SEND_TOOL), $(RECV_TOOL)"

build-module: prepare
	@if [ ! -d "$(KDIR)" ]; then \
		echo "[stage04] missing kernel build dir: $(KDIR)"; \
		echo "[stage04] build-module requires matching kernel headers."; \
		exit 2; \
	fi
	$(MAKE) -C $(KDIR) M=$(DRIVER_DIR) modules
	@cp -f $(DRIVER_DIR)/$(MODULE_NAME).ko $(OUTPUT_DIR)/ 2>/dev/null || true
	@echo "[stage04] built module: $(OUTPUT_DIR)/$(MODULE_NAME).ko"

build: build-userspace build-module

load:
	@./scripts/load_module.sh

unload:
	@./scripts/unload_module.sh

smoke:
	@./scripts/smoke.sh

clean:
	@if [ -d "$(KDIR)" ]; then \
		$(MAKE) -C $(KDIR) M=$(DRIVER_DIR) clean; \
	fi
	@rm -f $(SEND_TOOL) $(RECV_TOOL)
	@rm -f $(OUTPUT_DIR)/$(MODULE_NAME).ko
	@rm -f $(OUTPUT_DIR)/host_env_stage04.env $(OUTPUT_DIR)/stage04_report.md
	@echo "[stage04] cleaned"

distclean: clean
	@rm -rf $(WORKDIR_DIR)/* $(RECORDS_DIR)/*
	@echo "[stage04] distclean done"
