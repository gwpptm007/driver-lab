#!/usr/bin/env bash
set -euo pipefail
OUT_DIR=${1:?usage: $0 <record-dir>}
mkdir -p "$OUT_DIR/trace"
cat > "$OUT_DIR/trace/TRACE_TODO.md" <<'EOF'
补充当前项目的 trace / runtime 证据：
1. 说明准备采哪些点
2. 说明为什么这些点足够解释 patch 前后差异
3. 保存 trace 输出或关键摘要
EOF
echo "$OUT_DIR/trace/TRACE_TODO.md"
