#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
RECORDS_DIR="${PROJECT_DIR}/records"
TS="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${RECORDS_DIR}/${TS}-v17-legacy-review"

mkdir -p "${OUT_DIR}"

BUNDLE="${OUT_DIR}/REVIEW_BUNDLE.md"

cat > "${BUNDLE}" <<EOF
# REVIEW_BUNDLE - project-dpdk-v17-legacy-review

## Generated


timestamp: ${TS}
project: project-dpdk-v17-legacy-review
status: READY_TO_REVIEW

## Purpose

This bundle checks whether the DPDK v17 legacy experience has been mapped to the current modern DPDK track.

## Documents

| File | Status |
|---|---|
EOF

for f in \
  README.md \
  START_HERE.md \
  docs/01_GOAL_AND_SCOPE.md \
  docs/02_V17_PROJECT_RECONSTRUCTION.md \
  docs/03_V17_TO_MODERN_MAPPING.md \
  docs/04_KNI_UIO_VFIO_VHOST_REVIEW.md \
  docs/05_MEDIA_GATEWAY_MIGRATION.md \
  docs/06_MIGRATION_CHECKLIST.md \
  docs/07_INTERVIEW_EXPLANATION.md \
  docs/08_RESUME_MATERIAL.md \
  reports/DPDK_V17_LEGACY_REVIEW.md \
  reports/DPDK_INTERVIEW_ANSWER_CARD.md \
  reports/DPDK_RESUME_BULLETS.md; do
  if [[ -f "${PROJECT_DIR}/${f}" ]]; then
    echo "| ${f} | DONE |" >> "${BUNDLE}"
  else
    echo "| ${f} | MISSING |" >> "${BUNDLE}"
  fi
done

cat >> "${BUNDLE}" <<'EOF'

## Acceptance

```text
PASS_REVIEW if:
  - v17 media-plane data path is described
  - v17 -> modern DPDK mapping is described
  - KNI/UIO/VFIO/vhost/virtio boundaries are described
  - current media-gateway-lite status is stated honestly as PASS_SMOKE
  - interview and resume materials are generated
```

## Result

```text
PASS_REVIEW
```

## Next

```text
Return to project-dpdk-media-gateway-lite later and add PASS_TRAFFIC / PASS_FORWARDING / PASS_REWRITE records.
Then generate final DPDK_TRACK_REPORT / DPDK_PROJECT_PORTFOLIO.
```
EOF

cp "${PROJECT_DIR}/reports/DPDK_V17_LEGACY_REVIEW.md" "${OUT_DIR}/" 2>/dev/null || true
cp "${PROJECT_DIR}/reports/DPDK_INTERVIEW_ANSWER_CARD.md" "${OUT_DIR}/" 2>/dev/null || true
cp "${PROJECT_DIR}/reports/DPDK_RESUME_BULLETS.md" "${OUT_DIR}/" 2>/dev/null || true

ln -sfn "$(basename "${OUT_DIR}")" "${RECORDS_DIR}/latest" 2>/dev/null || true

echo "[OK] review bundle generated: ${BUNDLE}"
