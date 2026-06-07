# AF_XDP Path

## 路径定位

AF_XDP path 是 Linux 原生用户态 fastpath。它不像 DPDK 那样完全依赖 PMD 生态，而是从 XDP hook 早期收包，通过 XSKMAP redirect 把包导入 AF_XDP socket 和用户态 UMEM。

这条路径要回答：

```text
如何从驱动早期 XDP hook 把包 redirect 到用户态？
AF_XDP socket、UMEM、FILL/RX/TX/COMPLETION rings 如何共同管理 frame 生命周期？
copy mode 和 zero-copy mode 的边界在哪里？
```

## 覆盖范围

对应主线：

```text
linux-driver-lab/track-af-xdp/
```

主要阶段：

| Phase | 目标 | 状态 |
|-------|------|------|
| Phase 1 | XDP redirect basics | PASS_BASIC/ACTION/REDIRECT |
| Phase 2 | AF_XDP socket / UMEM / rings | PASS_SOCKET/UMEM/RX_TRAFFIC |
| Phase 3 | copy / zero-copy mode probe | PASS_COPY/NATIVE/ZC_PROBED |
| Phase 4 | mini forwarder | PASS_DROP/REFLECT/TRAFFIC/TX |
| Phase 5 | track summary | UPDATED |

## 关键机制

AF_XDP path 关注：

- XDP attach/detach。
- XDP_PASS / XDP_DROP / XDP_REDIRECT。
- XSKMAP。
- AF_XDP socket。
- UMEM frame 分配与复用。
- FILL ring。
- RX ring。
- TX ring。
- COMPLETION ring。
- copy / native / zero-copy mode 探测。
- mini forwarder 的 drop 和 reflect 模式。

## 已证明内容

已完成验证：

```text
BPF/XDP 程序可编译、attach、detach
XDP_PASS / XDP_DROP / XDP_REDIRECT 三种 action 验证
XDP redirect -> XSKMAP -> AF_XDP socket -> UMEM RX ring -> 用户态 poll 路径验证
FILL -> RX -> TX -> COMPLETION -> FILL frame 生命周期闭环
skb+copy 和 native+copy 可工作
zero-copy 不支持的环境边界已探测
```

关键突破：

```text
使用 veth pair 避免 local delivery 短路，确保 XDP hook 被触发。
Phase 4 reflect 首次同时获得 TX 和 COMPLETION 非零指标。
```

## 和 DPDK 的对比

| 维度 | DPDK | AF_XDP |
|------|------|--------|
| 入口 | PMD 接管设备 | XDP hook + XSKMAP |
| 内存 | DPDK hugepage mbuf | UMEM frame |
| 队列 | ethdev RX/TX queue | FILL/RX/TX/COMPLETION rings |
| 生态 | DPDK PMD | Linux kernel native |
| zero-copy | PMD/NIC 相关 | NIC driver 支持相关 |
| 优势 | 高度可控、成熟用户态数据面 | 保留 Linux 原生路径和 XDP 生态 |

## Evidence 入口

主要证据索引：

- `../../track-af-xdp/project-af-xdp-track-summary/reports/final/AF_XDP_TRACK_REPORT.md`
- `../../track-af-xdp/project-af-xdp-track-summary/reports/final/AF_XDP_RESUME_MATERIAL.md`
- `../../track-af-xdp/project-af-xdp-track-summary/records/STATUS_SNAPSHOT_LATEST.md`
- [../evidence/af_xdp_evidence.md](../evidence/af_xdp_evidence.md)

## 当前边界

准确表述：

- 已完成 XDP redirect 到 AF_XDP mini forwarder 的阶段化验证。
- 已明确当前环境下 zero-copy unsupported 是合理边界。

不要夸大：

- 没有完成真实 NIC zero-copy 高性能压测。
- mini forwarder 不是生产级转发器。
