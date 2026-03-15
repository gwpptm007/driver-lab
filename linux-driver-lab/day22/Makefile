# day22 Makefile
#
# 目标：把 day22 变成一个“目录内自足”的小工程。
# 说明：
#   1. 不调用前面 dayXX 的脚本。
#   2. 通过环境变量接入你本机已有的 Image / BusyBox / QEMU / 内核构建目录。
#   3. 除“内核编译产物”外，day22 相关代码、脚本、工具、模块骨架全部收口在本目录。

SHELL := /bin/bash
DAY22_ROOT := $(CURDIR)

.PHONY: help check build-tools build-lspci rootfs server qemu collect run module clean

help:
	@echo "day22 targets:"
	@echo "  make check        - 检查宿主机工具、路径与内核配置"
	@echo "  make build-tools  - 构建 guest 侧 C 工具 pci_sysfs_dump"
	@echo "  make build-lspci  - 尝试构建 guest 侧 aarch64 lspci（可选）"
	@echo "  make rootfs       - 构建 day22 独立 initramfs"
	@echo "  make server       - 启动 ivshmem-server"
	@echo "  make qemu         - 启动 QEMU 并运行 day22 自动检查"
	@echo "  make collect      - 从最近一次 serial.log 中切分 records"
	@echo "  make run          - 一键执行 day22 全流程"
	@echo "  make module       - 构建 day22_ivshmem_stub 内核模块（需 KDIR）"
	@echo "  make clean        - 清理 workdir 临时文件"

check:
	@$(DAY22_ROOT)/scripts/00_check_host_tools.sh
	@$(DAY22_ROOT)/scripts/01_check_kernel_config.sh || true

build-tools:
	@$(DAY22_ROOT)/scripts/02_build_guest_tools.sh

build-lspci:
	@$(DAY22_ROOT)/scripts/02_build_guest_lspci.sh

rootfs:
	@$(DAY22_ROOT)/scripts/03_prepare_rootfs.sh

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
