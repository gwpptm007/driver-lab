.PHONY: help check build-lspci kernel-module-tree build-tools module rootfs backend run clean

help:
	@echo "Day29 targets:"
	@echo "  make check              # 先检查环境；若已有 GUEST_LSPCI_BIN 可直接复用"
	@echo "  make build-lspci        # 仅在没有现成 arm64 lspci 时再构建"
	@echo "  make kernel-module-tree # 准备外部模块构建树"
	@echo "  make build-tools        # 构建 day29 用户态工具"
	@echo "  make module             # 构建 day29 内核模块"
	@echo "  sudo -E make rootfs     # 构建 day29 initramfs"
	@echo "  make backend            # 准备 runtime 目录"
	@echo "  sudo -E make run        # 运行完整流程并提取 records"
	@echo "  make clean              # 清理临时产物"

check:
	@bash scripts/00_check_env.sh

build-lspci:
	@bash scripts/02_build_guest_lspci.sh

kernel-module-tree:
	@bash scripts/10_prepare_kernel_module_tree.sh

build-tools:
	@bash scripts/03_build_tools.sh

module:
	@bash scripts/09_build_day29_module.sh

rootfs:
	@bash scripts/04_prepare_rootfs.sh

backend:
	@bash scripts/05_prepare_runtime_dir.sh

run:
	@bash scripts/07_run_all.sh

clean:
	@bash scripts/99_clean.sh
