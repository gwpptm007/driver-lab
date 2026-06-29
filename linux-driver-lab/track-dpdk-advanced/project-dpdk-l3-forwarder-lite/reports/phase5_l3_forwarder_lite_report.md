# Phase 5 Report: DPDK L3 Forwarder Lite

## 目标

把 DPDK fastpath 从 L2/UDP 示例推进到一个更像真实网关的数据面骨架：

```text
RX -> parse -> ACL -> route lookup -> TX -> per-rule stats
```

## 记录

```text
records/20260629-213104-l3-forwarder/
```

## 验收

```text
PASS_BUILD
PASS_ROUTE_CONFIG
PASS_L3_FORWARD
PASS_ACL_DROP
PASS_PER_RULE_STATS
PASS_PCAP_EVIDENCE
```

## 流量构造

脚本生成 48 个 IPv4/UDP 包：

- 24 个命中 `10.20.0.0/24 -> port1`，应转发。
- 12 个命中 UDP dst port `9999`，应 ACL drop。
- 12 个目的地址为 `10.99.0.77`，应 route miss drop。

## 运行结果

```text
rx_packets=48
forwarded_packets=24
acl_drops=12
route_miss_drops=12
tx_failed=0
ROUTE_STATS[0] hits=24
ACL_STATS[0] drops=12
```

## 结论

在 pcap PMD / net_null PMD 组合下，L3 parse、ACL drop、route lookup、TX burst 和 per-rule stats 已经形成可复现闭环。

这个阶段仍然诚实保留边界：

- 没有宣称真实 NIC 线速。
- 没有宣称完整 DPDK ACL/LPM library 覆盖。
- 多 queue / RSS 真实硬件能力仍以 Phase 2 的 boundary evidence 为准。
