.PHONY: help check build-lspci kernel-module-tree build-tools module rootfs backend run clean

help:
	@echo "Day26 targets:"
	@echo "  make check              # 检查宿主机环境"
	@echo "  make build-lspci        # 在 day26 独立目录内构建 arm64 lspci"
	@echo "  make kernel-module-tree # 准备外部模块构建树"
	@echo "  make build-tools        # 构建 day26 用户态工具"
	@echo "  make module             # 构建 day26 内核模块"
	@echo "  sudo -E make rootfs     # 构建 day26 initramfs"
	@echo "  make backend            # 准备 runtime 目录（EDU 不需要额外后端）"
	@echo "  sudo -E make run        # 运行完整流程"
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
	@bash scripts/09_build_day26_module.sh

rootfs:
	@bash scripts/04_prepare_rootfs.sh

backend:
	@bash scripts/05_prepare_runtime_dir.sh

run:
	@bash scripts/07_run_all.sh

clean:
	@bash scripts/99_clean.sh
