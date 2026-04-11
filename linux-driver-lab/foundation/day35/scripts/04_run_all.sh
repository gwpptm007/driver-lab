#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/common.sh"
ensure_day35_dirs

log_to_record collect.log bash "${DAY35_ROOT}/scripts/00_check_inputs.sh"
log_to_record collect.log python3 "${DAY35_ROOT}/scripts/01_collect_evidence_index.py"
log_to_record report.log python3 "${DAY35_ROOT}/scripts/02_generate_day35_report.py"
log_to_record risk.log python3 "${DAY35_ROOT}/scripts/03_generate_risk_register.py"

echo "[day35] 已生成："
echo "[day35]   ${OUTPUT_DIR}/day35_evidence_index.md"
echo "[day35]   ${OUTPUT_DIR}/day35_metrics_summary.csv"
echo "[day35]   ${OUTPUT_DIR}/day35_final_report.md"
echo "[day35]   ${OUTPUT_DIR}/day35_risk_register.md"
