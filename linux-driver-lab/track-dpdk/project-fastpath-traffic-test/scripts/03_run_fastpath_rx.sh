#!/usr/bin/env bash
#===============================================================================
# 03_run_fastpath_rx.sh - 运行 fastpath-lite（RX 单端口模式）
# 作用：启动 fastpath-lite，30 秒内轮询 DPDK 口并打印统计
# 输出：records/<tag>/FASTPATH_RX.log
# 注意：运行期间需要外部发包才能看到 rx>0
#===============================================================================
source "$(dirname "$0")/common.sh"

OUT="${RECORD_DIR}/FASTPATH_RX.log"
CMD_OUT="${RECORD_DIR}/FASTPATH_RX_COMMAND.txt"

if [[ ! -x "${FASTPATH_BIN}" ]]; then
  echo "[ERR] fastpath binary not found: ${FASTPATH_BIN}" >&2
  echo "Run: ./scripts/01_build_fastpath.sh" >&2
  exit 1
fi

CMD=("${FASTPATH_BIN}"
  -l "${FASTPATH_LCORES}"
  -n "${FASTPATH_MEMORY_CHANNELS}"
  --file-prefix "${FASTPATH_FILE_PREFIX}"
  -a "${DPDK_PCI}"
  --
  --run-seconds "${FASTPATH_RUN_SECONDS}"
  --stats-period "${FASTPATH_STATS_PERIOD}"
  --burst-size "${FASTPATH_BURST_SIZE}"
  --promisc "${FASTPATH_PROMISC}"
  --udp-only "${FASTPATH_UDP_ONLY}"
  --swap-mac "${FASTPATH_SWAP_MAC}"
  --rewrite "${FASTPATH_REWRITE_ENABLE}"
)

if [[ -n "${FASTPATH_EXTRA_APP_ARGS}" ]]; then
  # shellcheck disable=SC2206
  EXTRA_ARGS=( ${FASTPATH_EXTRA_APP_ARGS} )
  CMD+=("${EXTRA_ARGS[@]}")
fi

printf '%q ' "${CMD[@]}" > "${CMD_OUT}"
echo >> "${CMD_OUT}"

{
  echo "# FASTPATH_RX"
  echo
  log_env
  echo
  echo "## command"
  cat "${CMD_OUT}"
  echo
  echo "## note"
  echo "在本脚本运行期间，请从外部 VM/宿主机向 DPDK 口发送 UDP 流量。"
  echo
  "${CMD[@]}"
  rc=$?
  echo "rc=${rc}"
  exit "${rc}"
} 2>&1 | tee "${OUT}"

echo "[OK] fastpath rx run saved: ${OUT}"
