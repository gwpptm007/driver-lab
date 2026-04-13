# SPDX-License-Identifier: GPL-2.0
# stage06_arm64_migration Makefile

STAGE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include env/stage06_arm64_migration.env

.PHONY: help prepare report matrix diff resolve-host resolve-x86 resolve-arm64 \
        build-stage04-host build-stage04-arm64 dryrun-arm64 smoke clean

help:
	@echo "stage06_arm64_migration targets:"
	@echo "  make report             - 生成 stage06 环境/迁移报告"
	@echo "  make matrix             - 生成平台矩阵"
	@echo "  make diff               - 生成 stage04 -> stage06 差异报告"
	@echo "  make resolve-host       - 解析 host 平台参数"
	@echo "  make resolve-x86        - 解析 qemu-x86_64 平台参数"
	@echo "  make resolve-arm64      - 解析 qemu-arm64 平台参数"
	@echo "  make build-stage04-host - 尝试按 host 方式构建 stage04 模块"
	@echo "  make build-stage04-arm64- 尝试按 arm64 方式构建 stage04 模块"
	@echo "  make dryrun-arm64       - 生成 ARM64 QEMU 启动命令（不执行）"
	@echo "  make smoke              - 运行 stage06 全套 dry-run"

prepare:
	@mkdir -p output records workdir
	@chmod +x scripts/*.sh 2>/dev/null || true

report: prepare
	@./scripts/check_platform_env.sh
	@./scripts/generate_stage06_report.sh

matrix: prepare
	@./scripts/generate_platform_matrix.sh

resolve-host: prepare
	@TARGET_PROFILE=host ./scripts/resolve_platform_env.sh

resolve-x86: prepare
	@TARGET_PROFILE=qemu-x86_64 ./scripts/resolve_platform_env.sh

resolve-arm64: prepare
	@TARGET_PROFILE=qemu-arm64 ./scripts/resolve_platform_env.sh

diff: prepare
	@./scripts/collect_stage04_stage06_diff.sh

build-stage04-host: prepare resolve-host
	@TARGET_PROFILE=host ./scripts/build_stage04_for_target.sh

build-stage04-arm64: prepare resolve-arm64
	@TARGET_PROFILE=qemu-arm64 ./scripts/build_stage04_for_target.sh

dryrun-arm64: prepare resolve-arm64
	@./scripts/dryrun_arm64_qemu.sh

smoke: prepare
	@./scripts/smoke.sh

clean:
	@rm -f output/host_env_stage06.env
	@rm -f output/resolved_*.env
	@rm -f output/platform_matrix.md output/stage06_report.md output/stage04_stage06_diff.md
	@rm -f output/arm64_qemu_dryrun.sh output/build_stage04_*.log
	@echo "[stage06] cleaned"
