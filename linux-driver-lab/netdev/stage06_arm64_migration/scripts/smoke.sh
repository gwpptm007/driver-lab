#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)

"$ROOT_DIR/scripts/check_platform_env.sh"
TARGET_PROFILE=host "$ROOT_DIR/scripts/resolve_platform_env.sh"
TARGET_PROFILE=qemu-x86_64 "$ROOT_DIR/scripts/resolve_platform_env.sh"
TARGET_PROFILE=qemu-arm64 "$ROOT_DIR/scripts/resolve_platform_env.sh"
"$ROOT_DIR/scripts/generate_platform_matrix.sh"
"$ROOT_DIR/scripts/collect_stage04_stage06_diff.sh"
"$ROOT_DIR/scripts/dryrun_arm64_qemu.sh"
"$ROOT_DIR/scripts/generate_stage06_report.sh"

echo "[stage06] smoke completed"
