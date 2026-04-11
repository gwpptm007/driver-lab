#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DAY19_DIR="$ROOT_DIR/day19"

required=(
  "$DAY19_DIR/source_manifest.csv"
  "$DAY19_DIR/generate_day19_report.py"
)

for f in "${required[@]}"; do
  if [[ ! -f "$f" ]]; then
    echo "[ERROR] missing required file: $f" >&2
    exit 1
  fi
done

mkdir -p "$DAY19_DIR/output"
python3 "$DAY19_DIR/generate_day19_report.py"

echo
echo "[OK] Day19 outputs:"
echo "  - $DAY19_DIR/output/day19_compare_table.csv"
echo "  - $DAY19_DIR/output/day19_compare_report_draft.md"
echo "  - $DAY19_DIR/output/day19_compare_report_final.md"
echo "  - $DAY19_DIR/output/day19_risk_matrix.md"
echo "  - $DAY19_DIR/output/day19_summary.txt"
echo "  - $DAY19_DIR/output/day19_acceptance_checklist.md"
