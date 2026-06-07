# eBPF Observability

## 路径定位

eBPF observability path 是整个项目的观测和定位层。它不负责替代 netdev、DPDK 或 AF_XDP 转发路径，而是负责回答：

```text
网络包在 RX、GRO、TX queue、TX xmit、drop 等关键节点上发生了什么？
不同 interface 和 CPU 上的事件分布如何？
路径不变量是否符合预期？
```

## 覆盖范围

对应主线：

```text
linux-driver-lab/track-ebpf-observability/project-linux-network-observability/
```

主要模块：

| 模块 | 说明 |
|------|------|
| BPF 程序 | 捕获网络路径事件 |
| 用户态 observer | 汇总 per-interface / per-CPU stats |
| report generator | 输出 Markdown 报告 |
| drop reason | 分类丢包原因 |
| path invariant | 分析 RX/GRO/TX/drop 关系 |

## 关键机制

这条路径关注：

- RX packet event。
- GRO event。
- TX queue event。
- TX xmit event。
- drop event。
- per-interface 聚合。
- per-CPU 分布。
- drop reason 分类。
- RX -> GRO、TX-QUEUE -> TX-XMIT 等路径不变量。

## 已证明内容

已有报告可以输出：

```text
Interface RX packets / bytes
GRO count
TX queue packets / bytes
TX xmit packets / bytes
DROP count
drop reason table
per-CPU event distribution
path invariant judgement
```

示例结论中已经能看到：

```text
TX-QUEUE -> TX-XMIT: 100% OK
DROP rate: 可统计并分类
RX -> GRO: 可做比例分析
```

## 和其他路径的关系

| 路径 | eBPF 观测价值 |
|------|---------------|
| Kernel netdev | 观测 RX/GRO/TX/drop 与 NAPI/协议栈路径 |
| Real driver | 验证真实驱动运行期事件链 |
| Virtual net | 定位 host/guest/bridge/tap 路径中的事件和 drop |
| AF_XDP | 配合 XDP action、redirect、drop 做路径验证 |
| DPDK | 对绕过内核的 DPDK 路径覆盖有限，需要结合 DPDK 自身 stats |

## Evidence 入口

主要证据索引：

- `../../track-ebpf-observability/project-linux-network-observability/src/net_observer.bpf.c`
- `../../track-ebpf-observability/project-linux-network-observability/src/net_observer.c`
- `../../track-ebpf-observability/project-linux-network-observability/reports/net-observe-20260606-193715.md`
- [../evidence/ebpf_observability_evidence.md](../evidence/ebpf_observability_evidence.md)

## 当前边界

准确表述：

- 已形成 Linux 网络路径观测工具和报告输出。
- 能按 interface、CPU、event、drop reason 做基础定位。

不要夸大：

- 不是完整 APM/网络可观测性平台。
- 当前更偏实验和诊断报告，不是生产长期采集系统。
