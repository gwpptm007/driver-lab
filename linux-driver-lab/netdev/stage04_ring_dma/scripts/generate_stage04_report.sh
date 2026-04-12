#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
ENV_FILE="$ROOT_DIR/output/host_env_stage04.env"
REPORT_FILE="$ROOT_DIR/output/stage04_report.md"

[[ -f "$ENV_FILE" ]] || "$ROOT_DIR/scripts/check_host_env.sh"
source "$ENV_FILE"

READY=yes
if [[ "${KDIR_OK:-no}" != "yes" ]]; then
    READY=no
fi

cat > "$REPORT_FILE" <<EOF
# stage04_ring_dma report

## 环境摘要

- Host kernel: ${HOST_KERNEL}
- KDIR: ${KDIR}
- KDIR_OK: ${KDIR_OK}
- gcc available: ${HAVE_GCC}
- timeout available: ${HAVE_TIMEOUT}
- ip available: ${HAVE_IP}
- sudo available: ${HAVE_SUDO}
- debugfs root visible: ${DEBUGFS_ROOT}

## 本阶段目标

- 在 stage03 的 NAPI 教学闭环上，引入 TX/RX ring descriptor
- 用 streaming DMA 的 map/unmap 方式建立 TX/RX buffer ownership 语义
- 把 RX replenishment 作为独立关键主题做透

## 当前可执行性判断

- STAGE04_READY=${READY}
- 说明：如果 KDIR_OK=no，则当前环境只能完成 userspace / 脚本 / 文档检查，无法在本机编译模块。

## 预期 smoke 核心观察项

1. sender / receiver 能闭环
2. debugfs stats 中看到 tx_dma_map / tx_dma_unmap / rx_dma_map / rx_dma_unmap
3. ring dump 中看到 RX slot 在 POSTED / DONE 间轮转
4. refill 计数持续增加
EOF

echo "[stage04] generated report -> $REPORT_FILE"
