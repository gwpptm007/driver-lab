# track-af-xdp status snapshot

日期：2026-06-07

## Status table

| Phase | Lab | Status |
|---|---|---|
| Phase 1 | `lab-xdp-redirect-basics` | PASS_BASIC=YES, PASS_ACTION=YES, REDIRECT_MODEL_READY=YES |
| Phase 2 | `lab-af-xdp-socket-rings` | PASS_SOCKET_READY=YES, PASS_UMEM_RINGS=YES, PASS_RX_TRAFFIC=YES |
| Phase 3 | `lab-af-xdp-zero-copy-vs-copy` | PASS_COPY_BASELINE=YES, PASS_NATIVE_COPY=YES, ZERO_COPY_PROBED=YES |
| Phase 4 | `project-af-xdp-mini-forwarder` | PASS_DROP=YES, PASS_REFLECT=YES, PASS_TRAFFIC=YES, PASS_TX_REFLECT=YES |
| Phase 5 | `project-af-xdp-track-summary` | UPDATED (复测结果已收口) |

## 复测环境

- OS: Ubuntu 22.04.5 LTS, Kernel: 6.8.0-111-generic
- 测试拓扑: veth pair (veth-peer → veth-xdp)
- 模式: skb+copy (所有 Phase 统一)
- veth native XDP: 支持 (kernel 5.12+)
- veth zero-copy: 不支持 (no DMA, expected)

## Key documents

```
DONE README.md
DONE ROADMAP.md (updated 2026-06-07)
DONE project-af-xdp-track-summary/reports/final/AF_XDP_TRACK_REPORT.md
DONE project-af-xdp-track-summary/reports/final/AF_XDP_PROJECT_PORTFOLIO.md
DONE project-af-xdp-track-summary/reports/final/AF_XDP_INTERVIEW_NOTES.md
DONE project-af-xdp-track-summary/reports/final/AF_XDP_RESUME_MATERIAL.md
DONE project-af-xdp-track-summary/reports/final/AF_XDP_BACKLOG.md
```

## Records (per phase)

| Phase | Record Directory |
|---|---|
| Phase 1 | `lab-xdp-redirect-basics/records/20260607-132613-xdp-redirect-basics/` |
| Phase 2 | `lab-af-xdp-socket-rings/records/20260607-135550-af-xdp-socket-rings/` |
| Phase 3 | `lab-af-xdp-zero-copy-vs-copy/records/20260607-140717-af-xdp-zero-copy-vs-copy/` |
| Phase 4 | `project-af-xdp-mini-forwarder/records/20260607-140717-af-xdp-mini-forwarder/` |
