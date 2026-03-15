#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
CMD="${1:-help}"
shift || true

case "$CMD" in
  dry-run)
    "$SCRIPT_DIR/run_day20_regression.sh" --dry-run "$@"
    ;;
  all|smoke|trace|perf|stress)
    MODE="$CMD" "$SCRIPT_DIR/run_day20_regression.sh" "$@"
    ;;
  latest)
    "$SCRIPT_DIR/run_day20_latest.sh" "$@"
    ;;
  summary)
    "$SCRIPT_DIR/run_day20_summary.sh" "$@"
    ;;
  verify)
    "$SCRIPT_DIR/run_day20_verify.sh" "$@"
    ;;
  inspect)
    "$SCRIPT_DIR/run_day20_latest.sh" "$@"
    ;;
  help|--help|-h)
    cat <<'EOF'
Day20 suite entry

Usage:
  ./run_day20_suite.sh dry-run
  ./run_day20_suite.sh all
  ./run_day20_suite.sh smoke
  ./run_day20_suite.sh trace
  ./run_day20_suite.sh perf
  ./run_day20_suite.sh stress
  ./run_day20_suite.sh latest [record_dir]
  ./run_day20_suite.sh summary
  ./run_day20_suite.sh verify

Notes:
- dry-run: 只检查 image/rootfs/dtb/module 是否齐
- all: 依次执行 smoke + trace + perf + stress
- latest: 查看最近一次或指定 record
- verify: 生成 Day20 交付状态报告
EOF
    ;;
  *)
    echo "[ERROR] unknown command: $CMD" >&2
    exit 2
    ;;
esac
