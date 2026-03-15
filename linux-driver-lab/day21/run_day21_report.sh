#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "$SCRIPT_DIR/generate_day21_report.py"
echo "[OK] generated:"
echo "  $SCRIPT_DIR/output/day21_report_draft.md"
echo "  $SCRIPT_DIR/output/day21_report_final.md"
echo "  $SCRIPT_DIR/output/day21_report_submission.md"
echo "  $SCRIPT_DIR/output/day21_report_onepager.md"
echo "  $SCRIPT_DIR/output/day21_acceptance_checklist.md"
echo "  $SCRIPT_DIR/output/day21_data_snapshot.csv"
echo "  $SCRIPT_DIR/output/day21_submission_summary.txt"
