# DPDK_BACKLOG

> 用于记录 DPDK track 后续技术债。当前主线已经进入总结收口阶段，以下内容后续逐项回补。

## P0 - media-gateway-lite 真实流量闭环

### 1. 修 TX 成功后访问 mbuf 的潜在风险 — **FIXED (2026-06-07)**

问题：

```c
sent = rte_eth_tx_burst(..., m, 1);
if (sent == 1) {
    stats.port[r.out_port].tx++;
    stats.port[r.out_port].tx_bytes += rte_pktmbuf_pkt_len(m);  // BUG: mbuf 已归 PMD
}
```

`tx_burst` 成功后 mbuf 所有权交给 PMD，后续不应继续访问 `m`。

修法（已应用）：

```c
uint32_t pkt_len = rte_pktmbuf_pkt_len(m);  // 在 tx_burst 之前读取
sent = rte_eth_tx_burst(..., &m, 1);
if (sent == 1) {
    stats.port[r.out_port].tx++;
    stats.port[r.out_port].tx_bytes += pkt_len;
}
```

### 2. 修 stats parser 累计重复相加问题 — **FIXED (2026-06-07)**

现象：周期打印的 stats 本身是累计值，解析脚本不应把每个周期都加起来。

修法（已应用）：

```text
按每个 port / rule 取最后一次 sample
而不是所有 sample 求和
```

### 3. 新增真实 UDP 输入路径 — **DONE (2026-06-07)**

实现路径：**A. pcap PMD**（无需物理网卡，最稳最快的路径）

```text
gen_udp_pcap.py (Python stdlib) → udp_test.pcap → net_pcap rx → media-gateway-lite → net_null tx
```

新增工具：
- `tools/gen_udp_pcap.py`：纯 Python stdlib 生成含 UDP 报文的 pcap 文件
- `scripts/06_run_pcap_rx_test.sh`：一键运行 pcap PMD 测试

实测数据（10s run, 500 pcap packets infinite replay）：

```text
rx=161830784  rx_bytes=12460970368
ipv4=161830784
udp=161830784
```

验收结论：**rx>0, ipv4>0, udp>0 全部满足**。

---

## P1 - forwarding / rewrite 证据 — **DONE (2026-06-07)**

### PASS_FORWARDING — DONE

同一轮 pcap PMD 测试中，auto bidirectional rules 实现转发：

```text
port 0 rx=161830784  →  rule 0 hit=161830784  →  port 1 tx=161830784
rule_hit=161830784
tx=161830784
rte_eth_stats: port1 opackets=161830784
```

验收结论：**rule_hit>0, tx>0, ethdev stats 与软件 stats 一致**。

### PASS_REWRITE — DONE

使用显式 rewrite 规则测试（`--rule0-rewrite-dst-ip/--rule0-rewrite-dst-mac/--rule0-rewrite-dst-port`）：

```text
port 0 rewrite=161830784
rule 0 rewrite=161830784
```

规则配置：
```text
--rule0 0:1 --rule0-dst-port 9000
  --rule0-rewrite-dst-ip 10.10.20.20
  --rule0-rewrite-dst-mac 52:54:00:00:00:02
  --rule0-rewrite-dst-port 10000
```

验收结论：**rewrite_hit>0, rule_rewrite>0**

## P2 - 文档和报告补强

```text
1. ✅ 将 media-gateway-lite 从 PASS_SMOKE 升级到 PASS_TRAFFIC, report 已更新.
2. ✅ 为 fastpath-traffic-test 新增 pcap PMD 测试脚本 (scripts/06_run_pcap_rx_test.sh).
3. 将真实流量 records 写入 DPDK_TRACK_REPORT。
4. 简历 bullet 从”原型/smoke”升级为”真实流量转发/rewrite 验证”。
```

### fastpath-traffic-test pcap PMD 测试 — DONE (2026-06-07)

测试已运行并全部通过：

```text
Test 1 (traffic):  rx=111709760 ipv4=111709760 udp=111709760 tx=111709760
  verdict: PASS_SMOKE PASS_TRAFFIC PASS_FORWARDING

Test 2 (rewrite): rx=77210432 ipv4=77210432 udp=77210432 tx=77210432 rewrite=77210432
  verdict: PASS_SMOKE PASS_TRAFFIC PASS_FORWARDING PASS_REWRITE
```

记录位置：
- `records/20260607_155955-fastpath-pcap/`
- `records/20260607_160006-fastpath-pcap-rewrite/`

## 当前不做的事

```text
KNI 实现
完整 NAT/ALG
多线程 worker 调度
RSS/多队列性能调优
大规模压测
```

这些属于后续高级阶段，不影响当前 track 收口。
