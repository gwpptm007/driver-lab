#!/usr/bin/env bash
#===============================================================================
# 07_collect_stats.sh - 收集和解析统计数据
# 作用：从运行记录目录中收集 MEDIA_GATEWAY_*.log 并解析关键统计指标
# 说明：调用 tools/parse_gateway_stats.py 解析 DPDK 应用的 per-port 和 per-rule 统计
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

# 输出统计对比文件
OUT="${DIR}/COMPARE_STATS.txt"

# 查找该目录下所有 MEDIA_GATEWAY 日志文件
mapfile -t LOGS < <(find "${DIR}" -maxdepth 1 -type f -name 'MEDIA_GATEWAY_*.log' | sort)

{
  echo "# COMPARE_STATS"
  echo
  echo "record_dir=${DIR}"    # 记录目录路径（便于溯源）
  echo
  echo "## logs"              # 列出找到的日志文件

  # 无日志文件时提示
  if [[ "${#LOGS[@]}" -eq 0 ]]; then
    echo "NO_MEDIA_GATEWAY_LOG_FOUND"
  else
    printf '%s\n' "${LOGS[@]}"   # 打印所有日志文件路径
  fi
  echo

  #----------------------------------------
  # 调用 Python 脚本解析统计日志
  # parse_gateway_stats.py 提取：rx/tx/drops/rewrite/rule_hit 等计数器
  #----------------------------------------
  echo "## parsed"
  if [[ "${#LOGS[@]}" -gt 0 ]]; then
    python3 "${PROJECT_ROOT}/tools/parse_gateway_stats.py" "${LOGS[@]}"
  fi
} | tee "${OUT}"

echo "[OK] stats saved: ${OUT}"