#!/usr/bin/env bash
#===============================================================================
# 08_make_review_bundle.sh - 生成测试报告
# 作用：汇总单次测试运行的所有输出，生成标准化的 REVIEW_BUNDLE.md
# 说明：包含检查清单、日志列表、统计对比、判决建议
#===============================================================================
set -euo pipefail

# 引入公共变量和函数
source "$(dirname "$0")/common.sh"

#----------------------------------------
# 获取记录目录（支持传入目录路径，或自动查找最新目录）
#----------------------------------------
DIR="${1:-$(latest_record_dir)}"
if [[ -z "${DIR}" || ! -d "${DIR}" ]]; then
  echo "[ERR] no record dir found" >&2
  exit 1
fi

# 输出报告文件
OUT="${DIR}/REVIEW_BUNDLE.md"                 # 最终报告
COMPARE="${DIR}/COMPARE_STATS.txt"           # 统计对比（由 07_collect_stats.sh 生成）

# 查找所有 MEDIA_GATEWAY 日志
mapfile -t LOGS < <(find "${DIR}" -maxdepth 1 -type f -name 'MEDIA_GATEWAY_*.log' | sort)

{
  echo "# REVIEW_BUNDLE - project-dpdk-media-gateway-lite"
  echo
  echo "## Record directory"
  echo
  echo "\`${DIR}\`"        # 记录目录路径（便于跳转查看）
  echo

  #----------------------------------------
  # 检查清单：验证每个关键文件是否存在且非空
  # 使用 stat -c%s 获取文件字节数，0 表示空文件
  #----------------------------------------
  echo "## Checklist"
  echo
  echo "| Item | Status |"
  echo "|---|---|"

  check_file() {
    local file="$1" label="$2"
    if [[ -f "${file}" ]]; then
      local size
      size=$(stat -c%s "${file}" 2>/dev/null || echo 0)
      if [[ "${size}" -gt 0 ]]; then
        echo "| ${label} | DONE |"
      else
        echo "| ${label} | EMPTY |"
      fi
    else
      echo "| ${label} | MISSING |"
    fi
  }

  check_file "${DIR}/BUILD.log" "BUILD.log"
  check_file "${DIR}/COMPARE_STATS.txt" "COMPARE_STATS.txt"
  [[ "${#LOGS[@]}" -gt 0 ]] && echo "| MEDIA_GATEWAY log | DONE (${#LOGS[@]} files) |" || echo "| MEDIA_GATEWAY log | MISSING |"
  echo

  # 列出所有日志文件
  echo "## Logs"
  printf -- '- `%s`\n' "${LOGS[@]}"
  echo

  # 打印统计对比（如果已生成）
  if [[ -f "${COMPARE}" ]]; then
    echo "## COMPARE_STATS"
    echo
    sed -n '1,220p' "${COMPARE}"   # 限制输出前 220 行，避免过长
    echo
  fi

  #----------------------------------------
  # 判决建议：根据检查清单状态给出 PASS/FAIL 建议
  #----------------------------------------
  echo "## Suggested verdict"
  echo
  echo "- PASS_BUILD: build binary exists"                    # 二进制文件存在
  echo "- PASS_SMOKE: program starts, ports start, stats printed"  # 程序启动正常
  echo "- PASS_RULE_CONFIG: rules/rewrite config printed"    # 规则配置正确
  echo "- PASS_TRAFFIC/FORWARDING/REWRITE: require non-zero parsed counters"  # 需实际流量
} > "${OUT}"

# 在终端也打印报告
cat "${OUT}"
echo "[OK] review bundle saved: ${OUT}"