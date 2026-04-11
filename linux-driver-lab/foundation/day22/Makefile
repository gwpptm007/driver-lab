# day22 Makefile
#
# 这一版 day22 默认使用 ivshmem-plain：
#   1. D22 专注 PCI 枚举、BAR 可见性、lspci 证据归档。
#   2. 不再强依赖 ivshmem-server。
#   3. D25 再决定 IRQ/MSI 的实验后端。

SHELL := /bin/bash
DAY22_ROOT := $(CURDIR)

.PHONY: help discover-paths kernel-pci-prep check build-tools build-lspci selftest-tool rootfs backend server qemu collect run module clean

help:
	@echo "day22 targets:"
	@echo "  make discover-paths - 扫描当前仓库中常见的 Image/.config/BusyBox/lspci 路径"
	@echo "  make kernel-pci-prep - 帮你把 arm64 内核的 PCI/MSI/host 配置打开并生成报告"
	@echo "  make check         - 检查宿主机工具、路径与内核配置"
	@echo "  make build-tools   - 构建 guest 侧 C 工具 pci_sysfs_dump"
	@echo "  make build-lspci   - 尝试构建 guest 侧 aarch64 lspci（可选）"
	@echo "  make selftest-tool - 对 pci_sysfs_dump 做本地伪 sysfs 自测"
	@echo "  make rootfs        - 构建 day22 独立 initramfs"
	@echo "  make backend       - 准备 ivshmem-plain 后端文件"
	@echo "  make qemu          - 启动 QEMU 并运行 day22 自动检查"
	@echo "  make collect       - 从最近一次 serial.log 中切分 records"
	@echo "  make run           - 一键执行 day22 全流程"
	@echo "  make module        - 构建 day22_ivshmem_stub 内核模块（需 KDIR）"
	@echo "  make clean         - 清理 workdir 临时文件"
	@echo "  make server        - 兼容旧入口，内部等价于 make backend"

discover-paths:
	@$(DAY22_ROOT)/scripts/00_discover_common_paths.sh

kernel-pci-prep:
	@$(DAY22_ROOT)/scripts/11_prepare_kernel_pci_config.sh

check:
	@$(DAY22_ROOT)/scripts/00_check_host_tools.sh
	@$(DAY22_ROOT)/scripts/01_check_kernel_config.sh || true

build-tools:
	@$(DAY22_ROOT)/scripts/02_build_guest_tools.sh

build-lspci:
	@$(DAY22_ROOT)/scripts/02_build_guest_lspci.sh

selftest-tool:
	@$(DAY22_ROOT)/scripts/10_selftest_pci_sysfs_dump.sh

rootfs:
	@$(DAY22_ROOT)/scripts/03_prepare_rootfs.sh

backend:
	@$(DAY22_ROOT)/scripts/04_prepare_ivshmem_backend.sh

server:
	@$(DAY22_ROOT)/scripts/04_start_ivshmem_server.sh

qemu:
	@$(DAY22_ROOT)/scripts/05_run_qemu_ivshmem.sh

collect:
	@$(DAY22_ROOT)/scripts/06_extract_records.sh

run:
	@$(DAY22_ROOT)/scripts/07_run_all.sh

module:
	@$(DAY22_ROOT)/scripts/09_build_stub_module.sh

clean:
	@$(DAY22_ROOT)/scripts/08_clean.sh
	@$(MAKE) -C $(DAY22_ROOT)/tools clean >/dev/null 2>&1 || true
	@$(MAKE) -C $(DAY22_ROOT)/driver clean >/dev/null 2>&1 || true
