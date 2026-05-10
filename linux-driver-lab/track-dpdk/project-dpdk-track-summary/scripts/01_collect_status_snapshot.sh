#!/usr/bin/env bash
set -euo pipefail

SUMMARY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TRACK_DIR="$(cd "${SUMMARY_DIR}/.." && pwd)"
FINAL_DIR="${SUMMARY_DIR}/reports/final"

echo "# track-dpdk status snapshot"
echo
awk '/^\| [0-9]+ \|/ {print}' "${TRACK_DIR}/README.md" || true

echo
echo "## Key documents"
for f in \
    "${TRACK_DIR}/README.md" \
    "${TRACK_DIR}/ROADMAP_NEXT.md" \
    "${FINAL_DIR}/DPDK_TRACK_REPORT.md" \
    "${FINAL_DIR}/DPDK_PROJECT_PORTFOLIO.md" \
    "${FINAL_DIR}/DPDK_INTERVIEW_NOTES.md" \
    "${FINAL_DIR}/DPDK_RESUME_MATERIAL_FINAL.md" \
    "${FINAL_DIR}/DPDK_BACKLOG.md"; do
    if [[ -f "${f}" ]]; then
        echo "DONE ${f#${TRACK_DIR}/}"
    else
        echo "MISS ${f#${TRACK_DIR}/}"
    fi
done
