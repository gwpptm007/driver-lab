# 01_OVERVIEW - RSS / Multi-Queue capability probe

## 实验问题

这个 lab 用来回答：

```text
当前 PMD 支持多少 RX/TX queue？
RSS capability 是否可查询？
RETA / hash key / rss_hf 是否存在？
请求 2 个 RX queue 时，PMD 是 PASS 还是 BLOCKED？
queue-to-core mapping 应该如何描述？
```

## 实验路径

```text
pcap input
  -> net_pcap PMD
  -> rte_eth_dev_info_get()
  -> 检查 max_rx_queues / reta_size / rss_offloads
  -> 输出 PASS 或 BLOCKED evidence
```

## 验收项

```text
PASS_BUILD
QUEUE_CONFIG
RSS_QUERY
QUEUE_TO_CORE_DOC
```

## 文档入口

- `02_TEST_AND_VERIFY.md`：逐步测试命令与执行记录。
- `03_RESULT_ANALYSIS.md`：测试结果分析。
- `04_DEEP_LEARNING.md`：RSS/queue 原理、能力边界和 queue-to-core 模型。

## 当前边界

Phase 2 是 capability probe，不是生产级 RSS 调优。

如果 pcap PMD 或 VMware/vmxnet3 不支持目标能力，结果应该写成 `BLOCKED_*`，而不是伪造 PASS。

