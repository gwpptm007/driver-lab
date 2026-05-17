#!/usr/bin/env bash
set -euo pipefail

SUMMARY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TRACK_DIR="$(cd "${SUMMARY_DIR}/.." && pwd)"
FINAL_DIR="${SUMMARY_DIR}/reports/final"

cat <<'HEAD'
# track-af-xdp status snapshot

## Status table
HEAD

awk '/^\| Phase/ || /^\|---/ || /^\| [^|]+ \| `[^`]+` \|/ {print}' "${TRACK_DIR}/README.md" || true

cat <<'DOCS'

## Key documents
DOCS

for f in \
    "${TRACK_DIR}/README.md" \
    "${TRACK_DIR}/ROADMAP.md" \
    "${FINAL_DIR}/AF_XDP_TRACK_REPORT.md" \
    "${FINAL_DIR}/AF_XDP_PROJECT_PORTFOLIO.md" \
    "${FINAL_DIR}/AF_XDP_INTERVIEW_NOTES.md" \
    "${FINAL_DIR}/AF_XDP_RESUME_MATERIAL.md" \
    "${FINAL_DIR}/AF_XDP_BACKLOG.md"; do
    if [[ -f "${f}" ]]; then
        echo "DONE ${f#${TRACK_DIR}/}"
    else
        echo "MISS ${f#${TRACK_DIR}/}"
    fi
done
