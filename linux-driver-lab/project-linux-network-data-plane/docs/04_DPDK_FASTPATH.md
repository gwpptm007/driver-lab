# DPDK Fastpath

## 路径定位

DPDK fastpath 是用户态数据面路径。它和 kernel netdev 最大的差异是绕过内核协议栈，由用户态 PMD 轮询收包、处理和发包。

这条路径要回答：

```text
如何从 Linux 内核网络路径切换到 DPDK 用户态 fastpath？
PMD、hugepage、devbind、EAL、mempool、rx_burst/tx_burst 分别解决什么问题？
一个简化媒体网关如何完成 UDP classify、forward、rewrite 和 stats？
```

## 覆盖范围

对应主线：

```text
linux-driver-lab/track-dpdk/
```

主要阶段：

| 阶段 | 目标 | 状态 |
|------|------|------|
| `lab-vmxnet3-testpmd` | DPDK 环境、hugepage、vmxnet3 PMD、testpmd stats | PASS |
| `lab-vhost-user-basic` | vhost-user backend socket 最小验证 | PASS |
| `lab-virtio-user-vhost` | virtio-user frontend + vhost-user backend 本机对接 | PASS_WITH_WARN |
| `lab-dpdk-l2-forwarding` | 从 testpmd 过渡到自写 C 数据面 | PASS_SMOKE |
| `project-user-space-fastpath` | fastpath-lite：分类、rewrite 框架、统计 | PASS_SMOKE |
| `project-fastpath-traffic-test` | 真实流量测试框架 | PASS_TRAFFIC/FORWARDING/REWRITE |
| `project-dpdk-media-gateway-lite` | UDP 媒体网关原型 | PASS_TRAFFIC/FORWARDING/REWRITE |
| `project-dpdk-v17-legacy-review` | 历史 DPDK v17 经验复盘和现代化迁移 | PASS_REVIEW |

## 关键机制

DPDK path 关注：

- hugepage 与大页内存。
- `dpdk-devbind.py` 和 PMD 接管。
- `uio_pci_generic` / `vfio-pci` 的环境边界。
- `rte_eal_init()`。
- `rte_pktmbuf_pool_create()`。
- `rte_eth_dev_configure()`。
- RX/TX queue setup。
- `rte_eth_rx_burst()` / `rte_eth_tx_burst()`。
- per-port/per-rule/drop reason stats。
- UDP-only classify、forward 和 header rewrite。

## 已证明内容

当前最硬的结论来自 `project-dpdk-media-gateway-lite`：

```text
PASS_BUILD
PASS_SMOKE
PASS_RULE_CONFIG
PASS_TRAFFIC
PASS_FORWARDING
PASS_REWRITE
```

在 pcap PMD path 中已验证：

```text
真实 IPv4/UDP 流量输入
port0 -> port1 转发
rule hit 非 0
rewrite hit 非 0
软件 stats 与 ethdev stats 对照
```

## 和其他路径的关系

| 对比对象 | 差异 |
|----------|------|
| Kernel netdev | DPDK 绕过内核协议栈，用户态轮询处理 |
| Virtual net | vhost-user/virtio-user 是 DPDK 虚拟化数据面入口 |
| AF_XDP | AF_XDP 保留 Linux 原生 XDP/driver hook，DPDK 走 PMD 生态 |
| eBPF observability | DPDK 自身需要软件 stats，内核 eBPF 对绕过内核的路径覆盖有限 |

## Evidence 入口

主要证据索引：

- `../../track-dpdk/project-dpdk-track-summary/reports/final/DPDK_TRACK_REPORT.md`
- `../../track-dpdk/project-dpdk-track-summary/reports/final/DPDK_RESUME_MATERIAL_FINAL.md`
- `../../track-dpdk/project-dpdk-media-gateway-lite/reports/project-dpdk-media-gateway-lite_report.md`
- [../evidence/dpdk_evidence.md](../evidence/dpdk_evidence.md)

## 当前边界

准确表述：

- 已完成 DPDK 用户态数据面主线和 media-gateway-lite 原型。
- pcap PMD path 下 UDP traffic/forwarding/rewrite 已验证。

不要夸大：

- 不是生产级 DPDK 媒体网关。
- 没有完成大规模吞吐压测、多线程调度、RSS、多队列生产优化、KNI 回注。
