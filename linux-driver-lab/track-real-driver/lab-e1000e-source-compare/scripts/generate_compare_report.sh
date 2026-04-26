#!/usr/bin/env bash
set -euo pipefail
OUT_FILE=${1:-reports/virtio_vs_e1000e_compare_report.md}
mkdir -p "$(dirname "$OUT_FILE")"

cat > "$OUT_FILE" <<'EOF'
# virtio_net vs e1000/e1000e compare report

## 当前建议补充的核心维度

1. 设备模型
2. probe/remove 与私有结构组织
3. TX / RX 主路径
4. IRQ / NAPI / queue 组织
5. ethtool / stats / control plane
6. 与 `netdev/stage00~stage13` 的映射

## 结论草稿

- `virtio_net` 更像半虚拟化 NIC 驱动专题
- `e1000/e1000e` 更像传统 PCI NIC 驱动专题
- 两者一起构成“第二阶段之后的真实驱动双视角”
EOF

echo "$OUT_FILE"
