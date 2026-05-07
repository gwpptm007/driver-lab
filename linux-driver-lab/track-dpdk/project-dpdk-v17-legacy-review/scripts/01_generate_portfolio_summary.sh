#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TRACK_DIR="$(cd "${PROJECT_DIR}/.." && pwd)"
OUT="${TRACK_DIR}/DPDK_PROJECT_PORTFOLIO.md"

cat > "${OUT}" <<'EOF'
# DPDK_PROJECT_PORTFOLIO

## 能力线

```text
vmxnet3/testpmd
  -> vhost-user
  -> virtio-user
  -> l2fwd-lite
  -> fastpath-lite
  -> traffic-test
  -> media-gateway-lite
  -> v17 legacy review
```

## 当前状态

| 阶段 | 状态 | 说明 |
|---|---|---|
| lab-vmxnet3-testpmd | PASS | PMD 接管与 testpmd smoke |
| lab-vhost-user-basic | PASS | vhost-user socket/backend |
| lab-virtio-user-vhost | PASS_WITH_WARN | virtio-user + vhost-user 对接 |
| lab-dpdk-l2-forwarding | PASS_SMOKE | 自写 l2fwd-lite |
| project-user-space-fastpath | PASS_SMOKE | fastpath-lite 框架 |
| project-fastpath-traffic-test | READY_TO_TEST | traffic/rewrite 验证入口 |
| project-dpdk-media-gateway-lite | PASS_SMOKE | 双 vdev smoke，真实流量后补 |
| project-dpdk-v17-legacy-review | PASS_REVIEW | 旧经验迁移和面试材料 |

## 简历表达

```text
基于 DPDK 构建用户态数据面实验与媒体网关原型，完成 vmxnet3 PMD 接管、vhost-user/virtio-user、自研 l2fwd-lite/fastpath-lite、media-gateway-lite smoke 验证，并结合 DPDK v17 媒体面经验整理 KNI、UIO/VFIO、vhost/virtio、UDP fastpath、rewrite 与统计验收的现代化迁移复盘。
```

## 后续补强

```text
1. media-gateway-lite PASS_TRAFFIC
2. media-gateway-lite PASS_FORWARDING
3. media-gateway-lite PASS_REWRITE
4. DPDK_TRACK_REPORT 总结
5. 简历最终压缩版
```
EOF

echo "[OK] portfolio summary generated: ${OUT}"
