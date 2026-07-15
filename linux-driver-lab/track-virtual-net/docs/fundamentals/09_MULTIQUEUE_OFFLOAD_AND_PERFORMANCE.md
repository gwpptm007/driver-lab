# 09：多队列、offload 与性能测量

## 为什么 ping 不能代表性能

ping 主要验证小包可达和往返时延。虚拟网络的吞吐、CPU 开销、尾延迟和丢包还受 queue 数量、NAPI/softirq、virtqueue 通知、TAP/bridge、offload、guest/host 调度和 NUMA 影响。性能实验要回答“在什么条件下、测量了什么”，而不是只给一个 Mbps 数字。

## queue 关系必须端到端描述

一个可能的多队列路径是：

```text
guest virtio-net RX/TX queue i
  <-> virtqueue i
  <-> QEMU/vhost backend queue i
  <-> TAP queue i
  <-> host CPU / bridge processing
```

真实映射由 feature negotiation、QEMU 参数、TAP multiqueue、host 内核和 guest driver 共同决定。队列数不同步时，可能退化为单队列或初始化失败；不要只从某一侧的配置项推断最终映射。

## RSS、IRQ、NAPI 与 CPU

- **RSS** 决定多流量如何散列到硬件/虚拟 RX queue；
- **IRQ/MSI-X** 通知 CPU 某队列有工作；
- **NAPI** 用 poll 批量处理 RX，避免每包中断；
- **softirq/ksoftirqd** 可能在负载下承担处理；
- **vhost worker/QEMU thread** 也需要 CPU 时间。

性能报告至少记录 CPU topology、CPU pinning、guest/host vCPU、queue 数和 IRQ/softirq 分布。没有这些上下文，无法比较两轮结果。

## offload 会改变你看见的“包”

| 特性 | 可能改变的现象 | 观察风险 |
| --- | --- | --- |
| checksum offload | 校验在更晚阶段完成 | 抓包可能显示未计算/异常 checksum |
| TSO/GSO | 大包稍后分段 | 应用写入次数不等于 wire packet 数 |
| GRO/LRO | 多个入站包在更高层合并 | socket/协议栈计数不等于接口帧数 |
| VLAN offload | tag 可能在 metadata 中 | 抓包点不同，VLAN 表现不同 |

offload 不是错误，但它会改变不同观测点的粒度。比较 host TAP、bridge、guest interface 和应用层吞吐前，先记录各端 offload 状态。

## 一个可复现的性能协议

1. 先运行功能基线：双向 ARP/ICMP、接口状态和 FDB 正常；
2. 固定 guest image、QEMU 版本、vCPU 数、CPU 亲和性、queue 和 offload；
3. 用固定时长/并发/包大小的 `iperf3` 或受控生成器；
4. 同时采集吞吐、p50/p99 latency（如适用）、CPU、softirq、drop/error、queue stats；
5. 预热后测多轮，报告中位数与波动，而不是只保留最好一轮；
6. 每次只改变一个因子，例如 `vhost=off/on` 或单/多队列。

## 低风险的先后顺序

先把单队列、vhost off/on、固定 workload 的对照做可信，再探索多队列和 offload。若环境是嵌套虚拟机、共享 CPU 或没有稳定流量发生器，应把性能部分记录为“方法和能力边界”，不要伪造硬件级结论。

## 从本 track 到后续项目

- `track-dpdk`：继续研究 userspace poll-mode、virtio-user/vhost-user 与 hugepage；
- `track-af-xdp`：对照 XDP/AF_XDP 的 queue/UMEM ownership；
- `track-rdma-core`：对照更显式的 CQ/QP/内存注册与 CPU/NUMA 约束。

它们共享“队列与所有权”问题，但不能直接比较数字，除非 workload、拓扑和测量方法一致。
