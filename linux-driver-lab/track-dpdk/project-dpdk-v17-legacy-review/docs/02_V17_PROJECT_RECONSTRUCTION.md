# 02_V17_PROJECT_RECONSTRUCTION

## 旧 DPDK v17 媒体面项目的合理重构描述

根据已有经历，可以把旧项目描述为：

```text
基于 DPDK v17 的用户态媒体面转发模块，用于虚拟化/网元场景下的 UDP 报文高速收发、头部改写和转发。
```

核心特点：

```text
1. 用户态轮询收包，绕过内核协议栈常规路径
2. 使用 hugepage + mempool + mbuf 管理数据包内存
3. 使用 PMD 驱动从网卡队列批量收包/发包
4. 关注 UDP 媒体流，不处理完整 TCP/IP 协议栈
5. 根据网元方向进行 MAC / IP / UDP 端口改写
6. 必要时通过 KNI 或内核路径处理控制类/异常类报文
```

## 数据路径抽象

可以用下面这条路径讲：

```text
NIC RX queue
  -> PMD rx_burst
  -> rte_mbuf batch
  -> Ethernet parse
  -> ARP / IPv4 / UDP classify
  -> rule match
  -> header rewrite
  -> checksum update
  -> PMD tx_burst
  -> NIC TX queue
```

异常路径：

```text
non-UDP / unsupported packet
  -> drop / stats

control packet / kernel required packet
  -> KNI / tap / kernel assist path
```

## 旧项目中常见模块

| 模块 | 作用 | 当前 track 对应 |
|---|---|---|
| EAL 初始化 | 初始化 DPDK runtime | 所有 lab 的 `rte_eal_init` |
| hugepage | 大页内存，减少 TLB miss | `lab-vmxnet3-testpmd` hugepage 记录 |
| mempool | mbuf 对象池 | `l2fwd-lite` / `fastpath-lite` / `media-gateway-lite` |
| rx_burst/tx_burst | 批量收发包 | `project-user-space-fastpath` 和 media gateway |
| ARP/IP/UDP 解析 | 数据面分类 | `gateway_packet.c` |
| rewrite | 修改二三四层头 | `gateway_rule.c` / `gateway_packet.c` |
| stats | 统计与定位 | `gateway_stats.c` |
| KNI | 旧式回内核路径 | 本项目对照文档 |

## 面试时不要这么讲

不要只说：

```text
我用过 DPDK，收包转发，性能很好。
```

应该讲成：

```text
我当时做的是用户态媒体面转发路径，核心是用 DPDK PMD 从网卡队列批量收 UDP 媒体流，通过 mbuf/mempool 管理包内存，在用户态完成以太网、IPv4、UDP 头解析和网元方向相关的 MAC/IP/UDP 改写，再批量 tx_burst 发出。非媒体流量和控制类流量通过异常路径处理，旧版本里会用 KNI 或内核辅助路径承接。
```
