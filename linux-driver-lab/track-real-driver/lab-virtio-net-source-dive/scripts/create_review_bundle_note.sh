#!/usr/bin/env bash
set -euo pipefail
OUT_FILE=${1:-reports/review_bundle_note.md}
mkdir -p "$(dirname "$OUT_FILE")"
cat > "$OUT_FILE" <<'EOF'
# review bundle note

建议评审时至少一起查看：

- docs/02_VIRTIO_NET_ARCHITECTURE.md
- docs/03_PROBE_TX_RX_READING_ORDER.md
- docs/04_TX_PATH.md
- docs/05_RX_PATH.md
- docs/07_FEATURES_ETHTOOL_XDP.md
- reports/stage_vs_virtio_net_report.md
- 最近一轮 records/*/SUMMARY.md
EOF
echo "$OUT_FILE"
