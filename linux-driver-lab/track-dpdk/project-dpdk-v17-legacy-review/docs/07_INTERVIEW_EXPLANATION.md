# 07_INTERVIEW_EXPLANATION

## 1. 怎么介绍你的 DPDK 项目

推荐回答：

```text
我做过 DPDK 用户态数据面相关项目，旧项目基于 DPDK v17，主要处理虚拟化/网元场景下的 UDP 媒体流。数据路径是 PMD 从网卡队列批量收包，用户态解析 Ethernet/IPv4/UDP，根据网元方向做 MAC、IP、UDP 端口改写，再批量发送。当前我又用现代 DPDK 环境重新做了一条可复现实验链，从 vmxnet3/testpmd、vhost-user、virtio-user，到自写 l2fwd-lite、fastpath-lite 和 media-gateway-lite，用 records 验证每一步。
```

## 2. DPDK 为什么快

```text
DPDK 的核心不是一个点，而是一组机制：hugepage 降低内存管理和 TLB 压力，mempool/mbuf 避免频繁动态分配，PMD 轮询减少中断开销，rx_burst/tx_burst 批处理降低 per-packet 成本，同时把协议处理放在用户态专用 fastpath 中，避免完整内核协议栈路径。
```

## 3. PMD / mbuf / mempool 怎么解释

```text
PMD 是 poll mode driver，负责用户态轮询网卡队列；mbuf 是 DPDK 表示 packet buffer 的核心结构；mempool 是预分配的 mbuf 对象池。收包时 PMD 把报文放进 mbuf，应用批量拿到 mbuf 数组处理，发包时再把 mbuf 交给 PMD tx_burst。
```

## 4. KNI 怎么解释

```text
KNI 在旧 DPDK 项目里常用于把一部分报文从用户态 DPDK 路径送回 Linux 内核协议栈，比如管理流量、控制流量或需要内核网络功能的流量。但现代设计里我不会默认把 KNI 当首选，而会结合部署环境评估 tap、vhost-user、virtio-user、AF_XDP 或独立控制面通道。
```

## 5. 当前 track 和旧项目怎么对应

```text
旧项目经验给了我真实业务背景：UDP 媒体面、网元方向、头部改写、用户态高性能转发。当前 track 则补齐了可复现证据：从 DPDK 环境准备、PMD 接管、vhost/virtio 虚拟链路，到自写 DPDK C 程序和 media-gateway-lite 模块化实现。这样不是只说我做过，而是能把每个底层机制复现、解释和验证。
```

## 6. 当前不足怎么坦诚讲

```text
当前 media-gateway-lite 已完成项目骨架和 pcap + null PMD 下的 UDP、forwarding、rewrite records，对应 `PASS_PCAP_FUNCTIONAL/FORWARDING/REWRITE`。我不会把 vdev replay 包装成外部 wire、真实 NIC 或线速性能；这些证据按独立等级管理。
```

## 7. 面试亮点

```text
1. 有旧项目经验，不是纯学习 demo
2. 能解释 DPDK 数据面核心机制
3. 能把 v17 旧项目迁移到现代 DPDK 工程方式
4. 有从 PMD、vhost/virtio 到自写 fastpath 的完整学习链
5. 对测试证据有分级，不虚标结果
```
