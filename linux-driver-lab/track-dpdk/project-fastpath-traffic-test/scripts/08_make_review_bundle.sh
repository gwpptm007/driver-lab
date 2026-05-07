#!/usr/bin/env bash
#===============================================================================
# 08_make_review_bundle.sh - 生成评审报告
# 作用：汇总所有记录文件，生成 Markdown 格式评审包
# 输入：ENV_CHECK.txt, BUILD.log, PREPARE_VMXNET3.txt, FASTPATH_RX.log,
#      COMPARE_STATS.txt 等
# 输出：records/<tag>/REVIEW_BUNDLE.md
#===============================================================================
source "$(dirname "$0")/common.sh"

DIR="${1:-$(latest_record_dir)}"
if [[ -z "${DIR}" || ! -d "${DIR}" ]]; then
  echo "[ERR] no record dir found" >&2
  exit 1
fi

OUT="${DIR}/REVIEW_BUNDLE.md"
LOG="${DIR}/FASTPATH_RX.log"
COMPARE="${DIR}/COMPARE_STATS.txt"
{
  echo "# REVIEW_BUNDLE - project-fastpath-traffic-test"
  echo
  echo "## Record directory"
  echo
  echo "\`${DIR}\`"
  echo
  echo "## Checklist"
  echo
  echo "| Item | Status |"
  echo "|---|---|"
  for f in ENV_CHECK.txt BUILD.log PREPARE_VMXNET3.txt FASTPATH_RX.log UDP_SENDER_HINT.txt COMPARE_STATS.txt; do
    if [[ -f "${DIR}/${f}" ]]; then
      echo "| ${f} | DONE |"
    else
      echo "| ${f} | MISSING |"
    fi
  done
  echo
  echo "## Evidence grep"
  echo
  echo "| Evidence | Found |"
  echo "|---|---|"
  for pat in "fastpath-lite config" "policy: promisc" "port 0 started" "enter fastpath loop" "fastpath-lite software stats" "rte_eth_stats" "bye"; do
    if [[ -f "${LOG}" ]] && grep -q "${pat}" "${LOG}"; then
      echo "| ${pat} | YES |"
    else
      echo "| ${pat} | NO |"
    fi
  done
  echo
  echo "## Stats verdict"
  echo
  if [[ -f "${COMPARE}" ]]; then
    sed -n '/## verdict hint/,$p' "${COMPARE}"
  else
    echo "COMPARE_STATS.txt missing. Run ./scripts/07_compare_stats.sh"
  fi
  echo
  echo "## Suggested verdict"
  echo
  echo "- rx=0: PASS_SMOKE only."
  echo "- rx>0 and udp/ipv4>0: PASS_TRAFFIC."
  echo "- rewrite>0: PASS_REWRITE."
  echo "- tx>0 in two-port/vhost topology: PASS_FORWARDING."
} > "${OUT}"

echo "[OK] review bundle saved: ${OUT}"
