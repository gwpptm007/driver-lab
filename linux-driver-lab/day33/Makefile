.PHONY: help check fetch-pciutils build-lspci kernel-module-tree build-tools module rootfs backend run summarize-trace clean

help:
	@echo "Day33 targets:"
	@echo "  make check              # 检查 day33 宿主机环境（含 ftrace 关键配置）"
	@echo "  make fetch-pciutils     # 从 GitHub 获取 day33/third_party/pciutils"
	@echo "  make build-lspci        # 仅在没有现成 arm64 lspci 时再构建"
	@echo "  make kernel-module-tree # 准备外部模块构建树"
	@echo "  make build-tools        # 构建 day33 trace 用户态工具"
	@echo "  make module             # 构建 day33 trace 内核模块"
	@echo "  sudo -E make rootfs     # 构建 day33 initramfs"
	@echo "  make backend            # 准备 runtime 目录"
	@echo "  sudo -E make run        # 执行默认 day33 function_graph 采集流程"
	@echo "  make summarize-trace    # 基于当前 records 生成 trace 摘要"
	@echo "  make clean              # 清理 day33 临时产物"

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
	@bash scripts/09_build_day33_module.sh
rootfs:
	@bash scripts/04_prepare_rootfs.sh
backend:
	@bash scripts/05_prepare_runtime_dir.sh
run:
	@bash scripts/07_run_all.sh
summarize-trace:
	@bash scripts/11_generate_trace_summary.sh
clean:
	@bash scripts/99_clean.sh
