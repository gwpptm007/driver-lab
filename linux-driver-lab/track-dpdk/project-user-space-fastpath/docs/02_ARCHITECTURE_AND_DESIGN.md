# 02_ARCHITECTURE_AND_DESIGN

## 数据路径

```
NIC / vdev / vhost / virtio-user port
        ↓
rte_eth_rx_burst
        ↓
classify_and_rewrite()
        ├── Ethernet header check
        ├── ARP count / pass
        ├── IPv4 count
        ├── UDP count
        ├── optional UDP-only drop
        └── optional MAC/IP/UDP rewrite
        ↓
rte_eth_tx_burst 或 free/drop
```

## 端口配对

默认按初始化顺序配对：

```
port0 <-> port1
port2 <-> port3
```

如果只有一个 port，程序不会失败，而是进入 smoke 模式：

```
RX → classify/rewrite → no_peer_drop/free
```

这适合当前 VMware 测试机只有一个专用 VMXNET3 DPDK 口的情况。

## 为什么保留单端口 smoke

DPDK 项目的第一步不是吞吐，而是证明：

- EAL 能起来
- hugepage 可用
- PMD 能 probe
- port/queue 能启动
- app loop 能跑
- stats 能输出

等这些稳定后，再接第二个端口或外部流量源验证真实 forwarding。

## 支持的 rewrite

`fastpath-lite` 支持：

```bash
--rewrite-src-mac 02:00:00:00:00:11
--rewrite-dst-mac 02:00:00:00:00:22
--rewrite-src-ip 10.10.1.10
--rewrite-dst-ip 10.10.2.20
--rewrite-src-port 5000
--rewrite-dst-port 6000
```

这些参数会自动打开 `--rewrite 1`。

## checksum 策略

- IPv4 地址变化后，重新计算 IPv4 header checksum
- UDP checksum 在 IPv4 下允许为 0，demo 中对 rewrite 后的 UDP checksum 置 0
- 这样避免第一版引入 TX checksum offload 配置复杂度

## 为什么第一版不做 NAT 表

真实项目通常是：

```text
5-tuple → flow table → rewrite action → stats/action counters
```

当前第一版先做固定 rewrite 参数，目的是让代码结构清楚、容易复现。后续增强可以增加：

- CSV/JSON flow rule 加载
- rte_hash flow table
- per-flow counters
- aging/timer
- control socket 动态下发策略