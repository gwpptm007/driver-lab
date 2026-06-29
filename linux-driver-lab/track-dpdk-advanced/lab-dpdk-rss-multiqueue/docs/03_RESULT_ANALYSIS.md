# 测试结果分析

## 记录目录

```text
records/20260629-211820-rss-multiqueue
```

## Summary

```text
PASS_BUILD             PASS
QUEUE_CONFIG           BLOCKED_QUEUE_CONFIG
RSS_QUERY              BLOCKED_RSS
QUEUE_TO_CORE_DOC      PASS_QUEUE_TO_CORE_DOC
```

## 实测 capability

```text
driver_name=net_pcap
requested_rx_queues=2
max_rx_queues=1
max_tx_queues=1
reta_size=0
hash_key_size=0
rss_offloads_hex=0x0
```

## 实测 queue map

```text
queue_map rxq=0 lcore=1
queue_map rxq=1 lcore=2
```

## 实测 blocked reason

```text
blocked_reason=no_rss_offloads_or_reta
blocked_reason=max_rx_queues_lt_requested requested=2 max_rx_queues=1
```

## 结果解释模板

### PASS_QUEUE_CONFIG

出现条件：

```text
requested_rx_queues <= max_rx_queues
rte_eth_dev_configure() 成功
每个 RX queue setup 成功
```

### BLOCKED_QUEUE_CONFIG

常见原因：

```text
max_rx_queues_lt_requested
rte_eth_dev_configure_failed
rx_queue_setup_failed
```

如果 pcap PMD 只提供：

```text
max_rx_queues=1
```

而本实验请求：

```text
requested_rx_queues=2
```

则结论应为：

```text
BLOCKED_QUEUE_CONFIG
```

### PASS_RSS_QUERY

出现条件：

```text
flow_type_rss_offloads != 0
or reta_size != 0
```

### BLOCKED_RSS

出现条件：

```text
rss_offloads=0x0
reta_size=0
```

这说明当前 PMD 不暴露 RSS capability。

## 结论写法

本次 pcap PMD 得到 BLOCKED 结果，推荐写法：

```text
Phase 2 在 pcap PMD 下完成 capability probe。
当前 PMD 可用于稳定 packet input，但不提供真实 RSS/multiqueue 能力。
因此 RSS/multiqueue 在 pcap PMD 路径下记录为 BLOCKED evidence。
后续如需 PASS_QUEUE_CONFIG，需要切换到支持多队列/RSS 的真实 PMD。
```
