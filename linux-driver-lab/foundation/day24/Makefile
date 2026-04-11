SHELL := /bin/bash
DAY24_ROOT := $(CURDIR)

.PHONY: help discover-paths check kernel-module-tree build-lspci build-tools module rootfs backend qemu collect run clean

help:
	@echo "day24 targets:"
	@echo "  make discover-paths      - 扫描当前仓库中的常见 arm64 路径"
	@echo "  make check               - 检查宿主机工具、路径与内核配置"
	@echo "  make kernel-module-tree  - 准备外部模块构建所需的内核模块树"
	@echo "  make build-lspci         - 基于你自行准备的 third_party/pciutils/ 构建 guest 侧 arm64 静态 lspci"
	@echo "  make build-tools         - 构建 day24_mmio_tool（arm64 静态）"
	@echo "  make module              - 构建 day24_ivshmem_mmio.ko"
	@echo "  make rootfs              - 构建 day24 独立 initramfs"
	@echo "  make backend             - 准备 ivshmem-plain 后端文件"
	@echo "  make qemu                - 启动 QEMU 并执行 day24 自动流程"
	@echo "  make collect             - 归档 records"
	@echo "  make run                 - 一键执行 day24 全流程"
	@echo "  make clean               - 清理 workdir 与模块产物"

discover-paths:
	@$(DAY24_ROOT)/scripts/00_discover_common_paths.sh

check:
	@$(DAY24_ROOT)/scripts/00_check_host_tools.sh
	@$(DAY24_ROOT)/scripts/01_check_kernel_config.sh || true

kernel-module-tree:
	@$(DAY24_ROOT)/scripts/10_prepare_kernel_module_tree.sh

build-lspci:
	@$(DAY24_ROOT)/scripts/02_build_guest_lspci.sh

build-tools:
	@$(DAY24_ROOT)/scripts/03_build_day24_tools.sh

module:
	@$(DAY24_ROOT)/scripts/09_build_day24_module.sh

rootfs:
	@$(DAY24_ROOT)/scripts/04_prepare_rootfs.sh

backend:
	@$(DAY24_ROOT)/scripts/05_prepare_ivshmem_backend.sh

qemu:
	@$(DAY24_ROOT)/scripts/06_run_qemu_day24.sh

collect:
	@$(DAY24_ROOT)/scripts/07_extract_records.sh

run:
	@$(DAY24_ROOT)/scripts/08_run_all.sh

clean:
	@$(DAY24_ROOT)/scripts/11_clean.sh
	@$(MAKE) -C $(DAY24_ROOT)/driver clean >/dev/null 2>&1 || true
