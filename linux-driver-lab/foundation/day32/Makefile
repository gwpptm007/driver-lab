.PHONY: help check fetch-pciutils build-lspci kernel-module-tree build-tools module rootfs backend run perf-baseline perf-optimized compare-perf clean

help:
	@echo "Day32 targets:"
	@echo "  make check              # 检查 day32 宿主机环境"
	@echo "  make fetch-pciutils     # 从 GitHub 获取 day32/third_party/pciutils"
	@echo "  make build-lspci        # 仅在没有现成 arm64 lspci 时再构建"
	@echo "  make kernel-module-tree # 准备外部模块构建树"
	@echo "  make build-tools        # 构建 day32 perf 用户态工具"
	@echo "  make module             # 构建 day32 perf 内核模块"
	@echo "  sudo -E make rootfs     # 构建 day32 initramfs"
	@echo "  make backend            # 准备 runtime 目录"
	@echo "  sudo -E make run        # 执行默认 full perf 流程并提取 records"
	@echo "  sudo -E make perf-baseline   # 宿主 perf 围绕 baseline workload 采集"
	@echo "  sudo -E make perf-optimized  # 宿主 perf 围绕 optimized workload 采集"
	@echo "  make compare-perf       # 基于当前 records 输出前后对比摘要"
	@echo "  make clean              # 清理 day32 临时产物"

check:
	@bash scripts/00_check_env.sh
fetch-pciutils:
	@bash scripts/01_fetch_pciutils.sh
build-lspci:
	@bash scripts/02_build_guest_lspci.sh
kernel-module-tree:
	@bash scripts/10_prepare_kernel_module_tree.sh
build-tools:
	@bash scripts/03_build_tools.sh
module:
	@bash scripts/09_build_day32_module.sh
rootfs:
	@bash scripts/04_prepare_rootfs.sh
backend:
	@bash scripts/05_prepare_runtime_dir.sh
run:
	@bash scripts/07_run_all.sh
perf-baseline:
	@bash scripts/11_collect_perf_baseline.sh
perf-optimized:
	@bash scripts/12_collect_perf_optimized.sh
compare-perf:
	@bash scripts/13_compare_perf_reports.sh
clean:
	@bash scripts/99_clean.sh
