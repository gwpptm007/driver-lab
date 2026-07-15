# 00：15 分钟建立 DPDK-RDMA Gateway 心智模型

## 这不是把两个 demo 串起来

项目要证明的是：一个 DPDK RX 收到的 UDP payload，如何在不泄漏 mbuf 生命周期、不直接依赖 PMD 内存注册、并且可安全回收的前提下，通过 RDMA RC WRITE 写入远端注册内存。

```text
pcap PMD / future NIC
  -> DPDK RX + parser
  -> copy to bounded staging slot
  -> 32-byte local request descriptor
  -> SPSC request ring
  -> RDMA worker
  -> encode 40-byte wire header + payload
  -> registered send MR
  -> RC RDMA WRITE
  -> remote MR
  -> CQE validates generation and frees slot
```

它的关键不是“DPDK 很快、RDMA 很快”，而是每个 buffer 在任一时刻都有唯一、可解释的所有者。

## 三种内存，三条不同契约

| 内存 | 当前所有者 | 生命周期 | 为什么不能混用 |
| --- | --- | --- | --- |
| `rte_mbuf` data | PMD/mempool 与 RX loop | 收到后尽快释放 | 可分段、未必注册为 verbs MR、PMD 规则控制释放 |
| staging slot | gateway slot pool | `FREE/READY/INFLIGHT` | 是跨线程 handoff 的稳定 payload 副本 |
| RDMA send MR | RDMA worker/backend | post WR 到对应 CQE | 被 WR 引用时必须保持注册和有效 |
| remote MR | RDMA server | remote rkey/addr 有效期间 | 对端授予的能力，不是本地指针 |

一次有界 copy 是当前设计换来的边界清晰：DPDK 可以立刻释放 mbuf，RDMA worker 不必认识 PMD/mempool；verbs 也不必注册整个 DPDK 内存池。

## 本地 descriptor 不等于远端协议

`struct gateway_request` 是本地 producer/consumer 之间固定 32 字节的工作描述：带 `slot_id`、`generation`、ingress port、queue 与 payload length。它可以采用主机字节序，因为不跨机器。

wire header 固定 40 字节，以大端逐字段编码，带 magic/version/header length/reserved/request id/flow hash/payload length/generation。远端不能看见本地 `slot_id`，也不应依赖本机 C ABI。

## 正常状态转移

```text
FREE
  producer reserves + copies payload
READY
  consumer dequeues + posts WRITE
INFLIGHT
  matching CQE(slot_id, generation)
FREE
```

slot 被复用时 generation 增加。若 generation 1 的 CQE 迟到，而 slot 5 已作为 generation 2 在使用，旧 CQE 必须得到 `-ESTALE`；否则它会把正在使用的 slot 误回收。

## 64 包 Phase 4 的守恒关系

确定性 pcap 每四包有三个 UDP、一个 ICMP。因此正确的功能回归满足：

```text
rx = 64
udp = staged = dequeued = completed = 48
unsupported = 16
payload_bytes = 48 * 32
write_bytes = 48 * (40 + 32)
active_slots after worker drain = 0
```

守恒不等于高性能，但它能快速发现 parse、slot、ring、worker 或 CQE 回收阶段的丢失/重复。

## 三个不能越过的结论边界

1. pcap PMD 证明 parser 和 ingress 所有权，不证明真实 NIC RSS、DMA、突发吞吐。
2. RXE/Soft-RoCE 证明 verbs/QP/CQE 工程语义，不证明 RNIC PCIe、doorbell 或线速性能。
3. client CQE 证明该 signaled WR 完成；它不自动证明远端业务已经持久化或应用线程已经消费记录。

后续章节分别把这些边界拆开，不让“功能已通”掩盖资源和性能问题。
