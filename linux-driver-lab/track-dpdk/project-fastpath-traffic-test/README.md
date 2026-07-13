# project-fastpath-traffic-test

> `project-user-space-fastpath` 的真实流量验证项目。

## 定位

这不是新的 fastpath 数据面实现，而是一个测试工程：复用上一站的 `fastpath-lite`，补齐真实 UDP 流量、UDP-only、rewrite 和统计对照。

## 当前状态

- ✅ `PASS_SMOKE` - 已验证（2026-05-07, vmxnet3）
- ✅ `PASS_PCAP_FUNCTIONAL` - pcap PMD 协议计数已验证
- ✅ `PASS_PCAP_REWRITE` - `REWRITE_ENABLE=1` rewrite 计数已验证
- ✅ `PASS_PCAP_FORWARDING` - pcap+null 双 vdev 所有权/计数闭环

旧脚本仍输出 `PASS_TRAFFIC/PASS_FORWARDING/PASS_REWRITE`；这些 marker 在本项目中只表示 pcap/vdev scope，不表示外部 wire 或真实 NIC 性能。

## 测试拓扑

### A. pcap PMD（推荐首选）

```text
gen_udp_pcap.py (UDP pcap)
    |
net_pcap0 (rx from pcap, infinite replay)
    |
fastpath-lite (classify -> forward)
    |
net_null0 (tx accept + discard)
```

无需物理网卡, 无需 sudo, 无需外部发包源.

```bash
./scripts/06_run_pcap_rx_test.sh                    # PASS_TRAFFIC + PASS_FORWARDING
REWRITE_ENABLE=1 ./scripts/06_run_pcap_rx_test.sh    # +PASS_REWRITE
```

### B. 单 VMXNET3 + 外部发包源

```text
外部机器/另一台 VM
    |
VMware 网络
    |
ens192/vmxnet3/0000:0b:00.0
    |
fastpath-lite RX/classify/free
```

适合当前测试机只有一个 DPDK 物理口的情况。

### C. 双端口或 vhost/virtio-user

```text
port0 -> fastpath-lite -> port1
```

用于 `PASS_FORWARDING`（物理端口场景）。

## 验收等级

| 等级 | 条件 |
|---|---|
| `PASS_SMOKE` | fastpath-lite 能启动并打印 stats |
| `PASS_TRAFFIC` | 真实流量使 rx/ipv4/udp 计数非 0 |
| `PASS_REWRITE` | rewrite 配置加载，且真实 UDP 流量命中 rewrite |
| `PASS_FORWARDING` | 双端口或 vhost/virtio 拓扑下 rx/tx 均非 0 |
