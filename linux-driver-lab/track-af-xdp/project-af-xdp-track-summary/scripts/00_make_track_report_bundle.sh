#!/usr/bin/env bash
set -euo pipefail

SUMMARY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TRACK_DIR="$(cd "${SUMMARY_DIR}/.." && pwd)"
TS="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${SUMMARY_DIR}/records/${TS}-af-xdp-track-report"
FINAL_DIR="${SUMMARY_DIR}/reports/final"
DOCS_DIR="${SUMMARY_DIR}/docs"
mkdir -p "${OUT_DIR}"

copy_if_exists() {
    local src="$1"
    local dst="$2"
    if [[ -f "${src}" ]]; then
        cp "${src}" "${dst}"
        echo "DONE  ${src#${TRACK_DIR}/}" >> "${OUT_DIR}/FILES.txt"
    else
        echo "MISS  ${src#${TRACK_DIR}/}" >> "${OUT_DIR}/FILES.txt"
    fi
}

: > "${OUT_DIR}/FILES.txt"

copy_if_exists "${TRACK_DIR}/README.md" "${OUT_DIR}/README.md"
copy_if_exists "${TRACK_DIR}/ROADMAP.md" "${OUT_DIR}/ROADMAP.md"
copy_if_exists "${SUMMARY_DIR}/README.md" "${OUT_DIR}/SUMMARY_README.md"
copy_if_exists "${DOCS_DIR}/03_LAB_STATUS_MATRIX.md" "${OUT_DIR}/LAB_STATUS_MATRIX.md"
copy_if_exists "${FINAL_DIR}/AF_XDP_TRACK_REPORT.md" "${OUT_DIR}/AF_XDP_TRACK_REPORT.md"
copy_if_exists "${FINAL_DIR}/AF_XDP_PROJECT_PORTFOLIO.md" "${OUT_DIR}/AF_XDP_PROJECT_PORTFOLIO.md"
copy_if_exists "${FINAL_DIR}/AF_XDP_INTERVIEW_NOTES.md" "${OUT_DIR}/AF_XDP_INTERVIEW_NOTES.md"
copy_if_exists "${FINAL_DIR}/AF_XDP_RESUME_MATERIAL.md" "${OUT_DIR}/AF_XDP_RESUME_MATERIAL.md"
copy_if_exists "${FINAL_DIR}/AF_XDP_BACKLOG.md" "${OUT_DIR}/AF_XDP_BACKLOG.md"

cat > "${OUT_DIR}/STATUS_TABLE.md" <<'TABLE'
# AF_XDP Track Status Table

| 阶段 | 状态 |
|---|---|
| lab-xdp-redirect-basics | PASS_BASIC_ATTACH, DROP/REDIRECT backlog |
| lab-af-xdp-socket-rings | READY_TO_TEST |
| lab-af-xdp-zero-copy-vs-copy | READY_TO_TEST |
| project-af-xdp-mini-forwarder | READY_TO_TEST |
| project-af-xdp-track-summary | READY |
TABLE

cat > "${OUT_DIR}/REVIEW_BUNDLE.md" <<BUNDLE
# AF_XDP Track Report Bundle

Generated: ${TS}

## Included files

\`\`\`text
$(cat "${OUT_DIR}/FILES.txt")
\`\`\`

## Summary

This bundle is for AF_XDP track stage closeout. It includes the track report, portfolio, interview notes, resume material, and backlog for XDP/AF_XDP retest follow-up.

## Suggested reading order

1. README.md
2. SUMMARY_README.md
3. LAB_STATUS_MATRIX.md
4. AF_XDP_TRACK_REPORT.md
5. AF_XDP_PROJECT_PORTFOLIO.md
6. AF_XDP_INTERVIEW_NOTES.md
7. AF_XDP_RESUME_MATERIAL.md
8. AF_XDP_BACKLOG.md
BUNDLE

echo "[OK] AF_XDP track report bundle generated: ${OUT_DIR}"
