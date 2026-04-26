#!/usr/bin/env bash
set -euo pipefail
OUT=${1:?usage: $0 <record-dir>}
mkdir -p "$OUT"

cat > "$OUT/REVIEW_BUNDLE.md" <<'EOF'
# REVIEW BUNDLE

## Recommended review order

1. FINAL_PROJECT_REPORT.md
2. FINAL_TOPOLOGY.md
3. EVIDENCE_INDEX.md
4. INPUT_LABS.md
5. SHARE_SCRIPT.md

## Key questions

- tap/bridge path 是否跑通？
- vhost=off/on 是否有对照？
- two guest flow 是否跑通？
- 证据是否足够支撑结论？
EOF

echo "$OUT/REVIEW_BUNDLE.md"
