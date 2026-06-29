# Result Analysis

正式记录：

```text
records/20260629-213104-l3-forwarder/
```

判定标准：

```text
PASS_BUILD: app 构建成功
PASS_ROUTE_CONFIG: route 已打印且 route hits > 0
PASS_L3_FORWARD: forwarded_packets > 0 且 tx_failed == 0
PASS_ACL_DROP: ACL drops > 0 且 per-rule drops 对齐总数
PASS_PER_RULE_STATS: route hits == forwarded_packets
PASS_PCAP_EVIDENCE: pcap 文件存在且 RX 包数 > 0
```

这个结果的含义是：

```text
在 pcap PMD / net_null PMD 的可复现环境中，
L3 parse、ACL drop、route lookup、TX burst 和 per-rule stats 已经形成闭环。
```

它不代表：

```text
真实 NIC 线速 L3 forwarding 已完成。
完整 DPDK ACL/LPM library 已覆盖。
多 queue RSS 生产部署已验证。
```

## 本次结果

```text
rx_packets=48
forwarded_packets=24
acl_drops=12
route_miss_drops=12
tx_failed=0
ROUTE_STATS[0].hits=24
ACL_STATS[0].drops=12
```

这说明：

- 目的地址 `10.20.0.77` 且 UDP 目的端口 `9000` 的包被转发到 `net_null1`。
- UDP 目的端口 `9999` 的包先命中 ACL，被 drop。
- 目的地址 `10.99.0.77` 的包没有命中 route table，被 route miss drop。
- route hits 与 forwarded packets 对齐，ACL per-rule drops 与总 ACL drops 对齐。
