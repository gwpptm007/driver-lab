#!/usr/bin/env bash
set -euo pipefail

SUMMARY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TRACK_DIR="$(cd "${SUMMARY_DIR}/.." && pwd)"
TS="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${SUMMARY_DIR}/records/${TS}-dpdk-track-report"
FINAL_DIR="${SUMMARY_DIR}/reports/final"
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
copy_if_exists "${TRACK_DIR}/ROADMAP_NEXT.md" "${OUT_DIR}/ROADMAP_NEXT.md"
copy_if_exists "${SUMMARY_DIR}/README.md" "${OUT_DIR}/SUMMARY_README.md"
copy_if_exists "${FINAL_DIR}/DPDK_TRACK_REPORT.md" "${OUT_DIR}/DPDK_TRACK_REPORT.md"
copy_if_exists "${FINAL_DIR}/DPDK_PROJECT_PORTFOLIO.md" "${OUT_DIR}/DPDK_PROJECT_PORTFOLIO.md"
copy_if_exists "${FINAL_DIR}/DPDK_INTERVIEW_NOTES.md" "${OUT_DIR}/DPDK_INTERVIEW_NOTES.md"
copy_if_exists "${FINAL_DIR}/DPDK_RESUME_MATERIAL_FINAL.md" "${OUT_DIR}/DPDK_RESUME_MATERIAL_FINAL.md"
copy_if_exists "${FINAL_DIR}/DPDK_BACKLOG.md" "${OUT_DIR}/DPDK_BACKLOG.md"

cat > "${OUT_DIR}/STATUS_TABLE.md" <<'TABLE'
# DPDK Track Status Table

| 阶段 | 状态 |
|---|---|
| lab-vmxnet3-testpmd | PASS |
| lab-vhost-user-basic | PASS |
| lab-virtio-user-vhost | PASS_WITH_WARN |
| lab-dpdk-l2-forwarding | PASS_SMOKE |
| project-user-space-fastpath | PASS_SMOKE |
| project-fastpath-traffic-test | READY_TO_TEST |
| project-dpdk-media-gateway-lite | PASS_SMOKE |
| project-dpdk-v17-legacy-review | PASS_REVIEW |
| project-dpdk-track-summary | READY |
TABLE

cat > "${OUT_DIR}/REVIEW_BUNDLE.md" <<BUNDLE
# DPDK Track Report Bundle

Generated: ${TS}

## Included files

\`\`\`text
$(cat "${OUT_DIR}/FILES.txt")
\`\`\`

## Summary

This bundle is for DPDK track stage closeout. It includes the track report, portfolio, interview notes, final resume material, and backlog for media-gateway-lite traffic/forwarding/rewrite follow-up.

## Suggested reading order

1. README.md
2. SUMMARY_README.md
3. DPDK_TRACK_REPORT.md
4. DPDK_PROJECT_PORTFOLIO.md
5. DPDK_INTERVIEW_NOTES.md
6. DPDK_RESUME_MATERIAL_FINAL.md
7. DPDK_BACKLOG.md
BUNDLE

echo "[OK] track report bundle generated: ${OUT_DIR}"
