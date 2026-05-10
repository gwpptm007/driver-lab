# DPDK_BACKLOG

> 用于记录 DPDK track 后续技术债。当前主线已经进入总结收口阶段，以下内容后续逐项回补。

## P0 - media-gateway-lite 真实流量闭环

### 1. 修 TX 成功后访问 mbuf 的潜在风险

问题：

```c
sent = rte_eth_tx_burst(..., m, 1);
if (sent == 1) {
    stats.port[r.out_port].tx++;
    stats.port[r.out_port].tx_bytes += rte_pktmbuf_pkt_len(m);
}
```

`tx_burst` 成功后 mbuf 所有权交给 PMD，后续不应继续访问 `m`。

修法：

```c
uint32_t pkt_len = rte_pktmbuf_pkt_len(m);
sent = rte_eth_tx_burst(..., &m, 1);
if (sent == 1) {
    stats.port[r.out_port].tx++;
    stats.port[r.out_port].tx_bytes += pkt_len;
}
```

### 2. 修 stats parser 累计重复相加问题

现象：周期打印的 stats 本身是累计值，解析脚本不应把每个周期都加起来。

修法：

```text
按每个 port / rule 取最后一次 sample
而不是所有 sample 求和
```

### 3. 新增真实 UDP 输入路径

优先路径：

```text
A. pcap PMD: 预生成 UDP pcap -> net_pcap rx -> media-gateway-lite -> net_null tx
B. vhost/virtio-user: testpmd txonly -> virtio-user -> vhost socket -> media-gateway-lite
C. 外部 VM/宿主机: 外部发包 -> ens192/vmxnet3 -> media-gateway-lite
```

验收目标：

```text
rx > 0
ipv4 > 0
udp > 0
```

## P1 - forwarding / rewrite 证据

### PASS_FORWARDING

验收目标：

```text
rule_hit > 0
tx > 0
drop_no_route 可解释
rte_eth_stats 或软件 stats 有对应证据
```

### PASS_REWRITE

验收目标：

```text
rewrite_hit > 0
rewrite_bytes > 0
如果可抓包，抓包里能看到 MAC/IP/UDP port 变化
```

## P2 - 文档和报告补强

```text
1. 将 media-gateway-lite 从 PASS_SMOKE 升级到 PASS_TRAFFIC 后更新 README/ROADMAP。
2. 将真实流量 records 写入 DPDK_TRACK_REPORT。
3. 简历 bullet 从“原型/smoke”升级为“真实流量转发/rewrite 验证”。
```

## 当前不做的事

```text
KNI 实现
完整 NAT/ALG
多线程 worker 调度
RSS/多队列性能调优
大规模压测
```

这些属于后续高级阶段，不影响当前 track 收口。
