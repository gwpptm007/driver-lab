# AF_XDP Evidence

## 对应章节

- `../docs/05_AF_XDP_PATH.md`
- `../docs/10_CROSS_PATH_COMPARISON.md` (三种路径横向对比)

## 主入口

- `../../track-af-xdp/README.md`
- `../../track-af-xdp/project-af-xdp-track-summary/reports/final/AF_XDP_TRACK_REPORT.md`

## 关键证据

- `../../track-af-xdp/project-af-xdp-track-summary/reports/final/AF_XDP_TRACK_REPORT.md`
- `../../track-af-xdp/project-af-xdp-track-summary/reports/final/AF_XDP_RESUME_MATERIAL.md`
- `../../track-af-xdp/project-af-xdp-track-summary/reports/final/AF_XDP_PROJECT_PORTFOLIO.md`
- `../../track-af-xdp/project-af-xdp-track-summary/records/STATUS_SNAPSHOT_LATEST.md`

## 已证明

```text
Phase 1: XDP redirect basics PASS_BASIC/ACTION/REDIRECT
Phase 2: AF_XDP socket / UMEM / rings PASS_SOCKET/UMEM/RX_TRAFFIC
Phase 3: copy / zero-copy probe PASS_COPY/NATIVE/ZC_PROBED
Phase 4: mini forwarder PASS_DROP/REFLECT/TRAFFIC/TX
```

## 关键结论

```text
XDP_PASS / XDP_DROP / XDP_REDIRECT 已验证
XDP redirect -> XSKMAP -> AF_XDP socket -> UMEM RX ring -> 用户态 poll 已验证
FILL -> RX -> TX -> COMPLETION -> FILL 生命周期闭环已验证
veth pair 解决 local delivery 短路问题
zero-copy unsupported 边界已明确
```

## 边界

当前未完成真实 NIC zero-copy 高性能压测。
