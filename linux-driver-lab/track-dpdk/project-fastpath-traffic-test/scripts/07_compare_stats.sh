#!/usr/bin/env bash
#===============================================================================
# 07_compare_stats.sh - 对比统计结果
# 作用：从 FASTPATH_RX.log 提取软件统计，调用 Python 解析给出判定
# 输入：records/<tag>/FASTPATH_RX.log
# 输出：records/<tag>/COMPARE_STATS.txt
# 判定：rx=0 → PASS_SMOKE，rx>0 → PASS_TRAFFIC
#===============================================================================
source "$(dirname "$0")/common.sh"

DIR="${1:-$(latest_record_dir)}"
if [[ -z "${DIR}" || ! -d "${DIR}" ]]; then
  echo "[ERR] no record dir found" >&2
  exit 1
fi

OUT="${DIR}/COMPARE_STATS.txt"
LOG="${DIR}/FASTPATH_RX.log"
{
  echo "# COMPARE_STATS"
  echo
  echo "record_dir=${DIR}"
  echo "log=${LOG}"
  echo
  if [[ ! -f "${LOG}" ]]; then
    echo "FASTPATH_RX.log: MISSING"
    exit 0
  fi
  echo "## extracted software stats"
  grep -E "port [0-9]+: rx=|fastpath-lite software stats|rte_eth_stats" "${LOG}" || true
  echo
  echo "## verdict hint"
  python3 "${PROJECT_ROOT}/tools/parse_fastpath_stats.py" "${LOG}" || true
} | tee "${OUT}"

echo "[OK] compare stats saved: ${OUT}"
