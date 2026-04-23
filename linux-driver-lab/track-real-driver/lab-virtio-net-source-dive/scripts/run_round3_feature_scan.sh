#!/usr/bin/env bash
set -euo pipefail
OUT_DIR=${1:-$(./scripts/create_round_workspace.sh round3-feature-xdp)}
REPORT="$OUT_DIR/feature_xdp_checklist.txt"
cat > "$REPORT" <<'EOF'
手工检查以下主题并补充到 SUMMARY.md:
1. feature negotiation 相关入口
2. ethtool ops 与 stats 导出
3. offload 能力与 feature bits
4. XDP attach / fast path 入口
5. 回填 stage12~stage14 mapping
EOF
echo "Round3 output: $OUT_DIR"
