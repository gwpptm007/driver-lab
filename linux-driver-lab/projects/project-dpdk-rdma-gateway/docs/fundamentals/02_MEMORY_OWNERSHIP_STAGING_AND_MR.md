# 02：内存所有权、staging 与 MR

## 内存问题比 API 调用更早出现

DPDK 和 verbs 都使用用户态内存，但“用户态地址”不是足够的共享契约。一个缓冲区能否被某个设备或线程访问，取决于其所有权、连续性/分段、DMA 映射、注册状态、生命周期、NUMA 和错误回收路径。

## ingress mbuf 的正确处理

`rte_eth_rx_burst()` 返回的 mbuf 属于当前 RX path。它可能是多段链表；协议头和 payload 不应被假设为单一连续虚拟地址。当前 parser 使用 `rte_pktmbuf_read()` 安全读取，只有完整、长度合法的 UDP payload 才会占用 slot。

处理成功或失败后，RX loop 都应释放原 mbuf。Phase 2/4 选择 copy 的直接收益是：mbuf 的释放不再等待 RDMA completion；DPDK mempool 压力与 RDMA worker 的延迟隔离。

## staging slot 是 handoff 的唯一 payload 载体

每个 `gateway_staging_slot` 有 2048-byte、cache-line 对齐的 payload 区，对应一个独立 `gateway_slot_meta`。metadata 中的 `generation`、`payload_len` 和原子 `phase` 解释该 slot 是否可写、已发布还是正在被 RDMA 使用。

| 状态 | 能写 payload | 能发布 descriptor | 能给 RDMA SGE 使用 | 能被 producer 复用 |
| --- | --- | --- | --- | --- |
| FREE | 是 | 否 | 否 | 是 |
| READY | 否 | 已经发布 | 否 | 否 |
| INFLIGHT | 否 | 否 | 是 | 否 |

`gateway_slot_cancel_ready()` 仅用于“已经 copy 但 ring enqueue 失败”的未发布路径；它不能取消已经被 worker dequeue 的 INFLIGHT slot。

## send MR 与 remote MR 不是同一种能力

| MR | 所在主机 | 谁持有 key | 用途 | 释放前置条件 |
| --- | --- | --- | --- | --- |
| local/send MR | client | local lkey | 让本地 QP 读取待发送 wire record | 所有引用它的 WR 都已完成 |
| remote MR | server | remote rkey + address 传给 client | 允许 client 写入指定远端范围 | 不再允许任何 peer 使用 rkey/address |

RDMA WRITE 的本地 SGE 指向已注册的本地 send buffer；远端 address/rkey 是 server 授予的权限能力。把本地 slot 指针、remote address 和 `rte_mbuf` data 都称为“buffer 地址”会抹去关键安全边界。

## 为什么当前不会让 staging slot 直接成为 SGE

理论上可以把 staging region 注册成 MR，并让 worker 的 SGE 直接引用该 slot，从而少一次 encode/copy。当前没有这么做，因为 wire record = 40-byte header + payload，而 slot 只保存 payload；同时项目优先验证协议和回收边界。

未来 zero-copy/registered-staging 设计至少要新增：

1. staging region 注册及 lkey 生命周期；
2. header 与 payload 的 scatter-gather layout；
3. 多段 SGE、对齐、最大 SGE 和 device capability；
4. 直到 CQE 前 slot/MR 都不可回收的证明；
5. failure/flush 时未完成 WR 的统一 cleanup；
6. PMD 与 RNIC DMA/IOMMU/NUMA 的实际兼容证据。

因此“先 copy、后注册优化”是当前可解释实现，不是永久性能结论。

## 缓存与 false sharing

slot payload 对齐到 cache line 只能减少一部分共享问题。真正要避免的是 producer/consumer 高频同时写同一 metadata cache line、或者多个线程无约束扫描同一 slot pool。当前 SPSC 模型让 producer 负责准备、worker 负责 inflight/completion；扩为多线程时必须重新设计 slot 分片或所有权，而不能只把原子类型换成更强的 memory order。

## 可验证问题

- 是否所有 64 个 RX mbuf 都被释放？
- ring 满时 READY slot 是否被取消而非泄漏？
- 成功 CQE 前 slot 是否保持 INFLIGHT？
- remote write 长度是否始终小于 remote MR 边界？
- shutdown 时是否先 drain，再销毁 MR/QP/staging？

这些问题比“是否调用了 `ibv_post_send()`”更直接决定网关是否安全。
