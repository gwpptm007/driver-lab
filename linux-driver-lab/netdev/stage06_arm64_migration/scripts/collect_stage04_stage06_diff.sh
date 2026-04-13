#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
OUT_FILE="$ROOT_DIR/output/stage04_stage06_diff.md"
mkdir -p "$ROOT_DIR/output"

TARGET_PROFILE=host "$ROOT_DIR/scripts/resolve_platform_env.sh" >/dev/null
TARGET_PROFILE=qemu-arm64 "$ROOT_DIR/scripts/resolve_platform_env.sh" >/dev/null
# shellcheck source=/dev/null
source "$ROOT_DIR/output/resolved_host.env"
source "$ROOT_DIR/output/resolved_qemu-arm64.env"

cat > "$OUT_FILE" <<EOF
# stage04 -> stage06 diff

## 迁移对象

- 源阶段：stage04_ring_dma
- 目标阶段：stage06_arm64_migration

## northbound 保持不变

- net_device 视角
- ndo_start_xmit / NAPI / stats 的观察方法
- debugfs / smoke / records 的组织方式

## southbound 发生变化

| 维度 | stage04 默认 | stage06 目标 |
|---|---|---|
| arch | host / 未强绑 | arm64 为重点 |
| build | native gcc + host KDIR | cross toolchain + arm64 build dir |
| run | host 侧调试为主 | qemu-arm64 dry-run / 真机运行 |
| kernel image | 非必须 | ARM64 Image 必要 |
| rootfs | 非必须 | ARM64 rootfs / initrd 必要 |

## 当前解析到的 ARM64 关键参数

- QEMU_BIN: ${QEMU_BIN:-}
- CROSS_COMPILE: ${CROSS_COMPILE:-}
- KDIR / KERNEL_BUILD_DIR: ${KDIR:-}
- KERNEL_IMAGE: ${KERNEL_IMAGE:-}
- ROOTFS_IMAGE: ${ROOTFS_IMAGE:-}

## 推荐迁移清单

1. 先保证 stage04 可以在 arm64 build tree 上编译
2. 再生成 qemu-system-aarch64 命令行
3. 再做真正运行验证
4. 最后输出 ARM64 smoke 记录
EOF

echo "[stage06] diff report -> $OUT_FILE"
