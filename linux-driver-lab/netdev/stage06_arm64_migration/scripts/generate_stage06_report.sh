#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_FILE="$ROOT_DIR/output/stage06_report.md"
mkdir -p "$ROOT_DIR/output"

"$ROOT_DIR/scripts/check_platform_env.sh" >/dev/null
"$ROOT_DIR/scripts/generate_platform_matrix.sh" >/dev/null
"$ROOT_DIR/scripts/collect_stage04_stage06_diff.sh" >/dev/null

# shellcheck source=/dev/null
source "$ROOT_DIR/output/host_env_stage06.env"
source "$ROOT_DIR/output/resolved_qemu-arm64.env"

READY=no
REASONS=()
[[ "${HAVE_QEMU_ARM64:-no}" == yes ]] || REASONS+=("missing qemu-system-aarch64")
[[ "${HAVE_AARCH64_GCC:-no}" == yes ]] || REASONS+=("missing aarch64-linux-gnu-gcc")
[[ -n "${KDIR:-}" && -d "${KDIR:-/nonexistent}" ]] || REASONS+=("missing arm64 kernel build dir")
[[ -n "${KERNEL_IMAGE:-}" && -f "${KERNEL_IMAGE:-/nonexistent}" ]] || REASONS+=("missing arm64 kernel image")
[[ -n "${ROOTFS_IMAGE:-}" && -f "${ROOTFS_IMAGE:-/nonexistent}" ]] || REASONS+=("missing rootfs image")
if [[ ${#REASONS[@]} -eq 0 ]]; then
    READY=yes
fi

{
    echo '# stage06_arm64_migration report'
    echo
    echo '## 阶段定义'
    echo
    echo '- 当前阶段：ARM64 迁移与跨平台收口'
    echo '- 主迁移对象：stage04_ring_dma'
    echo '- 当前定位：先让 build/run/records/compat 都能落到 ARM64'
    echo
    echo '## host 能力'
    echo
    printf -- '- Host kernel: %s\n' "${HOST_KERNEL}"
    printf -- '- qemu-system-x86_64 available: %s\n' "${HAVE_QEMU_X86}"
    printf -- '- qemu-system-aarch64 available: %s\n' "${HAVE_QEMU_ARM64}"
    printf -- '- aarch64-linux-gnu-gcc available: %s\n' "${HAVE_AARCH64_GCC}"
    echo
    echo '## ARM64 解析结果'
    echo
    printf -- '- QEMU_BIN: %s\n' "${QEMU_BIN:-}"
    printf -- '- CROSS_COMPILE: %s\n' "${CROSS_COMPILE:-}"
    printf -- '- KDIR: %s\n' "${KDIR:-}"
    printf -- '- KERNEL_IMAGE: %s\n' "${KERNEL_IMAGE:-}"
    printf -- '- ROOTFS_IMAGE: %s\n' "${ROOTFS_IMAGE:-}"
    echo
    echo '## 当前可执行性判断'
    echo
    printf -- '- STAGE06_ARM64_READY=%s\n' "$READY"
    if [[ "$READY" == no ]]; then
        echo '- 阻塞项：'
        for r in "${REASONS[@]}"; do
            printf '  - %s\n' "$r"
        done
    fi
    echo
    echo '## 本阶段交付'
    echo
    echo '- 平台矩阵：`output/platform_matrix.md`'
    echo '- 迁移差异报告：`output/stage04_stage06_diff.md`'
    echo '- ARM64 dry-run 命令：`output/arm64_qemu_dryrun.sh`'
    echo '- 兼容层代码：`include/netdev_kcompat.h`'
} > "$OUT_FILE"

echo "[stage06] report -> $OUT_FILE"
