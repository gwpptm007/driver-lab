#!/usr/bin/env bash
set -euo pipefail
OUT_DIR=${1:?usage: $0 <record-dir>}
mkdir -p "$OUT_DIR"
cat > "$OUT_DIR/runtime_link_note.md" <<'EOF'
Link the current patch experiment to records from:
- ../lab-virtio-net-runtime-observe/
- relevant baseline ethtool -S
- relevant workload and dmesg evidence
EOF
echo "$OUT_DIR/runtime_link_note.md"
