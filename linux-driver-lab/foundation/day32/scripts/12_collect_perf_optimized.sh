#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
require_exec perf perf
# optimized 只保留『循环外准备一次映射』这条 workload，
# 便于和 baseline 做前后对比，而不被其他 workload 稀释热点。

export DAY32_PROFILE_MODE=optimized
export DAY32_RUN_DMA_LITE=0

bash "${DAY32_ROOT}/scripts/03_build_tools.sh"
bash "${DAY32_ROOT}/scripts/09_build_day32_module.sh"
bash "${DAY32_ROOT}/scripts/04_prepare_rootfs.sh"
bash "${DAY32_ROOT}/scripts/05_prepare_runtime_dir.sh"
rd="$(run_dir)"
echo '[day32] 宿主 perf stat：optimized workload'
perf stat -d -o "$rd/host-perf-optimized.stat.txt" bash "${DAY32_ROOT}/scripts/06_run_qemu_day32.sh" || true
perf record -F 49 -g -o "$rd/host-perf-optimized.data" bash "${DAY32_ROOT}/scripts/06_run_qemu_day32.sh" >/dev/null 2>&1 || true
perf report --stdio -i "$rd/host-perf-optimized.data" > "$rd/host-perf-optimized.report.txt" 2>/dev/null || true
bash "${DAY32_ROOT}/scripts/08_extract_records.sh"
