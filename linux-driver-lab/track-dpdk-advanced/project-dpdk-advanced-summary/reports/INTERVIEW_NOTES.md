# Interview Notes

## 一分钟版本

我做了一个 DPDK Advanced track，不只是跑 testpmd，而是把 DPDK 数据面的关键工程点拆成了实验：

- mbuf/mempool metadata 怎么看。
- RSS 和多队列为什么依赖 PMD/硬件能力。
- burst size、mempool cache、NUMA 变量怎么做对比。
- VFIO/IOMMU 和 UIO 的部署边界是什么。
- 最后写了一个 pcap PMD 到 net_null PMD 的 L3 forwarder lite，包含 route、ACL drop 和 per-rule stats。

## 深挖问题

### 为什么 Phase 2 是 BLOCKED？

因为当前 pcap PMD capability 显示 `max_rx_queues=1`、`reta_size=0`、`rss_offloads=0x0`。这证明环境不支持真实 RSS 多队列验证。我的处理方式是保留 queue-to-core 模型和 blocking evidence，而不是伪造多队列结果。

### 为什么 Phase 4 不直接切 VFIO？

当前 kernel cmdline 没有 IOMMU 参数，`/sys/kernel/iommu_groups` 为空。并且测试机依赖 SSH 管理网卡。贸然 bind/unbind 有断连风险，也不能证明 VFIO 的核心隔离能力。

### L3 forwarder lite 证明了什么？

证明了软件数据面逻辑闭环：

```text
pcap RX -> parse -> ACL -> route -> TX -> stats
```

它不证明真实 NIC 线速，也不等价于完整生产网关。

