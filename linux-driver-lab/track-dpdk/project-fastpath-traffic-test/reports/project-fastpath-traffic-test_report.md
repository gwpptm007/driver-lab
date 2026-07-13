# project-fastpath-traffic-test_report

## 目标

验证 `fastpath-lite` 的确定性 pcap PMD 功能流量处理能力。

## 当前状态

TESTED (2026-06-07, pcap PMD path).

## 实测结果

### Test 1: PASS_PCAP_FUNCTIONAL + PASS_PCAP_FORWARDING

```bash
./scripts/06_run_pcap_rx_test.sh
```

| 指标 | 值 |
|------|-----|
| port 0 rx | 111,709,760 |
| port 0 ipv4 | 111,709,760 |
| port 0 udp | 111,709,760 |
| port 1 tx | 111,709,760 |
| rte_eth_stats port0 opackets | 111,709,760 |

**verdict: PASS_SMOKE PASS_PCAP_FUNCTIONAL PASS_PCAP_FORWARDING**

### Test 2: + PASS_REWRITE

```bash
REWRITE_ENABLE=1 ./scripts/06_run_pcap_rx_test.sh
```

| 指标 | 值 |
|------|-----|
| port 0 rx | 77,210,432 |
| port 0 ipv4 | 77,210,432 |
| port 0 udp | 77,210,432 |
| port 0 rewrite | 77,210,432 |
| port 1 tx | 77,210,432 |
| rte_eth_stats port0 opackets | 77,210,432 |

**verdict: PASS_SMOKE PASS_PCAP_FUNCTIONAL PASS_PCAP_FORWARDING PASS_PCAP_REWRITE**

> 历史解析器 marker 为 `PASS_TRAFFIC/PASS_FORWARDING/PASS_REWRITE`。本报告按 pcap/vdev scope 重命名；数字来自 infinite replay，不能解释为外部流量或真实 NIC 性能。

## 测试拓扑

```text
gen_udp_pcap.py (500 UDP pcap)
    -> net_pcap0 (rx from pcap, infinite replay)
    -> fastpath-lite (classify + forward)
    -> net_null0 (TX accept + discard)
```

## 记录位置

- `records/20260607_155955-fastpath-pcap/` — traffic test
- `records/20260607_160006-fastpath-pcap-rewrite/` — rewrite test
