# project-dpdk-l3-forwarder-lite

DPDK Advanced Phase 5：一个小而完整的 L3 forwarding / ACL / per-rule stats 项目。

## 目标

在当前 VMware 测试环境里继续使用可复现的 pcap PMD：

```text
pcap PMD input -> IPv4/UDP parse -> ACL drop or L3 forward -> net_null PMD output
```

这个项目不声称是真实 NIC 线速转发，重点是把 L3 数据面工程骨架讲清楚：

- route table / longest-prefix 思路。
- ACL drop rule。
- per-rule stats。
- pcap evidence。
- DPDK port RX/TX 生命周期。

## 快速执行

```bash
cd linux-driver-lab/track-dpdk-advanced/project-dpdk-l3-forwarder-lite
chmod +x scripts/*.sh tools/*.py
./scripts/00_check_env.sh
./scripts/01_build.sh
./scripts/02_run_pcap_l3_forward.sh
./scripts/03_collect_report.sh
```

## 验收项

```text
PASS_BUILD
PASS_ROUTE_CONFIG
PASS_L3_FORWARD
PASS_ACL_DROP
PASS_PER_RULE_STATS
PASS_PCAP_EVIDENCE
```

## 文档入口

- `docs/01_OVERVIEW.md`
- `docs/02_TEST_AND_VERIFY.md`
- `docs/03_RESULT_ANALYSIS.md`
- `docs/04_DEEP_LEARNING.md`

## 正式记录

```text
records/20260629-213104-l3-forwarder/
```

