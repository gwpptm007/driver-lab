#!/usr/bin/env bash
set -euo pipefail
OUT=${1:?usage: $0 <record-dir>}
mkdir -p "$OUT"

cat > "$OUT/INPUT_LABS.md" <<'EOF'
# INPUT LABS

## lab-virtio-tap-bridge-path
- records path:
- key evidence:

## lab-virtio-vhost-kick-notify
- records path:
- key evidence:

## lab-two-guest-bridge-flow
- records path:
- key evidence:
EOF

echo "$OUT/INPUT_LABS.md"
