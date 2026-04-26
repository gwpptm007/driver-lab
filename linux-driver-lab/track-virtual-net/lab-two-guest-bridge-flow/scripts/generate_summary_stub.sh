#!/usr/bin/env bash
set -euo pipefail
OUT=${1:-reports/two_guest_flow_summary_stub.md}
mkdir -p "$(dirname "$OUT")"
cat > "$OUT" <<'EOF'
# two guest flow summary stub

## 拓扑是否创建成功
- br-vnet0:
- tap-vnet-a:
- tap-vnet-b:

## guest A/B 是否启动
- guest A:
- guest B:

## ping 是否成功
- A -> B:
- B -> A:

## FDB 是否学习两个 MAC
- guest A MAC:
- guest B MAC:

## tcpdump 是否完整
- tap A:
- bridge:
- tap B:

## 结论
- 
EOF
echo "$OUT"
