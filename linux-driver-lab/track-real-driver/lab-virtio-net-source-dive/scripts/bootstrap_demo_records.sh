#!/usr/bin/env bash
set -euo pipefail
TARGET_BASE=${1:-records/demo-copy}
mkdir -p "$TARGET_BASE"
cp -r records/examples/2026-demo-round1-arch "$TARGET_BASE/"
cp -r records/examples/2026-demo-round2-txrx "$TARGET_BASE/"
cp -r records/examples/2026-demo-round3-feature-xdp "$TARGET_BASE/"
echo "demo records copied to: $TARGET_BASE"
