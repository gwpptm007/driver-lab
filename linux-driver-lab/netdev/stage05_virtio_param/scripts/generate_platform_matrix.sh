#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_FILE="$ROOT_DIR/output/platform_matrix.md"
mkdir -p "$ROOT_DIR/output"
source "$ROOT_DIR/scripts/lib_stage05.sh"
QEMU_X86_BIN=${QEMU_SYSTEM_X86_64:-qemu-system-x86_64}
QEMU_ARM_BIN=${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}
QEMU_X86_OK=$(stage05_has_cmd "$QEMU_X86_BIN")
QEMU_ARM_OK=$(stage05_has_cmd "$QEMU_ARM_BIN")
AARCH64_GCC_OK=$(stage05_has_cmd aarch64-linux-gnu-gcc)
cat > "$OUT_FILE" <<EOF
# stage05 平台矩阵

| TARGET_ARCH | RUN_MODE | 典型用途 | 主要工具链 | 关键依赖 | 当前建议 |
|---|---|---|---|---|---|
| host | host | 文档/脚本/分析输出 | gcc=${HOST_CC:-gcc} | 无额外 QEMU 依赖 | 当前首选 |
| x86_64 | qemu-x86_64 | 未来可选 x86 QEMU 路线 | gcc=${HOST_CC:-gcc} | ${QEMU_X86_BIN}=${QEMU_X86_OK} | 可选 |
| arm64 | qemu-arm64 | stage06 ARM64 迁移主路线 | aarch64-linux-gnu-gcc=${AARCH64_GCC_OK} | ${QEMU_ARM_BIN}=${QEMU_ARM_OK} | 下一阶段重点 |
EOF
echo "[stage05] platform matrix -> $OUT_FILE"
