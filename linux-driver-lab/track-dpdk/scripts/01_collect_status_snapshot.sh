#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "# track-dpdk status snapshot"
echo
awk '/^\| [0-9]+ \|/ {print}' "${ROOT_DIR}/README.md" || true

echo
echo "## Key documents"
for f in README.md ROADMAP_NEXT.md DPDK_TRACK_REPORT.md DPDK_PROJECT_PORTFOLIO.md DPDK_INTERVIEW_NOTES.md DPDK_RESUME_MATERIAL_FINAL.md DPDK_BACKLOG.md; do
    if [[ -f "${ROOT_DIR}/${f}" ]]; then
        echo "DONE ${f}"
    else
        echo "MISS ${f}"
    fi
done
