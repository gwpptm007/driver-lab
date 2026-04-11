SHELL := /bin/bash
ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
export DAY25_ROOT := $(ROOT_DIR)

-include $(ROOT_DIR)/env/day25.env
-include $(ROOT_DIR)/env/local.example.env
-include $(ROOT_DIR)/env/local.wq7.env

.PHONY: help check build-lspci build-tools kernel-module-tree module rootfs backend run collect clean

help:
	@echo "Day25 targets:"
	@echo "  make check               # 检查环境与配置"
	@echo "  make build-lspci         # 构建 guest 侧 arm64 静态 lspci"
	@echo "  make build-tools         # 构建 day25 用户态工具"
	@echo "  make kernel-module-tree  # 准备/补齐内核模块符号树"
	@echo "  make module              # 构建 day25 PCI/IRQ 模块"
	@echo "  sudo -E make rootfs      # 构建 day25 initramfs"
	@echo "  make backend             # 准备 day25 运行目录"
	@echo "  sudo -E make run         # 执行 day25 全流程"
	@echo "  make clean               # 清理 day25 产物"

check:
	@bash $(ROOT_DIR)/scripts/00_check_env.sh

build-lspci:
	@bash $(ROOT_DIR)/scripts/02_build_guest_lspci.sh

build-tools:
	@bash $(ROOT_DIR)/scripts/03_build_tools.sh

kernel-module-tree:
	@bash $(ROOT_DIR)/scripts/10_prepare_kernel_module_tree.sh

module:
	@bash $(ROOT_DIR)/scripts/09_build_day25_module.sh

rootfs:
	@bash $(ROOT_DIR)/scripts/04_prepare_rootfs.sh

backend:
	@bash $(ROOT_DIR)/scripts/05_prepare_runtime_dir.sh

run:
	@bash $(ROOT_DIR)/scripts/07_run_all.sh

collect:
	@bash $(ROOT_DIR)/scripts/08_extract_records.sh

clean:
	@bash $(ROOT_DIR)/scripts/99_clean.sh
