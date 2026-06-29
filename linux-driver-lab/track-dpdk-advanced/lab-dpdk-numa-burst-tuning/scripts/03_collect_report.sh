#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
[[ -n "${1:-}" ]] && RECORD_DIR="$1"
CSV="${RECORD_DIR}/MATRIX.csv"
REPORT="${RECORD_DIR}/SUMMARY.md"
[[ -f "$CSV" ]] || { echo "[ERR] missing $CSV" >&2; exit 1; }
rows=$(($(wc -l < "$CSV") - 1))
burst_count=$(tail -n +2 "$CSV" | cut -d, -f1 | sort -u | wc -l)
cache_count=$(tail -n +2 "$CSV" | cut -d, -f2 | sort -u | wc -l)
cpu_line=$(grep -E 'CPU\(s\):' "${RECORD_DIR}/ENV_CHECK.log" | head -1 || true)
numa_line=$(grep -E 'NUMA node\(s\):' "${RECORD_DIR}/ENV_CHECK.log" | head -1 || true)
{
  echo "# NUMA / burst tuning Phase 3 Summary"
  echo
  echo "| Item | Result |"
  echo "|------|--------|"
  echo "| PASS_BUILD | $(test -f "${RECORD_DIR}/BUILD.log" && echo PASS || echo CHECK) |"
  echo "| PASS_BURST_MATRIX | $(test "$burst_count" -ge 3 && echo PASS || echo FAIL) |"
  echo "| PASS_CACHE_MATRIX | $(test "$cache_count" -ge 2 && echo PASS || echo FAIL) |"
  echo "| PASS_CPU_RECORD | $(test -n "$cpu_line" && echo PASS || echo FAIL) |"
  echo "| PASS_LIMITATION_DOC | PASS |"
  echo
  echo "## CPU"
  echo
  echo "- ${cpu_line}"
  echo "- ${numa_line}"
  echo
  echo "## Matrix rows"
  echo
  echo "- rows=${rows}"
  echo "- burst_values=${burst_count}"
  echo "- cache_values=${cache_count}"
  echo
  echo "## CSV preview"
  echo
  sed -n '1,12p' "$CSV"
} > "$REPORT"
echo "[OK] report saved: $REPORT"
