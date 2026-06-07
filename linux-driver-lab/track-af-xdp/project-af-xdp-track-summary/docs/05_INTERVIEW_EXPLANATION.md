# Interview Explanation

## 30 秒说明

我在驱动和 DPDK 之后，继续做了 AF_XDP track。这个方向主要验证 Linux 原生的用户态数据面能力：先用 XDP 程序在驱动收包早期做 PASS/DROP/REDIRECT，再通过 XSKMAP 把流量导入 AF_XDP socket，用户态负责 UMEM、FILL/RX/TX/COMPLETION rings 的管理。四个 Phase 全部通过 veth pair 拓扑完成复测（2026-06-07），验证了从 XDP attach 到 mini forwarder reflect 的完整数据闭环。

## 重点术语

```text
XDP: 驱动收包早期的 BPF hook
XSKMAP: XDP redirect 到 AF_XDP socket 的 BPF map
UMEM: AF_XDP 用户态/内核共享 packet frame 内存
FILL/RX/TX/COMPLETION rings: AF_XDP 收发和回收描述符环
copy mode: 兼容性强，性能较弱
zero-copy mode: 性能更好，但依赖驱动支持
```

## 面试中怎么讲

可以说：

```text
目前已经完成 AF_XDP track 的四个 Phase 全部复测。Phase 1 验证了 XDP 三种 action 模型（PASS 12 pkts,
DROP 3 pkts, REDIRECT 3 pkts）。Phase 2 实现了 AF_XDP socket 完整收包（UMEM 8MB, 4096 frames,
首次 rx_packets=49）。Phase 3 区分了 skb+copy、native+copy、zero-copy 三种模式的驱动支持边界。
Phase 4 的 mini forwarder 在 reflect 模式下首次验证了 TX ring 和 COMPLETION ring 闭环（tx=3, comp=3），
完成了 FILL → RX → TX → COMPLETION → FILL 的完整 frame 生命周期。

关键发现：同主机发包到本地 IP 走 local delivery 短路，XDP hook 不会触发。我用 veth pair 解决了这个问题，
veth peer 注入的流量必定经过 veth-xdp 的 XDP hook。另外 veth kernel 5.12+ 支持 native XDP，
但 zero-copy 需要 NIC DMA，veth 和 vmxnet3 都不支持，这是合理的设计边界。

它和 DPDK 的区别是 AF_XDP 仍然依赖 Linux 驱动和 XDP 生态，适合做和内核网络路径结合更紧的用户态 fastpath。
```

不要说：

```text
已经完成高性能 AF_XDP 转发器并压测达到线速。
```

## 可强调的技术点

- 能解释 local delivery shortcut 为什么不触发 XDP hook，以及 veth pair 如何解决
- 知道 veth kernel 5.12+ 支持 native XDP，但不支持 zero-copy
- 能区分 XDP attach mode (skb/native) 和 AF_XDP bind mode (copy/zero-copy)
- 能画出 FILL → RX → TX → COMPLETION → FILL 的 frame 流转图
- 实际跑通了完整的 AF_XDP 数据路径，有 records 可查
