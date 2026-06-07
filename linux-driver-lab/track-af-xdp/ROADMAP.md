# track-af-xdp ROADMAP

## Phase 1: lab-xdp-redirect-basics

状态：**PASS_BASIC=YES, PASS_ACTION=YES, REDIRECT_MODEL_READY=YES**（2026-06-07 复测通过）。

已证明：

- BPF 程序可编译；
- XDP 可 attach/detach 到 veth-xdp/ens192；
- XDP_PASS (12 pkts) / XDP_DROP (3 pkts) / XDP_REDIRECT (3 pkts) 全部验证；
- skb/native 模式均可工作；
- veth pair 拓扑解决了同主机发包 XDP hook 不触发的问题。

记录：`records/20260607-132613-xdp-redirect-basics/`

## Phase 2: lab-af-xdp-socket-rings

状态：**PASS_SOCKET_READY=YES, PASS_UMEM_RINGS=YES, PASS_RX_TRAFFIC=YES**（2026-06-07 复测通过）。

已证明：

- UMEM 创建与 frame 管理 (4096 frames, 2048 bytes/frame, 8MB)；
- AF_XDP socket 创建 (XDP_COPY 模式)；
- FILL/RX/TX/COMPLETION 四类 ring 初始化；
- XSKMAP 注册 (queue 0 → socket fd)；
- XDP redirect → AF_XDP socket → 用户态 poll 收包 (rx_packets=49 首轮, rx_packets=3 脚本轮)；
- BPF per-CPU stats map 统计。

记录：`records/20260607-135550-af-xdp-socket-rings/`

## Phase 3: lab-af-xdp-zero-copy-vs-copy

状态：**PASS_COPY_BASELINE=YES, PASS_NATIVE_COPY=YES, ZERO_COPY_PROBED=YES**（2026-06-07 复测通过）。

已证明：

- skb+copy 基线通过 (rx_packets=3)；
- native+copy 在 veth 上通过 (rx_packets=3)，veth kernel 5.12+ 支持 native XDP；
- native+zero-copy 在 veth 上不支持 (xsk_socket__create: Operation not supported)，这是预期结果；
- COMPARE_MODES 分类判定完成，fallback 策略已记录。

记录：`records/20260607-140717-af-xdp-zero-copy-vs-copy/`

## Phase 4: project-af-xdp-mini-forwarder

状态：**PASS_BUILD=YES, PASS_DROP_SMOKE=YES, PASS_REFLECT_SMOKE=YES, PASS_TRAFFIC=YES, PASS_TX_REFLECT=YES**（2026-06-07 复测通过）。

已证明：

- drop 模式：rx=3, dropped=3, fill_recycled=3
- reflect 模式：rx=3, tx=3, comp=3 — 首次验证 TX ring 和 COMPLETION ring 完整生命周期
- FILL → RX → TX → COMPLETION → FILL 完整 frame 流转

记录：`records/20260607-140717-af-xdp-mini-forwarder/`

## Phase 5: project-af-xdp-track-summary

状态：`READY`，待根据复测结果刷新最终报告。

目标：把 AF_XDP track 当前阶段收口成可读、可归档、可面试表达的项目材料。

输出：

- `AF_XDP_TRACK_REPORT.md`；
- `AF_XDP_PROJECT_PORTFOLIO.md`；
- `AF_XDP_INTERVIEW_NOTES.md`；
- `AF_XDP_RESUME_MATERIAL.md`；
- `AF_XDP_BACKLOG.md`。

原则：

```text
不把 summary 文档散放在 track-af-xdp 根目录；
summary 作为独立 project，自己维护 docs/reports/scripts/records。
```

