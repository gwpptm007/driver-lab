# SPDX-License-Identifier: GPL-2.0
STAGE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
include env/stage05_virtio_param.env

OUTPUT_DIR := $(STAGE_DIR)/output
RECORDS_DIR := $(STAGE_DIR)/records
WORKDIR_DIR := $(STAGE_DIR)/workdir

.PHONY: help prepare report virtio-map compare platform-matrix resolve-platform smoke clean distclean

help:
	@echo "stage05_virtio_param targets:"
	@echo "  make report"
	@echo "  make virtio-map"
	@echo "  make compare"
	@echo "  make platform-matrix"
	@echo "  make resolve-platform TARGET_ARCH=... RUN_MODE=..."
	@echo "  make smoke"

prepare:
	@mkdir -p $(OUTPUT_DIR) $(RECORDS_DIR) $(WORKDIR_DIR)
	@chmod +x scripts/*.sh 2>/dev/null || true

report: prepare
	@./scripts/check_host_env.sh
	@./scripts/generate_stage05_report.sh
	@echo "[stage05] report -> $(OUTPUT_DIR)/stage05_report.md"

virtio-map: prepare
	@./scripts/collect_virtio_net_map.sh
	@echo "[stage05] virtio map -> $(OUTPUT_DIR)/virtio_net_map.md"

compare: prepare
	@./scripts/generate_comparison_report.sh
	@echo "[stage05] comparison -> $(OUTPUT_DIR)/stage04_vs_virtio_report.md"

platform-matrix: prepare
	@./scripts/generate_platform_matrix.sh
	@echo "[stage05] platform matrix -> $(OUTPUT_DIR)/platform_matrix.md"

resolve-platform: prepare
	@./scripts/resolve_platform_env.sh

smoke: prepare
	@./scripts/smoke.sh

clean:
	@rm -f $(OUTPUT_DIR)/*
	@echo "[stage05] cleaned"

distclean: clean
	@rm -rf $(RECORDS_DIR)/* $(WORKDIR_DIR)/*
	@echo "[stage05] distclean done"
