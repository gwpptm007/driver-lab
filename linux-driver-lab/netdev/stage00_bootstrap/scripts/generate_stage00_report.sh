#!/usr/bin/env bash
set -euo pipefail
mkdir -p output
REPORT=output/stage00_report.md
PATHS=output/discovered_paths.env
TOOLS=output/host_tools.txt
READY=yes
[[ -f "$PATHS" ]] || READY=no
[[ -f "$TOOLS" ]] || READY=no
cat > "$REPORT" <<EOF
# Stage00 Bootstrap Report

## Summary
- TARGET_ARCH: ${TARGET_ARCH:-host}
- RUN_MODE: ${RUN_MODE:-host}
- STAGE00_READY: $READY

## Files
- discovered paths: $PATHS
- host tools: $TOOLS

## Notes
- Stage00 is architecture-neutral by design.
- ARM64 migration is intentionally deferred to stage06.
EOF

echo "[stage00] report written to $REPORT"
