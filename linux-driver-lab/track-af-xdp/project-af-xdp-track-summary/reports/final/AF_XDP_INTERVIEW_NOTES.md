# AF_XDP Interview Notes

## AF_XDP 是什么

AF_XDP 是 Linux 提供的一种高性能用户态 packet socket。它通常和 XDP 配合使用：XDP 程序通过 `bpf_redirect_map()` 把包 redirect 到 XSKMAP 中注册的 AF_XDP socket，用户态通过 UMEM 和 ring 描述符收发包。

## AF_XDP 和 DPDK 的区别

| 维度 | DPDK | AF_XDP |
|---|---|---|
| 驱动模型 | PMD 接管设备 | 复用 Linux 驱动/XDP |
| 内存模型 | hugepage / mbuf | UMEM / frame |
| 收发模型 | RX/TX burst | RX/TX rings |
| 生态关系 | 更偏独立用户态数据面 | 更贴近 Linux 内核网络生态 |
| zero-copy | 依赖 PMD/网卡 | 依赖 AF_XDP 驱动支持 |

## UMEM 和 rings 怎么讲

```text
UMEM 是 AF_XDP 用来承载 packet frame 的共享内存区域。
FILL ring 把空闲 frame 交给内核接收。
RX ring 让用户态拿到已收包 frame。
TX ring 让用户态提交待发送 frame。
COMPLETION ring 通知用户态哪些 TX frame 可以回收。
```

## copy / zero-copy 怎么讲

copy mode 兼容性更强，不要求驱动支持 zero-copy；zero-copy 性能更好，但必须驱动实现对应 AF_XDP zero-copy 能力。测试环境如果是 VMware `vmxnet3` 或 veth，zero-copy 探测失败是合理结果，应记录为环境不支持，而不是功能失败。

已实测：skb+copy 基线 3 pkts，native+copy 在 veth 上 3 pkts，native+zero-copy `xsk_socket__create: Operation not supported`（veth 无 DMA，预期失败）。

## 当前项目怎么讲（2026-06-07 更新）

```text
我做了 AF_XDP 的阶段化实验：先验证 XDP attach 和三种 action 模型（PASS/DROP/REDIRECT 均有非零流量），
再实现 AF_XDP socket/rings（UMEM 8MB, 4096 frames, 首次 rx_packets=49），
随后做 copy/zero-copy 探测（skb+copy 和 native+copy 均通过，zero-copy 在 veth 上不支持是预期结果），
最后组合成 mini forwarder（drop 模式 rx=3/drop=3, reflect 模式首次达成 tx=3/comp=3）。
这个项目让我把 XDP、XSKMAP、UMEM、rings 和用户态 poll loop 串起来理解，
并验证了 FILL → RX → TX → COMPLETION → FILL 的完整 frame 生命周期闭环。
```

## 关键数据点（面试时可以用）

| 指标 | 数据 |
|---|---|
| UMEM | 8MB, 4096 frames, 2048 bytes/frame |
| Rings | FILL=2048, RX=2048, TX=2048, COMP=2048 |
| Phase 1 流量 | PASS: 12 pkts, DROP: 3 pkts, REDIRECT: 3 pkts |
| Phase 2 流量 | rx_packets=49 (首轮, 6663 bytes) |
| Phase 3 模式 | skb+copy: PASS, native+copy: PASS, zero-copy: UNSUPPORTED |
| Phase 4 反射 | rx=3, tx=3, comp=3 (首次 TX/COMP 闭环) |
| 测试拓扑 | veth pair (kernel 5.12+ 支持 native XDP) |
