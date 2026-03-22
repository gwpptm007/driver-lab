SHELL := /bin/bash
DAY23_ROOT := $(CURDIR)

.PHONY: help discover-paths check kernel-module-tree build-lspci module rootfs backend qemu collect run clean

help:
	@echo "day23 targets:"
	@echo "  make discover-paths      - 扫描当前仓库中的常见 arm64 路径"
	@echo "  make check               - 检查宿主机工具、路径与内核配置"
	@echo "  make kernel-module-tree  - 准备外部模块构建所需的内核模块树"
	@echo "  make build-lspci         - 构建 guest 侧 arm64 静态 lspci（可选）"
	@echo "  make module              - 构建 day23_ivshmem_probe.ko"
	@echo "  make rootfs              - 构建 day23 独立 initramfs"
	@echo "  make backend             - 准备 ivshmem-plain 后端文件"
	@echo "  make qemu                - 启动 QEMU 并执行 day23 自动流程"
	@echo "  make collect             - 归档 records"
	@echo "  make run                 - 一键执行 day23 全流程"
	@echo "  make clean               - 清理 workdir 与模块产物"

discover-paths:
	@$(DAY23_ROOT)/scripts/00_discover_common_paths.sh

check:
	@$(DAY23_ROOT)/scripts/00_check_host_tools.sh
	@$(DAY23_ROOT)/scripts/01_check_kernel_config.sh || true

kernel-module-tree:
	@$(DAY23_ROOT)/scripts/10_prepare_kernel_module_tree.sh

build-lspci:
	@$(DAY23_ROOT)/scripts/02_build_guest_lspci.sh

module:
	@$(DAY23_ROOT)/scripts/09_build_day23_module.sh

rootfs:
	@$(DAY23_ROOT)/scripts/03_prepare_rootfs.sh

backend:
	@$(DAY23_ROOT)/scripts/04_prepare_ivshmem_backend.sh

qemu:
	@$(DAY23_ROOT)/scripts/05_run_qemu_day23.sh

collect:
	@$(DAY23_ROOT)/scripts/06_extract_records.sh

run:
	@$(DAY23_ROOT)/scripts/07_run_all.sh

clean:
	@$(DAY23_ROOT)/scripts/08_clean.sh
	@$(MAKE) -C $(DAY23_ROOT)/driver clean >/dev/null 2>&1 || true
