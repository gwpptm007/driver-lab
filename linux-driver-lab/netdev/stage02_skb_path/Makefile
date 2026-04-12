STAGE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include env/stage02_skb_path.env

MODULE_NAME := netdev_stage02
DRIVER_DIR := $(STAGE_DIR)/driver
TOOLS_DIR  := $(STAGE_DIR)/tools
OUTPUT_DIR := $(STAGE_DIR)/output
RECORDS_DIR := $(STAGE_DIR)/records
WORKDIR_DIR := $(STAGE_DIR)/workdir
REPORT_FILE := $(OUTPUT_DIR)/stage02_report.md
SEND_TOOL := $(TOOLS_DIR)/send_stage02_frame
RECV_TOOL := $(TOOLS_DIR)/recv_stage02_frame

KDIR ?= /lib/modules/$(shell uname -r)/build
HOST_CC ?= gcc

.PHONY: help prepare report build-userspace build-module build load unload smoke clean distclean

help:
	@echo "stage02_skb_path targets:"
	@echo "  make report          - 生成阶段环境报告"
	@echo "  make build-userspace - 编译 sender / receiver 工具"
	@echo "  make build-module    - 编译内核模块（需要内核头文件）"
	@echo "  make build           - 同时执行 userspace + module 构建"
	@echo "  make load            - 加载模块（支持 LOOP_MODE=copy|clone）"
	@echo "  make unload          - 卸载模块"
	@echo "  make smoke           - 运行 TX/RX 软件闭环 smoke"
	@echo "  make clean           - 清理构建产物"

prepare:
	@mkdir -p $(OUTPUT_DIR) $(RECORDS_DIR) $(WORKDIR_DIR)
	@chmod +x scripts/*.sh 2>/dev/null || true

report: prepare
	@./scripts/check_host_env.sh
	@./scripts/generate_stage02_report.sh
	@echo "[stage02] report -> $(REPORT_FILE)"

build-userspace: prepare
	$(HOST_CC) -O2 -Wall -Wextra -std=c11 -o $(SEND_TOOL) $(TOOLS_DIR)/send_stage02_frame.c
	$(HOST_CC) -O2 -Wall -Wextra -std=c11 -o $(RECV_TOOL) $(TOOLS_DIR)/recv_stage02_frame.c
	@echo "[stage02] built userspace tools: $(SEND_TOOL), $(RECV_TOOL)"

build-module: prepare
	@if [ ! -d "$(KDIR)" ]; then \
		echo "[stage02] missing kernel build dir: $(KDIR)"; \
		echo "[stage02] build-module requires matching kernel headers."; \
		exit 2; \
	fi
	$(MAKE) -C $(KDIR) M=$(DRIVER_DIR) modules
	@cp -f $(DRIVER_DIR)/$(MODULE_NAME).ko $(OUTPUT_DIR)/ 2>/dev/null || true
	@echo "[stage02] built module: $(OUTPUT_DIR)/$(MODULE_NAME).ko"

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
	@rm -f $(OUTPUT_DIR)/host_env_stage02.env $(OUTPUT_DIR)/stage02_report.md
	@echo "[stage02] cleaned"

distclean: clean
	@rm -rf $(WORKDIR_DIR)/* $(RECORDS_DIR)/*
	@echo "[stage02] distclean done"
