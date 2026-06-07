# Backlog and Retest Plan

> 2026-06-07 更新：Backlog 1-4 全部完成，所有 Phase 已通过复测。

## Backlog 1: lab-xdp-redirect-basics 补测 — DONE

目标达成：从 `PASS_BASIC_ATTACH` 升级到 `PASS_BASIC=YES, PASS_ACTION=YES, REDIRECT_MODEL_READY=YES`。

- XDP_PASS: 12 pkts, 628 bytes
- XDP_DROP: 3 pkts, 126 bytes
- XDP_REDIRECT: 3 pkts, 126 bytes
- 使用 veth pair 拓扑注入流量

详见 `lab-xdp-redirect-basics/docs/04_RETEST_20260607.md`

## Backlog 2: lab-af-xdp-socket-rings 测试 — DONE

目标达成：UMEM、AF_XDP socket、FILL/RX/TX/COMPLETION rings 已验证。

- UMEM_READY: frames=4096, frame_size=2048, bytes=8388608
- XSK_SOCKET_READY, FILL_RING_READY, XSKMAP_REGISTERED
- rx_packets=49 (首轮 6663 bytes), rx_packets=3 (脚本轮)

详见 `lab-af-xdp-socket-rings/docs/04_RETEST_20260607.md`

## Backlog 3: zero-copy 探测 — DONE

目标达成：明确 veth/vmxnet3 环境不支持 zero-copy。

- skb+copy baseline: PROBE_RC=0, rx_packets=3
- native+copy probe: PROBE_RC=0, rx_packets=3
- native+zero-copy probe: PROBE_RC=1, `xsk_socket__create: Operation not supported`

详见 `lab-af-xdp-zero-copy-vs-copy/docs/04_RETEST_20260607.md`

## Backlog 4: mini forwarder 项目验证 — DONE

目标达成：从 `READY_TO_TEST` 推进到全项 PASS。

- DROP: rx=3, dropped=3, fill_recycled=3
- REFLECT: rx=3, tx=3, comp=3 — 首次验证 TX 和 COMPLETION ring

详见 `project-af-xdp-mini-forwarder/docs/04_RETEST_20260607.md`

## 新 Backlog

### Backlog 5: project-af-xdp-traffic-test

给 mini forwarder 补完整的 veth/namespace 测试闭环：
- 多流量模式 (UDP flood, 不同包大小)
- ring occupancy 监控
- busy poll / need_wakeup 对比
- pps 基线

### Backlog 6: DPDK vs AF_XDP 对比文档

- PMD vs kernel driver
- mbuf vs UMEM frame
- rx_burst/tx_burst vs rings
- hugepage vs mmap UMEM
- vhost/virtio-user vs XSKMAP redirect
