# TEST_RECORD_20260713_PHASE1

## 1. 环境

- 主机：`192.168.65.135`
- DPDK：`21.11.9`
- PMD：`net_pcap` RX + `net_null` TX
- EAL：`--no-pci --no-huge`，IOVA VA
- CPU：8 lcores，1 NUMA node

## 2. 完整命令

```bash
cd /home/wq7/workspace/driver-lab/linux-driver-lab/track-dpdk-advanced/project-dpdk-flow-pipeline
chmod +x scripts/*.sh tests/*.sh tools/*.py
bash -n scripts/*.sh tests/*.sh
make clean
make
make test
```

## 3. 关键证据

```text
FLOW_HASH_CREATE_PASS
FLOW_RULE_LOAD_PASS count=3
FLOW_PORT_CAPABILITY max_rx_queues=1 max_tx_queues=0 reta_size=0 rss_offloads=0x0
RSS_MULTI_QUEUE_BOUNDARY_BLOCKED
RTE_FLOW_BOUNDARY_BLOCKED ret=-38 type=1 message=Function not implemented
FLOW_RESULT hash_hits=48 hash_misses=16 rule_drop=16 forward=16 mark=16 default_drop=16 invalid=0
FLOW_LATENCY samples=64 p50_cycles=75 p99_cycles=250 max_cycles=1675 p99_ns=100
FLOW_RULE[0] action=DROP mark=0 packets=16 bytes=896
FLOW_RULE[1] action=MARK mark=42 packets=16 bytes=896
FLOW_RULE[2] action=FORWARD mark=0 packets=16 bytes=944
APP_RESULT rx=64 tx=32 tx_failed=0 freed=32
DPDK_FLOW_PIPELINE_PHASE1_PASS
cleanup=complete result=pass
PASS: DPDK rte_hash flow actions and latency smoke
script_summary name=flow_pipeline_smoke status=pass
```

## 4. 结论

`rte_hash` 精确匹配、三类规则动作、miss 默认动作、per-rule stats、mbuf 所有权和 latency 采集链路 PASS。p99 数字仅是本次 64 包 pcap software decision 样本，不代表真实 NIC 性能。

RSS/multi-queue 和 `rte_flow` 已形成可执行 capability evidence；当前 PMD 不支持，因此状态是明确 boundary，而不是伪造 PASS。
