# 01_OVERVIEW - mbuf / mempool deep dive

## 实验问题

这个 lab 聚焦 DPDK packet memory model：

```text
mbuf 里保存了哪些 packet metadata？
mempool cache 如何影响 packet buffer 分配？
rx_burst 收到的 packet 如何进入 mbuf inspect 路径？
软件 stats 和 ethdev stats 如何对齐？
```

## 实验路径

```text
udp_input.pcap
  -> net_pcap PMD
  -> rte_eth_rx_burst()
  -> struct rte_mbuf
  -> 打印 metadata
  -> rte_pktmbuf_free()
```

## 观察字段

```text
mbuf->buf_addr
mbuf->buf_iova
mbuf->data_off
mbuf->data_len
mbuf->pkt_len
mbuf->nb_segs
mbuf->port
mbuf->ol_flags
mbuf->packet_type
mbuf->rss_hash
```

## 验收项

```text
PASS_BUILD
PASS_PCAP_RX
PASS_MBUF_METADATA
PASS_MEMPOOL_CONFIG
PASS_STATS_CONSISTENCY
```

## 文档入口

- `02_TEST_AND_VERIFY.md`：逐步测试命令与执行记录。
- `03_RESULT_ANALYSIS.md`：测试结果分析。
- `04_DEEP_LEARNING.md`：mbuf/mempool 原理、Mermaid 图和生命周期拆解。

## 当前边界

- 不做真实 NIC 性能调优。
- 不做 RSS 多队列。
- 不做 VFIO/IOMMU 环境改造。
- 只聚焦 mbuf/mempool 与 packet metadata。

